// SPDX-License-Identifier: GPL-2.0
#include "bpf_runtime.h"

#include <bpf/bpf.h>
#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>

#include "ugreen_bpf_shared.h"

static int set_autoload(struct bpf_object *object, const char *name,
                        bool enabled)
{
    struct bpf_program *program = bpf_object__find_program_by_name(object, name);

    if (!program)
        return -ENOENT;
    return bpf_program__set_autoload(program, enabled);
}

static int attach_tc(struct bpf_runtime *runtime)
{
    struct bpf_program *ingress =
        bpf_object__find_program_by_name(runtime->object,
                                         "ugreen_network_ingress");
    struct bpf_program *egress =
        bpf_object__find_program_by_name(runtime->object,
                                         "ugreen_network_egress");
    struct bpf_tc_hook qdisc = {
        .sz = sizeof(qdisc),
        .ifindex = runtime->ifindex,
        .attach_point = BPF_TC_INGRESS | BPF_TC_EGRESS,
    };
    struct bpf_tc_hook ingress_hook = {
        .sz = sizeof(ingress_hook),
        .ifindex = runtime->ifindex,
        .attach_point = BPF_TC_INGRESS,
    };
    struct bpf_tc_hook egress_hook = {
        .sz = sizeof(egress_hook),
        .ifindex = runtime->ifindex,
        .attach_point = BPF_TC_EGRESS,
    };
    struct bpf_tc_opts ingress_options = {
        .sz = sizeof(ingress_options),
        .handle = 1,
        .priority = UGREEN_TC_PRIORITY,
    };
    struct bpf_tc_opts egress_options = {
        .sz = sizeof(egress_options),
        .handle = 2,
        .priority = UGREEN_TC_PRIORITY,
    };
    int error;

    if (!ingress || !egress)
        return -ENOENT;
    error = bpf_tc_hook_create(&qdisc);
    if (error && error != -EEXIST)
        return error;

    ingress_options.prog_fd = bpf_program__fd(ingress);
    error = bpf_tc_attach(&ingress_hook, &ingress_options);
    if (error)
        return error;
    runtime->ingress_attached = true;

    egress_options.prog_fd = bpf_program__fd(egress);
    error = bpf_tc_attach(&egress_hook, &egress_options);
    if (error)
        return error;
    runtime->egress_attached = true;
    return 0;
}

static void detach_tc(struct bpf_runtime *runtime)
{
    struct bpf_tc_hook hook = {
        .sz = sizeof(hook),
        .ifindex = runtime->ifindex,
    };
    struct bpf_tc_opts options = {
        .sz = sizeof(options),
        .priority = UGREEN_TC_PRIORITY,
    };

    if (runtime->ingress_attached) {
        hook.attach_point = BPF_TC_INGRESS;
        options.handle = 1;
        (void)bpf_tc_detach(&hook, &options);
        runtime->ingress_attached = false;
    }
    if (runtime->egress_attached) {
        hook.attach_point = BPF_TC_EGRESS;
        options.handle = 2;
        (void)bpf_tc_detach(&hook, &options);
        runtime->egress_attached = false;
    }
}

int bpf_runtime_start(struct bpf_runtime *runtime,
                      const struct config *config,
                      ring_buffer_sample_fn callback, void *callback_context)
{
    struct ugreen_bpf_config bpf_config = {
        .network_interval_ns =
            (uint64_t)config->network_interval_ms * 1000000ULL,
        .disk_interval_ns =
            (uint64_t)config->disk_interval_ms * 1000000ULL,
    };
    struct bpf_program *disk_program;
    struct bpf_map *map;
    uint32_t key = 0;
    int error;

    memset(runtime, 0, sizeof(*runtime));
    runtime->object = bpf_object__open_file(config->bpf_object, NULL);
    if (!runtime->object)
        return -(errno ? errno : EIO);

    error = set_autoload(runtime->object, "ugreen_network_ingress",
                         config->network_enabled);
    if (!error)
        error = set_autoload(runtime->object, "ugreen_network_egress",
                             config->network_enabled);
    if (!error)
        error = set_autoload(runtime->object, "ugreen_disk_issue",
                             config->disks_enabled);
    if (error < 0)
        goto fail;

    error = bpf_object__load(runtime->object);
    if (error < 0)
        goto fail;

    map = bpf_object__find_map_by_name(runtime->object, "config_map");
    if (!map) {
        error = -ENOENT;
        goto fail;
    }
    if (bpf_map_update_elem(bpf_map__fd(map), &key, &bpf_config, BPF_ANY) < 0) {
        error = -errno;
        goto fail;
    }

    if (config->network_enabled) {
        runtime->ifindex = (int)if_nametoindex(config->interface);
        if (!runtime->ifindex) {
            error = -ENODEV;
            goto fail;
        }
        error = attach_tc(runtime);
        if (error < 0)
            goto fail;
    }

    if (config->disks_enabled) {
        disk_program = bpf_object__find_program_by_name(runtime->object,
                                                        "ugreen_disk_issue");
        if (!disk_program) {
            error = -ENOENT;
            goto fail;
        }
        runtime->disk_link = bpf_program__attach_tracepoint(
            disk_program, "block", "block_rq_issue");
        error = libbpf_get_error(runtime->disk_link);
        if (error) {
            runtime->disk_link = NULL;
            goto fail;
        }
    }

    map = bpf_object__find_map_by_name(runtime->object, "events");
    if (!map) {
        error = -ENOENT;
        goto fail;
    }
    runtime->ring = ring_buffer__new(bpf_map__fd(map), callback,
                                     callback_context, NULL);
    if (!runtime->ring) {
        error = -(errno ? errno : EIO);
        goto fail;
    }
    return 0;

fail:
    bpf_runtime_stop(runtime);
    return error;
}

void bpf_runtime_stop(struct bpf_runtime *runtime)
{
    detach_tc(runtime);
    if (runtime->disk_link)
        bpf_link__destroy(runtime->disk_link);
    if (runtime->ring)
        ring_buffer__free(runtime->ring);
    if (runtime->object)
        bpf_object__close(runtime->object);
    memset(runtime, 0, sizeof(*runtime));
}

int bpf_runtime_epoll_fd(const struct bpf_runtime *runtime)
{
    return runtime->ring ? ring_buffer__epoll_fd(runtime->ring) : -ENOENT;
}

int bpf_runtime_consume(struct bpf_runtime *runtime)
{
    return runtime->ring ? ring_buffer__consume(runtime->ring) : -ENOENT;
}

static int clear_map(struct bpf_map *map)
{
    uint32_t key;
    int fd;

    if (!map)
        return -ENOENT;
    fd = bpf_map__fd(map);
    while (bpf_map_get_next_key(fd, NULL, &key) == 0) {
        if (bpf_map_delete_elem(fd, &key) < 0 && errno != ENOENT)
            return -errno;
    }
    return 0;
}

int bpf_runtime_clear_disk_targets(struct bpf_runtime *runtime)
{
    int error = clear_map(bpf_object__find_map_by_name(runtime->object,
                                                        "disk_targets"));
    if (error < 0)
        return error;
    return clear_map(bpf_object__find_map_by_name(runtime->object,
                                                   "disk_states"));
}

int bpf_runtime_set_disk_target(struct bpf_runtime *runtime,
                                uint32_t dev, uint8_t led_id)
{
    struct disk_state_value {
        uint64_t next_emit_ns;
    } state = {0};
    struct bpf_map *targets = bpf_object__find_map_by_name(runtime->object,
                                                           "disk_targets");
    struct bpf_map *states = bpf_object__find_map_by_name(runtime->object,
                                                          "disk_states");
    uint32_t value = led_id;

    if (!targets || !states)
        return -ENOENT;
    if (bpf_map_update_elem(bpf_map__fd(states), &dev, &state, BPF_ANY) < 0)
        return -errno;
    if (bpf_map_update_elem(bpf_map__fd(targets), &dev, &value, BPF_ANY) < 0) {
        int error = -errno;
        (void)bpf_map_delete_elem(bpf_map__fd(states), &dev);
        return error;
    }
    return 0;
}
