// SPDX-License-Identifier: GPL-2.0
#ifndef UGREEN_BPF_SHARED_H
#define UGREEN_BPF_SHARED_H

#include <linux/types.h>

enum ugreen_event_kind {
    UGREEN_EVENT_NETWORK = 1,
    UGREEN_EVENT_DISK = 2,
};

struct ugreen_bpf_config {
    __u64 network_interval_ns;
    __u64 disk_interval_ns;
};

struct ugreen_led_event {
    __u64 timestamp_ns;
    __u64 first_bytes;
    __u64 second_bytes;
    __u32 source;
    __u16 kind;
    __u8 led_id;
    __u8 reserved;
};

#endif
