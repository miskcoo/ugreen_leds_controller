// SPDX-License-Identifier: GPL-2.0
#include "maps.bpf.h"

struct config_map_definition config_map SEC(".maps");
struct network_state_map_definition network_state_map SEC(".maps");
struct disk_targets_map_definition disk_targets SEC(".maps");
struct disk_states_map_definition disk_states SEC(".maps");
struct events_map_definition events SEC(".maps");

char LICENSE[] SEC("license") = "GPL";
