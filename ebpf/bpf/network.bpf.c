// SPDX-License-Identifier: GPL-2.0
#include <linux/bpf.h>
#include <linux/pkt_cls.h>
#include <stdbool.h>
#include <bpf/bpf_helpers.h>

#include "maps.bpf.h"

#define DEFAULT_INTERVAL_NS (100ULL * 1000ULL * 1000ULL)

static __always_inline int account_packet(struct __sk_buff *skb, bool is_rx)
{
    const __u32 key = 0;
    const struct ugreen_bpf_config *config;
    struct network_state *state;
    struct ugreen_led_event event = {};
    __u64 interval = DEFAULT_INTERVAL_NS;
    __u64 now;
    bool emit = false;

    state = bpf_map_lookup_elem(&network_state_map, &key);
    if (!state)
        return TC_ACT_OK;

    config = bpf_map_lookup_elem(&config_map, &key);
    if (config && config->network_interval_ns)
        interval = config->network_interval_ns;

    now = bpf_ktime_get_ns();
    bpf_spin_lock(&state->lock);

    if (is_rx)
        state->rx_bytes += skb->len;
    else
        state->tx_bytes += skb->len;

    if (!state->next_emit_ns || now >= state->next_emit_ns) {
        event.timestamp_ns = now;
        event.first_bytes = state->rx_bytes;
        event.second_bytes = state->tx_bytes;
        event.source = skb->ifindex;
        event.kind = UGREEN_EVENT_NETWORK;

        state->rx_bytes = 0;
        state->tx_bytes = 0;
        state->next_emit_ns = now + interval;
        emit = true;
    }

    bpf_spin_unlock(&state->lock);
    if (emit)
        bpf_ringbuf_output(&events, &event, sizeof(event), 0);

    return TC_ACT_OK;
}

SEC("tc")
int ugreen_network_ingress(struct __sk_buff *skb)
{
    return account_packet(skb, true);
}

SEC("tc")
int ugreen_network_egress(struct __sk_buff *skb)
{
    return account_packet(skb, false);
}
