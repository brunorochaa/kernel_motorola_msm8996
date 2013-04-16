/*
 * Symbol USB barcode to serial driver
 *
 * Copyright (C) 2009 Greg Kroah-Hartman <gregkh@suse.de>
 * Copyright (C) 2009 Novell Inc.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License version
 *	2 as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include <linux/uaccess.h>

static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(0x05e0, 0x0600) },
	{ },
};
MODULE_DEVICE_TABLE(usb, id_table);

/* This structure holds all of the individual device information */
struct symbol_private {
	struct usb_device *udev;
	spinlock_t lock;	/* protects the following flags */
	bool throttled;
	bool actually_throttled;
	bool rts;
};

static void symbol_int_callback(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	struct symbol_private *priv = usb_get_serial_data(port->serial);
	unsigned char *data = urb->transfer_buffer;
	int status = urb->status;
	int result;
	int data_length;

	switch (status) {
	case 0:
		/* success */
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		/* this urb is terminated, clean up */
		dev_dbg(&port->dev, "%s - urb shutting down with status: %d\n",
			__func__, status);
		return;
	default:
		dev_dbg(&port->dev, "%s - nonzero urb status received: %d\n",
			__func__, status);
		goto exit;
	}

	usb_serial_debug_data(&port->dev, __func__, urb->actual_length, data);

	if (urb->actual_length > 1) {
		data_length = urb->actual_length - 1;

		/*
		 * Data from the device comes with a 1 byte header:
		 *
		 * <size of data>data...
		 * 	This is real data to be sent to the tty layer
		 * we pretty much just ignore the size and send everything
		 * else to the tty layer.
		 */
		tty_insert_flip_string(&port->port, &data[1], data_length);
		tty_flip_buffer_push(&port->port);
	} else {
		dev_dbg(&priv->udev->dev,
			"Improper amount of data received from the device, "
			"%d bytes", urb->actual_length);
	}

exit:
	spin_lock(&priv->lock);

	/* Continue trying to always read if we should */
	if (!priv->throttled) {
		result = usb_submit_urb(port->interrupt_in_urb, GFP_ATOMIC);
		if (result)
			dev_err(&port->dev,
			    "%s - failed resubmitting read urb, error %d\n",
							__func__, result);
	} else
		priv->actually_throttled = true;
	spin_unlock(&priv->lock);
}

static int symbol_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	struct symbol_private *priv = usb_get_serial_data(port->serial);
	unsigned long flags;
	int result = 0;

	spin_lock_irqsave(&priv->lock, flags);
	priv->throttled = false;
	priv->actually_throttled = false;
	spin_unlock_irqrestore(&priv->lock, flags);

	/* Start reading from the device */
	result = usb_submit_urb(port->interrupt_in_urb, GFP_KERNEL);
	if (result)
		dev_err(&port->dev,
			"%s - failed resubmitting read urb, error %d\n",
			__func__, result);
	return result;
}

static void symbol_close(struct usb_serial_port *port)
{
	usb_kill_urb(port->interrupt_in_urb);
}

static void symbol_throttle(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct symbol_private *priv = usb_get_serial_data(port->serial);

	spin_lock_irq(&priv->lock);
	priv->throttled = true;
	spin_unlock_irq(&priv->lock);
}

static void symbol_unthrottle(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct symbol_private *priv = usb_get_serial_data(port->serial);
	int result;
	bool was_throttled;

	spin_lock_irq(&priv->lock);
	priv->throttled = false;
	was_throttled = priv->actually_throttled;
	priv->actually_throttled = false;
	spin_unlock_irq(&priv->lock);

	if (was_throttled) {
		result = usb_submit_urb(port->interrupt_in_urb, GFP_KERNEL);
		if (result)
			dev_err(&port->dev,
				"%s - failed submitting read urb, error %d\n",
							__func__, result);
	}
}

static int symbol_startup(struct usb_serial *serial)
{
	struct symbol_private *priv;

	if (!serial->num_interrupt_in) {
		dev_err(&serial->dev->dev, "no interrupt-in endpoint\n");
		return -ENODEV;
	}

	/* create our private serial structure */
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (priv == NULL) {
		dev_err(&serial->dev->dev, "%s - Out of memory\n", __func__);
		return -ENOMEM;
	}
	spin_lock_init(&priv->lock);
	priv->udev = serial->dev;

	usb_set_serial_data(serial, priv);
	return 0;
}

static void symbol_release(struct usb_serial *serial)
{
	struct symbol_private *priv = usb_get_serial_data(serial);

	kfree(priv);
}

static struct usb_serial_driver symbol_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"symbol",
	},
	.id_table =		id_table,
	.num_ports =		1,
	.attach =		symbol_startup,
	.open =			symbol_open,
	.close =		symbol_close,
	.release =		symbol_release,
	.throttle = 		symbol_throttle,
	.unthrottle =		symbol_unthrottle,
	.read_int_callback =	symbol_int_callback,
};

static struct usb_serial_driver * const serial_drivers[] = {
	&symbol_device, NULL
};

module_usb_serial_driver(serial_drivers, id_table);

MODULE_LICENSE("GPL");
