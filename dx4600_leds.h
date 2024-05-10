#ifndef __DX4600_LEDS_H__
#define __DX4600_LEDS_H__

#include "i2c.h"

class dx4600_leds_t {

    i2c_device_t _i2c;

public:

    enum class op_mode_t : uint8_t {
        off, on, blink, breath
    };

    struct led_data_t {
        op_mode_t op_mode;
        int brightness;
        int color_r, color_g, color_b;
        int t_on, t_off;

    };

public:
    int start();

    led_data_t get_status(int id);
    int set_onoff(int id, uint8_t status);
    int set_blink(int id, uint16_t t_on, uint16_t t_off);
    int set_rgb(int id, uint8_t r, uint8_t g, uint8_t b);
    int set_brightness(int id, uint8_t brightness);

private:
    int _change_status(int id, uint8_t command, std::array<uint8_t, 3> params);
};



#endif
