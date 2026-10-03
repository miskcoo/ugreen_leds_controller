// SPDX-License-Identifier: GPL-2.0
#ifndef UGREEN_SCHEDULER_H
#define UGREEN_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

struct pulse_scheduler {
    long double rx_credit;
    long double tx_credit;
    uint64_t rx_pulses;
    uint64_t tx_pulses;
};

bool scheduler_choose_rx(struct pulse_scheduler *scheduler,
                         uint64_t rx_bytes, uint64_t tx_bytes);

#endif
