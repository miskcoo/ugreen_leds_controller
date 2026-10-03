// SPDX-License-Identifier: GPL-2.0
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "disk_layout.h"
#include "scheduler.h"

static void test_weighted_ratio(void)
{
    struct pulse_scheduler scheduler = {0};
    unsigned rx = 0;
    unsigned tx = 0;
    unsigned i;

    for (i = 0; i < 100; ++i) {
        if (scheduler_choose_rx(&scheduler, 800, 200))
            ++rx;
        else
            ++tx;
    }
    assert(rx == 80);
    assert(tx == 20);
}

static void test_direction_change_without_idle(void)
{
    struct pulse_scheduler scheduler = {0};
    unsigned i;

    for (i = 0; i < 200; ++i)
        (void)scheduler_choose_rx(&scheduler, 1000000, 1000);
    assert(!scheduler_choose_rx(&scheduler, 1000, 1000000));
}

static void test_config_and_cli_precedence(void)
{
    static const char contents[] =
        "NETDEV_EBPF_INTERVAL_MS=175\n"
        "NETDEV_EBPF_PULSE_MS=61 # inline comment\n"
        "COLOR_NETDEV_RX=\"1 2 3\"\n"
        "COLOR_NETDEV_TX='4 5 6'\n"
        "BRIGHTNESS_NETDEV_LED=77\n"
        "DISK_EBPF_INTERVAL_MS=120\n"
        "DISK_EBPF_HOLD_MS=350\n"
        "DISK_EBPF_PULSE_MS=50\n"
        "MAPPING_METHOD=serial\n"
        "DISK_SERIAL=\"one two three\"\n"
        "LED_INVERT=false\n"
        "COLOR_DISK_HEALTH=\"10 20 30\"\n"
        "COLOR_DISK_HEALTH_PER_DISK[disk2]=\"11 22 33\"\n"
        "BRIGHTNESS_POWER=88\n"
        "COLOR_POWER=\"40 50 60\"\n"
        "BLINK_TYPE_POWER=\"breath 400 600\"\n";
    char path[] = "/tmp/ugreen-ledd-config-XXXXXX";
    struct config config;
    char *argv[] = {
        "ugreen-ledd-test", "--config", path, "-i", "test0",
        "--interval-ms", "200", "--rx-color", "9,8,7",
        "--disk-hold-ms", "500", NULL,
    };
    int fd = mkstemp(path);

    assert(fd >= 0);
    assert(write(fd, contents, sizeof(contents) - 1) ==
           (ssize_t)(sizeof(contents) - 1));
    assert(close(fd) == 0);

    assert(config_parse_options(11, argv, &config) == 0);
    assert(config.network_enabled);
    assert(!strcmp(config.interface, "test0"));
    assert(config.network_interval_ms == 200);
    assert(config.network_pulse_ms == 61);
    assert(config.network_brightness == 77);
    assert(config.network_rx_color.r == 9 &&
           config.network_rx_color.g == 8 &&
           config.network_rx_color.b == 7);
    assert(config.disk_interval_ms == 120);
    assert(config.disk_hold_ms == 500);
    assert(config.disk_pulse_ms == 50);
    assert(config.disk_mapping == DISK_MAPPING_SERIAL);
    assert(!strcmp(config.disk_serials[0], "one"));
    assert(!strcmp(config.disk_serials[2], "three"));
    assert(!config.disk_invert);
    assert(config.disk_color.r == 10 && config.disk_color.g == 20 &&
           config.disk_color.b == 30);
    assert(!config.disk_color_set[0]);
    assert(config.disk_color_set[1]);
    assert(config_disk_color(&config, 1).r == 11 &&
           config_disk_color(&config, 1).g == 22 &&
           config_disk_color(&config, 1).b == 33);
    assert(config.power_brightness == 88);
    assert(config.power_color.r == 40 && config.power_color.g == 50 &&
           config.power_color.b == 60);
    assert(config.power_mode == POWER_MODE_BREATH);
    assert(config.power_on_ms == 400 && config.power_off_ms == 600);
    assert(unlink(path) == 0);
}

static void test_power_only_is_valid(void)
{
    struct config config;
    char path[] = "/tmp/ugreen-ledd-empty-config-XXXXXX";
    char *argv[] = {
        "ugreen-ledd-test", "--config", path,
        "--no-network", "--no-disks", NULL,
    };
    int fd = mkstemp(path);

    assert(fd >= 0);
    assert(close(fd) == 0);
    assert(config_parse_options(5, argv, &config) == 0);
    assert(config.power_enabled);
    assert(!config.network_enabled);
    assert(!config.disks_enabled);
    assert(!config.disk_invert);
    assert(unlink(path) == 0);
}

static void test_repository_config_is_accepted(void)
{
    struct config config;

    config_set_defaults(&config);
    assert(config_load_file("../scripts/ugreen-leds.conf", &config) == 0);
    assert(config.disk_interval_ms == 100);
    assert(config.disk_hold_ms == 200);
    assert(config.disk_pulse_ms == 45);
    assert(!config.disk_invert);
    assert(!config.network_enabled);
}

static void test_dxp6800_layout(void)
{
    const char *product = "DXP6800 Pro";

    assert(disk_layout_slot_count(product) == 6);
    assert(disk_layout_hctl_slot(product, 6, "/devices/2:0:0:0") == 0);
    assert(disk_layout_hctl_slot(product, 6, "/devices/5:0:0:0") == 3);
    assert(disk_layout_hctl_slot(product, 6, "/devices/0:0:0:0") == 4);
    assert(disk_layout_hctl_slot(product, 6, "/devices/1:0:0:0") == 5);
    assert(disk_layout_ata_slot(product, 6, "/pci/ata3/host2") == 0);
    assert(disk_layout_ata_slot(product, 6, "/pci/ata6/host5") == 3);
    assert(disk_layout_ata_slot(product, 6, "/pci/ata1/host0") == 4);
    assert(disk_layout_ata_slot(product, 6, "/pci/ata2/host1") == 5);
}

static void test_standard_layout(void)
{
    assert(disk_layout_slot_count("DXP4800 Pro") == 4);
    assert(disk_layout_hctl_slot("DXP4800 Pro", 4, "3:0:0:0") == 3);
    assert(disk_layout_ata_slot("DXP4800 Pro", 4,
                                "/devices/ata2/host1") == 1);
}

int main(void)
{
    test_weighted_ratio();
    test_direction_change_without_idle();
    test_config_and_cli_precedence();
    test_power_only_is_valid();
    test_repository_config_is_accepted();
    test_dxp6800_layout();
    test_standard_layout();
    return 0;
}
