#include "../comedidev.h"
#include "comedi_fc.h"

#include "addi-data/addi_common.h"
#include "addi-data/addi_amcc_s5933.h"

#include "addi-data/addi_eeprom.c"
#include "addi-data/hwdrv_apci2032.c"
#include "addi-data/addi_common.c"

static const struct addi_board apci2032_boardtypes[] = {
	{
		.pc_DriverName		= "apci2032",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x1004,
		.i_IorangeBase0		= 4,
		.i_IorangeBase1		= APCI2032_ADDRESS_RANGE,
		.i_PCIEeprom		= ADDIDATA_EEPROM,
		.pc_EepromChip		= ADDIDATA_93C76,
		.i_NbrDoChannel		= 32,
		.i_DoMaxdata		= 0xffffffff,
		.i_Timer		= 1,
		.interrupt		= v_APCI2032_Interrupt,
		.reset			= i_APCI2032_Reset,
		.do_config		= i_APCI2032_ConfigDigitalOutput,
		.do_write		= i_APCI2032_WriteDigitalOutput,
		.do_bits		= i_APCI2032_ReadDigitalOutput,
		.do_read		= i_APCI2032_ReadInterruptStatus,
		.timer_config		= i_APCI2032_ConfigWatchdog,
		.timer_write		= i_APCI2032_StartStopWriteWatchdog,
		.timer_read		= i_APCI2032_ReadWatchdog,
	},
};

static struct comedi_driver apci2032_driver = {
	.driver_name	= "addi_apci_2032",
	.module		= THIS_MODULE,
	.attach_pci	= addi_attach_pci,
	.detach		= i_ADDI_Detach,
	.num_names	= ARRAY_SIZE(apci2032_boardtypes),
	.board_name	= &apci2032_boardtypes[0].pc_DriverName,
	.offset		= sizeof(struct addi_board),
};

static int __devinit apci2032_pci_probe(struct pci_dev *dev,
					const struct pci_device_id *ent)
{
	return comedi_pci_auto_config(dev, &apci2032_driver);
}

static void __devexit apci2032_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static DEFINE_PCI_DEVICE_TABLE(apci2032_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x1004) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, apci2032_pci_table);

static struct pci_driver apci2032_pci_driver = {
	.name		= "addi_apci_2032",
	.id_table	= apci2032_pci_table,
	.probe		= apci2032_pci_probe,
	.remove		= __devexit_p(apci2032_pci_remove),
};
module_comedi_pci_driver(apci2032_driver, apci2032_pci_driver);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
