// SPDX-License-Identifier: GPL-2.0
#ifndef UGREEN_NETWORK_LED_H
#define UGREEN_NETWORK_LED_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "led_controller.h"
#include "scheduler.h"

struct network_led {
    struct pulse_scheduler scheduler;
    struct led_controller *controller;
    const struct config *config;
    uint64_t pending_rx_bytes;
    uint64_t pending_tx_bytes;
    bool led_on;
    int timer_fd;
};

int network_led_init(struct network_led *network,
                     struct led_controller *controller,
                     const struct config *config);
void network_led_add_sample(struct network_led *network,
                            uint64_t rx_bytes, uint64_t tx_bytes);
int network_led_render(struct network_led *network);
int network_led_handle_timer(struct network_led *network);
void network_led_cleanup(struct network_led *network);

#endif
