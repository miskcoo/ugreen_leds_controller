// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include "led_controller.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define UGREEN_LED_I2C_ADDR 0x3a
#define UGREEN_I2C_SYSFS "/sys/class/i2c-dev"
#define UGREEN_I2C_ADAPTER_PREFIX "SMBus I801 adapter"
#define COMMAND_RETRY_COUNT 5U
#define COMMAND_INITIAL_DELAY_US 1000U
#define COMMAND_RETRY_DELAY_US 30000U
#define COMMAND_STATUS_DELAY_US 2000U

static int i2c_write_block(int fd, uint8_t command,
                           const uint8_t *data, size_t size)
{
    union i2c_smbus_data smbus_data = {0};
    struct i2c_smbus_ioctl_data ioctl_data;
    size_t i;

    if (size > I2C_SMBUS_BLOCK_MAX)
        return -EMSGSIZE;

    smbus_data.block[0] = (uint8_t)size;
    for (i = 0; i < size; ++i)
        smbus_data.block[i + 1] = data[i];

    ioctl_data.read_write = I2C_SMBUS_WRITE;
    ioctl_data.command = command;
    ioctl_data.size = I2C_SMBUS_I2C_BLOCK_DATA;
    ioctl_data.data = &smbus_data;

    return ioctl(fd, I2C_SMBUS, &ioctl_data) < 0 ? -errno : 0;
}

static int i2c_read_byte(int fd, uint8_t command, uint8_t *value)
{
    union i2c_smbus_data smbus_data = {0};
    struct i2c_smbus_ioctl_data ioctl_data = {
        .read_write = I2C_SMBUS_READ,
        .command = command,
        .size = I2C_SMBUS_BYTE_DATA,
        .data = &smbus_data,
    };

    if (ioctl(fd, I2C_SMBUS, &ioctl_data) < 0)
        return -errno;
    *value = smbus_data.byte & 0xff;
    return 0;
}

static int read_first_line(const char *path, char *buffer, size_t length)
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

static int find_i2c_device(char *output, size_t length)
{
    struct dirent *entry;
    DIR *directory = opendir(UGREEN_I2C_SYSFS);
    int result = -ENOENT;

    if (!directory)
        return -errno;

    while ((entry = readdir(directory)) != NULL) {
        char name_path[PATH_MAX];
        char adapter_name[256];
        int written;

        if (strncmp(entry->d_name, "i2c-", 4) != 0)
            continue;
        written = snprintf(name_path, sizeof(name_path), "%s/%s/device/name",
                           UGREEN_I2C_SYSFS, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(name_path))
            continue;
        if (read_first_line(name_path, adapter_name, sizeof(adapter_name)) < 0)
            continue;
        if (strncmp(adapter_name, UGREEN_I2C_ADAPTER_PREFIX,
                    strlen(UGREEN_I2C_ADAPTER_PREFIX)) != 0)
            continue;

        written = snprintf(output, length, "/dev/%s", entry->d_name);
        result = written < 0 || (size_t)written >= length ? -ENAMETOOLONG : 0;
        break;
    }

    closedir(directory);
    return result;
}

int led_controller_open(struct led_controller *controller,
                        const char *override_path)
{
    char detected[PATH_MAX];
    const char *path = override_path;
    int error;

    memset(controller, 0, sizeof(*controller));
    controller->fd = -1;
    if (!path) {
        error = find_i2c_device(detected, sizeof(detected));
        if (error < 0)
            return error;
        path = detected;
    }

    controller->fd = open(path, O_RDWR | O_CLOEXEC);
    if (controller->fd < 0)
        return -errno;
    if (ioctl(controller->fd, I2C_SLAVE, UGREEN_LED_I2C_ADDR) < 0) {
        error = -errno;
        led_controller_close(controller);
        return error;
    }

    snprintf(controller->path, sizeof(controller->path), "%s", path);
    return 0;
}

void led_controller_close(struct led_controller *controller)
{
    if (controller->fd >= 0)
        close(controller->fd);
    controller->fd = -1;
}

void led_controller_invalidate(struct led_controller *controller,
                               uint8_t led_id)
{
    if (led_id >= UGREEN_LED_COUNT)
        return;
    memset(&controller->cache[led_id], 0,
           sizeof(controller->cache[led_id]));
}

static unsigned checksum(const uint8_t *data, size_t size)
{
    unsigned sum = 0;
    size_t i;

    for (i = 0; i < size; ++i)
        sum += data[i];
    return sum;
}

static int change_status_once(struct led_controller *controller,
                              uint8_t led_id, uint8_t command,
                              uint8_t p0, uint8_t p1,
                              uint8_t p2, uint8_t p3)
{
    uint8_t data[12] = {
        0x00, 0xa0, 0x01, 0x00, 0x00, command,
        p0, p1, p2, p3, 0x00, 0x00,
    };
    unsigned sum = checksum(data, 10);

    data[10] = (uint8_t)(sum >> 8);
    data[11] = (uint8_t)sum;
    data[0] = led_id;
    return i2c_write_block(controller->fd, led_id, data, sizeof(data));
}

static int change_status(struct led_controller *controller, uint8_t led_id,
                         uint8_t command, uint8_t p0, uint8_t p1,
                         uint8_t p2, uint8_t p3)
{
    const char *stage = "unknown stage";
    uint8_t accepted = 0;
    unsigned attempt;
    int error = -EIO;

    if (led_id >= UGREEN_LED_COUNT)
        return -EINVAL;

    for (attempt = 0; attempt < COMMAND_RETRY_COUNT; ++attempt) {
        if (usleep(attempt ? COMMAND_RETRY_DELAY_US
                           : COMMAND_INITIAL_DELAY_US) < 0 && errno != EINTR)
            return -errno;

        error = change_status_once(controller, led_id, command,
                                   p0, p1, p2, p3);
        if (error < 0) {
            stage = "SMBus write";
            continue;
        }
        if (usleep(COMMAND_STATUS_DELAY_US) < 0 && errno != EINTR)
            return -errno;

        error = i2c_read_byte(controller->fd, 0x80, &accepted);
        if (!error && accepted == 1)
            return 0;
        if (error < 0) {
            stage = "MCU acknowledgement read";
        } else {
            stage = "MCU command rejection";
            error = -EREMOTEIO;
        }
    }

    led_controller_invalidate(controller, led_id);
    fprintf(stderr,
            "LED id=%u command=0x%02x failed after %u attempts during %s: "
            "%s (last acknowledgement=0x%02x)\n",
            led_id, command, COMMAND_RETRY_COUNT, stage,
            strerror(-error), accepted);
    return error;
}

int led_controller_set_color(struct led_controller *controller,
                             uint8_t led_id, struct rgb color)
{
    struct led_cache *cache;
    int error;

    if (led_id >= UGREEN_LED_COUNT)
        return -EINVAL;
    cache = &controller->cache[led_id];
    if (cache->color_valid && rgb_equal(cache->color, color))
        return 0;

    error = change_status(controller, led_id, 0x02,
                          color.r, color.g, color.b, 0);
    if (!error) {
        cache->color = color;
        cache->color_valid = true;
    }
    return error;
}

int led_controller_set_brightness(struct led_controller *controller,
                                  uint8_t led_id, uint8_t brightness)
{
    struct led_cache *cache;
    int error;

    if (led_id >= UGREEN_LED_COUNT)
        return -EINVAL;
    cache = &controller->cache[led_id];
    if (cache->brightness_valid && cache->brightness == brightness)
        return 0;

    error = change_status(controller, led_id, 0x01, brightness, 0, 0, 0);
    if (!error) {
        cache->brightness = brightness;
        cache->brightness_valid = true;
    }
    return error;
}

int led_controller_set_on(struct led_controller *controller,
                          uint8_t led_id, bool on)
{
    struct led_cache *cache;
    enum led_mode mode = on ? LED_MODE_ON : LED_MODE_OFF;
    int error;

    if (led_id >= UGREEN_LED_COUNT)
        return -EINVAL;
    cache = &controller->cache[led_id];
    if (cache->mode == mode)
        return 0;

    error = change_status(controller, led_id, 0x03, on ? 1 : 0, 0, 0, 0);
    if (!error)
        cache->mode = mode;
    return error;
}

int led_controller_set_blink(struct led_controller *controller,
                             uint8_t led_id, uint16_t on_ms,
                             uint16_t off_ms, bool breath)
{
    struct led_cache *cache;
    enum led_mode mode = breath ? LED_MODE_BREATH : LED_MODE_BLINK;
    uint16_t cycle = on_ms + off_ms;
    int error;

    if (led_id >= UGREEN_LED_COUNT || cycle < on_ms)
        return -EINVAL;
    cache = &controller->cache[led_id];
    if (cache->mode == mode && cache->on_ms == on_ms &&
        cache->off_ms == off_ms)
        return 0;

    error = change_status(controller, led_id, breath ? 0x05 : 0x04,
                          (uint8_t)(cycle >> 8), (uint8_t)cycle,
                          (uint8_t)(on_ms >> 8), (uint8_t)on_ms);
    if (!error) {
        cache->mode = mode;
        cache->on_ms = on_ms;
        cache->off_ms = off_ms;
    }
    return error;
}
