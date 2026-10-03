// SPDX-License-Identifier: GPL-2.0
#ifndef UGREEN_BPF_RUNTIME_H
#define UGREEN_BPF_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include <bpf/libbpf.h>

#include "config.h"

#define UGREEN_TC_PRIORITY 49152U

struct bpf_runtime {
    struct bpf_object *object;
    struct bpf_link *disk_link;
    struct ring_buffer *ring;
    int ifindex;
    bool ingress_attached;
    bool egress_attached;
};

int bpf_runtime_start(struct bpf_runtime *runtime,
                      const struct config *config,
                      ring_buffer_sample_fn callback, void *callback_context);
void bpf_runtime_stop(struct bpf_runtime *runtime);
int bpf_runtime_epoll_fd(const struct bpf_runtime *runtime);
int bpf_runtime_consume(struct bpf_runtime *runtime);
int bpf_runtime_clear_disk_targets(struct bpf_runtime *runtime);
int bpf_runtime_set_disk_target(struct bpf_runtime *runtime,
                                uint32_t dev, uint8_t led_id);

#endif
