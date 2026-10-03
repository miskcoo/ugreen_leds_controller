// SPDX-License-Identifier: GPL-2.0
#include <linux/bpf.h>
#include <stdbool.h>
#include <bpf/bpf_helpers.h>

#include "maps.bpf.h"

#define DEFAULT_INTERVAL_NS (100ULL * 1000ULL * 1000ULL)

/* Common tracepoint fields occupy the first eight bytes. */
struct block_rq_issue_context {
    __u64 common;
    __u32 dev;
};

SEC("tracepoint/block/block_rq_issue")
int ugreen_disk_issue(struct block_rq_issue_context *ctx)
{
    const __u32 config_key = 0;
    const struct ugreen_bpf_config *config;
    const __u32 *led_id;
    struct disk_state *state;
    struct ugreen_led_event event = {};
    __u32 dev = ctx->dev;
    __u64 interval = DEFAULT_INTERVAL_NS;
    __u64 now;
    __u64 old_deadline;

    led_id = bpf_map_lookup_elem(&disk_targets, &dev);
    if (!led_id)
        return 0;

    state = bpf_map_lookup_elem(&disk_states, &dev);
    if (!state)
        return 0;

    config = bpf_map_lookup_elem(&config_map, &config_key);
    if (config && config->disk_interval_ns)
        interval = config->disk_interval_ns;

    now = bpf_ktime_get_ns();
    old_deadline = state->next_emit_ns;
    if ((!old_deadline || now >= old_deadline) &&
        __sync_val_compare_and_swap(&state->next_emit_ns, old_deadline,
                                    now + interval) == old_deadline) {
        event.timestamp_ns = now;
        event.source = dev;
        event.kind = UGREEN_EVENT_DISK;
        event.led_id = (__u8)*led_id;
        bpf_ringbuf_output(&events, &event, sizeof(event), 0);
    }

    return 0;
}
