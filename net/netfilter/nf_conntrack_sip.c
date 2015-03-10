/* SIP extension for IP connection tracking.
 *
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 * (C) 2005 by Christian Hentschel <chentschel@arnet.com.ar>
 * based on RR's ip_conntrack_ftp.c and other modules.
 * (C) 2007 United Security Providers
 * (C) 2007, 2008 Patrick McHardy <kaber@trash.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/skbuff.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/netfilter.h>
#include <net/tcp.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include <linux/netfilter/nf_conntrack_sip.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_l3proto.h>
#include <net/netfilter/nf_nat_l4proto.h>
#include <net/netfilter/nf_queue.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Christian Hentschel <chentschel@arnet.com.ar>");
MODULE_DESCRIPTION("SIP connection tracking helper");
MODULE_ALIAS("ip_conntrack_sip");
MODULE_ALIAS_NFCT_HELPER("sip");

#define MAX_PORTS	8
static unsigned short ports[MAX_PORTS];
static unsigned int ports_c;
module_param_array(ports, ushort, &ports_c, 0400);
MODULE_PARM_DESC(ports, "port numbers of SIP servers");

static unsigned int sip_timeout __read_mostly = SIP_TIMEOUT;
module_param(sip_timeout, uint, 0600);
MODULE_PARM_DESC(sip_timeout, "timeout for the master SIP session");

static int sip_direct_signalling __read_mostly = 1;
module_param(sip_direct_signalling, int, 0600);
MODULE_PARM_DESC(sip_direct_signalling, "expect incoming calls from registrar "
					"only (default 1)");

const struct nf_nat_sip_hooks *nf_nat_sip_hooks;
EXPORT_SYMBOL_GPL(nf_nat_sip_hooks);
static struct ctl_table_header *sip_sysctl_header;
static unsigned nf_ct_disable_sip_alg;
static int sip_direct_media = 1;
static unsigned nf_ct_enable_sip_segmentation;
static int packet_count;
static
int proc_sip_segment(struct ctl_table *ctl, int write,
		     void __user *buffer, size_t *lenp, loff_t *ppos);

static struct ctl_table sip_sysctl_tbl[] = {
	{
		.procname     = "nf_conntrack_disable_sip_alg",
		.data         = &nf_ct_disable_sip_alg,
		.maxlen       = sizeof(unsigned int),
		.mode         = 0644,
		.proc_handler = proc_dointvec,
	},
	{
		.procname     = "nf_conntrack_sip_direct_media",
		.data         = &sip_direct_media,
		.maxlen       = sizeof(int),
		.mode         = 0644,
		.proc_handler = proc_dointvec,
	},
	{
		.procname     = "nf_conntrack_enable_sip_segmentation",
		.data         = &nf_ct_enable_sip_segmentation,
		.maxlen       = sizeof(unsigned int),
		.mode         = 0644,
		.proc_handler = proc_sip_segment,
	},
	{}
};

unsigned int (*nf_nat_sip_hook)(struct sk_buff *skb, unsigned int protoff,
				unsigned int dataoff, const char **dptr,
				unsigned int *datalen) __read_mostly;

static void sip_calculate_parameters(s16 *diff, s16 *tdiff,
				     unsigned int *dataoff, const char **dptr,
				     unsigned int *datalen,
				     unsigned int msglen, unsigned int origlen)
{
	*diff	 = msglen - origlen;
	*tdiff	+= *diff;
	*dataoff += msglen;
	*dptr	+= msglen;
	*datalen  = *datalen + *diff - msglen;
}

static void sip_update_params(enum ip_conntrack_dir dir,
			      unsigned int *msglen, unsigned int *origlen,
			      const char **dptr, unsigned int *datalen,
			      bool skb_is_combined, struct nf_conn *ct)
{
	if (skb_is_combined) {
		/* The msglen of first skb has the total msg length of
		 * the two fragments. hence after combining,we update
		 * the msglen to that of the msglen of first skb
		 */
		*msglen = (dir == IP_CT_DIR_ORIGINAL) ?
		  ct->segment.msg_length[0] : ct->segment.msg_length[1];
		*origlen = *msglen;
		*dptr = ct->dptr_prev;
		*datalen = *msglen;
	}
}

/* This function is to save all the information of the first segment
 * that will be needed for combining the two segments
 */
static bool sip_save_segment_info(struct nf_conn *ct, struct sk_buff *skb,
				  unsigned int msglen, unsigned int datalen,
				  const char *dptr,
				  enum ip_conntrack_info ctinfo)
{
	enum ip_conntrack_dir dir = IP_CT_DIR_MAX;
	bool skip = false;

	/* one set of information is saved per direction ,also only one segment
	 * per direction is queued based on the assumption that after the first
	 * complete message leaves the kernel, only then the next fragmented
	 * segment will reach the kernel
	 */
	dir = CTINFO2DIR(ctinfo);
	if (dir == IP_CT_DIR_ORIGINAL) {
		/* here we check if there is already an element queued for this
		 * direction, in that case we do not queue the next element,we
		 * make skip 1.ideally this scenario should never be hit
		 */
		if (ct->sip_original_dir == 1) {
			skip = true;
		} else {
			ct->segment.msg_length[0] = msglen;
			ct->segment.data_len[0] = datalen;
			ct->segment.skb_len[0] = skb->len;
			ct->dptr_prev = dptr;
			ct->sip_original_dir = 1;
			skip = false;
		}
	} else {
		if (ct->sip_reply_dir == 1) {
			skip = true;
		} else {
			ct->segment.msg_length[1] = msglen;
			ct->segment.data_len[1] = datalen;
			ct->segment.skb_len[1] = skb->len;
			ct->dptr_prev = dptr;
			ct->sip_reply_dir = 1;
			skip = false;
		}
	}
	return skip;
}

static struct sip_list *sip_coalesce_segments(struct nf_conn *ct,
					      struct sk_buff **skb_ref,
					      unsigned int dataoff,
					      struct sk_buff **combined_skb_ref,
					      bool *skip_sip_process,
					      bool do_not_process,
					      enum ip_conntrack_info ctinfo,
					      bool *success)

{
	struct list_head *list_trav_node;
	struct list_head *list_backup_node;
	struct nf_conn *ct_list;
	enum ip_conntrack_info ctinfo_list;
	enum ip_conntrack_dir dir_list;
	enum ip_conntrack_dir dir = IP_CT_DIR_MAX;
	const struct tcphdr *th_old;
	unsigned int prev_data_len;
	unsigned int seq_no, seq_old, exp_seq_no;
	const struct tcphdr *th_new;
	bool fragstolen = false;
	int delta_truesize = 0;
	struct sip_list *sip_entry = NULL;

	th_new = (struct tcphdr *)(skb_network_header(*skb_ref) +
		ip_hdrlen(*skb_ref));
	seq_no = ntohl(th_new->seq);
	dir = CTINFO2DIR(ctinfo);
	/* traverse the list it would have 1 or 2 elements. 1 element per
	 * direction at max
	 */
	list_for_each_safe(list_trav_node, list_backup_node,
			   &ct->sip_segment_list){
		sip_entry = list_entry(list_trav_node, struct sip_list, list);
		ct_list = nf_ct_get(sip_entry->entry->skb, &ctinfo_list);
		dir_list = CTINFO2DIR(ctinfo_list);
		/* take an element and check if its direction matches with the
		 * current one
		 */
		if (dir_list == dir) {
			/* once we have the two elements to be combined we do
			 * another check. match the next expected seq no of the
			 * packet in the list with the seq no of the current
			 * packet.this is to be protected  against out of order
			 * fragments
			 */
			th_old = ((struct tcphdr *)(skb_network_header
				(sip_entry->entry->skb) +
				ip_hdrlen(sip_entry->entry->skb)));

			prev_data_len = (dir == IP_CT_DIR_ORIGINAL) ?
			 ct->segment.data_len[0] : ct->segment.data_len[1];
			seq_old = (ntohl(th_old->seq));
			exp_seq_no = seq_old+prev_data_len;

			if (exp_seq_no == seq_no) {
				/* Found packets to be combined.Pull header from
				 * second skb when preparing combined skb.This
				 * shifts the second skb start pointer to its
				 * data that was initially at the start of its
				 * headers.This so that the  combined skb has
				 * the tcp ip headerof the first skb followed
				 * by the data of first skb followed by the data
				 * of second skb.
				 */
				skb_pull(*skb_ref, dataoff);
				if (skb_try_coalesce(sip_entry->entry->skb,
						     *skb_ref, &fragstolen,
							&delta_truesize)) {
					pr_debug(" Combining segments\n");
					*combined_skb_ref =
							  sip_entry->entry->skb;
					*success = true;
					list_del(list_trav_node);
					} else{
					skb_push(*skb_ref, dataoff);
					}
			}
		} else if (do_not_process) {
			*skip_sip_process = true;
		}
	}
	return sip_entry;
}

static void recalc_header(struct sk_buff *skb, unsigned int skblen,
			  unsigned int oldlen, unsigned int protoff)
{
	unsigned int datalen;
	struct tcphdr *tcph;
	const struct nf_nat_l3proto *l3proto;

	/* here we recalculate ip and tcp headers */
	if (nf_ct_l3num((struct nf_conn *)skb->nfct) == NFPROTO_IPV4) {
			/* fix IP hdr checksum information */
			ip_hdr(skb)->tot_len = htons(skblen);
			ip_send_check(ip_hdr(skb));
	} else {
		ipv6_hdr(skb)->payload_len =
				htons(skblen - sizeof(struct ipv6hdr));
	}
	datalen = skb->len - protoff;
	tcph = (struct tcphdr *)((void *)skb->data + protoff);
	l3proto = __nf_nat_l3proto_find(nf_ct_l3num
					((struct nf_conn *)skb->nfct));
	l3proto->csum_recalc(skb, IPPROTO_TCP, tcph, &tcph->check,
			     datalen, oldlen);
}
EXPORT_SYMBOL(nf_nat_sip_hook);
void (*nf_nat_sip_seq_adjust_hook)(struct sk_buff *skb, unsigned int protoff,
				   s16 off) __read_mostly;
EXPORT_SYMBOL(nf_nat_sip_seq_adjust_hook);
unsigned int (*nf_nat_sip_expect_hook)(struct sk_buff *skb,
				       unsigned int protoff,
				       unsigned int dataoff,
				       const char **dptr,
				       unsigned int *datalen,
				       struct nf_conntrack_expect *exp,
				       unsigned int matchoff,
				       unsigned int matchlen) __read_mostly;
EXPORT_SYMBOL(nf_nat_sip_expect_hook);

unsigned int (*nf_nat_sdp_addr_hook)(struct sk_buff *skb, unsigned int protoff,
				     unsigned int dataoff,
				     const char **dptr,
				     unsigned int *datalen,
				     unsigned int sdpoff,
				     enum sdp_header_types type,
				     enum sdp_header_types term,
				     const union nf_inet_addr *addr)
				     __read_mostly;
EXPORT_SYMBOL(nf_nat_sdp_addr_hook);

unsigned int (*nf_nat_sdp_port_hook)(struct sk_buff *skb, unsigned int protoff,
				     unsigned int dataoff,
				     const char **dptr,
				     unsigned int *datalen,
				     unsigned int matchoff,
				     unsigned int matchlen,
				     u_int16_t port) __read_mostly;
EXPORT_SYMBOL(nf_nat_sdp_port_hook);

unsigned int (*nf_nat_sdp_session_hook)(struct sk_buff *skb,
					unsigned int protoff,
					unsigned int dataoff,
					const char **dptr,
					unsigned int *datalen,
					unsigned int sdpoff,
					const union nf_inet_addr *addr)
					__read_mostly;
EXPORT_SYMBOL(nf_nat_sdp_session_hook);

unsigned int (*nf_nat_sdp_media_hook)(struct sk_buff *skb, unsigned int protoff,
				      unsigned int dataoff,
				      const char **dptr,
				      unsigned int *datalen,
				      struct nf_conntrack_expect *rtp_exp,
				      struct nf_conntrack_expect *rtcp_exp,
				      unsigned int mediaoff,
				      unsigned int medialen,
				      union nf_inet_addr *rtp_addr)
				      __read_mostly;
EXPORT_SYMBOL(nf_nat_sdp_media_hook);

static int string_len(const struct nf_conn *ct, const char *dptr,
		      const char *limit, int *shift)
{
	int len = 0;

	while (dptr < limit && isalpha(*dptr)) {
		dptr++;
		len++;
	}
	return len;
}

static int nf_sip_enqueue_packet(struct nf_queue_entry *entry,
				 unsigned int queuenum)
{
	enum ip_conntrack_info ctinfo_list;
	struct nf_conn *ct_temp;
	struct sip_list *node = kzalloc(sizeof(*node),
			GFP_ATOMIC | __GFP_NOWARN);
	if (!node)
		return XT_CONTINUE;

	ct_temp = nf_ct_get(entry->skb, &ctinfo_list);
	node->entry = entry;
	list_add(&node->list, &ct_temp->sip_segment_list);
	return 0;
}

static const struct nf_queue_handler nf_sip_qh = {
	.outfn	= &nf_sip_enqueue_packet,
};

static
int proc_sip_segment(struct ctl_table *ctl, int write,
		     void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (nf_ct_enable_sip_segmentation) {
		pr_debug("registering queue handler\n");
		nf_register_queue_handler(&nf_sip_qh);
	} else {
		pr_debug("de-registering queue handler\n");
		nf_unregister_queue_handler();
	}
	return ret;
}

static int digits_len(const struct nf_conn *ct, const char *dptr,
		      const char *limit, int *shift)
{
	int len = 0;
	while (dptr < limit && isdigit(*dptr)) {
		dptr++;
		len++;
	}
	return len;
}

static int iswordc(const char c)
{
	if (isalnum(c) || c == '!' || c == '"' || c == '%' ||
	    (c >= '(' && c <= '/') || c == ':' || c == '<' || c == '>' ||
	    c == '?' || (c >= '[' && c <= ']') || c == '_' || c == '`' ||
	    c == '{' || c == '}' || c == '~')
		return 1;
	return 0;
}

static int word_len(const char *dptr, const char *limit)
{
	int len = 0;
	while (dptr < limit && iswordc(*dptr)) {
		dptr++;
		len++;
	}
	return len;
}

static int callid_len(const struct nf_conn *ct, const char *dptr,
		      const char *limit, int *shift)
{
	int len, domain_len;

	len = word_len(dptr, limit);
	dptr += len;
	if (!len || dptr == limit || *dptr != '@')
		return len;
	dptr++;
	len++;

	domain_len = word_len(dptr, limit);
	if (!domain_len)
		return 0;
	return len + domain_len;
}

/* get media type + port length */
static int media_len(const struct nf_conn *ct, const char *dptr,
		     const char *limit, int *shift)
{
	int len = string_len(ct, dptr, limit, shift);

	dptr += len;
	if (dptr >= limit || *dptr != ' ')
		return 0;
	len++;
	dptr++;

	return len + digits_len(ct, dptr, limit, shift);
}

static int sip_parse_addr(const struct nf_conn *ct, const char *cp,
			  const char **endp, union nf_inet_addr *addr,
			  const char *limit, bool delim)
{
	const char *end;
	int ret;

	if (!ct)
		return 0;

	memset(addr, 0, sizeof(*addr));
	switch (nf_ct_l3num(ct)) {
	case AF_INET:
		ret = in4_pton(cp, limit - cp, (u8 *)&addr->ip, -1, &end);
		if (ret == 0)
			return 0;
		break;
	case AF_INET6:
		if (cp < limit && *cp == '[')
			cp++;
		else if (delim)
			return 0;

		ret = in6_pton(cp, limit - cp, (u8 *)&addr->ip6, -1, &end);
		if (ret == 0)
			return 0;

		if (end < limit && *end == ']')
			end++;
		else if (delim)
			return 0;
		break;
	default:
		BUG();
	}

	if (endp)
		*endp = end;
	return 1;
}

/* skip ip address. returns its length. */
static int epaddr_len(const struct nf_conn *ct, const char *dptr,
		      const char *limit, int *shift)
{
	union nf_inet_addr addr;
	const char *aux = dptr;

	if (!sip_parse_addr(ct, dptr, &dptr, &addr, limit, true)) {
		pr_debug("ip: %s parse failed.!\n", dptr);
		return 0;
	}

	/* Port number */
	if (*dptr == ':') {
		dptr++;
		dptr += digits_len(ct, dptr, limit, shift);
	}
	return dptr - aux;
}

/* get address length, skiping user info. */
static int skp_epaddr_len(const struct nf_conn *ct, const char *dptr,
			  const char *limit, int *shift)
{
	const char *start = dptr;
	int s = *shift;

	/* Search for @, but stop at the end of the line.
	 * We are inside a sip: URI, so we don't need to worry about
	 * continuation lines. */
	while (dptr < limit &&
	       *dptr != '@' && *dptr != '\r' && *dptr != '\n') {
		(*shift)++;
		dptr++;
	}

	if (dptr < limit && *dptr == '@') {
		dptr++;
		(*shift)++;
	} else {
		dptr = start;
		*shift = s;
	}

	return epaddr_len(ct, dptr, limit, shift);
}

/* Parse a SIP request line of the form:
 *
 * Request-Line = Method SP Request-URI SP SIP-Version CRLF
 *
 * and return the offset and length of the address contained in the Request-URI.
 */
int ct_sip_parse_request(const struct nf_conn *ct,
			 const char *dptr, unsigned int datalen,
			 unsigned int *matchoff, unsigned int *matchlen,
			 union nf_inet_addr *addr, __be16 *port)
{
	const char *start = dptr, *limit = dptr + datalen, *end;
	unsigned int mlen;
	unsigned int p;
	int shift = 0;

	/* Skip method and following whitespace */
	mlen = string_len(ct, dptr, limit, NULL);
	if (!mlen)
		return 0;
	dptr += mlen;
	if (++dptr >= limit)
		return 0;

	/* Find SIP URI */
	for (; dptr < limit - strlen("sip:"); dptr++) {
		if (*dptr == '\r' || *dptr == '\n')
			return -1;
		if (strncasecmp(dptr, "sip:", strlen("sip:")) == 0) {
			dptr += strlen("sip:");
			break;
		}
	}
	if (!skp_epaddr_len(ct, dptr, limit, &shift))
		return 0;
	dptr += shift;

	if (!sip_parse_addr(ct, dptr, &end, addr, limit, true))
		return -1;
	if (end < limit && *end == ':') {
		end++;
		p = simple_strtoul(end, (char **)&end, 10);
		if (p < 1024 || p > 65535)
			return -1;
		*port = htons(p);
	} else
		*port = htons(SIP_PORT);

	if (end == dptr)
		return 0;
	*matchoff = dptr - start;
	*matchlen = end - dptr;
	return 1;
}
EXPORT_SYMBOL_GPL(ct_sip_parse_request);

/* SIP header parsing: SIP headers are located at the beginning of a line, but
 * may span several lines, in which case the continuation lines begin with a
 * whitespace character. RFC 2543 allows lines to be terminated with CR, LF or
 * CRLF, RFC 3261 allows only CRLF, we support both.
 *
 * Headers are followed by (optionally) whitespace, a colon, again (optionally)
 * whitespace and the values. Whitespace in this context means any amount of
 * tabs, spaces and continuation lines, which are treated as a single whitespace
 * character.
 *
 * Some headers may appear multiple times. A comma separated list of values is
 * equivalent to multiple headers.
 */
static const struct sip_header ct_sip_hdrs[] = {
	[SIP_HDR_CSEQ]			= SIP_HDR("CSeq", NULL, NULL, digits_len),
	[SIP_HDR_FROM]			= SIP_HDR("From", "f", "sip:", skp_epaddr_len),
	[SIP_HDR_TO]			= SIP_HDR("To", "t", "sip:", skp_epaddr_len),
	[SIP_HDR_CONTACT]		= SIP_HDR("Contact", "m", "sip:", skp_epaddr_len),
	[SIP_HDR_VIA_UDP]		= SIP_HDR("Via", "v", "UDP ", epaddr_len),
	[SIP_HDR_VIA_TCP]		= SIP_HDR("Via", "v", "TCP ", epaddr_len),
	[SIP_HDR_EXPIRES]		= SIP_HDR("Expires", NULL, NULL, digits_len),
	[SIP_HDR_CONTENT_LENGTH]	= SIP_HDR("Content-Length", "l", NULL, digits_len),
	[SIP_HDR_CALL_ID]		= SIP_HDR("Call-Id", "i", NULL, callid_len),
};

static const char *sip_follow_continuation(const char *dptr, const char *limit)
{
	/* Walk past newline */
	if (++dptr >= limit)
		return NULL;

	/* Skip '\n' in CR LF */
	if (*(dptr - 1) == '\r' && *dptr == '\n') {
		if (++dptr >= limit)
			return NULL;
	}

	/* Continuation line? */
	if (*dptr != ' ' && *dptr != '\t')
		return NULL;

	/* skip leading whitespace */
	for (; dptr < limit; dptr++) {
		if (*dptr != ' ' && *dptr != '\t')
			break;
	}
	return dptr;
}

static const char *sip_skip_whitespace(const char *dptr, const char *limit)
{
	for (; dptr < limit; dptr++) {
		if (*dptr == ' ')
			continue;
		if (*dptr != '\r' && *dptr != '\n')
			break;
		dptr = sip_follow_continuation(dptr, limit);
		if (dptr == NULL)
			return NULL;
	}
	return dptr;
}

/* Search within a SIP header value, dealing with continuation lines */
static const char *ct_sip_header_search(const char *dptr, const char *limit,
					const char *needle, unsigned int len)
{
	for (limit -= len; dptr < limit; dptr++) {
		if (*dptr == '\r' || *dptr == '\n') {
			dptr = sip_follow_continuation(dptr, limit);
			if (dptr == NULL)
				break;
			continue;
		}

		if (strncasecmp(dptr, needle, len) == 0)
			return dptr;
	}
	return NULL;
}

int ct_sip_get_header(const struct nf_conn *ct, const char *dptr,
		      unsigned int dataoff, unsigned int datalen,
		      enum sip_header_types type,
		      unsigned int *matchoff, unsigned int *matchlen)
{
	const struct sip_header *hdr = &ct_sip_hdrs[type];
	const char *start = dptr, *limit = dptr + datalen;
	int shift = 0;

	for (dptr += dataoff; dptr < limit; dptr++) {
		/* Find beginning of line */
		if (*dptr != '\r' && *dptr != '\n')
			continue;
		if (++dptr >= limit)
			break;
		if (*(dptr - 1) == '\r' && *dptr == '\n') {
			if (++dptr >= limit)
				break;
		}

		/* Skip continuation lines */
		if (*dptr == ' ' || *dptr == '\t')
			continue;

		/* Find header. Compact headers must be followed by a
		 * non-alphabetic character to avoid mismatches. */
		if (limit - dptr >= hdr->len &&
		    strncasecmp(dptr, hdr->name, hdr->len) == 0)
			dptr += hdr->len;
		else if (hdr->cname && limit - dptr >= hdr->clen + 1 &&
			 strncasecmp(dptr, hdr->cname, hdr->clen) == 0 &&
			 !isalpha(*(dptr + hdr->clen)))
			dptr += hdr->clen;
		else
			continue;

		/* Find and skip colon */
		dptr = sip_skip_whitespace(dptr, limit);
		if (dptr == NULL)
			break;
		if (*dptr != ':' || ++dptr >= limit)
			break;

		/* Skip whitespace after colon */
		dptr = sip_skip_whitespace(dptr, limit);
		if (dptr == NULL)
			break;

		*matchoff = dptr - start;
		if (hdr->search) {
			dptr = ct_sip_header_search(dptr, limit, hdr->search,
						    hdr->slen);
			if (!dptr)
				return -1;
			dptr += hdr->slen;
		}

		*matchlen = hdr->match_len(ct, dptr, limit, &shift);
		if (!*matchlen)
			return -1;
		*matchoff = dptr - start + shift;
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(ct_sip_get_header);

/* Get next header field in a list of comma separated values */
static int ct_sip_next_header(const struct nf_conn *ct, const char *dptr,
			      unsigned int dataoff, unsigned int datalen,
			      enum sip_header_types type,
			      unsigned int *matchoff, unsigned int *matchlen)
{
	const struct sip_header *hdr = &ct_sip_hdrs[type];
	const char *start = dptr, *limit = dptr + datalen;
	int shift = 0;

	dptr += dataoff;

	dptr = ct_sip_header_search(dptr, limit, ",", strlen(","));
	if (!dptr)
		return 0;

	dptr = ct_sip_header_search(dptr, limit, hdr->search, hdr->slen);
	if (!dptr)
		return 0;
	dptr += hdr->slen;

	*matchoff = dptr - start;
	*matchlen = hdr->match_len(ct, dptr, limit, &shift);
	if (!*matchlen)
		return -1;
	*matchoff += shift;
	return 1;
}

/* Walk through headers until a parsable one is found or no header of the
 * given type is left. */
static int ct_sip_walk_headers(const struct nf_conn *ct, const char *dptr,
			       unsigned int dataoff, unsigned int datalen,
			       enum sip_header_types type, int *in_header,
			       unsigned int *matchoff, unsigned int *matchlen)
{
	int ret;

	if (in_header && *in_header) {
		while (1) {
			ret = ct_sip_next_header(ct, dptr, dataoff, datalen,
						 type, matchoff, matchlen);
			if (ret > 0)
				return ret;
			if (ret == 0)
				break;
			dataoff += *matchoff;
		}
		*in_header = 0;
	}

	while (1) {
		ret = ct_sip_get_header(ct, dptr, dataoff, datalen,
					type, matchoff, matchlen);
		if (ret > 0)
			break;
		if (ret == 0)
			return ret;
		dataoff += *matchoff;
	}

	if (in_header)
		*in_header = 1;
	return 1;
}

/* Locate a SIP header, parse the URI and return the offset and length of
 * the address as well as the address and port themselves. A stream of
 * headers can be parsed by handing in a non-NULL datalen and in_header
 * pointer.
 */
int ct_sip_parse_header_uri(const struct nf_conn *ct, const char *dptr,
			    unsigned int *dataoff, unsigned int datalen,
			    enum sip_header_types type, int *in_header,
			    unsigned int *matchoff, unsigned int *matchlen,
			    union nf_inet_addr *addr, __be16 *port)
{
	const char *c, *limit = dptr + datalen;
	unsigned int p;
	int ret;

	ret = ct_sip_walk_headers(ct, dptr, dataoff ? *dataoff : 0, datalen,
				  type, in_header, matchoff, matchlen);
	WARN_ON(ret < 0);
	if (ret == 0)
		return ret;

	if (!sip_parse_addr(ct, dptr + *matchoff, &c, addr, limit, true))
		return -1;
	if (*c == ':') {
		c++;
		p = simple_strtoul(c, (char **)&c, 10);
		if (p < 1024 || p > 65535)
			return -1;
		*port = htons(p);
	} else
		*port = htons(SIP_PORT);

	if (dataoff)
		*dataoff = c - dptr;
	return 1;
}
EXPORT_SYMBOL_GPL(ct_sip_parse_header_uri);

static int ct_sip_parse_param(const struct nf_conn *ct, const char *dptr,
			      unsigned int dataoff, unsigned int datalen,
			      const char *name,
			      unsigned int *matchoff, unsigned int *matchlen)
{
	const char *limit = dptr + datalen;
	const char *start;
	const char *end;

	limit = ct_sip_header_search(dptr + dataoff, limit, ",", strlen(","));
	if (!limit)
		limit = dptr + datalen;

	start = ct_sip_header_search(dptr + dataoff, limit, name, strlen(name));
	if (!start)
		return 0;
	start += strlen(name);

	end = ct_sip_header_search(start, limit, ";", strlen(";"));
	if (!end)
		end = limit;

	*matchoff = start - dptr;
	*matchlen = end - start;
	return 1;
}

/* Parse address from header parameter and return address, offset and length */
int ct_sip_parse_address_param(const struct nf_conn *ct, const char *dptr,
			       unsigned int dataoff, unsigned int datalen,
			       const char *name,
			       unsigned int *matchoff, unsigned int *matchlen,
			       union nf_inet_addr *addr, bool delim)
{
	const char *limit = dptr + datalen;
	const char *start, *end;

	limit = ct_sip_header_search(dptr + dataoff, limit, ",", strlen(","));
	if (!limit)
		limit = dptr + datalen;

	start = ct_sip_header_search(dptr + dataoff, limit, name, strlen(name));
	if (!start)
		return 0;

	start += strlen(name);
	if (!sip_parse_addr(ct, start, &end, addr, limit, delim))
		return 0;
	*matchoff = start - dptr;
	*matchlen = end - start;
	return 1;
}
EXPORT_SYMBOL_GPL(ct_sip_parse_address_param);

/* Parse numerical header parameter and return value, offset and length */
int ct_sip_parse_numerical_param(const struct nf_conn *ct, const char *dptr,
				 unsigned int dataoff, unsigned int datalen,
				 const char *name,
				 unsigned int *matchoff, unsigned int *matchlen,
				 unsigned int *val)
{
	const char *limit = dptr + datalen;
	const char *start;
	char *end;

	limit = ct_sip_header_search(dptr + dataoff, limit, ",", strlen(","));
	if (!limit)
		limit = dptr + datalen;

	start = ct_sip_header_search(dptr + dataoff, limit, name, strlen(name));
	if (!start)
		return 0;

	start += strlen(name);
	*val = simple_strtoul(start, &end, 0);
	if (start == end)
		return 0;
	if (matchoff && matchlen) {
		*matchoff = start - dptr;
		*matchlen = end - start;
	}
	return 1;
}
EXPORT_SYMBOL_GPL(ct_sip_parse_numerical_param);

static int ct_sip_parse_transport(struct nf_conn *ct, const char *dptr,
				  unsigned int dataoff, unsigned int datalen,
				  u8 *proto)
{
	unsigned int matchoff, matchlen;

	if (ct_sip_parse_param(ct, dptr, dataoff, datalen, "transport=",
			       &matchoff, &matchlen)) {
		if (!strncasecmp(dptr + matchoff, "TCP", strlen("TCP")))
			*proto = IPPROTO_TCP;
		else if (!strncasecmp(dptr + matchoff, "UDP", strlen("UDP")))
			*proto = IPPROTO_UDP;
		else
			return 0;

		if (*proto != nf_ct_protonum(ct))
			return 0;
	} else
		*proto = nf_ct_protonum(ct);

	return 1;
}

static int sdp_parse_addr(const struct nf_conn *ct, const char *cp,
			  const char **endp, union nf_inet_addr *addr,
			  const char *limit)
{
	const char *end;
	int ret;

	memset(addr, 0, sizeof(*addr));
	switch (nf_ct_l3num(ct)) {
	case AF_INET:
		ret = in4_pton(cp, limit - cp, (u8 *)&addr->ip, -1, &end);
		break;
	case AF_INET6:
		ret = in6_pton(cp, limit - cp, (u8 *)&addr->ip6, -1, &end);
		break;
	default:
		BUG();
	}

	if (ret == 0)
		return 0;
	if (endp)
		*endp = end;
	return 1;
}

/* skip ip address. returns its length. */
static int sdp_addr_len(const struct nf_conn *ct, const char *dptr,
			const char *limit, int *shift)
{
	union nf_inet_addr addr;
	const char *aux = dptr;

	if (!sdp_parse_addr(ct, dptr, &dptr, &addr, limit)) {
		pr_debug("ip: %s parse failed.!\n", dptr);
		return 0;
	}

	return dptr - aux;
}

/* SDP header parsing: a SDP session description contains an ordered set of
 * headers, starting with a section containing general session parameters,
 * optionally followed by multiple media descriptions.
 *
 * SDP headers always start at the beginning of a line. According to RFC 2327:
 * "The sequence CRLF (0x0d0a) is used to end a record, although parsers should
 * be tolerant and also accept records terminated with a single newline
 * character". We handle both cases.
 */
static const struct sip_header ct_sdp_hdrs_v4[] = {
	[SDP_HDR_VERSION]	= SDP_HDR("v=", NULL, digits_len),
	[SDP_HDR_OWNER]		= SDP_HDR("o=", "IN IP4 ", sdp_addr_len),
	[SDP_HDR_CONNECTION]	= SDP_HDR("c=", "IN IP4 ", sdp_addr_len),
	[SDP_HDR_MEDIA]		= SDP_HDR("m=", NULL, media_len),
};

static const struct sip_header ct_sdp_hdrs_v6[] = {
	[SDP_HDR_VERSION]	= SDP_HDR("v=", NULL, digits_len),
	[SDP_HDR_OWNER]		= SDP_HDR("o=", "IN IP6 ", sdp_addr_len),
	[SDP_HDR_CONNECTION]	= SDP_HDR("c=", "IN IP6 ", sdp_addr_len),
	[SDP_HDR_MEDIA]		= SDP_HDR("m=", NULL, media_len),
};

/* Linear string search within SDP header values */
static const char *ct_sdp_header_search(const char *dptr, const char *limit,
					const char *needle, unsigned int len)
{
	for (limit -= len; dptr < limit; dptr++) {
		if (*dptr == '\r' || *dptr == '\n')
			break;
		if (strncmp(dptr, needle, len) == 0)
			return dptr;
	}
	return NULL;
}

/* Locate a SDP header (optionally a substring within the header value),
 * optionally stopping at the first occurrence of the term header, parse
 * it and return the offset and length of the data we're interested in.
 */
int ct_sip_get_sdp_header(const struct nf_conn *ct, const char *dptr,
			  unsigned int dataoff, unsigned int datalen,
			  enum sdp_header_types type,
			  enum sdp_header_types term,
			  unsigned int *matchoff, unsigned int *matchlen)
{
	const struct sip_header *hdrs, *hdr, *thdr;
	const char *start = dptr, *limit = dptr + datalen;
	int shift = 0;

	hdrs = nf_ct_l3num(ct) == NFPROTO_IPV4 ? ct_sdp_hdrs_v4 : ct_sdp_hdrs_v6;
	hdr = &hdrs[type];
	thdr = &hdrs[term];

	for (dptr += dataoff; dptr < limit; dptr++) {
		/* Find beginning of line */
		if (*dptr != '\r' && *dptr != '\n')
			continue;
		if (++dptr >= limit)
			break;
		if (*(dptr - 1) == '\r' && *dptr == '\n') {
			if (++dptr >= limit)
				break;
		}

		if (term != SDP_HDR_UNSPEC &&
		    limit - dptr >= thdr->len &&
		    strncasecmp(dptr, thdr->name, thdr->len) == 0)
			break;
		else if (limit - dptr >= hdr->len &&
			 strncasecmp(dptr, hdr->name, hdr->len) == 0)
			dptr += hdr->len;
		else
			continue;

		*matchoff = dptr - start;
		if (hdr->search) {
			dptr = ct_sdp_header_search(dptr, limit, hdr->search,
						    hdr->slen);
			if (!dptr)
				return -1;
			dptr += hdr->slen;
		}

		*matchlen = hdr->match_len(ct, dptr, limit, &shift);
		if (!*matchlen)
			return -1;
		*matchoff = dptr - start + shift;
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(ct_sip_get_sdp_header);

static int ct_sip_parse_sdp_addr(const struct nf_conn *ct, const char *dptr,
				 unsigned int dataoff, unsigned int datalen,
				 enum sdp_header_types type,
				 enum sdp_header_types term,
				 unsigned int *matchoff, unsigned int *matchlen,
				 union nf_inet_addr *addr)
{
	int ret;

	ret = ct_sip_get_sdp_header(ct, dptr, dataoff, datalen, type, term,
				    matchoff, matchlen);
	if (ret <= 0)
		return ret;

	if (!sdp_parse_addr(ct, dptr + *matchoff, NULL, addr,
			    dptr + *matchoff + *matchlen))
		return -1;
	return 1;
}

static int refresh_signalling_expectation(struct nf_conn *ct,
					  union nf_inet_addr *addr,
					  u8 proto, __be16 port,
					  unsigned int expires)
{
	struct nf_conn_help *help = nfct_help(ct);
	struct nf_conntrack_expect *exp;
	struct hlist_node *next;
	int found = 0;

	spin_lock_bh(&nf_conntrack_expect_lock);
	hlist_for_each_entry_safe(exp, next, &help->expectations, lnode) {
		if (exp->class != SIP_EXPECT_SIGNALLING ||
		    !nf_inet_addr_cmp(&exp->tuple.dst.u3, addr) ||
		    exp->tuple.dst.protonum != proto ||
		    exp->tuple.dst.u.udp.port != port)
			continue;
		if (!del_timer(&exp->timeout))
			continue;
		exp->flags &= ~NF_CT_EXPECT_INACTIVE;
		exp->timeout.expires = jiffies + expires * HZ;
		add_timer(&exp->timeout);
		found = 1;
		break;
	}
	spin_unlock_bh(&nf_conntrack_expect_lock);
	return found;
}

static void flush_expectations(struct nf_conn *ct, bool media)
{
	struct nf_conn_help *help = nfct_help(ct);
	struct nf_conntrack_expect *exp;
	struct hlist_node *next;

	spin_lock_bh(&nf_conntrack_expect_lock);
	hlist_for_each_entry_safe(exp, next, &help->expectations, lnode) {
		if ((exp->class != SIP_EXPECT_SIGNALLING) ^ media)
			continue;
		if (!del_timer(&exp->timeout))
			continue;
		nf_ct_unlink_expect(exp);
		nf_ct_expect_put(exp);
		if (!media)
			break;
	}
	spin_unlock_bh(&nf_conntrack_expect_lock);
}

static int set_expected_rtp_rtcp(struct sk_buff *skb, unsigned int protoff,
				 unsigned int dataoff,
				 const char **dptr, unsigned int *datalen,
				 union nf_inet_addr *daddr, __be16 port,
				 enum sip_expectation_classes class,
				 unsigned int mediaoff, unsigned int medialen)
{
	struct nf_conntrack_expect *exp, *rtp_exp, *rtcp_exp;
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
	struct net *net = nf_ct_net(ct);
	enum ip_conntrack_dir dir = CTINFO2DIR(ctinfo);
	union nf_inet_addr *saddr;
	struct nf_conntrack_tuple tuple;
	int direct_rtp = 0, skip_expect = 0, ret = NF_DROP;
	u_int16_t base_port;
	__be16 rtp_port, rtcp_port;
	const struct nf_nat_sip_hooks *hooks;

	saddr = NULL;
	if (sip_direct_media) {
		if (!nf_inet_addr_cmp(daddr, &ct->tuplehash[dir].tuple.src.u3))
			return NF_ACCEPT;
		saddr = &ct->tuplehash[!dir].tuple.src.u3;
	}

	/* We need to check whether the registration exists before attempting
	 * to register it since we can see the same media description multiple
	 * times on different connections in case multiple endpoints receive
	 * the same call.
	 *
	 * RTP optimization: if we find a matching media channel expectation
	 * and both the expectation and this connection are SNATed, we assume
	 * both sides can reach each other directly and use the final
	 * destination address from the expectation. We still need to keep
	 * the NATed expectations for media that might arrive from the
	 * outside, and additionally need to expect the direct RTP stream
	 * in case it passes through us even without NAT.
	 */
	memset(&tuple, 0, sizeof(tuple));
	if (saddr)
		tuple.src.u3 = *saddr;
	tuple.src.l3num		= nf_ct_l3num(ct);
	tuple.dst.protonum	= IPPROTO_UDP;
	tuple.dst.u3		= *daddr;
	tuple.dst.u.udp.port	= port;

	rcu_read_lock();
	do {
		exp = __nf_ct_expect_find(net, nf_ct_zone(ct), &tuple);

		if (!exp || exp->master == ct ||
		    nfct_help(exp->master)->helper != nfct_help(ct)->helper ||
		    exp->class != class)
			break;
#ifdef CONFIG_NF_NAT_NEEDED
		if (!direct_rtp &&
		    (!nf_inet_addr_cmp(&exp->saved_addr, &exp->tuple.dst.u3) ||
		     exp->saved_proto.udp.port != exp->tuple.dst.u.udp.port) &&
		    ct->status & IPS_NAT_MASK) {
			*daddr			= exp->saved_addr;
			tuple.dst.u3		= exp->saved_addr;
			tuple.dst.u.udp.port	= exp->saved_proto.udp.port;
			direct_rtp = 1;
		} else
#endif
			skip_expect = 1;
	} while (!skip_expect);

	base_port = ntohs(tuple.dst.u.udp.port) & ~1;
	rtp_port = htons(base_port);
	rtcp_port = htons(base_port + 1);

	if (direct_rtp) {
		hooks = rcu_dereference(nf_nat_sip_hooks);
		if (hooks &&
		    !hooks->sdp_port(skb, protoff, dataoff, dptr, datalen,
				     mediaoff, medialen, ntohs(rtp_port)))
			goto err1;
	}

	if (skip_expect) {
		rcu_read_unlock();
		return NF_ACCEPT;
	}

	rtp_exp = nf_ct_expect_alloc(ct);
	if (rtp_exp == NULL)
		goto err1;
	nf_ct_expect_init(rtp_exp, class, nf_ct_l3num(ct), saddr, daddr,
			  IPPROTO_UDP, NULL, &rtp_port);

	rtcp_exp = nf_ct_expect_alloc(ct);
	if (rtcp_exp == NULL)
		goto err2;
	nf_ct_expect_init(rtcp_exp, class, nf_ct_l3num(ct), saddr, daddr,
			  IPPROTO_UDP, NULL, &rtcp_port);

	hooks = rcu_dereference(nf_nat_sip_hooks);
	if (hooks && ct->status & IPS_NAT_MASK && !direct_rtp)
		ret = hooks->sdp_media(skb, protoff, dataoff, dptr,
				       datalen, rtp_exp, rtcp_exp,
				       mediaoff, medialen, daddr);
	else {
		if (nf_ct_expect_related(rtp_exp) == 0) {
			if (nf_ct_expect_related(rtcp_exp) != 0)
				nf_ct_unexpect_related(rtp_exp);
			else
				ret = NF_ACCEPT;
		}
	}
	nf_ct_expect_put(rtcp_exp);
err2:
	nf_ct_expect_put(rtp_exp);
err1:
	rcu_read_unlock();
	return ret;
}

static const struct sdp_media_type sdp_media_types[] = {
	SDP_MEDIA_TYPE("audio ", SIP_EXPECT_AUDIO),
	SDP_MEDIA_TYPE("video ", SIP_EXPECT_VIDEO),
	SDP_MEDIA_TYPE("image ", SIP_EXPECT_IMAGE),
};

static const struct sdp_media_type *sdp_media_type(const char *dptr,
						   unsigned int matchoff,
						   unsigned int matchlen)
{
	const struct sdp_media_type *t;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(sdp_media_types); i++) {
		t = &sdp_media_types[i];
		if (matchlen < t->len ||
		    strncmp(dptr + matchoff, t->name, t->len))
			continue;
		return t;
	}
	return NULL;
}

static int process_sdp(struct sk_buff *skb, unsigned int protoff,
		       unsigned int dataoff,
		       const char **dptr, unsigned int *datalen,
		       unsigned int cseq)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
	unsigned int matchoff, matchlen;
	unsigned int mediaoff, medialen;
	unsigned int sdpoff;
	unsigned int caddr_len, maddr_len;
	unsigned int i;
	union nf_inet_addr caddr, maddr, rtp_addr;
	const struct nf_nat_sip_hooks *hooks;
	unsigned int port;
	const struct sdp_media_type *t;
	int ret = NF_ACCEPT;

	hooks = rcu_dereference(nf_nat_sip_hooks);

	/* Find beginning of session description */
	if (ct_sip_get_sdp_header(ct, *dptr, 0, *datalen,
				  SDP_HDR_VERSION, SDP_HDR_UNSPEC,
				  &matchoff, &matchlen) <= 0)
		return NF_ACCEPT;
	sdpoff = matchoff;

	/* The connection information is contained in the session description
	 * and/or once per media description. The first media description marks
	 * the end of the session description. */
	caddr_len = 0;
	if (ct_sip_parse_sdp_addr(ct, *dptr, sdpoff, *datalen,
				  SDP_HDR_CONNECTION, SDP_HDR_MEDIA,
				  &matchoff, &matchlen, &caddr) > 0)
		caddr_len = matchlen;

	mediaoff = sdpoff;
	for (i = 0; i < ARRAY_SIZE(sdp_media_types); ) {
		if (ct_sip_get_sdp_header(ct, *dptr, mediaoff, *datalen,
					  SDP_HDR_MEDIA, SDP_HDR_UNSPEC,
					  &mediaoff, &medialen) <= 0)
			break;

		/* Get media type and port number. A media port value of zero
		 * indicates an inactive stream. */
		t = sdp_media_type(*dptr, mediaoff, medialen);
		if (!t) {
			mediaoff += medialen;
			continue;
		}
		mediaoff += t->len;
		medialen -= t->len;

		port = simple_strtoul(*dptr + mediaoff, NULL, 10);
		if (port == 0)
			continue;
		if (port < 1024 || port > 65535) {
			nf_ct_helper_log(skb, ct, "wrong port %u", port);
			return NF_DROP;
		}

		/* The media description overrides the session description. */
		maddr_len = 0;
		if (ct_sip_parse_sdp_addr(ct, *dptr, mediaoff, *datalen,
					  SDP_HDR_CONNECTION, SDP_HDR_MEDIA,
					  &matchoff, &matchlen, &maddr) > 0) {
			maddr_len = matchlen;
			memcpy(&rtp_addr, &maddr, sizeof(rtp_addr));
		} else if (caddr_len)
			memcpy(&rtp_addr, &caddr, sizeof(rtp_addr));
		else {
			nf_ct_helper_log(skb, ct, "cannot parse SDP message");
			return NF_DROP;
		}

		ret = set_expected_rtp_rtcp(skb, protoff, dataoff,
					    dptr, datalen,
					    &rtp_addr, htons(port), t->class,
					    mediaoff, medialen);
		if (ret != NF_ACCEPT) {
			nf_ct_helper_log(skb, ct,
					 "cannot add expectation for voice");
			return ret;
		}

		/* Update media connection address if present */
		if (maddr_len && hooks && ct->status & IPS_NAT_MASK) {
			ret = hooks->sdp_addr(skb, protoff, dataoff,
					      dptr, datalen, mediaoff,
					      SDP_HDR_CONNECTION,
					      SDP_HDR_MEDIA,
					      &rtp_addr);
			if (ret != NF_ACCEPT) {
				nf_ct_helper_log(skb, ct, "cannot mangle SDP");
				return ret;
			}
		}
		i++;
	}

	/* Update session connection and owner addresses */
	hooks = rcu_dereference(nf_nat_sip_hooks);
	if (hooks && ct->status & IPS_NAT_MASK)
		ret = hooks->sdp_session(skb, protoff, dataoff,
					 dptr, datalen, sdpoff,
					 &rtp_addr);

	return ret;
}
static int process_invite_response(struct sk_buff *skb, unsigned int protoff,
				   unsigned int dataoff,
				   const char **dptr, unsigned int *datalen,
				   unsigned int cseq, unsigned int code)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
	struct nf_ct_sip_master *ct_sip_info = nfct_help_data(ct);

	if ((code >= 100 && code <= 199) ||
	    (code >= 200 && code <= 299))
		return process_sdp(skb, protoff, dataoff, dptr, datalen, cseq);
	else if (ct_sip_info->invite_cseq == cseq)
		flush_expectations(ct, true);
	return NF_ACCEPT;
}

static int process_update_response(struct sk_buff *skb, unsigned int protoff,
				   unsigned int dataoff,
				   const char **dptr, unsigned int *datalen,
				   unsigned int cseq, unsigned int code)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
	struct nf_ct_sip_master *ct_sip_info = nfct_help_data(ct);

	if ((code >= 100 && code <= 199) ||
	    (code >= 200 && code <= 299))
		return process_sdp(skb, protoff, dataoff, dptr, datalen, cseq);
	else if (ct_sip_info->invite_cseq == cseq)
		flush_expectations(ct, true);
	return NF_ACCEPT;
}

static int process_prack_response(struct sk_buff *skb, unsigned int protoff,
				  unsigned int dataoff,
				  const char **dptr, unsigned int *datalen,
				  unsigned int cseq, unsigned int code)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
	struct nf_ct_sip_master *ct_sip_info = nfct_help_data(ct);

	if ((code >= 100 && code <= 199) ||
	    (code >= 200 && code <= 299))
		return process_sdp(skb, protoff, dataoff, dptr, datalen, cseq);
	else if (ct_sip_info->invite_cseq == cseq)
		flush_expectations(ct, true);
	return NF_ACCEPT;
}

static int process_invite_request(struct sk_buff *skb, unsigned int protoff,
				  unsigned int dataoff,
				  const char **dptr, unsigned int *datalen,
				  unsigned int cseq)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
	struct nf_ct_sip_master *ct_sip_info = nfct_help_data(ct);
	unsigned int ret;

	flush_expectations(ct, true);
	ret = process_sdp(skb, protoff, dataoff, dptr, datalen, cseq);
	if (ret == NF_ACCEPT)
		ct_sip_info->invite_cseq = cseq;
	return ret;
}

static int process_bye_request(struct sk_buff *skb, unsigned int protoff,
			       unsigned int dataoff,
			       const char **dptr, unsigned int *datalen,
			       unsigned int cseq)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);

	flush_expectations(ct, true);
	return NF_ACCEPT;
}

/* Parse a REGISTER request and create a permanent expectation for incoming
 * signalling connections. The expectation is marked inactive and is activated
 * when receiving a response indicating success from the registrar.
 */
static int process_register_request(struct sk_buff *skb, unsigned int protoff,
				    unsigned int dataoff,
				    const char **dptr, unsigned int *datalen,
				    unsigned int cseq)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
	struct nf_ct_sip_master *ct_sip_info = nfct_help_data(ct);
	enum ip_conntrack_dir dir = CTINFO2DIR(ctinfo);
	unsigned int matchoff, matchlen;
	struct nf_conntrack_expect *exp;
	union nf_inet_addr *saddr, daddr;
	const struct nf_nat_sip_hooks *hooks;
	__be16 port;
	u8 proto;
	unsigned int expires = 0;
	int ret;

	/* Expected connections can not register again. */
	if (ct->status & IPS_EXPECTED)
		return NF_ACCEPT;

	/* We must check the expiration time: a value of zero signals the
	 * registrar to release the binding. We'll remove our expectation
	 * when receiving the new bindings in the response, but we don't
	 * want to create new ones.
	 *
	 * The expiration time may be contained in Expires: header, the
	 * Contact: header parameters or the URI parameters.
	 */
	if (ct_sip_get_header(ct, *dptr, 0, *datalen, SIP_HDR_EXPIRES,
			      &matchoff, &matchlen) > 0)
		expires = simple_strtoul(*dptr + matchoff, NULL, 10);

	ret = ct_sip_parse_header_uri(ct, *dptr, NULL, *datalen,
				      SIP_HDR_CONTACT, NULL,
				      &matchoff, &matchlen, &daddr, &port);
	if (ret < 0) {
		nf_ct_helper_log(skb, ct, "cannot parse contact");
		return NF_DROP;
	} else if (ret == 0)
		return NF_ACCEPT;

	/* We don't support third-party registrations */
	if (!nf_inet_addr_cmp(&ct->tuplehash[dir].tuple.src.u3, &daddr))
		return NF_ACCEPT;

	if (ct_sip_parse_transport(ct, *dptr, matchoff + matchlen, *datalen,
				   &proto) == 0)
		return NF_ACCEPT;

	if (ct_sip_parse_numerical_param(ct, *dptr,
					 matchoff + matchlen, *datalen,
					 "expires=", NULL, NULL, &expires) < 0) {
		nf_ct_helper_log(skb, ct, "cannot parse expires");
		return NF_DROP;
	}

	if (expires == 0) {
		ret = NF_ACCEPT;
		goto store_cseq;
	}

	exp = nf_ct_expect_alloc(ct);
	if (!exp) {
		nf_ct_helper_log(skb, ct, "cannot alloc expectation");
		return NF_DROP;
	}

	saddr = NULL;
	if (sip_direct_signalling)
		saddr = &ct->tuplehash[!dir].tuple.src.u3;

	nf_ct_expect_init(exp, SIP_EXPECT_SIGNALLING, nf_ct_l3num(ct),
			  saddr, &daddr, proto, NULL, &port);
	exp->timeout.expires = sip_timeout * HZ;
	exp->helper = nfct_help(ct)->helper;
	exp->flags = NF_CT_EXPECT_PERMANENT | NF_CT_EXPECT_INACTIVE;

	hooks = rcu_dereference(nf_nat_sip_hooks);
	if (hooks && ct->status & IPS_NAT_MASK)
		ret = hooks->expect(skb, protoff, dataoff, dptr, datalen,
				    exp, matchoff, matchlen);
	else {
		if (nf_ct_expect_related(exp) != 0) {
			nf_ct_helper_log(skb, ct, "cannot add expectation");
			ret = NF_DROP;
		} else
			ret = NF_ACCEPT;
	}
	nf_ct_expect_put(exp);

store_cseq:
	if (ret == NF_ACCEPT)
		ct_sip_info->register_cseq = cseq;
	return ret;
}

static int process_register_response(struct sk_buff *skb, unsigned int protoff,
				     unsigned int dataoff,
				     const char **dptr, unsigned int *datalen,
				     unsigned int cseq, unsigned int code)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
	struct nf_ct_sip_master *ct_sip_info = nfct_help_data(ct);
	enum ip_conntrack_dir dir = CTINFO2DIR(ctinfo);
	union nf_inet_addr addr;
	__be16 port;
	u8 proto;
	unsigned int matchoff, matchlen, coff = 0;
	unsigned int expires = 0;
	int in_contact = 0, ret;

	/* According to RFC 3261, "UAs MUST NOT send a new registration until
	 * they have received a final response from the registrar for the
	 * previous one or the previous REGISTER request has timed out".
	 *
	 * However, some servers fail to detect retransmissions and send late
	 * responses, so we store the sequence number of the last valid
	 * request and compare it here.
	 */
	if (ct_sip_info->register_cseq != cseq)
		return NF_ACCEPT;

	if (code >= 100 && code <= 199)
		return NF_ACCEPT;
	if (code < 200 || code > 299)
		goto flush;

	if (ct_sip_get_header(ct, *dptr, 0, *datalen, SIP_HDR_EXPIRES,
			      &matchoff, &matchlen) > 0)
		expires = simple_strtoul(*dptr + matchoff, NULL, 10);

	while (1) {
		unsigned int c_expires = expires;

		ret = ct_sip_parse_header_uri(ct, *dptr, &coff, *datalen,
					      SIP_HDR_CONTACT, &in_contact,
					      &matchoff, &matchlen,
					      &addr, &port);
		if (ret < 0) {
			nf_ct_helper_log(skb, ct, "cannot parse contact");
			return NF_DROP;
		} else if (ret == 0)
			break;

		/* We don't support third-party registrations */
		if (!nf_inet_addr_cmp(&ct->tuplehash[dir].tuple.dst.u3, &addr))
			continue;

		if (ct_sip_parse_transport(ct, *dptr, matchoff + matchlen,
					   *datalen, &proto) == 0)
			continue;

		ret = ct_sip_parse_numerical_param(ct, *dptr,
						   matchoff + matchlen,
						   *datalen, "expires=",
						   NULL, NULL, &c_expires);
		if (ret < 0) {
			nf_ct_helper_log(skb, ct, "cannot parse expires");
			return NF_DROP;
		}
		if (c_expires == 0)
			break;
		if (refresh_signalling_expectation(ct, &addr, proto, port,
						   c_expires))
			return NF_ACCEPT;
	}

flush:
	flush_expectations(ct, false);
	return NF_ACCEPT;
}

static const struct sip_handler sip_handlers[] = {
	SIP_HANDLER("INVITE", process_invite_request, process_invite_response),
	SIP_HANDLER("UPDATE", process_sdp, process_update_response),
	SIP_HANDLER("ACK", process_sdp, NULL),
	SIP_HANDLER("PRACK", process_sdp, process_prack_response),
	SIP_HANDLER("BYE", process_bye_request, NULL),
	SIP_HANDLER("REGISTER", process_register_request, process_register_response),
};

static int process_sip_response(struct sk_buff *skb, unsigned int protoff,
				unsigned int dataoff,
				const char **dptr, unsigned int *datalen)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
	unsigned int matchoff, matchlen, matchend;
	unsigned int code, cseq, i;

	if (*datalen < strlen("SIP/2.0 200"))
		return NF_ACCEPT;
	code = simple_strtoul(*dptr + strlen("SIP/2.0 "), NULL, 10);
	if (!code) {
		nf_ct_helper_log(skb, ct, "cannot get code");
		return NF_DROP;
	}

	if (ct_sip_get_header(ct, *dptr, 0, *datalen, SIP_HDR_CSEQ,
			      &matchoff, &matchlen) <= 0) {
		nf_ct_helper_log(skb, ct, "cannot parse cseq");
		return NF_DROP;
	}
	cseq = simple_strtoul(*dptr + matchoff, NULL, 10);
	if (!cseq) {
		nf_ct_helper_log(skb, ct, "cannot get cseq");
		return NF_DROP;
	}
	matchend = matchoff + matchlen + 1;

	for (i = 0; i < ARRAY_SIZE(sip_handlers); i++) {
		const struct sip_handler *handler;

		handler = &sip_handlers[i];
		if (handler->response == NULL)
			continue;
		if (*datalen < matchend + handler->len ||
		    strncasecmp(*dptr + matchend, handler->method, handler->len))
			continue;
		return handler->response(skb, protoff, dataoff, dptr, datalen,
					 cseq, code);
	}
	return NF_ACCEPT;
}

static int process_sip_request(struct sk_buff *skb, unsigned int protoff,
			       unsigned int dataoff,
			       const char **dptr, unsigned int *datalen)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
	struct nf_ct_sip_master *ct_sip_info = nfct_help_data(ct);
	enum ip_conntrack_dir dir = CTINFO2DIR(ctinfo);
	unsigned int matchoff, matchlen;
	unsigned int cseq, i;
	union nf_inet_addr addr;
	__be16 port;

	/* Many Cisco IP phones use a high source port for SIP requests, but
	 * listen for the response on port 5060.  If we are the local
	 * router for one of these phones, save the port number from the
	 * Via: header so that nf_nat_sip can redirect the responses to
	 * the correct port.
	 */
	if (ct_sip_parse_header_uri(ct, *dptr, NULL, *datalen,
				    SIP_HDR_VIA_UDP, NULL, &matchoff,
				    &matchlen, &addr, &port) > 0 &&
	    port != ct->tuplehash[dir].tuple.src.u.udp.port &&
	    nf_inet_addr_cmp(&addr, &ct->tuplehash[dir].tuple.src.u3) &&
		(dir == IP_CT_DIR_ORIGINAL))
		ct_sip_info->forced_dport = port;

	for (i = 0; i < ARRAY_SIZE(sip_handlers); i++) {
		const struct sip_handler *handler;

		handler = &sip_handlers[i];
		if (handler->request == NULL)
			continue;
		if (*datalen < handler->len ||
		    strncasecmp(*dptr, handler->method, handler->len))
			continue;

		if (ct_sip_get_header(ct, *dptr, 0, *datalen, SIP_HDR_CSEQ,
				      &matchoff, &matchlen) <= 0) {
			nf_ct_helper_log(skb, ct, "cannot parse cseq");
			return NF_DROP;
		}
		cseq = simple_strtoul(*dptr + matchoff, NULL, 10);
		if (!cseq) {
			nf_ct_helper_log(skb, ct, "cannot get cseq");
			return NF_DROP;
		}

		return handler->request(skb, protoff, dataoff, dptr, datalen,
					cseq);
	}
	return NF_ACCEPT;
}

static int process_sip_msg(struct sk_buff *skb, struct nf_conn *ct,
			   unsigned int protoff, unsigned int dataoff,
			   const char **dptr, unsigned int *datalen)
{
	const struct nf_nat_sip_hooks *hooks;
	int ret;

	if (nf_ct_disable_sip_alg)
		return NF_ACCEPT;
	if (strncasecmp(*dptr, "SIP/2.0 ", strlen("SIP/2.0 ")) != 0)
		ret = process_sip_request(skb, protoff, dataoff, dptr, datalen);
	else
		ret = process_sip_response(skb, protoff, dataoff, dptr, datalen);

	if (ret == NF_ACCEPT && ct->status & IPS_NAT_MASK) {
		hooks = rcu_dereference(nf_nat_sip_hooks);
		if (hooks && !hooks->msg(skb, protoff, dataoff,
					 dptr, datalen)) {
			nf_ct_helper_log(skb, ct, "cannot NAT SIP message");
			ret = NF_DROP;
		}
	}

	return ret;
}

static int sip_help_tcp(struct sk_buff *skb, unsigned int protoff,
			struct nf_conn *ct, enum ip_conntrack_info ctinfo)
{
	struct tcphdr *th, _tcph;
	unsigned int dataoff;
	unsigned int matchoff, matchlen, clen;
	const char *dptr, *end;
	s16 diff, tdiff = 0;
	int ret = NF_ACCEPT;
	bool term;
	unsigned int datalen = 0, msglen = 0, origlen = 0;
	unsigned int dataoff_orig = 0;
	unsigned int splitlen, oldlen, oldlen1;
	struct sip_list *sip_entry = NULL;
	bool skip_sip_process = false;
	bool do_not_process = false;
	bool skip = false;
	bool skb_is_combined = false;
	enum ip_conntrack_dir dir = IP_CT_DIR_MAX;
	struct sk_buff *combined_skb = NULL;
	bool content_len_exists = 1;

	packet_count++;
	pr_debug("packet count %d\n", packet_count);

	if (nf_ct_disable_sip_alg)
		return NF_ACCEPT;

	if (ctinfo != IP_CT_ESTABLISHED &&
	    ctinfo != IP_CT_ESTABLISHED_REPLY)
		return NF_ACCEPT;

	/* No Data ? */
	th = skb_header_pointer(skb, protoff, sizeof(_tcph), &_tcph);
	if (th == NULL)
		return NF_ACCEPT;
	dataoff = protoff + th->doff * 4;
	if (dataoff >= skb->len)
		return NF_ACCEPT;

	nf_ct_refresh(ct, skb, sip_timeout * HZ);

	if (unlikely(skb_linearize(skb)))
		return NF_DROP;

	dptr = skb->data + dataoff;
	datalen = skb->len - dataoff;
	if (datalen < strlen("SIP/2.0 200"))
		return NF_ACCEPT;

	/* here we save the original datalength and data offset of the skb, this
	 * is needed later to split combined skbs
	 */
	oldlen1 = skb->len - protoff;
	dataoff_orig = dataoff;

	if (!ct)
		return NF_DROP;
	while (1) {
		if (ct_sip_get_header(ct, dptr, 0, datalen,
				      SIP_HDR_CONTENT_LENGTH,
				      &matchoff, &matchlen) <= 0){
			if (nf_ct_enable_sip_segmentation) {
				do_not_process = true;
				content_len_exists = 0;
				goto destination;
			} else {
				break;
			}
		}

		clen = simple_strtoul(dptr + matchoff, (char **)&end, 10);
		if (dptr + matchoff == end) {
			break;
		}

		term = false;
		for (; end + strlen("\r\n\r\n") <= dptr + datalen; end++) {
			if (end[0] == '\r' && end[1] == '\n' &&
			    end[2] == '\r' && end[3] == '\n') {
				term = true;
				break;
			}
		}
		if (!term)
			break;

		end += strlen("\r\n\r\n") + clen;
destination:
		if (content_len_exists == 0) {
			origlen = datalen;
			msglen = origlen;
		} else {
			origlen = end - dptr;
			msglen = origlen;
		}
		pr_debug("mslgen %d datalen %d\n", msglen, datalen);
		dir = CTINFO2DIR(ctinfo);
		combined_skb = skb;
		if (nf_ct_enable_sip_segmentation) {
			/* Segmented Packet */
			if (msglen > datalen) {
				skip = sip_save_segment_info(ct, skb, msglen,
							     datalen, dptr,
							     ctinfo);
				if (!skip)
					return NF_QUEUE;
			}
			/* Traverse list to find prev segment */
			/*Traverse the list if list non empty */
			if (((&ct->sip_segment_list)->next) !=
				(&ct->sip_segment_list)) {
				/* Combine segments if they are fragments of
				 *  the same message.
				*/
				sip_entry = sip_coalesce_segments(ct, &skb,
								  dataoff,
								  &combined_skb,
							&skip_sip_process,
								do_not_process,
								  ctinfo,
							&skb_is_combined);
				sip_update_params(dir, &msglen, &origlen, &dptr,
						  &datalen,
						  skb_is_combined, ct);

				if (skip_sip_process)
					goto here;
				} else if (do_not_process) {
					goto here;
				}
		} else if (msglen > datalen) {
			return NF_ACCEPT;
		}
		/* process the combined skb having the complete SIP message */
		ret = process_sip_msg(combined_skb, ct, protoff, dataoff,
				      &dptr, &msglen);

		/* process_sip_* functions report why this packet is dropped */
		if (ret != NF_ACCEPT)
			break;
		sip_calculate_parameters(&diff, &tdiff, &dataoff, &dptr,
					 &datalen, msglen, origlen);
		if (nf_ct_enable_sip_segmentation && skb_is_combined)
			break;
	}
	if (skb_is_combined) {
		/* once combined skb is processed, split the skbs again The
		 * length to split at is the same as length of first skb. Any
		 * changes in the combined skb length because of SIP processing
		 * will reflect in the second fragment
		 */
		splitlen = (dir == IP_CT_DIR_ORIGINAL) ?
				ct->segment.skb_len[0] : ct->segment.skb_len[1];
		oldlen = combined_skb->len - protoff;
		skb_split(combined_skb, skb, splitlen);
		/* Headers need to be recalculated since during SIP processing
		 * headers are calculated based on the change in length of the
		 * combined message
		 */
		recalc_header(combined_skb, splitlen, oldlen, protoff);
		/* Reinject the first skb now that the processing is complete */
		if (sip_entry) {
			nf_reinject(sip_entry->entry, NF_ACCEPT);
			kfree(sip_entry);
		}
		skb->len = (oldlen1 + protoff) + tdiff - dataoff_orig;
		/* After splitting, push the headers back to the first skb which
		 * were removed before combining the skbs.This moves the skb
		 * begin pointer back to the beginning of its headers
		 */
		skb_push(skb, dataoff_orig);
		/* Since the length of this second segment willbe affected
		 * because of SIP processing,we need to recalculate its header
		 * as well.
		 */
		recalc_header(skb, skb->len, oldlen1, protoff);
		/* Now that the processing is done and the first skb reinjected.
		 * We allow addition of fragmented skbs to the list for this
		 * direction
		 */
		if (dir == IP_CT_DIR_ORIGINAL)
			ct->sip_original_dir = 0;
		else
			ct->sip_reply_dir = 0;
	}

here:

	if (ret == NF_ACCEPT && ct->status & IPS_NAT_MASK) {
		const struct nf_nat_sip_hooks *hooks;

		hooks = rcu_dereference(nf_nat_sip_hooks);
		if (hooks)
			hooks->seq_adjust(skb, protoff, tdiff);
	}

	return ret;
}

static int sip_help_udp(struct sk_buff *skb, unsigned int protoff,
			struct nf_conn *ct, enum ip_conntrack_info ctinfo)
{
	unsigned int dataoff, datalen;
	const char *dptr;

	/* No Data ? */
	dataoff = protoff + sizeof(struct udphdr);
	if (dataoff >= skb->len)
		return NF_ACCEPT;

	nf_ct_refresh(ct, skb, sip_timeout * HZ);

	if (unlikely(skb_linearize(skb)))
		return NF_DROP;

	dptr = skb->data + dataoff;
	datalen = skb->len - dataoff;
	if (datalen < strlen("SIP/2.0 200"))
		return NF_ACCEPT;

	return process_sip_msg(skb, ct, protoff, dataoff, &dptr, &datalen);
}

static struct nf_conntrack_helper sip[MAX_PORTS][4] __read_mostly;

static const struct nf_conntrack_expect_policy sip_exp_policy[SIP_EXPECT_MAX + 1] = {
	[SIP_EXPECT_SIGNALLING] = {
		.name		= "signalling",
		.max_expected	= 1,
		.timeout	= 3 * 60,
	},
	[SIP_EXPECT_AUDIO] = {
		.name		= "audio",
		.max_expected	= 2 * IP_CT_DIR_MAX,
		.timeout	= 3 * 60,
	},
	[SIP_EXPECT_VIDEO] = {
		.name		= "video",
		.max_expected	= 2 * IP_CT_DIR_MAX,
		.timeout	= 3 * 60,
	},
	[SIP_EXPECT_IMAGE] = {
		.name		= "image",
		.max_expected	= IP_CT_DIR_MAX,
		.timeout	= 3 * 60,
	},
};

static void nf_conntrack_sip_fini(void)
{
	int i, j;

	for (i = 0; i < ports_c; i++) {
		for (j = 0; j < ARRAY_SIZE(sip[i]); j++) {
			if (sip[i][j].me == NULL)
				continue;
			nf_conntrack_helper_unregister(&sip[i][j]);
		}
	}
}

static int __init nf_conntrack_sip_init(void)
{
	int i, j, ret;

	sip_sysctl_header = register_net_sysctl(&init_net, "net/netfilter",
						sip_sysctl_tbl);
	if (!sip_sysctl_header)
		pr_debug("nf_ct_sip:Unable to register SIP systbl\n");

	if (nf_ct_disable_sip_alg)
		pr_debug("nf_ct_sip: SIP ALG disabled\n");
	else
		pr_debug("nf_ct_sip: SIP ALG enabled\n");

	if (ports_c == 0)
		ports[ports_c++] = SIP_PORT;

	for (i = 0; i < ports_c; i++) {
		memset(&sip[i], 0, sizeof(sip[i]));

		sip[i][0].tuple.src.l3num = AF_INET;
		sip[i][0].tuple.dst.protonum = IPPROTO_UDP;
		sip[i][0].help = sip_help_udp;
		sip[i][1].tuple.src.l3num = AF_INET;
		sip[i][1].tuple.dst.protonum = IPPROTO_TCP;
		sip[i][1].help = sip_help_tcp;

		sip[i][2].tuple.src.l3num = AF_INET6;
		sip[i][2].tuple.dst.protonum = IPPROTO_UDP;
		sip[i][2].help = sip_help_udp;
		sip[i][3].tuple.src.l3num = AF_INET6;
		sip[i][3].tuple.dst.protonum = IPPROTO_TCP;
		sip[i][3].help = sip_help_tcp;

		for (j = 0; j < ARRAY_SIZE(sip[i]); j++) {
			sip[i][j].data_len = sizeof(struct nf_ct_sip_master);
			sip[i][j].tuple.src.u.udp.port = htons(ports[i]);
			sip[i][j].expect_policy = sip_exp_policy;
			sip[i][j].expect_class_max = SIP_EXPECT_MAX;
			sip[i][j].me = THIS_MODULE;

			if (ports[i] == SIP_PORT)
				sprintf(sip[i][j].name, "sip");
			else
				sprintf(sip[i][j].name, "sip-%u", i);

			pr_debug("port #%u: %u\n", i, ports[i]);

			ret = nf_conntrack_helper_register(&sip[i][j]);
			if (ret) {
				printk(KERN_ERR "nf_ct_sip: failed to register"
				       " helper for pf: %u port: %u\n",
				       sip[i][j].tuple.src.l3num, ports[i]);
				nf_conntrack_sip_fini();
				return ret;
			}
		}
	}
	return 0;
}

module_init(nf_conntrack_sip_init);
module_exit(nf_conntrack_sip_fini);
