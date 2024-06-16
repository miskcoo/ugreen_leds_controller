#ifndef __UGREEN_LEDS_H__
#define __UGREEN_LEDS_H__

#include <cstdint>
#include <iostream>
#include <memory>

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

#define UGREEN_LED_SOCKET_PATH "/tmp/led-ugreen.socket"

class ugreen_leds_t {

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

        led_data_t() : is_available(false) {}
    };

public:
    virtual int start() = 0;

    virtual ~ugreen_leds_t() = default;

    virtual led_data_t get_status(led_type_t id) = 0;
    virtual int set_onoff(led_type_t id, uint8_t status) = 0;
    virtual int set_rgb(led_type_t id, uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual int set_brightness(led_type_t id, uint8_t brightness) = 0;
    virtual int set_blink(led_type_t id, uint16_t t_on, uint16_t t_off) = 0;
    virtual int set_breath(led_type_t id, uint16_t t_on, uint16_t t_off) = 0;
    virtual int set_oneshot(led_type_t id, uint16_t t_on, uint16_t t_off) {
        std::cerr << "oneshot mode is not supported" << std::endl;
        return -1;
    }
    virtual int shot(led_type_t id) { 
        std::cerr << "oneshot mode is not supported" << std::endl;
        return -1;
    }

    virtual const char *get_name() = 0;

public:
    static std::shared_ptr<ugreen_leds_t> create_i2c_controller();
    static std::shared_ptr<ugreen_leds_t> create_kmod_controller();
    static std::shared_ptr<ugreen_leds_t> create_socket_controller();
};


#endif
