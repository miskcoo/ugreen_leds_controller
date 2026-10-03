// SPDX-License-Identifier: GPL-2.0
#ifndef UGREEN_MAPS_BPF_H
#define UGREEN_MAPS_BPF_H

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#include "ugreen_bpf_shared.h"

struct network_state {
    struct bpf_spin_lock lock;
    __u32 pad;
    __u64 rx_bytes;
    __u64 tx_bytes;
    __u64 next_emit_ns;
};

struct disk_state {
    __u64 next_emit_ns;
};

struct config_map_definition {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct ugreen_bpf_config);
};

struct network_state_map_definition {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct network_state);
};

struct disk_targets_map_definition {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8);
    __type(key, __u32);
    __type(value, __u32);
};

struct disk_states_map_definition {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8);
    __type(key, __u32);
    __type(value, struct disk_state);
};

struct events_map_definition {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
};

extern struct config_map_definition config_map;
extern struct network_state_map_definition network_state_map;
extern struct disk_targets_map_definition disk_targets;
extern struct disk_states_map_definition disk_states;
extern struct events_map_definition events;

#endif
