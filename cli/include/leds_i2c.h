#ifndef __UGREEN_LEDS_I2C_H__
#define __UGREEN_LEDS_I2C_H__

#include <array>
#include <optional>

#include "i2c.h"
#include "ugreen_leds.h"

// #define UGREEN_LED_I2C_DEV   "/dev/i2c-1"
#define UGREEN_LED_I2C_ADDR  0x3a

#define MAX_RETRY_COUNT 5
#define USLEEP_READ_STATUS_INTERVAL 8000
#define USLEEP_READ_STATUS_RETRY_INTERVAL 3000
#define USLEEP_MODIFICATION_INTERVAL 500
#define USLEEP_MODIFICATION_RETRY_INTERVAL 3000
#define USLEEP_MODIFICATION_QUERY_RESULT_INTERVAL 2000

class ugreen_leds_i2c : public ugreen_leds_t {

    i2c_device_t _i2c;

public:
    virtual ~ugreen_leds_i2c() = default;

    virtual int start();
    virtual led_data_t get_status(led_type_t id);
    virtual int set_onoff(led_type_t id, uint8_t status);
    virtual int set_rgb(led_type_t id, uint8_t r, uint8_t g, uint8_t b);
    virtual int set_brightness(led_type_t id, uint8_t brightness);
    virtual int set_blink(led_type_t id, uint16_t t_on, uint16_t t_off);
    virtual int set_breath(led_type_t id, uint16_t t_on, uint16_t t_off);

    virtual const char* get_name() { return "ugreen_leds_i2c"; }

private:
    bool _is_last_modification_successful();
    int _set_blink_or_breath(uint8_t command, led_type_t id, uint16_t t_on, uint16_t t_off);
    int _change_status_once(led_type_t id, uint8_t command, std::array<std::optional<uint8_t>, 4> params);
    int _change_status(led_type_t id, uint8_t command, std::array<std::optional<uint8_t>, 4> params);
    led_data_t _get_status_once(led_type_t id);
};



#endif
