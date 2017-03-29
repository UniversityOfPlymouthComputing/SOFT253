/*
 * mbed library program
 *  LPS25H MEMS pressure sensor: 260-1260 hPa absolute digital output barometer
 *   made by STMicroelectronics
 *   http://www.st-japan.co.jp/web/catalog/sense_power/FM89/SC1316/PF255230
 *
 * Copyright (c) 2015 Kenji Arai / JH1PJL
 *  http://www.page.sannet.ne.jp/kenjia/index.html
 *  http://mbed.org/users/kenjiArai/
 *      Created: Feburary  21st, 2015
 *      Revised: Feburary  22nd, 2015
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 *---------------- REFERENCE ----------------------------------------------------------------------
 * Original Information
 *  http://www.st-japan.co.jp/web/jp/catalog/sense_power/FM89/SC1316/PF255230
 * AN4450 Application Note (as follows)
 *  http://www.st.com/st-web-ui/static/active/jp/
 *         resource/technical/document/application_note/DM00108837.pdf
 * Device kit
 *  http://akizukidenshi.com/catalog/g/gK-08338/
 */

#ifndef LPS25H_H
#define LPS25H_H

#include "mbed.h"

//  LPS25H Address
//  7bit address = 0b101110x(0x5c or 0x5d depends on SA0/SDO)
#define LPS25H_G_CHIP_ADDR  (0x5c << 1)    // SA0(=SDO pin) = Ground
#define LPS25H_V_CHIP_ADDR  (0x5d << 1)    // SA0(=SDO pin) = Vdd

// MODE Selection
#define FIFO_HW_FILTER          1
#define FIFO_BYPASS             0

//   LPS25H ID
#define I_AM_LPS25H            0xbd

//  Register's definition
#define LPS25H_WHO_AM_I        0x0f
#define LPS25H_RES_CONF        0x10

#define LPS25H_CTRL_REG1       0x20
#define LPS25H_CTRL_REG2       0x21
#define LPS25H_CTRL_REG3       0x22
#define LPS25H_CTRL_REG4       0x23

#define LPS25H_STATUS_REG      0x27
#define LPS25H_PRESS_POUT_XL   0x28
#define LPS25H_PRESS_OUT_L     0x29
#define LPS25H_PRESS_OUT_H     0x2a
#define LPS25H_TEMP_OUT_L      0x2b
#define LPS25H_TEMP_OUT_H      0x2c

#define LPS25H_FIFO_CTRL       0x2e
#define LPS25H_FIFO_STATUS     0x2e

// Control Reg.
#define PD              (0UL << 7)
#define ACTIVE          (1UL << 7)
#define ODR_ONESHOT     (0UL << 4)
#define ODR_1HZ         (1UL << 4)
#define ODR_7HZ         (1UL << 4)
#define ODR_12R5HZ      (2UL << 4)
#define ODR_25HZ        (3UL << 4)
#define BDU_SET         (1UL << 2)
#define CR_STD_SET      (ACTIVE + ODR_7HZ + BDU_SET)

// FIFO Control
#define FIFO_MEAN_MODE         0xc0
#define FIFO_SAMPLE_2          0x01
#define FIFO_SAMPLE_4          0x03
#define FIFO_SAMPLE_8          0x07
#define FIFO_SAMPLE_16         0x0f
#define FIFO_SAMPLE_32         0x1f

/** Interface for STMicronics MEMS pressure sensor
 *      Chip: LPS25H
 *
 * @code
 * #include "mbed.h"
 * #include "LPS25H.h"
 *
 * // I2C Communication
 * LPS25H baro(p_sda, p_scl, LPS25H_G_CHIP_ADDR);
 * // If you connected I2C line not only this device but also other devices,
 * //     you need to declare following method.
 * I2C i2c(dp5,dp27);              // SDA, SCL
 * LPS25H baro(i2c, LPS25H_G_CHIP_ADDR);
 *
 * int main() {
 *   while( trure){
 *      baro.get();
 *      printf("Presere: 0x%6.1f, Temperature: 0x%+4.1f\r\n", baro.pressue(), baro.temperature());
 *      wait(1.0);
 *   }
 * }
 * @endcode
 */

class LPS25H
{
public:
    /** Configure data pin
      * @param data SDA and SCL pins
      * @param device address LPS25H(SA0=0 or 1), LPS25H_G_CHIP_ADDR or LPS25H_V_CHIP_ADDR
      * @param Operation mode FIFO_HW_FILTER(default) or FIFO_BYPASS (Option parameter)
      */
    LPS25H(PinName p_sda, PinName p_scl, uint8_t addr);
    LPS25H(PinName p_sda, PinName p_scl, uint8_t addr, uint8_t mode);

    /** Configure data pin (with other devices on I2C line)
      * @param I2C previous definition
      * @param device address LPS25H(SA0=0 or 1), LPS25H_G_CHIP_ADDR or LPS25H_V_CHIP_ADDR
      * @param Operation mode FIFO_HW_FILTER(default) or FIFO_BYPASS (Option parameter)
      */
    LPS25H(I2C& p_i2c, uint8_t addr);
    LPS25H(I2C& p_i2c, uint8_t addr, uint8_t mode);

    /** Start convertion & data save
      * @param none
      * @return none
      */
    void get(void);

    /** Read pressure data
      * @param none
      * @return humidity
      */
    float pressure(void);

    /** Read temperature data
      * @param none
      * @return temperature
      */
    float temperature(void);

    /** Read a ID number
      * @param none
      * @return if STM MEMS LPS25H, it should be I_AM_ LPS25H
      */
    uint8_t read_id(void);

    /** Read Data Ready flag
      * @param none
      * @return 1 = Ready
      */
    uint8_t data_ready(void);

    /** Set I2C clock frequency
      * @param freq.
      * @return none
      */
    void frequency(int hz);

    /** Read register (general purpose)
      * @param register's address
      * @return register data
      */
    uint8_t read_reg(uint8_t addr);

    /** Write register (general purpose)
      * @param register's address
      * @param data
      * @return none
      */
    void write_reg(uint8_t addr, uint8_t data);

protected:
    I2C _i2c;

    void init(void);

private:
    char dt[6];            // working buffer
    uint8_t LPS25H_addr;   // Sensor address
    uint8_t LPS25H_id;     // ID
    uint8_t LPS25H_ready;  // Device is on I2C line = 1, not = 0
    uint8_t LPS25H_mode;   // Operation mode
    uint32_t press;        // pressure raw data
    int16_t temp;          // temperature raw data
};

#endif      // LPS25H_H
