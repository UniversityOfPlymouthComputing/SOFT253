
#include "mbed.h"

//#include "rtos.h"
#include "hts221.h"
#include "LPS25H.h"


DigitalOut myled(LED1);
I2C i2c2(I2C_SDA, I2C_SCL);

float tempCelsius = 25.50;
float humi = 55;
int humiMax = 100; 
char cmd=0;
uint32_t seconds = 0, minutes=0, hours=0; 

LPS25H barometer(i2c2, LPS25H_V_CHIP_ADDR);



int main()
  {
  hts221_init();
  HTS221_Calib();
  printf("SOFT253 simple Temperature Humidity and Pressure Sensor Monitor\n\r");
  printf("Using the X-NUCLEO-IKS01A1 shield and MBED Libraries\n\r");
    //printf("%#x\n\r",barometer.read_id());
    
  while(1) 
    {
      cmd=NULL;
      while(cmd==NULL){cmd=getchar();}
      if(cmd=='?'){
        printf("SOFT253 simple Temperature Humidity and Pressure Sensor Monitor\n\r");
        printf("Using the X-NUCLEO-IKS01A1 shield and MBED Libraries\n\r");
      }
      if(cmd=='A'){
        HTS221_ReadTempHumi(&tempCelsius, &humi);
        printf("%4.2fC %3.1f%%", tempCelsius, humi);
        barometer.get();
        printf(" %6.1f %4.1f\r\n", barometer.pressure(), barometer.temperature());
        myled = 1; // LED is ON
        Thread::wait(200); // 200 ms NB 'Thread::wait(int d);' !!! d is in milliseconds! 
        myled = 0; // LED is OFF
        Thread::wait(100); // 100 ms
      }
    }
  }
  


/*#include "mbed.h"

DigitalOut led1(LED1);

// main() runs in its own thread in the OS
int main() {
    while (true) {
        led1 = !led1;
        wait(0.5);
    }
}
*/
