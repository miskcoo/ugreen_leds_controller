
#include <unistd.h>
#include <iostream>

#include "dx4600_leds.h"

void print_leds_status(dx4600_leds_t *leds) {

    for (auto i : { DX4600_LED_POWER, DX4600_LED_NETDEV, DX4600_LED_DISK1, DX4600_LED_DISK2, DX4600_LED_DISK3, DX4600_LED_DISK4 } ) {
        usleep(1500);
        auto data = leds->get_status(i);
        std::cout 
            << "ID = " << (int)i 
            << ", op_mode = " << (int)data.op_mode
            << ", brightness = " << (int)data.brightness
            << ", R = " << (int)data.color_r
            << ", G = " << (int)data.color_g
            << ", B = " << (int)data.color_b
            << ", t_on = " << (int)data.t_on
            << ", t_off = " << (int)data.t_off << std::endl;
    }
}

int main()
{
    dx4600_leds_t leds;
     if (leds.start() == 0) {
         std::cout << "i2c device opened" << std::endl;
     }

     print_leds_status(&leds);

     usleep(15000);
     std::cout << leds.set_onoff(DX4600_LED_DISK1, 0) << std::endl;
     usleep(15000);
//     std::cout << leds.set_rgb(DX4600_LED_DISK1, 0xff, 0x00, 0xff) << std::endl;
//     usleep(15000);
//     std::cout << leds.set_blink(DX4600_LED_DISK1, 0x100, 0x200) << std::endl;
     usleep(15000);
     std::cout << leds.set_blink(DX4600_LED_POWER, 0x64, 0x200) << std::endl;

     print_leds_status(&leds);


//    std::cout << leds.write_block_data(0x2, { 
//        2, 0, 0, 0, 1, 0, 0, 1, 0xa0, 0, 0, 0 }) << std::endl;
//        usleep(1500);

    return 0;
}

