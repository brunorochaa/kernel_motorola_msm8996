/*
 * $Id: id.h,v 1.4 2005/06/12 04:19:19 mchehab Exp $
 */

/* FIXME: this temporarely, until these are included in linux/i2c-id.h */

/* drivers */
#ifndef  I2C_DRIVERID_TVMIXER
# define I2C_DRIVERID_TVMIXER I2C_DRIVERID_EXP0
#endif
#ifndef  I2C_DRIVERID_TVAUDIO
# define I2C_DRIVERID_TVAUDIO I2C_DRIVERID_EXP1
#endif

/* chips */
#ifndef  I2C_DRIVERID_DPL3518
# define I2C_DRIVERID_DPL3518 I2C_DRIVERID_EXP2
#endif
#ifndef  I2C_DRIVERID_TDA9873
# define I2C_DRIVERID_TDA9873 I2C_DRIVERID_EXP3
#endif
#ifndef  I2C_DRIVERID_TDA9875
# define I2C_DRIVERID_TDA9875 I2C_DRIVERID_EXP0+4
#endif
#ifndef  I2C_DRIVERID_PIC16C54_PV951
# define I2C_DRIVERID_PIC16C54_PV951 I2C_DRIVERID_EXP0+5
#endif
#ifndef  I2C_DRIVERID_TDA7432
# define I2C_DRIVERID_TDA7432 I2C_DRIVERID_EXP0+6
#endif
#ifndef  I2C_DRIVERID_TDA9874
# define I2C_DRIVERID_TDA9874 I2C_DRIVERID_EXP0+7
#endif
#ifndef  I2C_DRIVERID_SAA6752HS
# define I2C_DRIVERID_SAA6752HS I2C_DRIVERID_EXP0+8
#endif

/* algorithms */
#ifndef I2C_ALGO_SAA7134
# define I2C_ALGO_SAA7134 0x090000
#endif
