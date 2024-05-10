
#include <unistd.h>
#include <iostream>

#include "dx4600_leds.h"

int main()
{
    dx4600_leds_t leds;
     if (leds.start() == 0) {
         std::cout << "i2c device opened" << std::endl;
     }


    usleep(15000);
    std::cout << leds.set_onoff(2, 1) << std::endl;
    usleep(15000);
    std::cout << leds.set_rgb(2, 0xff, 0x00, 0xff) << std::endl;
    usleep(15000);
    std::cout << leds.set_blink(2, 0x100, 0x100) << std::endl;
    usleep(15000);
    std::cout << leds.set_blink(0, 0x100, 0x200) << std::endl;


    for (int i = 0; i < 6; ++i) {
        usleep(1500);
        auto data = leds.get_status(i);
        std::cout 
            << ", op_mode = " << (int)data.op_mode
            << ", brightness = " << data.brightness
            << ", R = " << data.color_r
            << ", G = " << data.color_g
            << ", B = " << data.color_b
            << ", t_on = " << data.t_on
            << ", t_off = " << data.t_off << std::endl;
    }

//    std::cout << leds.write_block_data(0x2, { 
//        2, 0, 0, 0, 1, 0, 0, 1, 0xa0, 0, 0, 0 }) << std::endl;
//        usleep(1500);

    return 0;
}

