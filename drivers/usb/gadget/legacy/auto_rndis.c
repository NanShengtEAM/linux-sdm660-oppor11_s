// SPDX-License-Identifier: GPL-2.0+
/*
 * auto_rndis.c -- Auto RNDIS Gadget Driver
 *
 * Proudly written with AI
 *
 * when it loaded,it wait UDC (USB Device Controller) register.
 *
 * device connect by USB and opening a RNDIS Gadget,
 * to easily use ssh and more with pc.
 * and my English is too bad Orz
 *
 * Copyright (C) 2026
 */

/* #define VERBOSE_DEBUG */

#include <linux/kernel.h>
#include <linux/netdevice.h>

#include "u_ether.h"
#include "u_rndis.h"
#include "rndis.h"

#define DRIVER_DESC	"OPPO R11s Linux Device"
#define DRIVER_VERSION	"1.1.4.5"

/*
 * default use rndis
 */
#define RNDIS_VENDOR_NUM	0x0525
#define RNDIS_PRODUCT_NUM	0xa4a2

/*-------------------------------------------------------------------------*/
USB_GADGET_COMPOSITE_OPTIONS();

USB_ETHERNET_MODULE_PARAMETERS();

static struct usb_device_descriptor device_desc = {
	.bLength =		sizeof(device_desc),
	.bDescriptorType =	USB_DT_DEVICE,

	/* .bcdUSB = DYNAMIC */

	.bDeviceClass =		USB_CLASS_COMM,
	.bDeviceSubClass =	0,
	.bDeviceProtocol =	0,
	/* .bMaxPacketSize0 = f(hardware) */

	.idVendor =		cpu_to_le16(RNDIS_VENDOR_NUM),
	.idProduct =		cpu_to_le16(RNDIS_PRODUCT_NUM),
	/* .bcdDevice = f(hardware) */
	/* .iManufacturer = DYNAMIC */
	/* .iProduct = DYNAMIC */
	.bNumConfigurations =	1,
};

static const struct usb_descriptor_header *otg_desc[2];

static struct usb_string strings_dev[] = {
	[USB_GADGET_MANUFACTURER_IDX].s = "Linux",
	[USB_GADGET_PRODUCT_IDX].s = DRIVER_DESC,
	[USB_GADGET_SERIAL_IDX].s = "",
	{  } /* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};

static struct usb_function_instance *fi_rndis;
static struct usb_function *f_rndis;

/*-------------------------------------------------------------------------*/

/*
 * RNDIS CONFIGS
 */
static int rndis_do_config(struct usb_configuration *c)
{
	int status;

	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
		c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	f_rndis = usb_get_function(fi_rndis);
	if (IS_ERR(f_rndis))
		return PTR_ERR(f_rndis);

	status = usb_add_function(c, f_rndis);
	if (status < 0)
		usb_put_function(f_rndis);

	return status;
}

static struct usb_configuration rndis_config_driver = {
	.label			= "RNDIS",
	.bConfigurationValue	= 1,
	/* .iConfiguration = DYNAMIC */
	.bmAttributes		= USB_CONFIG_ATT_SELFPOWER,
};

/*-------------------------------------------------------------------------*/

static int auto_rndis_bind(struct usb_composite_dev *cdev)
{
	struct usb_gadget	*gadget = cdev->gadget;
	struct f_rndis_opts	*rndis_opts;
	struct net_device	*net;
	int			status;

	fi_rndis = usb_get_function_instance("rndis");
	if (IS_ERR(fi_rndis))
		return PTR_ERR(fi_rndis);

	rndis_opts = container_of(fi_rndis, struct f_rndis_opts, func_inst);
	net = rndis_opts->net;

	gether_set_qmult(net, qmult);
	if (!gether_set_host_addr(net, host_addr))
		pr_info("using host ethernet address: %s", host_addr);
	if (!gether_set_dev_addr(net, dev_addr))
		pr_info("using self ethernet address: %s", dev_addr);

	gether_set_gadget(net, cdev->gadget);
	status = gether_register_netdev(net);
	if (status)
		goto fail;

	rndis_opts->bound = true;

	status = usb_string_ids_tab(cdev, strings_dev);
	if (status < 0)
		goto fail_netdev;
	device_desc.iManufacturer = strings_dev[USB_GADGET_MANUFACTURER_IDX].id;
	device_desc.iProduct = strings_dev[USB_GADGET_PRODUCT_IDX].id;

	if (gadget_is_otg(gadget) && !otg_desc[0]) {
		struct usb_descriptor_header *usb_desc;

		usb_desc = usb_otg_descriptor_alloc(gadget);
		if (!usb_desc) {
			status = -ENOMEM;
			goto fail_netdev;
		}
		usb_otg_descriptor_init(gadget, usb_desc);
		otg_desc[0] = usb_desc;
		otg_desc[1] = NULL;
	}

	status = usb_add_config(cdev, &rndis_config_driver, rndis_do_config);
	if (status < 0)
		goto fail_otg_desc;

	usb_composite_overwrite_options(cdev, &coverwrite);
	dev_info(&gadget->dev, "%s, version: " DRIVER_VERSION "\n",
			DRIVER_DESC);
	dev_info(&gadget->dev, "RNDIS gadget ready, interface: usb0\n");

	return 0;

fail_otg_desc:
	kfree(otg_desc[0]);
	otg_desc[0] = NULL;
fail_netdev:
	gether_cleanup(net);
fail:
	usb_put_function_instance(fi_rndis);
	return status;
}

static int auto_rndis_unbind(struct usb_composite_dev *cdev)
{
	usb_put_function(f_rndis);
	usb_put_function_instance(fi_rndis);

	kfree(otg_desc[0]);
	otg_desc[0] = NULL;

	return 0;
}

static struct usb_composite_driver auto_rndis_driver = {
	.name		= "g_auto_rndis",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.max_speed	= USB_SPEED_SUPER,
	.bind		= auto_rndis_bind,
	.unbind		= auto_rndis_unbind,
};

module_usb_composite_driver(auto_rndis_driver);

MODULE_DESCRIPTION("Auto RNDIS Gadget");
MODULE_AUTHOR("NanShengTeam and kimi 2.5");
MODULE_LICENSE("GPL");
