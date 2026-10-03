// SPDX-License-Identifier: GPL-2.0
#include "power_led.h"

int power_led_apply(struct led_controller *controller,
                    const struct config *config)
{
    int error;

    error = led_controller_set_color(controller, UGREEN_LED_POWER,
                                     config->power_color);
    if (!error)
        error = led_controller_set_brightness(
            controller, UGREEN_LED_POWER, (uint8_t)config->power_brightness);
    if (error < 0)
        return error;

    switch (config->power_mode) {
    case POWER_MODE_ON:
        return led_controller_set_on(controller, UGREEN_LED_POWER, true);
    case POWER_MODE_BLINK:
        return led_controller_set_blink(
            controller, UGREEN_LED_POWER,
            (uint16_t)config->power_on_ms,
            (uint16_t)config->power_off_ms, false);
    case POWER_MODE_BREATH:
        return led_controller_set_blink(
            controller, UGREEN_LED_POWER,
            (uint16_t)config->power_on_ms,
            (uint16_t)config->power_off_ms, true);
    }
    return -1;
}
