// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_unsigned(const char *text, unsigned minimum,
                          unsigned maximum, unsigned *output)
{
    char *end = NULL;
    unsigned long value;

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno || end == text || *end || value < minimum || value > maximum)
        return -EINVAL;
    *output = (unsigned)value;
    return 0;
}

static int parse_boolean(const char *text, bool *output)
{
    if (!strcasecmp(text, "true") || !strcmp(text, "1") ||
        !strcasecmp(text, "yes")) {
        *output = true;
        return 0;
    }
    if (!strcasecmp(text, "false") || !strcmp(text, "0") ||
        !strcasecmp(text, "no")) {
        *output = false;
        return 0;
    }
    return -EINVAL;
}

static int parse_rgb(const char *text, struct rgb *output)
{
    unsigned r, g, b;
    char tail;

    if (sscanf(text, "%u,%u,%u%c", &r, &g, &b, &tail) != 3 &&
        sscanf(text, "%u %u %u%c", &r, &g, &b, &tail) != 3)
        return -EINVAL;
    if (r > 255 || g > 255 || b > 255)
        return -ERANGE;
    output->r = (uint8_t)r;
    output->g = (uint8_t)g;
    output->b = (uint8_t)b;
    return 0;
}

static char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text))
        ++text;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        --end;
    *end = '\0';
    return text;
}

static char *config_value(char *text)
{
    char quote = '\0';
    char *cursor;
    size_t length;

    for (cursor = text; *cursor; ++cursor) {
        if ((*cursor == '\'' || *cursor == '"') &&
            (!quote || quote == *cursor))
            quote = quote ? '\0' : *cursor;
        else if (*cursor == '#' && !quote) {
            *cursor = '\0';
            break;
        }
    }

    text = trim(text);
    length = strlen(text);
    if (length >= 2 && ((text[0] == '"' && text[length - 1] == '"') ||
                        (text[0] == '\'' && text[length - 1] == '\''))) {
        text[length - 1] = '\0';
        ++text;
    }
    return text;
}

static int parse_mapping(const char *text, enum disk_mapping_method *output)
{
    if (!strcmp(text, "ata"))
        *output = DISK_MAPPING_ATA;
    else if (!strcmp(text, "hctl"))
        *output = DISK_MAPPING_HCTL;
    else if (!strcmp(text, "serial"))
        *output = DISK_MAPPING_SERIAL;
    else
        return -EINVAL;
    return 0;
}

static int copy_interface(const char *text, struct config *config)
{
    size_t length = strlen(text);

    if (!length || length >= sizeof(config->interface))
        return -EINVAL;
    memcpy(config->interface, text, length + 1);
    config->network_enabled = true;
    return 0;
}

static int parse_serials(const char *text, struct config *config)
{
    char *copy = strdup(text);
    char *save = NULL;
    char *token;
    unsigned slot = 0;

    if (!copy)
        return -ENOMEM;
    memset(config->disk_serials, 0, sizeof(config->disk_serials));
    for (token = strtok_r(copy, " \t,", &save); token;
         token = strtok_r(NULL, " \t,", &save)) {
        if (slot >= UGREEN_DISK_COUNT ||
            strlen(token) >= sizeof(config->disk_serials[slot])) {
            free(copy);
            return -E2BIG;
        }
        snprintf(config->disk_serials[slot],
                 sizeof(config->disk_serials[slot]), "%s", token);
        ++slot;
    }
    free(copy);
    return 0;
}

static int parse_power_mode(const char *text, struct config *config)
{
    char mode[16];
    unsigned on_ms = 0;
    unsigned off_ms = 0;
    char tail;
    int fields = sscanf(text, "%15s %u %u %c", mode, &on_ms, &off_ms, &tail);

    if (fields == 1 && !strcmp(mode, "none")) {
        config->power_mode = POWER_MODE_ON;
        return 0;
    }
    if (fields != 3 || on_ms > UINT16_MAX || off_ms > UINT16_MAX ||
        on_ms + off_ms > UINT16_MAX)
        return -EINVAL;
    if (!strcmp(mode, "blink"))
        config->power_mode = POWER_MODE_BLINK;
    else if (!strcmp(mode, "breath"))
        config->power_mode = POWER_MODE_BREATH;
    else
        return -EINVAL;
    config->power_on_ms = on_ms;
    config->power_off_ms = off_ms;
    return 0;
}

static int per_disk_color_slot(const char *key)
{
    unsigned disk;
    char tail;

    if (sscanf(key, "COLOR_DISK_HEALTH_PER_DISK[disk%u]%c",
               &disk, &tail) != 1 || disk < 1 || disk > UGREEN_DISK_COUNT)
        return -1;
    return (int)disk - 1;
}

void config_set_defaults(struct config *config)
{
    memset(config, 0, sizeof(*config));
    config->bpf_object = UGREEN_DEFAULT_BPF_OBJECT;
    config->config_path = UGREEN_DEFAULT_CONFIG_PATH;
    config->disks_enabled = true;
    config->power_enabled = true;

    config->network_interval_ms = 100;
    config->network_pulse_ms = 45;
    config->network_brightness = 255;
    config->network_rx_color = (struct rgb){0, 96, 255};
    config->network_tx_color = (struct rgb){255, 0, 0};

    config->disk_interval_ms = 100;
    config->disk_hold_ms = 200;
    config->disk_pulse_ms = 45;
    config->disk_brightness = 255;
    config->disk_color = (struct rgb){255, 255, 255};
    config->disk_invert = false;
    config->disk_mapping = DISK_MAPPING_ATA;

    config->power_brightness = 255;
    config->power_color = (struct rgb){255, 255, 255};
    config->power_mode = POWER_MODE_ON;
}

int config_load_file(const char *path, struct config *config)
{
    char *line = NULL;
    size_t capacity = 0;
    unsigned line_number = 0;
    FILE *file = fopen(path, "re");
    int result = 0;

    if (!file)
        return -errno;

    while (getline(&line, &capacity, file) >= 0) {
        char *key;
        char *value;
        char *equals;
        int color_slot;
        int error = 0;

        ++line_number;
        key = trim(line);
        if (!*key || *key == '#' || !strncmp(key, "declare ", 8))
            continue;
        equals = strchr(key, '=');
        if (!equals)
            continue;
        *equals = '\0';
        value = config_value(equals + 1);
        key = trim(key);

        if (!strcmp(key, "NETDEV_INTERFACE")) {
            if (!*value) {
                config->interface[0] = '\0';
                config->network_enabled = false;
            } else {
                error = copy_interface(value, config);
            }
        }
        else if (!strcmp(key, "NETDEV_EBPF_INTERVAL_MS"))
            error = parse_unsigned(value, 1, 1000,
                                   &config->network_interval_ms);
        else if (!strcmp(key, "NETDEV_EBPF_PULSE_MS"))
            error = parse_unsigned(value, 0, 1000,
                                   &config->network_pulse_ms);
        else if (!strcmp(key, "COLOR_NETDEV_RX"))
            error = parse_rgb(value, &config->network_rx_color);
        else if (!strcmp(key, "COLOR_NETDEV_TX"))
            error = parse_rgb(value, &config->network_tx_color);
        else if (!strcmp(key, "BRIGHTNESS_NETDEV_LED"))
            error = parse_unsigned(value, 0, 255,
                                   &config->network_brightness);
        else if (!strcmp(key, "DISK_EBPF_INTERVAL_MS"))
            error = parse_unsigned(value, 1, 1000,
                                   &config->disk_interval_ms);
        else if (!strcmp(key, "DISK_EBPF_HOLD_MS"))
            error = parse_unsigned(value, 1, 10000,
                                   &config->disk_hold_ms);
        else if (!strcmp(key, "DISK_EBPF_PULSE_MS"))
            error = parse_unsigned(value, 1, 1000,
                                   &config->disk_pulse_ms);
        else if (!strcmp(key, "BRIGHTNESS_DISK_LEDS"))
            error = parse_unsigned(value, 0, 255,
                                   &config->disk_brightness);
        else if (!strcmp(key, "COLOR_DISK_HEALTH"))
            error = parse_rgb(value, &config->disk_color);
        else if ((color_slot = per_disk_color_slot(key)) >= 0) {
            error = parse_rgb(value, &config->disk_colors[color_slot]);
            if (!error)
                config->disk_color_set[color_slot] = true;
        }
        else if (!strcmp(key, "LED_INVERT"))
            error = parse_boolean(value, &config->disk_invert);
        else if (!strcmp(key, "MAPPING_METHOD"))
            error = parse_mapping(value, &config->disk_mapping);
        else if (!strcmp(key, "DISK_SERIAL"))
            error = parse_serials(value, config);
        else if (!strcmp(key, "BRIGHTNESS_POWER"))
            error = parse_unsigned(value, 0, 255,
                                   &config->power_brightness);
        else if (!strcmp(key, "COLOR_POWER"))
            error = parse_rgb(value, &config->power_color);
        else if (!strcmp(key, "BLINK_TYPE_POWER"))
            error = parse_power_mode(value, config);
        else
            continue;

        if (error < 0) {
            fprintf(stderr, "%s:%u: invalid value for %s\n",
                    path, line_number, key);
            result = error;
            break;
        }
    }

    if (ferror(file) && !result)
        result = -(errno ? errno : EIO);
    free(line);
    fclose(file);
    return result;
}

static int find_config_path(int argc, char **argv, const char **path,
                            bool *explicit_path)
{
    int i;

    *path = UGREEN_DEFAULT_CONFIG_PATH;
    *explicit_path = false;
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--config")) {
            if (++i >= argc)
                return -EINVAL;
            *path = argv[i];
            *explicit_path = true;
        } else if (!strncmp(argv[i], "--config=", 9)) {
            *path = argv[i] + 9;
            *explicit_path = true;
        }
    }
    return 0;
}

void config_usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [options]\n\n"
        "Unified event-driven UGREEN power, network, and disk LED daemon.\n\n"
        "  -i, --interface IFACE       enable network monitoring on IFACE\n"
        "  -o, --bpf-object PATH       BPF object (default: %s)\n"
        "      --config PATH           configuration file (default: %s)\n"
        "      --i2c-device PATH       override automatic I2C detection\n"
        "      --interval-ms N         network event interval\n"
        "      --pulse-ms N            network pulse width\n"
        "      --rx-color R,G,B        network RX color\n"
        "      --tx-color R,G,B        network TX color\n"
        "      --brightness N          network brightness\n"
        "      --disk-interval-ms N    disk event interval\n"
        "      --disk-hold-ms N        disk idle deadline before steady state\n"
        "      --disk-pulse-ms N       disk blink on/off pulse width\n"
        "      --no-network            disable network monitoring\n"
        "      --no-disks              disable disk monitoring\n"
        "      --no-power              do not configure the power LED\n"
        "  -v, --verbose               print discovery and activity events\n"
        "  -h, --help                  show this help\n",
        argv0, UGREEN_DEFAULT_BPF_OBJECT, UGREEN_DEFAULT_CONFIG_PATH);
}

int config_parse_options(int argc, char **argv, struct config *config)
{
    enum {
        OPT_INTERVAL = 1000,
        OPT_PULSE,
        OPT_RX_COLOR,
        OPT_TX_COLOR,
        OPT_BRIGHTNESS,
        OPT_I2C_DEVICE,
        OPT_CONFIG,
        OPT_DISK_INTERVAL,
        OPT_DISK_HOLD,
        OPT_DISK_PULSE,
        OPT_NO_NETWORK,
        OPT_NO_DISKS,
        OPT_NO_POWER,
    };
    static const struct option options[] = {
        {"interface", required_argument, NULL, 'i'},
        {"bpf-object", required_argument, NULL, 'o'},
        {"interval-ms", required_argument, NULL, OPT_INTERVAL},
        {"pulse-ms", required_argument, NULL, OPT_PULSE},
        {"rx-color", required_argument, NULL, OPT_RX_COLOR},
        {"tx-color", required_argument, NULL, OPT_TX_COLOR},
        {"brightness", required_argument, NULL, OPT_BRIGHTNESS},
        {"i2c-device", required_argument, NULL, OPT_I2C_DEVICE},
        {"config", required_argument, NULL, OPT_CONFIG},
        {"disk-interval-ms", required_argument, NULL, OPT_DISK_INTERVAL},
        {"disk-hold-ms", required_argument, NULL, OPT_DISK_HOLD},
        {"disk-pulse-ms", required_argument, NULL, OPT_DISK_PULSE},
        {"no-network", no_argument, NULL, OPT_NO_NETWORK},
        {"no-disks", no_argument, NULL, OPT_NO_DISKS},
        {"no-power", no_argument, NULL, OPT_NO_POWER},
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };
    const char *config_path;
    bool explicit_config;
    int error;

    config_set_defaults(config);
    error = find_config_path(argc, argv, &config_path, &explicit_config);
    if (error < 0)
        return error;
    config->config_path = config_path;
    error = config_load_file(config_path, config);
    if (error < 0 && (error != -ENOENT || explicit_config)) {
        if (error == -ENOENT)
            fprintf(stderr, "configuration file '%s' does not exist\n",
                    config_path);
        else if (error != -EINVAL)
            fprintf(stderr, "could not read configuration file '%s': %s\n",
                    config_path, strerror(-error));
        return error;
    }

    optind = 1;
    for (;;) {
        int option = getopt_long(argc, argv, "i:o:vh", options, NULL);
        if (option == -1)
            break;
        error = 0;
        switch (option) {
        case 'i':
            error = copy_interface(optarg, config);
            break;
        case 'o': config->bpf_object = optarg; break;
        case 'v': config->verbose = true; break;
        case 'h': config_usage(argv[0]); exit(EXIT_SUCCESS);
        case OPT_CONFIG: config->config_path = optarg; break;
        case OPT_I2C_DEVICE: config->i2c_device = optarg; break;
        case OPT_NO_NETWORK: config->network_enabled = false; break;
        case OPT_NO_DISKS: config->disks_enabled = false; break;
        case OPT_NO_POWER: config->power_enabled = false; break;
        case OPT_INTERVAL:
            error = parse_unsigned(optarg, 1, 1000,
                                   &config->network_interval_ms); break;
        case OPT_PULSE:
            error = parse_unsigned(optarg, 0, 1000,
                                   &config->network_pulse_ms); break;
        case OPT_RX_COLOR:
            error = parse_rgb(optarg, &config->network_rx_color); break;
        case OPT_TX_COLOR:
            error = parse_rgb(optarg, &config->network_tx_color); break;
        case OPT_BRIGHTNESS:
            error = parse_unsigned(optarg, 0, 255,
                                   &config->network_brightness); break;
        case OPT_DISK_INTERVAL:
            error = parse_unsigned(optarg, 1, 1000,
                                   &config->disk_interval_ms); break;
        case OPT_DISK_HOLD:
            error = parse_unsigned(optarg, 1, 10000,
                                   &config->disk_hold_ms); break;
        case OPT_DISK_PULSE:
            error = parse_unsigned(optarg, 1, 1000,
                                   &config->disk_pulse_ms); break;
        default:
            return -EINVAL;
        }
        if (error < 0)
            return error;
    }

    if (optind != argc || config->disk_pulse_ms >= config->disk_interval_ms ||
        (!config->power_enabled && !config->disks_enabled &&
         !config->network_enabled))
        return -EINVAL;
    return 0;
}
