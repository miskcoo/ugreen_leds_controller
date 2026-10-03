// SPDX-License-Identifier: GPL-2.0
#ifndef UGREEN_TYPES_H
#define UGREEN_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define UGREEN_LED_COUNT 10
#define UGREEN_DISK_COUNT 8
#define UGREEN_LED_POWER 0
#define UGREEN_LED_NETWORK 1
#define UGREEN_LED_DISK_FIRST 2

struct rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

static inline bool rgb_equal(struct rgb a, struct rgb b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

#endif
