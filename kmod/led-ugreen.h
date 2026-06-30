#ifndef __UGREEN_LED_H
#define __UGREEN_LED_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/leds.h>


#define MODULE_NAME             ( "led-ugreen" )

#define UGREEN_LED_SLAVE_ADDR       ( 0x3a )
#define UGREEN_LED_SLAVE_NAME       ( "led-ugreen" )

#define UGREEN_POWER_LED_COUNT              ( 1 )
#define UGREEN_DEFAULT_NETDEV_LED_COUNT     ( 1 )
#define UGREEN_DEFAULT_PROBE_LED_COUNT      ( 10 )
#define UGREEN_MAX_NETDEV_LED_COUNT         ( 9 )
#define UGREEN_MAX_DISK_LED_COUNT           ( 9 )
#define UGREEN_MAX_LED_NUMBER               \
    ( UGREEN_POWER_LED_COUNT + UGREEN_MAX_NETDEV_LED_COUNT + UGREEN_MAX_DISK_LED_COUNT )
#define UGREEN_LED_CHANGE_STATE_RETRY_COUNT   ( 5 )

#define UGREEN_LED_STATE_OFF        ( 0 )
#define UGREEN_LED_STATE_ON         ( 1 )
#define UGREEN_LED_STATE_BLINK      ( 2 )
#define UGREEN_LED_STATE_BREATH     ( 3 )
#define UGREEN_LED_STATE_INVALID    ( 4 )

static const char *ugreen_led_state_name[] = { "off", "on", "blink", "breath", "unknown" };

struct ugreen_led_array;

struct ugreen_led_state {
    u8 status;
    u8 r, g, b;
    u8 brightness;
    u16 t_on, t_cycle;

    u8 led_id;
    struct led_classdev cdev;
    struct ugreen_led_array *priv;
};

/* The MCU reports a chip id (word) at register UGREEN_LED_REG_CHIP_ID. The
 * value 0xc5b2 identifies a broad MCU family used by multiple UGREEN LED
 * protocol variants, so it must not be used to choose the write framing. */
#define UGREEN_LED_REG_CHIP_ID    ( 0x5a )

enum ugreen_led_write_protocol {
    // The 12-bytes write protocol using `i2c_smbus_write_i2c_block_data`
    UGREEN_LED_WRITE_PROTOCOL_LEGACY = 0,

    // The block-write framing for a series of models was first worked out by
    // @klein0r for the iDX6011 Pro
    // (fork: github.com/klein0r/ugreen_leds_controller).
    UGREEN_LED_WRITE_PROTOCOL_SMBUS_BLOCK,
};

struct ugreen_led_array {
    struct i2c_client *client;
    struct mutex mutex;
    u16 chip_id;
    enum ugreen_led_write_protocol write_protocol;
    struct ugreen_led_state state[UGREEN_MAX_LED_NUMBER];
};


#endif // __UGREEN_LED_H
