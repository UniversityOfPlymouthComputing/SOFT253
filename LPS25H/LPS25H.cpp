/*
 * mbed library program
 *  LPS25H MEMS pressure sensor: 260-1260 hPa absolute digital output barometer
 *   made by STMicroelectronics
 *   http://www.st-japan.co.jp/web/catalog/sense_power/FM89/SC1316/PF255230?s_searchtype=partnumber
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

#include "LPS25H.h"

LPS25H::LPS25H (PinName p_sda, PinName p_scl, uint8_t addr) : _i2c(p_sda, p_scl)
{
    LPS25H_addr = addr;
    LPS25H_mode = FIFO_HW_FILTER;
    init();
}

LPS25H::LPS25H (PinName p_sda, PinName p_scl, uint8_t addr, uint8_t mode) : _i2c(p_sda, p_scl)
{
    LPS25H_addr = addr;
    LPS25H_mode = mode;
    init();
}

LPS25H::LPS25H (I2C& p_i2c, uint8_t addr) : _i2c(p_i2c)
{
    LPS25H_addr = addr;
    LPS25H_mode = FIFO_HW_FILTER;
    init();
}

LPS25H::LPS25H (I2C& p_i2c, uint8_t addr, uint8_t mode) : _i2c(p_i2c)
{
    LPS25H_addr = addr;
    LPS25H_mode = mode;
    init();
}

/////////////// Initialize ////////////////////////////////
void LPS25H::init(void)
{
    _i2c.frequency(100000);
    // Check acc is available of not
    dt[0] = LPS25H_WHO_AM_I;
    _i2c.write(LPS25H_addr, dt, 1, true);
    _i2c.read(LPS25H_addr, dt, 1, false);
    if (dt[0] == I_AM_LPS25H) {
        LPS25H_id = I_AM_LPS25H;
        LPS25H_ready = 1;
    } else {
        LPS25H_id = 0;
        LPS25H_ready = 0;
        return;     // acc chip is NOT on I2C line then terminate
    }
    if (LPS25H_mode == FIFO_HW_FILTER){
        // Hardware digital filter
        // AN4450 April 2014 Rev1 P20/26, Filter enabling and suggested configuration
        dt[0] = LPS25H_RES_CONF;
        dt[1] = 0x05;
        _i2c.write(LPS25H_addr, dt, 2, false);
        dt[0] = LPS25H_FIFO_CTRL;
        dt[1] = 0xdf;
        _i2c.write(LPS25H_addr, dt, 2, false);
        dt[0] = LPS25H_CTRL_REG2;
        dt[1] = 0x40;
        _i2c.write(LPS25H_addr, dt, 2, false);
        dt[0] = LPS25H_CTRL_REG1;
        dt[1] = 0x90;
        _i2c.write(LPS25H_addr, dt, 2, false);
    } else {
        dt[0] = LPS25H_CTRL_REG2;
        dt[1] = 0x0;
        _i2c.write(LPS25H_addr, dt, 2, false);       
        dt[0] = LPS25H_CTRL_REG1;
        dt[1] = CR_STD_SET;
        _i2c.write(LPS25H_addr, dt, 2, false);
    }
}

/////////////// Start conv. and gwt all data //////////////
void LPS25H::get(void)
{
    if (LPS25H_ready == 0) {
        press = 0;
        temp = 0;
        return;
    }
    dt[0] = LPS25H_PRESS_POUT_XL | 0x80;
    _i2c.write(LPS25H_addr, dt, 1, true);
    _i2c.read(LPS25H_addr, dt, 3, false);
    press = dt[2] << 16 | dt[1] << 8 | dt[0];
    dt[0] = LPS25H_TEMP_OUT_L | 0x80;
    _i2c.write(LPS25H_addr, dt, 1, true);
    _i2c.read(LPS25H_addr, dt, 2, false);
    temp = dt[1] << 8 | dt[0];
}

/////////////// Read data from sensor /////////////////////
float LPS25H::pressure()
{
    return (float)press / 4096;
}

/////////////// Read data from sensor /////////////////////
float LPS25H::temperature()
{
    return (float)temp / 480  + 42.5;
}

/////////////// ID ////////////////////////////////////////
uint8_t LPS25H::read_id()
{
    dt[0] = LPS25H_WHO_AM_I;
    _i2c.write(LPS25H_addr, dt, 1, true);
    _i2c.read(LPS25H_addr, dt, 1, false);
    return (uint8_t)dt[0];
}

/////////////// I2C Freq. /////////////////////////////////
void LPS25H::frequency(int hz)
{
    _i2c.frequency(hz);
}

/////////////// General purpose R/W ///////////////////////
uint8_t LPS25H::read_reg(uint8_t addr)
{
    if (LPS25H_ready == 1) {
        dt[0] = addr;
        _i2c.write(LPS25H_addr, dt, 1, true);
        _i2c.read(LPS25H_addr, dt, 1, false);
    } else {
        dt[0] = 0xff;
    }
    return (uint8_t)dt[0];
}

void LPS25H::write_reg(uint8_t addr, uint8_t data)
{
    if (LPS25H_ready == 1) {
        dt[0] = addr;
        dt[1] = data;
        _i2c.write(LPS25H_addr, dt, 2, false);
    }
}
