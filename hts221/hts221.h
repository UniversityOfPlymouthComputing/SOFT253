/* Copyright (c) 2009 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#ifndef HTS221_H
#define HTS221_H

/*lint ++flb "Enter library region" */

#include <stdbool.h>
#include <stdint.h>

#define ADDRESS_WHO_AM_I (0x0FU) //!< WHO_AM_I register identifies the device. Expected value is 0xBC.

#define HTS221_WriteADDE         0xBE
#define HTS221_ReadADDE          0xBF
#define HTS221_TempHumi_OUT      0x28
#define HTS221_CALIB             0x30

#define MaxTemp       120
#define MinTemp       -40
#define MaxHumi       100
#define MinHumi       0

// Humidity and temperature resolution mode, to ocnfigure sample average
#define TRes_1        000 // Number of temperature samples take 2^1,2^2...etc.
#define TRes_2        001
#define TRes_3        010
#define TRes_4        011
#define TRes_5        100
#define TRes_6        101
#define TRes_7        102
#define TRes_8        103

#define HRes_2        000 // Number of humidity samples take 2^2,2^3...etc.
#define HRes_3        001
#define HRes_4        010
#define HRes_5        011
#define HRes_6        100
#define HRes_7        101
#define HRes_8        102
#define HRes_9        103

// Control register 1
#define PD_On                    0x80 // Power down mode
#define PD_Off                   0x00 // Active mode

#define BDU_On                   0x04 // Block data update, this feature prevents the reading of LSB and MSB related to different samples.
#define BDU_Off                  0x00 
// Define output data rate
#define ODR_OneShot              0x00
#define ODR_1Hz                  0x01
#define ODR_7Hz                  0x02
#define ODR_12_5Hz               0x03
// Control register 2
#define Boot                     0x80
#define NoBoot                   0x00
#define HeaterOn                 0x02
#define HeaterOff                0x00
#define New_OS                   0x01 // One shot, a single acquisition of temperature and humidity is started
#define No_OS                    0x00
// Control register 3
#define DRDY_H                   0x00 // Data Ready output signal active high(default)
#define DRDY_L                   0x80 // Data Ready output signal active low
#define PP_OD_PP                 0x00 // Push-pull on PIN3(DRDY)(default)
#define PP_OD_OD                 0x40 // Open Drain on PIN3(DRDY)
#define DRDY_EN                  0x04 // Data Ready enable
#define DRDY_NON                 0x00 // Data Ready disable(default)

// Status register
#define H_DA_On                  0x02 // Humidity data avialable, set to 1 whenever a new humidity sample is available.
#define H_DA_Off                 0x00
#define T_DA_On                  0x01 // Temperature data avialable, set to 1 whenever a new humidity sample is available.
#define T_DA_Off                 0x00
 
bool hts221_init(void);

void hts221_register_write(uint8_t register_address, const uint8_t value);

void hts221_register_read(char register_address, char *destination, uint8_t number_of_bytes);

bool hts221_verify_product_id(void);

void HTS221_Calib(void);

void HTS221_ReadTempHumi( float *pTemp , float *pHumi);

float linear_interpolation(int16_t x0, float y0, int16_t x1, float y1, float mes);

#endif /* HTS221_H */

