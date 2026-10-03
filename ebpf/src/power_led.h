// SPDX-License-Identifier: GPL-2.0
#ifndef UGREEN_POWER_LED_H
#define UGREEN_POWER_LED_H

#include "config.h"
#include "led_controller.h"

int power_led_apply(struct led_controller *controller,
                    const struct config *config);

#endif
