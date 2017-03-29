/* Copyright (c) 2009 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *df
 */

#include <stdbool.h>
#include <stdint.h>

#include <mbed.h>
#include "hts221.h"

I2C i2c(I2C_SDA, I2C_SCL);    //SDA, SCL


/*lint ++flb "Enter library region" */

// Gill


//

static const char expected_who_am_i = 0xBCU; //!< Expected value to get from WHO_AM_I register.

uint8_t H0_rH_x2; 
uint8_t H1_rH_x2;
uint16_t T0_degC_x8;
uint16_t T1_degC_x8;

int16_t H0_T0_OUT;
int16_t H1_T0_OUT;
int16_t T0_OUT;
int16_t T1_OUT;

float T0_DegC_cal;
float T1_DegC_cal;
float H0_RH_cal;
float H1_RH_cal;

bool hts221_init(void)
{   
  bool transfer_succeeded = true;

  i2c.frequency(400000);
    hts221_register_write(0x10 , TRes_4 << 3 | HRes_5); 
    hts221_register_write(0x20 , PD_On | BDU_On  | ODR_1Hz); // Control register 1
    hts221_register_write(0x21 , NoBoot | HeaterOff  | No_OS); // Control register 2
    hts221_register_write(0x22 , DRDY_H | PP_OD_PP | DRDY_NON); // Control register 3
                                
  // Read and verify product ID
  transfer_succeeded &= hts221_verify_product_id();

  return transfer_succeeded;
}

bool hts221_verify_product_id(void)
{
    char who_am_i[1];
    hts221_register_read(ADDRESS_WHO_AM_I, &who_am_i[0], 1);
    if (who_am_i[0] != expected_who_am_i) return false;
    else return true;
}

void hts221_register_write(uint8_t register_address, uint8_t value)
{   
    char w2_data[2];
    
    w2_data[0] = register_address;
    w2_data[1] = value;
    i2c.write(HTS221_WriteADDE, w2_data, 2);      

}

void hts221_register_read(char register_address, char *destination, uint8_t number_of_bytes)
{
  i2c.write(HTS221_WriteADDE, &register_address, 1, 1);
  i2c.read(HTS221_WriteADDE, destination, number_of_bytes);   //Note by Tsungta, API may have a bug
  
  //runaboud read function added by Tsungta
/*  if (number_of_bytes == 1) {
      i2c.write(HTS221_WriteADDE, &register_address, 1, 1);
      i2c.write(HTS221_ReadADDE);
      *destination = i2c.read(0);
      i2c.stop();
  } else {
      register_address |= 0x80;
      i2c.write(HTS221_WriteADDE, &register_address, 1, 1);
      i2c.write(HTS221_ReadADDE);
      while (number_of_bytes-- > 0)
        *destination++ = i2c.read(0);
      i2c.stop();      
  }*/    
}
               
void HTS221_Calib(void) 
{
    char cal_data[16];
    
    hts221_register_read(0xB0, cal_data, 16);

  H0_rH_x2 = cal_data[0]; 
  H1_rH_x2 = cal_data[1];
  T0_degC_x8 = ((cal_data[5] & 0x03) << 8) + cal_data[2]; //MSB + LSB in 
  T1_degC_x8 = ((cal_data[5] & 0x0C) << 6) + cal_data[3]; // MSB

  H0_T0_OUT = (cal_data[7] << 8) + cal_data[6];  
  H1_T0_OUT = (cal_data[11] << 8) + cal_data[10];
  T0_OUT = (cal_data[13] << 8) + cal_data[12];
  T1_OUT = (cal_data[15] << 8) + cal_data[14];
    
    // convert negative 2's complement values to native negative value
    if (H0_T0_OUT&0x8000) H0_T0_OUT = -(0x8000-(0x7fff&H0_T0_OUT));
    if (H1_T0_OUT&0x8000) H1_T0_OUT = -(0x8000-(0x7fff&H1_T0_OUT));       //((~H1_T0_OUT)+1);//
    if (T0_OUT&0x8000) T0_OUT = -(0x8000-(0x7fff&T0_OUT));
    if (T1_OUT&0x8000) T1_OUT = -(0x8000-(0x7fff&T1_OUT));
    
  T0_DegC_cal = (float) T0_degC_x8/8;
  T1_DegC_cal = (float) T1_degC_x8/8;
  H0_RH_cal = (float) H0_rH_x2/2;
  H1_RH_cal = (float) H1_rH_x2/2;

}
    
void HTS221_ReadTempHumi( float *pTemp , float *pHumi)
{
  char sensor_data[4];
  int16_t H_OUT;
  int16_t T_OUT;   

    hts221_register_read(0xA8, sensor_data, 4);
    
    H_OUT = (sensor_data[1] << 8) + sensor_data[0];
    T_OUT = (sensor_data[3] << 8) + sensor_data[2];
    
    // convert negative 2's complement values to native negative value  
    if (H_OUT&0x8000) H_OUT = -(0x8000-(0x7fff&H_OUT));   //((~H_OUT)+1);;
    if (T_OUT&0x8000) T_OUT = -(0x8000-(0x7fff&T_OUT));
    
    *pTemp = linear_interpolation(T0_OUT, T0_DegC_cal, T1_OUT, T1_DegC_cal, T_OUT);
    *pHumi = linear_interpolation(H0_T0_OUT, H0_RH_cal, H1_T0_OUT, H1_RH_cal, H_OUT);
    // Constraint  for measurement after calibration 
    if ((int)*pHumi>MaxHumi-1 | (int)*pHumi==-72) *pHumi = MaxHumi;  
    if ((int)*pHumi<MinHumi ) *pHumi = MinHumi;
    if ((int)*pTemp>MaxTemp-1) *pHumi = MaxTemp; 
    if ((int)*pHumi<MinTemp ) *pHumi = MinTemp ;  
}

float linear_interpolation(int16_t x0, float y0, int16_t x1, float y1, float mes)
{
   float a = (float) ((y1 - y0) / (x1 - x0));
   float b = (float) -a*x0 + y0;
   float cal = (float) a * mes + b;
   return cal;
}

/*lint --flb "Leave library region" */

