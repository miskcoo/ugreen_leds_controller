// SPDX-License-Identifier: GPL-2.0
#include "scheduler.h"

#include <stdint.h>

static uint64_t saturating_add_u64(uint64_t a, uint64_t b)
{
    return UINT64_MAX - a < b ? UINT64_MAX : a + b;
}

bool scheduler_choose_rx(struct pulse_scheduler *scheduler,
                         uint64_t rx_bytes, uint64_t tx_bytes)
{
    long double total_bytes;
    bool rx;

    if (!tx_bytes) {
        rx = true;
    } else if (!rx_bytes) {
        rx = false;
    } else {
        total_bytes = (long double)rx_bytes + (long double)tx_bytes;
        scheduler->rx_credit += (long double)rx_bytes / total_bytes;
        scheduler->tx_credit += (long double)tx_bytes / total_bytes;

        rx = scheduler->rx_credit >= scheduler->tx_credit;
        if (rx)
            scheduler->rx_credit -= 1.0L;
        else
            scheduler->tx_credit -= 1.0L;
    }

    if (rx)
        scheduler->rx_pulses = saturating_add_u64(scheduler->rx_pulses, 1);
    else
        scheduler->tx_pulses = saturating_add_u64(scheduler->tx_pulses, 1);

    return rx;
}
