/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2007, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************

	Module Name:
	rt3370.c

	Abstract:
	Specific funcitons and variables for RT30xx.

	Revision History:
	Who         When          What
	--------    ----------    ----------------------------------------------
*/

#ifdef RT3370

#include "../rt_config.h"


#ifndef RTMP_RF_RW_SUPPORT
#error "You Should Enable compile flag RTMP_RF_RW_SUPPORT for this chip"
#endif // RTMP_RF_RW_SUPPORT //


VOID NICInitRT3370RFRegisters(IN PRTMP_ADAPTER pAd)
{
		INT i;
	// Driver must read EEPROM to get RfIcType before initial RF registers
	// Initialize RF register to default value
	if (IS_RT3090(pAd)||IS_RT3390(pAd)||IS_RT3572(pAd))
	{
		// Init RF calibration
		// Driver should toggle RF R30 bit7 before init RF registers
		UINT32 RfReg = 0, data;

		RT30xxReadRFRegister(pAd, RF_R30, (PUCHAR)&RfReg);
		RfReg |= 0x80;
		RT30xxWriteRFRegister(pAd, RF_R30, (UCHAR)RfReg);
		RTMPusecDelay(1000);
		RfReg &= 0x7F;
		RT30xxWriteRFRegister(pAd, RF_R30, (UCHAR)RfReg);

		// init R24, R31
		RT30xxWriteRFRegister(pAd, RF_R24, 0x0F);
		RT30xxWriteRFRegister(pAd, RF_R31, 0x0F);

		if (IS_RT3390(pAd))
		{
			// patch LNA_PE_G1 failed issue
			RTMP_IO_READ32(pAd, GPIO_SWITCH, &data);
			data &= ~(0x20);
			RTMP_IO_WRITE32(pAd, GPIO_SWITCH, data);

			// RF registers initialization
			for (i = 0; i < NUM_RF_REG_PARMS_OVER_RT3390; i++)
			{
				RT30xxWriteRFRegister(pAd, RFRegTableOverRT3390[i].Register, RFRegTableOverRT3390[i].Value);
			}
		}

		// patch LNA_PE_G1 failed issue
		RTMP_IO_READ32(pAd, GPIO_SWITCH, &data);
		data &= ~(0x20);
		RTMP_IO_WRITE32(pAd, GPIO_SWITCH, data);

		// Initialize RF register to default value
		for (i = 0; i < NUM_RF_REG_PARMS_OVER_RT3390; i++)
		{
			RT30xxWriteRFRegister(pAd, RT30xx_RFRegTable[i].Register, RT30xx_RFRegTable[i].Value);
		}

		// Driver should set RF R6 bit6 on before calibration
		RT30xxReadRFRegister(pAd, RF_R06, (PUCHAR)&RfReg);
		RfReg |= 0x40;
		RT30xxWriteRFRegister(pAd, RF_R06, (UCHAR)RfReg);

		//For RF filter Calibration
		RTMPFilterCalibration(pAd);

		// Initialize RF R27 register, set RF R27 must be behind RTMPFilterCalibration()
		if ((pAd->MACVersion & 0xffff) < 0x0211)
			RT30xxWriteRFRegister(pAd, RF_R27, 0x3);

		// set led open drain enable
		RTMP_IO_READ32(pAd, OPT_14, &data);
		data |= 0x01;
		RTMP_IO_WRITE32(pAd, OPT_14, data);

		// set default antenna as main
		if (pAd->RfIcType == RFIC_3020)
			AsicSetRxAnt(pAd, pAd->RxAnt.Pair1PrimaryRxAnt);

		// add by johnli, RF power sequence setup, load RF normal operation-mode setup
		RT30xxLoadRFNormalModeSetup(pAd);
	}

}
#endif // RT3070 //
