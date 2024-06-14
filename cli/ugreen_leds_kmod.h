#ifndef __UGREEN_LEDS_KMOD_H__
#define __UGREEN_LEDS_KMOD_H__

#include "ugreen_leds.h"
#include <map>
#include <filesystem>

class ugreen_leds_kmod : public ugreen_leds_t {

    // (led_id, led_class_path)
    std::map<int, std::filesystem::path> _available_leds;

public:
    virtual ~ugreen_leds_kmod() = default;

    virtual int start();
    virtual led_data_t get_status(led_type_t id);
    virtual int set_onoff(led_type_t id, uint8_t status);
    virtual int set_rgb(led_type_t id, uint8_t r, uint8_t g, uint8_t b);
    virtual int set_brightness(led_type_t id, uint8_t brightness);
    virtual int set_blink(led_type_t id, uint16_t t_on, uint16_t t_off);
    virtual int set_breath(led_type_t id, uint16_t t_on, uint16_t t_off);
    virtual int set_oneshot(led_type_t id, uint16_t t_on, uint16_t t_off);
    virtual int shot(led_type_t id);
};



#endif
