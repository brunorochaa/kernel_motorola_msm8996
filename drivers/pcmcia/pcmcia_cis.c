/*
 * PCMCIA high-level CIS access functions
 *
 * The initial developer of the original code is David A. Hinds
 * <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
 * are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.
 *
 * Copyright (C) 1999	     David A. Hinds
 * Copyright (C) 2004-2009   Dominik Brodowski
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>

#include <pcmcia/cs_types.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ss.h>
#include <pcmcia/cs.h>
#include <pcmcia/ds.h>
#include "cs_internal.h"


/**
 * pccard_read_tuple() - internal CIS tuple access
 * @s:		the struct pcmcia_socket where the card is inserted
 * @function:	the device function we loop for
 * @code:	which CIS code shall we look for?
 * @parse:	buffer where the tuple shall be parsed (or NULL, if no parse)
 *
 * pccard_read_tuple() reads out one tuple and attempts to parse it
 */
int pccard_read_tuple(struct pcmcia_socket *s, unsigned int function,
		cisdata_t code, void *parse)
{
	tuple_t tuple;
	cisdata_t *buf;
	int ret;

	buf = kmalloc(256, GFP_KERNEL);
	if (buf == NULL) {
		dev_printk(KERN_WARNING, &s->dev, "no memory to read tuple\n");
		return -ENOMEM;
	}
	tuple.DesiredTuple = code;
	tuple.Attributes = 0;
	if (function == BIND_FN_ALL)
		tuple.Attributes = TUPLE_RETURN_COMMON;
	ret = pccard_get_first_tuple(s, function, &tuple);
	if (ret != 0)
		goto done;
	tuple.TupleData = buf;
	tuple.TupleOffset = 0;
	tuple.TupleDataMax = 255;
	ret = pccard_get_tuple_data(s, &tuple);
	if (ret != 0)
		goto done;
	ret = pcmcia_parse_tuple(&tuple, parse);
done:
	kfree(buf);
	return ret;
}


/**
 * pccard_loop_tuple() - loop over tuples in the CIS
 * @s:		the struct pcmcia_socket where the card is inserted
 * @function:	the device function we loop for
 * @code:	which CIS code shall we look for?
 * @parse:	buffer where the tuple shall be parsed (or NULL, if no parse)
 * @priv_data:	private data to be passed to the loop_tuple function.
 * @loop_tuple:	function to call for each CIS entry of type @function. IT
 *		gets passed the raw tuple, the paresed tuple (if @parse is
 *		set) and @priv_data.
 *
 * pccard_loop_tuple() loops over all CIS entries of type @function, and
 * calls the @loop_tuple function for each entry. If the call to @loop_tuple
 * returns 0, the loop exits. Returns 0 on success or errorcode otherwise.
 */
int pccard_loop_tuple(struct pcmcia_socket *s, unsigned int function,
		      cisdata_t code, cisparse_t *parse, void *priv_data,
		      int (*loop_tuple) (tuple_t *tuple,
					 cisparse_t *parse,
					 void *priv_data))
{
	tuple_t tuple;
	cisdata_t *buf;
	int ret;

	buf = kzalloc(256, GFP_KERNEL);
	if (buf == NULL) {
		dev_printk(KERN_WARNING, &s->dev, "no memory to read tuple\n");
		return -ENOMEM;
	}

	tuple.TupleData = buf;
	tuple.TupleDataMax = 255;
	tuple.TupleOffset = 0;
	tuple.DesiredTuple = code;
	tuple.Attributes = 0;

	ret = pccard_get_first_tuple(s, function, &tuple);
	while (!ret) {
		if (pccard_get_tuple_data(s, &tuple))
			goto next_entry;

		if (parse)
			if (pcmcia_parse_tuple(&tuple, parse))
				goto next_entry;

		ret = loop_tuple(&tuple, parse, priv_data);
		if (!ret)
			break;

next_entry:
		ret = pccard_get_next_tuple(s, function, &tuple);
	}

	kfree(buf);
	return ret;
}

struct pcmcia_cfg_mem {
	struct pcmcia_device *p_dev;
	void *priv_data;
	int (*conf_check) (struct pcmcia_device *p_dev,
			   cistpl_cftable_entry_t *cfg,
			   cistpl_cftable_entry_t *dflt,
			   unsigned int vcc,
			   void *priv_data);
	cisparse_t parse;
	cistpl_cftable_entry_t dflt;
};

/**
 * pcmcia_do_loop_config() - internal helper for pcmcia_loop_config()
 *
 * pcmcia_do_loop_config() is the internal callback for the call from
 * pcmcia_loop_config() to pccard_loop_tuple(). Data is transferred
 * by a struct pcmcia_cfg_mem.
 */
static int pcmcia_do_loop_config(tuple_t *tuple, cisparse_t *parse, void *priv)
{
	cistpl_cftable_entry_t *cfg = &parse->cftable_entry;
	struct pcmcia_cfg_mem *cfg_mem = priv;

	/* default values */
	cfg_mem->p_dev->conf.ConfigIndex = cfg->index;
	if (cfg->flags & CISTPL_CFTABLE_DEFAULT)
		cfg_mem->dflt = *cfg;

	return cfg_mem->conf_check(cfg_mem->p_dev, cfg, &cfg_mem->dflt,
				   cfg_mem->p_dev->socket->socket.Vcc,
				   cfg_mem->priv_data);
}

/**
 * pcmcia_loop_config() - loop over configuration options
 * @p_dev:	the struct pcmcia_device which we need to loop for.
 * @conf_check:	function to call for each configuration option.
 *		It gets passed the struct pcmcia_device, the CIS data
 *		describing the configuration option, and private data
 *		being passed to pcmcia_loop_config()
 * @priv_data:	private data to be passed to the conf_check function.
 *
 * pcmcia_loop_config() loops over all configuration options, and calls
 * the driver-specific conf_check() for each one, checking whether
 * it is a valid one. Returns 0 on success or errorcode otherwise.
 */
int pcmcia_loop_config(struct pcmcia_device *p_dev,
		       int	(*conf_check)	(struct pcmcia_device *p_dev,
						 cistpl_cftable_entry_t *cfg,
						 cistpl_cftable_entry_t *dflt,
						 unsigned int vcc,
						 void *priv_data),
		       void *priv_data)
{
	struct pcmcia_cfg_mem *cfg_mem;
	int ret;

	cfg_mem = kzalloc(sizeof(struct pcmcia_cfg_mem), GFP_KERNEL);
	if (cfg_mem == NULL)
		return -ENOMEM;

	cfg_mem->p_dev = p_dev;
	cfg_mem->conf_check = conf_check;
	cfg_mem->priv_data = priv_data;

	ret = pccard_loop_tuple(p_dev->socket, p_dev->func,
				CISTPL_CFTABLE_ENTRY, &cfg_mem->parse,
				cfg_mem, pcmcia_do_loop_config);

	kfree(cfg_mem);
	return ret;
}
EXPORT_SYMBOL(pcmcia_loop_config);


struct pcmcia_loop_mem {
	struct pcmcia_device *p_dev;
	void *priv_data;
	int (*loop_tuple) (struct pcmcia_device *p_dev,
			   tuple_t *tuple,
			   void *priv_data);
};

/**
 * pcmcia_do_loop_tuple() - internal helper for pcmcia_loop_config()
 *
 * pcmcia_do_loop_tuple() is the internal callback for the call from
 * pcmcia_loop_tuple() to pccard_loop_tuple(). Data is transferred
 * by a struct pcmcia_cfg_mem.
 */
static int pcmcia_do_loop_tuple(tuple_t *tuple, cisparse_t *parse, void *priv)
{
	struct pcmcia_loop_mem *loop = priv;

	return loop->loop_tuple(loop->p_dev, tuple, loop->priv_data);
};

/**
 * pcmcia_loop_tuple() - loop over tuples in the CIS
 * @p_dev:	the struct pcmcia_device which we need to loop for.
 * @code:	which CIS code shall we look for?
 * @priv_data:	private data to be passed to the loop_tuple function.
 * @loop_tuple:	function to call for each CIS entry of type @function. IT
 *		gets passed the raw tuple and @priv_data.
 *
 * pcmcia_loop_tuple() loops over all CIS entries of type @function, and
 * calls the @loop_tuple function for each entry. If the call to @loop_tuple
 * returns 0, the loop exits. Returns 0 on success or errorcode otherwise.
 */
int pcmcia_loop_tuple(struct pcmcia_device *p_dev, cisdata_t code,
		      int (*loop_tuple) (struct pcmcia_device *p_dev,
					 tuple_t *tuple,
					 void *priv_data),
		      void *priv_data)
{
	struct pcmcia_loop_mem loop = {
		.p_dev = p_dev,
		.loop_tuple = loop_tuple,
		.priv_data = priv_data};

	return pccard_loop_tuple(p_dev->socket, p_dev->func, code, NULL,
				 &loop, pcmcia_do_loop_tuple);
}
EXPORT_SYMBOL(pcmcia_loop_tuple);


struct pcmcia_loop_get {
	size_t len;
	cisdata_t **buf;
};

/**
 * pcmcia_do_get_tuple() - internal helper for pcmcia_get_tuple()
 *
 * pcmcia_do_get_tuple() is the internal callback for the call from
 * pcmcia_get_tuple() to pcmcia_loop_tuple(). As we're only interested in
 * the first tuple, return 0 unconditionally. Create a memory buffer large
 * enough to hold the content of the tuple, and fill it with the tuple data.
 * The caller is responsible to free the buffer.
 */
static int pcmcia_do_get_tuple(struct pcmcia_device *p_dev, tuple_t *tuple,
			       void *priv)
{
	struct pcmcia_loop_get *get = priv;

	*get->buf = kzalloc(tuple->TupleDataLen, GFP_KERNEL);
	if (*get->buf) {
		get->len = tuple->TupleDataLen;
		memcpy(*get->buf, tuple->TupleData, tuple->TupleDataLen);
	} else
		dev_dbg(&p_dev->dev, "do_get_tuple: out of memory\n");
	return 0;
}

/**
 * pcmcia_get_tuple() - get first tuple from CIS
 * @p_dev:	the struct pcmcia_device which we need to loop for.
 * @code:	which CIS code shall we look for?
 * @buf:        pointer to store the buffer to.
 *
 * pcmcia_get_tuple() gets the content of the first CIS entry of type @code.
 * It returns the buffer length (or zero). The caller is responsible to free
 * the buffer passed in @buf.
 */
size_t pcmcia_get_tuple(struct pcmcia_device *p_dev, cisdata_t code,
			unsigned char **buf)
{
	struct pcmcia_loop_get get = {
		.len = 0,
		.buf = buf,
	};

	*get.buf = NULL;
	pcmcia_loop_tuple(p_dev, code, pcmcia_do_get_tuple, &get);

	return get.len;
}
EXPORT_SYMBOL(pcmcia_get_tuple);


/**
 * pcmcia_do_get_mac() - internal helper for pcmcia_get_mac_from_cis()
 *
 * pcmcia_do_get_mac() is the internal callback for the call from
 * pcmcia_get_mac_from_cis() to pcmcia_loop_tuple(). We check whether the
 * tuple contains a proper LAN_NODE_ID of length 6, and copy the data
 * to struct net_device->dev_addr[i].
 */
static int pcmcia_do_get_mac(struct pcmcia_device *p_dev, tuple_t *tuple,
			     void *priv)
{
	struct net_device *dev = priv;
	int i;

	if (tuple->TupleData[0] != CISTPL_FUNCE_LAN_NODE_ID)
		return -EINVAL;
	if (tuple->TupleDataLen < ETH_ALEN + 2) {
		dev_warn(&p_dev->dev, "Invalid CIS tuple length for "
			"LAN_NODE_ID\n");
		return -EINVAL;
	}

	if (tuple->TupleData[1] != ETH_ALEN) {
		dev_warn(&p_dev->dev, "Invalid header for LAN_NODE_ID\n");
		return -EINVAL;
	}
	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = tuple->TupleData[i+2];
	return 0;
}

/**
 * pcmcia_get_mac_from_cis() - read out MAC address from CISTPL_FUNCE
 * @p_dev:	the struct pcmcia_device for which we want the address.
 * @dev:	a properly prepared struct net_device to store the info to.
 *
 * pcmcia_get_mac_from_cis() reads out the hardware MAC address from
 * CISTPL_FUNCE and stores it into struct net_device *dev->dev_addr which
 * must be set up properly by the driver (see examples!).
 */
int pcmcia_get_mac_from_cis(struct pcmcia_device *p_dev, struct net_device *dev)
{
	return pcmcia_loop_tuple(p_dev, CISTPL_FUNCE, pcmcia_do_get_mac, dev);
}
EXPORT_SYMBOL(pcmcia_get_mac_from_cis);

