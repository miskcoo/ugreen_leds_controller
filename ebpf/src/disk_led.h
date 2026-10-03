// SPDX-License-Identifier: GPL-2.0
#ifndef UGREEN_DISK_LED_H
#define UGREEN_DISK_LED_H

#include <stdbool.h>
#include <stdint.h>

#include "bpf_runtime.h"
#include "config.h"
#include "led_controller.h"
#include "types.h"

struct disk_slot {
    uint8_t led_id;
    bool present;
    bool active;
    uint32_t dev;
    char name[64];
    uint64_t deadline_ns;
};

struct disk_leds {
    struct disk_slot slots[UGREEN_DISK_COUNT];
    struct led_controller *controller;
    const struct config *config;
    unsigned slot_count;
    char product_name[128];
    int timer_fd;
    int uevent_fd;
};

int disk_leds_init(struct disk_leds *disks,
                   struct led_controller *controller,
                   const struct config *config);
int disk_leds_sync_bpf(struct disk_leds *disks,
                       struct bpf_runtime *runtime);
int disk_leds_handle_activity(struct disk_leds *disks, uint8_t led_id);
int disk_leds_handle_timer(struct disk_leds *disks);
int disk_leds_handle_uevent(struct disk_leds *disks,
                            struct bpf_runtime *runtime);
void disk_leds_cleanup(struct disk_leds *disks);

#endif
