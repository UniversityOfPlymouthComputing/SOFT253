#include "mbed.h"
#include "rtos.h"
//This template include mbed-RTOS - if you rebuild all, you will see some warnings.

//Digital outputs
DigitalOut onBoardLED(LED1);


//Main thread
int main() {

	while(1) {
		onBoardLED = 1;
		wait(0.2);
		onBoardLED = 0;
		wait(0.2);
	}
			
}

