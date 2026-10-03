// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include "disk_led.h"
#include "disk_layout.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#define SYS_BLOCK "/sys/class/block"
#define DMI_PRODUCT_NAME "/sys/class/dmi/id/product_name"
#define KERNEL_MINOR_BITS 20U

static uint64_t monotonic_ns(void)
{
    struct timespec time;

    if (clock_gettime(CLOCK_MONOTONIC, &time) < 0)
        return 0;
    return (uint64_t)time.tv_sec * 1000000000ULL + (uint64_t)time.tv_nsec;
}

static int read_line(const char *path, char *buffer, size_t length)
{
    FILE *file = fopen(path, "re");

    if (!file)
        return -errno;
    if (!fgets(buffer, (int)length, file)) {
        int error = ferror(file) ? -(errno ? errno : EIO) : -EIO;
        fclose(file);
        return error;
    }
    fclose(file);
    buffer[strcspn(buffer, "\r\n")] = '\0';
    return 0;
}

static int parse_dev(const char *name, uint32_t *dev, unsigned *major_out,
                     unsigned *minor_out)
{
    char path[PATH_MAX];
    char value[64];
    unsigned major;
    unsigned minor;
    char tail;

    if (snprintf(path, sizeof(path), SYS_BLOCK "/%s/dev", name) >=
        (int)sizeof(path))
        return -ENAMETOOLONG;
    if (read_line(path, value, sizeof(value)) < 0)
        return -ENOENT;
    if (sscanf(value, "%u:%u%c", &major, &minor, &tail) != 2 ||
        major >= (1U << (32U - KERNEL_MINOR_BITS)) ||
        minor >= (1U << KERNEL_MINOR_BITS))
        return -EINVAL;

    *dev = (major << KERNEL_MINOR_BITS) | minor;
    *major_out = major;
    *minor_out = minor;
    return 0;
}

static int read_udev_serial(unsigned major, unsigned minor,
                            char *serial, size_t length)
{
    char path[PATH_MAX];
    char *line = NULL;
    size_t capacity = 0;
    FILE *file;
    int result = -ENOENT;

    if (snprintf(path, sizeof(path), "/run/udev/data/b%u:%u",
                 major, minor) >= (int)sizeof(path))
        return -ENAMETOOLONG;
    file = fopen(path, "re");
    if (!file)
        return -errno;

    while (getline(&line, &capacity, file) >= 0) {
        const char prefix[] = "E:ID_SERIAL_SHORT=";
        if (!strncmp(line, prefix, sizeof(prefix) - 1)) {
            char *value = line + sizeof(prefix) - 1;
            value[strcspn(value, "\r\n")] = '\0';
            if (strlen(value) >= length)
                result = -ENAMETOOLONG;
            else {
                snprintf(serial, length, "%s", value);
                result = 0;
            }
            break;
        }
    }
    free(line);
    fclose(file);
    return result;
}

static int serial_slot(const struct disk_leds *disks,
                       unsigned major, unsigned minor)
{
    char serial[UGREEN_MAX_SERIAL_LENGTH];
    unsigned slot;

    if (read_udev_serial(major, minor, serial, sizeof(serial)) < 0)
        return -1;
    for (slot = 0; slot < disks->slot_count; ++slot) {
        if (disks->config->disk_serials[slot][0] &&
            !strcmp(serial, disks->config->disk_serials[slot]))
            return (int)slot;
    }
    return -1;
}

static int device_slot(const struct disk_leds *disks, const char *name,
                       unsigned major, unsigned minor)
{
    char device_link[PATH_MAX];
    char block_link[PATH_MAX];
    char resolved[PATH_MAX];

    if (disks->config->disk_mapping == DISK_MAPPING_SERIAL)
        return serial_slot(disks, major, minor);

    if (snprintf(device_link, sizeof(device_link), SYS_BLOCK "/%s/device",
                 name) >= (int)sizeof(device_link) ||
        !realpath(device_link, resolved))
        return -1;
    if (disks->config->disk_mapping == DISK_MAPPING_HCTL)
        return disk_layout_hctl_slot(disks->product_name, disks->slot_count,
                                     resolved);

    if (snprintf(block_link, sizeof(block_link), SYS_BLOCK "/%s", name) >=
        (int)sizeof(block_link) || !realpath(block_link, resolved))
        return -1;
    return disk_layout_ata_slot(disks->product_name, disks->slot_count,
                                resolved);
}

static int discover(struct disk_leds *disks, struct disk_slot *found)
{
    struct dirent *entry;
    DIR *directory = opendir(SYS_BLOCK);

    if (!directory)
        return -errno;
    while ((entry = readdir(directory)) != NULL) {
        char partition_path[PATH_MAX];
        uint32_t dev;
        unsigned major;
        unsigned minor;
        int slot;

        if (entry->d_name[0] == '.')
            continue;
        if (snprintf(partition_path, sizeof(partition_path),
                     SYS_BLOCK "/%s/partition", entry->d_name) >=
            (int)sizeof(partition_path) || access(partition_path, F_OK) == 0)
            continue;
        if (parse_dev(entry->d_name, &dev, &major, &minor) < 0)
            continue;
        slot = device_slot(disks, entry->d_name, major, minor);
        if (slot < 0 || (unsigned)slot >= disks->slot_count)
            continue;
        if (found[slot].present) {
            fprintf(stderr, "multiple block devices map to disk%d; ignoring %s\n",
                    slot + 1, entry->d_name);
            continue;
        }

        found[slot].led_id = UGREEN_LED_DISK_FIRST + (uint8_t)slot;
        found[slot].present = true;
        found[slot].dev = dev;
        if (strlen(entry->d_name) >= sizeof(found[slot].name))
            continue;
        memcpy(found[slot].name, entry->d_name, strlen(entry->d_name) + 1);
    }
    closedir(directory);
    return 0;
}

static int set_steady(struct disk_leds *disks, struct disk_slot *slot)
{
    unsigned index = slot->led_id - UGREEN_LED_DISK_FIRST;
    int error;

    error = led_controller_set_color(disks->controller, slot->led_id,
                                     config_disk_color(disks->config, index));
    if (!error)
        error = led_controller_set_brightness(
            disks->controller, slot->led_id,
            (uint8_t)disks->config->disk_brightness);
    if (!error)
        error = led_controller_set_on(disks->controller, slot->led_id,
                                      disks->config->disk_invert);
    return error;
}

static int arm_timer(struct disk_leds *disks)
{
    struct itimerspec timer = {0};
    uint64_t earliest = UINT64_MAX;
    unsigned slot;

    for (slot = 0; slot < disks->slot_count; ++slot) {
        if (disks->slots[slot].active &&
            disks->slots[slot].deadline_ns < earliest)
            earliest = disks->slots[slot].deadline_ns;
    }
    if (earliest != UINT64_MAX) {
        timer.it_value.tv_sec = earliest / 1000000000ULL;
        timer.it_value.tv_nsec = (long)(earliest % 1000000000ULL);
    }
    return timerfd_settime(disks->timer_fd, TFD_TIMER_ABSTIME,
                           &timer, NULL) < 0 ? -errno : 0;
}

static int open_uevent_socket(void)
{
    struct sockaddr_nl address = {
        .nl_family = AF_NETLINK,
        .nl_pid = (uint32_t)getpid(),
        .nl_groups = 1,
    };
    int fd = socket(AF_NETLINK,
                    SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
                    NETLINK_KOBJECT_UEVENT);

    if (fd < 0)
        return -errno;
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        int error = -errno;
        close(fd);
        return error;
    }
    return fd;
}

static int rescan(struct disk_leds *disks)
{
    struct disk_slot found[UGREEN_DISK_COUNT] = {0};
    int first_error = 0;
    unsigned slot;
    int error = discover(disks, found);

    if (error < 0)
        return error;
    for (slot = 0; slot < disks->slot_count; ++slot) {
        struct disk_slot *old = &disks->slots[slot];
        struct disk_slot *new = &found[slot];

        new->led_id = UGREEN_LED_DISK_FIRST + (uint8_t)slot;
        if (old->present == new->present && old->dev == new->dev)
            continue;

        if (disks->config->verbose) {
            if (new->present)
                fprintf(stderr, "disk%d: %s dev=0x%x appeared\n",
                        slot + 1, new->name, new->dev);
            else
                fprintf(stderr, "disk%d: device removed\n", slot + 1);
        }
        led_controller_invalidate(disks->controller, new->led_id);
        if (new->present)
            error = set_steady(disks, new);
        else
            error = led_controller_set_on(disks->controller,
                                          new->led_id, false);
        if (error < 0 && !first_error)
            first_error = error;
        *old = *new;
    }
    return first_error;
}

int disk_leds_init(struct disk_leds *disks,
                   struct led_controller *controller,
                   const struct config *config)
{
    unsigned slot;
    int error;

    memset(disks, 0, sizeof(*disks));
    disks->controller = controller;
    disks->config = config;
    disks->timer_fd = -1;
    disks->uevent_fd = -1;
    if (read_line(DMI_PRODUCT_NAME, disks->product_name,
                  sizeof(disks->product_name)) < 0)
        snprintf(disks->product_name, sizeof(disks->product_name), "unknown");
    disks->slot_count = disk_layout_slot_count(disks->product_name);
    for (slot = 0; slot < UGREEN_DISK_COUNT; ++slot) {
        disks->slots[slot].led_id = UGREEN_LED_DISK_FIRST + (uint8_t)slot;
        disks->slots[slot].dev = UINT32_MAX;
    }

    disks->timer_fd = timerfd_create(CLOCK_MONOTONIC,
                                     TFD_CLOEXEC | TFD_NONBLOCK);
    if (disks->timer_fd < 0)
        return -errno;
    disks->uevent_fd = open_uevent_socket();
    if (disks->uevent_fd < 0) {
        error = disks->uevent_fd;
        close(disks->timer_fd);
        disks->timer_fd = -1;
        return error;
    }

    if (config->verbose)
        fprintf(stderr,
                "disk discovery: model='%s' slots=%u mapping=%s "
                "invert=%s interval=%ums pulse=%ums hold=%ums\n",
                disks->product_name, disks->slot_count,
                config->disk_mapping == DISK_MAPPING_ATA ? "ata" :
                config->disk_mapping == DISK_MAPPING_HCTL ? "hctl" : "serial",
                config->disk_invert ? "on" : "off",
                config->disk_interval_ms, config->disk_pulse_ms,
                config->disk_hold_ms);
    return rescan(disks);
}

int disk_leds_sync_bpf(struct disk_leds *disks,
                       struct bpf_runtime *runtime)
{
    unsigned slot;
    int error = bpf_runtime_clear_disk_targets(runtime);

    if (error < 0)
        return error;
    for (slot = 0; slot < disks->slot_count; ++slot) {
        if (!disks->slots[slot].present)
            continue;
        error = bpf_runtime_set_disk_target(runtime, disks->slots[slot].dev,
                                            disks->slots[slot].led_id);
        if (error < 0)
            return error;
    }
    return 0;
}

int disk_leds_handle_activity(struct disk_leds *disks, uint8_t led_id)
{
    struct disk_slot *slot;
    unsigned index;
    unsigned on_ms;
    unsigned off_ms;
    int error = 0;

    if (led_id < UGREEN_LED_DISK_FIRST)
        return -EINVAL;
    index = led_id - UGREEN_LED_DISK_FIRST;
    if (index >= disks->slot_count || !disks->slots[index].present)
        return 0;
    slot = &disks->slots[index];
    slot->deadline_ns = monotonic_ns() +
        (uint64_t)disks->config->disk_hold_ms * 1000000ULL;

    if (!slot->active) {
        if (disks->config->disk_invert) {
            off_ms = disks->config->disk_pulse_ms;
            on_ms = disks->config->disk_interval_ms - off_ms;
        } else {
            on_ms = disks->config->disk_pulse_ms;
            off_ms = disks->config->disk_interval_ms - on_ms;
        }
        error = led_controller_set_blink(disks->controller, led_id,
                                         (uint16_t)on_ms,
                                         (uint16_t)off_ms, false);
        if (error < 0) {
            fprintf(stderr, "failed to start disk%d activity blink: %s\n",
                    index + 1, strerror(-error));
            led_controller_invalidate(disks->controller, led_id);
        } else {
            slot->active = true;
            if (disks->config->verbose)
                fprintf(stderr, "disk%d activity\n", index + 1);
        }
    }
    if (slot->active)
        return arm_timer(disks);
    return 0;
}

int disk_leds_handle_timer(struct disk_leds *disks)
{
    uint64_t expirations;
    uint64_t now = monotonic_ns();
    unsigned slot;
    ssize_t size = read(disks->timer_fd, &expirations, sizeof(expirations));

    if (size < 0 && errno != EAGAIN)
        return -errno;
    for (slot = 0; slot < disks->slot_count; ++slot) {
        struct disk_slot *disk = &disks->slots[slot];
        int error;

        if (!disk->active || disk->deadline_ns > now)
            continue;
        error = set_steady(disks, disk);
        if (error < 0) {
            fprintf(stderr, "failed to restore disk%d LED: %s; continuing\n",
                    slot + 1, strerror(-error));
            led_controller_invalidate(disks->controller, disk->led_id);
        }
        disk->active = false;
        disk->deadline_ns = 0;
    }
    return arm_timer(disks);
}

static bool is_block_uevent(char *buffer, ssize_t length)
{
    bool block = false;
    bool relevant_action = false;
    char *field = buffer;
    char *end = buffer + length;

    while (field < end && *field) {
        if (!strcmp(field, "SUBSYSTEM=block"))
            block = true;
        if (!strcmp(field, "ACTION=add") || !strcmp(field, "ACTION=remove") ||
            !strcmp(field, "ACTION=change"))
            relevant_action = true;
        field += strlen(field) + 1;
    }
    return block && relevant_action;
}

int disk_leds_handle_uevent(struct disk_leds *disks,
                            struct bpf_runtime *runtime)
{
    char buffer[8192];
    bool rescan_needed = false;

    for (;;) {
        ssize_t length = recv(disks->uevent_fd, buffer, sizeof(buffer) - 1,
                              MSG_DONTWAIT);
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return -errno;
        }
        buffer[length] = '\0';
        if (is_block_uevent(buffer, length))
            rescan_needed = true;
    }
    if (!rescan_needed)
        return 0;
    if (rescan(disks) < 0)
        fprintf(stderr, "disk rescan encountered an LED update failure\n");
    return disk_leds_sync_bpf(disks, runtime);
}

void disk_leds_cleanup(struct disk_leds *disks)
{
    unsigned slot;

    for (slot = 0; slot < disks->slot_count; ++slot) {
        if (disks->slots[slot].present)
            (void)set_steady(disks, &disks->slots[slot]);
    }
    if (disks->timer_fd >= 0)
        close(disks->timer_fd);
    if (disks->uevent_fd >= 0)
        close(disks->uevent_fd);
    disks->timer_fd = -1;
    disks->uevent_fd = -1;
}
