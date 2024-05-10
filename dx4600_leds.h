#ifndef __DX4600_LEDS_H__
#define __DX4600_LEDS_H__

#include <array>
#include <optional>

#include "i2c.h"

#define DX4600_LED_POWER    dx4600_leds_t::led_type_t::power
#define DX4600_LED_NETDEV   dx4600_leds_t::led_type_t::netdev
#define DX4600_LED_DISK1    dx4600_leds_t::led_type_t::disk1
#define DX4600_LED_DISK2    dx4600_leds_t::led_type_t::disk2
#define DX4600_LED_DISK3    dx4600_leds_t::led_type_t::disk3
#define DX4600_LED_DISK4    dx4600_leds_t::led_type_t::disk4

#define DX4600_LED_I2C_DEV   "/dev/i2c-1"
#define DX4600_LED_I2C_ADDR  0x3a

class dx4600_leds_t {

    i2c_device_t _i2c;

public:

    enum class op_mode_t : uint8_t {
        off = 0, on, blink, breath
    };

    enum class led_type_t : uint8_t {
        power = 0, netdev, disk1, disk2, disk3, disk4
    };

    struct led_data_t {
        op_mode_t op_mode;
        uint8_t brightness;
        uint8_t color_r, color_g, color_b;
        uint16_t t_on, t_off;

    };

public:
    int start(const char *dev_path = DX4600_LED_I2C_DEV, uint8_t dev_addr = DX4600_LED_I2C_ADDR);

    led_data_t get_status(led_type_t id);
    int set_onoff(led_type_t id, uint8_t status);
    int set_blink(led_type_t id, uint16_t t_on, uint16_t t_off);
    int set_rgb(led_type_t id, uint8_t r, uint8_t g, uint8_t b);
    int set_brightness(led_type_t id, uint8_t brightness);

private:
    int _change_status(led_type_t id, uint8_t command, std::array<std::optional<uint8_t>, 4> params);
};



#endif
