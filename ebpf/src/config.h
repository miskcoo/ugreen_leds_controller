// SPDX-License-Identifier: GPL-2.0
#ifndef UGREEN_CONFIG_H
#define UGREEN_CONFIG_H

#include <stdbool.h>
#include <net/if.h>

#include "types.h"

#define UGREEN_DEFAULT_BPF_OBJECT "./ugreen_led.bpf.o"
#define UGREEN_DEFAULT_CONFIG_PATH "/etc/ugreen-leds.conf"
#define UGREEN_MAX_SERIAL_LENGTH 128

enum power_mode {
    POWER_MODE_ON,
    POWER_MODE_BLINK,
    POWER_MODE_BREATH,
};

enum disk_mapping_method {
    DISK_MAPPING_ATA,
    DISK_MAPPING_HCTL,
    DISK_MAPPING_SERIAL,
};

struct config {
    char interface[IFNAMSIZ];
    const char *bpf_object;
    const char *config_path;
    const char *i2c_device;
    bool verbose;
    bool network_enabled;
    bool disks_enabled;
    bool power_enabled;

    unsigned network_interval_ms;
    unsigned network_pulse_ms;
    unsigned network_brightness;
    struct rgb network_rx_color;
    struct rgb network_tx_color;

    unsigned disk_interval_ms;
    unsigned disk_hold_ms;
    unsigned disk_pulse_ms;
    unsigned disk_brightness;
    struct rgb disk_color;
    struct rgb disk_colors[UGREEN_DISK_COUNT];
    bool disk_color_set[UGREEN_DISK_COUNT];
    bool disk_invert;
    enum disk_mapping_method disk_mapping;
    char disk_serials[UGREEN_DISK_COUNT][UGREEN_MAX_SERIAL_LENGTH];

    unsigned power_brightness;
    struct rgb power_color;
    enum power_mode power_mode;
    unsigned power_on_ms;
    unsigned power_off_ms;
};

void config_set_defaults(struct config *config);
int config_load_file(const char *path, struct config *config);
int config_parse_options(int argc, char **argv, struct config *config);
void config_usage(const char *argv0);

static inline struct rgb config_disk_color(const struct config *config,
                                           unsigned slot)
{
    return slot < UGREEN_DISK_COUNT && config->disk_color_set[slot]
        ? config->disk_colors[slot] : config->disk_color;
}

#endif
