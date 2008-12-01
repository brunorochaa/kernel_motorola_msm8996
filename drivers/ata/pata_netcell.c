/*
 *    pata_netcell.c - Netcell PATA driver
 *
 *	(c) 2006 Red Hat
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>
#include <linux/ata.h>

#define DRV_NAME	"pata_netcell"
#define DRV_VERSION	"0.1.7"

/* No PIO or DMA methods needed for this device */

static struct scsi_host_template netcell_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};

static struct ata_port_operations netcell_ops = {
	.inherits	= &ata_bmdma_port_ops,
	.cable_detect		= ata_cable_80wire,
};


/**
 *	netcell_init_one - Register Netcell ATA PCI device with kernel services
 *	@pdev: PCI device to register
 *	@ent: Entry in netcell_pci_tbl matching with @pdev
 *
 *	Called from kernel PCI layer.
 *
 *	LOCKING:
 *	Inherited from PCI layer (may sleep).
 *
 *	RETURNS:
 *	Zero on success, or -ERRNO value.
 */

static int netcell_init_one (struct pci_dev *pdev, const struct pci_device_id *ent)
{
	static int printed_version;
	static const struct ata_port_info info = {
		.flags		= ATA_FLAG_SLAVE_POSS,
		/* Actually we don't really care about these as the
		   firmware deals with it */
		.pio_mask	= 0x1f,	/* pio0-4 */
		.mwdma_mask	= 0x07, /* mwdma0-2 */
		.udma_mask 	= ATA_UDMA5, /* UDMA 133 */
		.port_ops	= &netcell_ops,
	};
	const struct ata_port_info *port_info[] = { &info, NULL };
	int rc;

	if (!printed_version++)
		dev_printk(KERN_DEBUG, &pdev->dev,
			   "version " DRV_VERSION "\n");

	rc = pcim_enable_device(pdev);
	if (rc)
		return rc;

	/* Any chip specific setup/optimisation/messages here */
	ata_pci_bmdma_clear_simplex(pdev);

	/* And let the library code do the work */
	return ata_pci_sff_init_one(pdev, port_info, &netcell_sht, NULL);
}

static const struct pci_device_id netcell_pci_tbl[] = {
	{ PCI_VDEVICE(NETCELL, PCI_DEVICE_ID_REVOLUTION), },

	{ }	/* terminate list */
};

static struct pci_driver netcell_pci_driver = {
	.name			= DRV_NAME,
	.id_table		= netcell_pci_tbl,
	.probe			= netcell_init_one,
	.remove			= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend		= ata_pci_device_suspend,
	.resume			= ata_pci_device_resume,
#endif
};

static int __init netcell_init(void)
{
	return pci_register_driver(&netcell_pci_driver);
}

static void __exit netcell_exit(void)
{
	pci_unregister_driver(&netcell_pci_driver);
}

module_init(netcell_init);
module_exit(netcell_exit);

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("SCSI low-level driver for Netcell PATA RAID");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, netcell_pci_tbl);
MODULE_VERSION(DRV_VERSION);

