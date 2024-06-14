#ifndef __UGREEN_LEDS_KMOD_H__
#define __UGREEN_LEDS_KMOD_H__

#include "ugreen_leds.h"

class ugreen_leds_socket : public ugreen_leds_t {

    int sockfd;

public:
    ugreen_leds_socket() : sockfd(-1) { }
    virtual ~ugreen_leds_socket();

    virtual int start();
    virtual led_data_t get_status(led_type_t id);
    virtual int set_onoff(led_type_t id, uint8_t status);
    virtual int set_rgb(led_type_t id, uint8_t r, uint8_t g, uint8_t b);
    virtual int set_brightness(led_type_t id, uint8_t brightness);
    virtual int set_blink(led_type_t id, uint16_t t_on, uint16_t t_off);
    virtual int set_breath(led_type_t id, uint16_t t_on, uint16_t t_off);
};



#endif
