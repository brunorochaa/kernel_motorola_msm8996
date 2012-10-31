#include "../comedidev.h"
#include "comedi_fc.h"

#include "addi-data/addi_common.h"
#include "addi-data/addi_amcc_s5933.h"

#include "addi-data/addi_eeprom.c"
#include "addi-data/hwdrv_apci3xxx.c"
#include "addi-data/addi_common.c"

static const struct addi_board apci3xxx_boardtypes[] = {
	{
		.pc_DriverName		= "apci3000-16",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3010,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_AiMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3000-8",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x300F,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_AiMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3000-4",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x300E,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 4,
		.i_NbrAiChannelDiff	= 2,
		.i_AiChannelList	= 4,
		.i_AiMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3006-16",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3013,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3006-8",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3014,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3006-4",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3015,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 4,
		.i_NbrAiChannelDiff	= 2,
		.i_AiChannelList	= 4,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3010-16",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3016,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_AiMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3010-8",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3017,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_AiMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3010-4",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3018,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 4,
		.i_NbrAiChannelDiff	= 2,
		.i_AiChannelList	= 4,
		.i_AiMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3016-16",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3019,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3016-8",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x301A,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3016-4",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x301B,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 4,
		.i_NbrAiChannelDiff	= 2,
		.i_AiChannelList	= 4,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3100-16-4",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x301C,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 4095,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3100-8-4",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x301D,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 4095,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3106-16-4",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x301E,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 65535,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3106-8-4",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x301F,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 65535,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3110-16-4",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3020,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 4095,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3110-8-4",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3021,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 4095,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3116-16-4",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3022,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 65535,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3116-8-4",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3023,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 65535,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	}, {
		.pc_DriverName		= "apci3003",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x300B,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 4,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.b_AvailableConvertUnit	= 7,
		.ui_MinAcquisitiontimeNs = 2500,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
	}, {
		.pc_DriverName		= "apci3002-16",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3002,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannelDiff	= 16,
		.i_AiChannelList	= 16,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
	}, {
		.pc_DriverName		= "apci3002-8",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3003,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 8,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
	}, {
		.pc_DriverName		= "apci3002-4",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3004,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 4,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_read		= i_APCI3XXX_InsnReadDigitalInput,
		.di_bits		= i_APCI3XXX_InsnBitsDigitalInput,
		.do_write		= i_APCI3XXX_InsnWriteDigitalOutput,
		.do_bits		= i_APCI3XXX_InsnBitsDigitalOutput,
		.do_read		= i_APCI3XXX_InsnReadDigitalOutput,
	}, {
		.pc_DriverName		= "apci3500",
		.i_VendorId		= PCI_VENDOR_ID_ADDIDATA,
		.i_DeviceId		= 0x3024,
		.i_IorangeBase0		= 256,
		.i_IorangeBase1		= 256,
		.i_IorangeBase2		= 256,
		.i_IorangeBase3		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAoChannel		= 4,
		.i_AoMaxdata		= 4095,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrTTLChannel	= 24,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
};

static struct comedi_driver apci3xxx_driver = {
	.driver_name	= "addi_apci_3xxx",
	.module		= THIS_MODULE,
	.attach_pci	= addi_attach_pci,
	.detach		= i_ADDI_Detach,
	.num_names	= ARRAY_SIZE(apci3xxx_boardtypes),
	.board_name	= &apci3xxx_boardtypes[0].pc_DriverName,
	.offset		= sizeof(struct addi_board),
};

static int __devinit apci3xxx_pci_probe(struct pci_dev *dev,
					const struct pci_device_id *ent)
{
	return comedi_pci_auto_config(dev, &apci3xxx_driver);
}

static void __devexit apci3xxx_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static DEFINE_PCI_DEVICE_TABLE(apci3xxx_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3010) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x300f) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x300e) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3013) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3014) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3015) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3016) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3017) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3018) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3019) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x301a) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x301b) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x301c) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x301d) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x301e) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x301f) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3020) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3021) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3022) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3023) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x300B) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3002) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3003) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3004) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADDIDATA, 0x3024) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, apci3xxx_pci_table);

static struct pci_driver apci3xxx_pci_driver = {
	.name		= "addi_apci_3xxx",
	.id_table	= apci3xxx_pci_table,
	.probe		= apci3xxx_pci_probe,
	.remove		= __devexit_p(apci3xxx_pci_remove),
};
module_comedi_pci_driver(apci3xxx_driver, apci3xxx_pci_driver);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
