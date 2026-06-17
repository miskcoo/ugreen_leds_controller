#ifndef __UGREEN_LED_H
#define __UGREEN_LED_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/leds.h>


#define MODULE_NAME             ( "led-ugreen" )

#define UGREEN_LED_SLAVE_ADDR       ( 0x3a )
#define UGREEN_LED_SLAVE_NAME       ( "led-ugreen" )

#define UGREEN_MAX_LED_NUMBER           ( 10 )
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
 * value 0xc5b2 identifies the MCU found in the UGREEN DXP4800 GT and AI NAS
 * iDX6011 Pro (UGREEN's factory driver refers to this part as "iDX601x"); it
 * requires SMBus block-write framing. Other models report a different or absent
 * id and use the legacy i2c-block-write framing. Constants are named after the
 * measured id rather than the vendor part name.
 *
 * The block-write framing for this MCU family was first worked out by
 * @klein0r for the iDX6011 Pro (fork: github.com/klein0r/ugreen_leds_controller);
 * here it is gated on the chip id read from the device so it also covers the
 * DXP4800 GT without relying on the DMI model name. */
#define UGREEN_LED_REG_CHIP_ID    ( 0x5a )
#define UGREEN_LED_CHIP_ID_C5B2   ( 0xc5b2 )

struct ugreen_led_array {
    struct i2c_client *client;
    struct mutex mutex;
    u16 chip_id;
    struct ugreen_led_state state[UGREEN_MAX_LED_NUMBER];
};


#endif // __UGREEN_LED_H
