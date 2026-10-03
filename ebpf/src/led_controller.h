// SPDX-License-Identifier: GPL-2.0
#ifndef UGREEN_LED_CONTROLLER_H
#define UGREEN_LED_CONTROLLER_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "types.h"

enum led_mode {
    LED_MODE_UNKNOWN,
    LED_MODE_OFF,
    LED_MODE_ON,
    LED_MODE_BLINK,
    LED_MODE_BREATH,
};

struct led_cache {
    bool color_valid;
    bool brightness_valid;
    struct rgb color;
    uint8_t brightness;
    enum led_mode mode;
    uint16_t on_ms;
    uint16_t off_ms;
};

struct led_controller {
    int fd;
    char path[PATH_MAX];
    struct led_cache cache[UGREEN_LED_COUNT];
};

int led_controller_open(struct led_controller *controller,
                        const char *override_path);
void led_controller_close(struct led_controller *controller);
void led_controller_invalidate(struct led_controller *controller,
                               uint8_t led_id);
int led_controller_set_color(struct led_controller *controller,
                             uint8_t led_id, struct rgb color);
int led_controller_set_brightness(struct led_controller *controller,
                                  uint8_t led_id, uint8_t brightness);
int led_controller_set_on(struct led_controller *controller,
                          uint8_t led_id, bool on);
int led_controller_set_blink(struct led_controller *controller,
                             uint8_t led_id, uint16_t on_ms,
                             uint16_t off_ms, bool breath);

#endif
