#ifndef __UGREEN_LEDS_H__
#define __UGREEN_LEDS_H__

#include <array>
#include <optional>

#include "i2c.h"

#define UGREEN_LED_POWER    ugreen_leds_t::led_type_t::power
#define UGREEN_LED_NETDEV   ugreen_leds_t::led_type_t::netdev
#define UGREEN_LED_DISK1    ugreen_leds_t::led_type_t::disk1
#define UGREEN_LED_DISK2    ugreen_leds_t::led_type_t::disk2
#define UGREEN_LED_DISK3    ugreen_leds_t::led_type_t::disk3
#define UGREEN_LED_DISK4    ugreen_leds_t::led_type_t::disk4
#define UGREEN_LED_DISK5    ugreen_leds_t::led_type_t::disk5
#define UGREEN_LED_DISK6    ugreen_leds_t::led_type_t::disk6
#define UGREEN_LED_DISK7    ugreen_leds_t::led_type_t::disk7
#define UGREEN_LED_DISK8    ugreen_leds_t::led_type_t::disk8

// #define UGREEN_LED_I2C_DEV   "/dev/i2c-1"
#define UGREEN_LED_I2C_ADDR  0x3a

class ugreen_leds_t {

    i2c_device_t _i2c;

public:

    enum class op_mode_t : uint8_t {
        off = 0, on, blink, breath
    };

    enum class led_type_t : uint8_t {
        power = 0, netdev, disk1, disk2, disk3, disk4, disk5, disk6, disk7, disk8
    };

    struct led_data_t {
        bool is_available;
        op_mode_t op_mode;
        uint8_t brightness;
        uint8_t color_r, color_g, color_b;
        uint16_t t_on, t_off;

    };

public:
    int start();

    led_data_t get_status(led_type_t id);
    int set_onoff(led_type_t id, uint8_t status);
    int set_rgb(led_type_t id, uint8_t r, uint8_t g, uint8_t b);
    int set_brightness(led_type_t id, uint8_t brightness);
    int set_blink(led_type_t id, uint16_t t_on, uint16_t t_off);
    int set_breath(led_type_t id, uint16_t t_on, uint16_t t_off);

    bool is_last_modification_successful();

private:
    int _set_blink_or_breath(uint8_t command, led_type_t id, uint16_t t_on, uint16_t t_off);
    int _change_status(led_type_t id, uint8_t command, std::array<std::optional<uint8_t>, 4> params);
};



#endif
