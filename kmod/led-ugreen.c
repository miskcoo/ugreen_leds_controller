// SPDX-License-Identifier: GPL-2.0-only
/*
 *     UGREEN NAS LED driver.
 *
 *	Copyright (C) 2024.
 *	Author: Yuhao Zhou <miskcoo@gmail.com>
 */

#include "linux/stddef.h"
#include <linux/init.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/proc_fs.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "led-ugreen.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static struct ugreen_led_state *lcdev_to_ugreen_led_state(struct led_classdev *led_cdev) {
    return container_of(led_cdev, struct ugreen_led_state, cdev);
}

static int ugreen_led_change_state(
    struct i2c_client *client, 
    u8 led_id, 
    u8 command,
    u8 param1,
    u8 param2,
    u8 param3,
    u8 param4
) {
    // compute the checksum
    u16 cksum = 0xa1 + (u16)command + param1 + param2 + param3 + param4;

    // construct the write buffer
    u8 buf[12] = { 
        led_id, 
        0xa0, 0x01, 0x00, 0x00, 
        command, 
        param1, param2, param3, param4,
        (u8)((cksum >> 8) & 0xff),
        (u8)(cksum & 0xff)
    };

    // write the buffer to the I2C device by sending block data 
    s32 rc = i2c_smbus_write_i2c_block_data(client, led_id, 12, buf);

    // check the return code
    if (rc < 0) {
        pr_err("%s: i2c_smbus_write_i2c_block_data failed with id %d,"
                "cmd 0x%x, params (0x%x, 0x%x, 0x%x, 0x%x), err %d", 
                __func__, led_id, command, param1, param2, param3, param4, rc);
        return rc;
    }

    return 0;
}

// get the state of the DX4600 LEDs 
static int ugreen_led_get_state(
        struct i2c_client *client, 
        u8 led_id, 
        struct ugreen_led_state *state
) {
    if (!state) {
        pr_err("%s: invalid state buffer", __func__);
        return -1;
    }

    // read the state of the LED from the I2C device
    u8 buf[11];
    s32 rc = i2c_smbus_read_i2c_block_data(client, 0x81 + led_id, 11, (u8 *)buf);

    // check the return code
    if (rc < 0) {
        pr_err("%s: i2c_smbus_read_i2c_block_data failed with id %d, err %d", 
                __func__, led_id, rc);
        return rc;
    }

    // compute the checksum of received data
    u16 sum = 0;
    for (int i = 0; i < 9; ++i) {
        sum += buf[i];
    }

    // check the checksum
    if (sum == 0 || (sum != (((u16)buf[9] << 8) | buf[10]))) {
        return -1;
    }

    // parse the state of the LED
    state->status = buf[0];
    state->brightness = buf[1];
    state->r = buf[2];
    state->g = buf[3];
    state->b = buf[4];
    u16 t_hight = (((int)buf[5]) << 8) | buf[6];
    u16 t_low = (((int)buf[7]) << 8) | buf[8];
    state->t_on = t_low;
    state->t_cycle = t_hight;

    return 0;
}

static bool ugreen_led_get_last_command_status(struct i2c_client *client) {

    // read the status byte from the I2C device
    s32 rc = i2c_smbus_read_byte_data(client, 0x80);

    // check the return code
    if (rc < 0) {
        pr_err("%s: i2c_smbus_read_byte_data failed with err %d", __func__, rc);
        return false;
    }

    return rc == 1;
}

static int ugreen_led_change_state_robust(
    struct i2c_client *client, 
    u8 led_id, 
    u8 command,
    u8 param1,
    u8 param2,
    u8 param3,
    u8 param4
) {
    int rc = 0;
    for (int i = 0; i < UGREEN_LED_CHANGE_STATE_RETRY_COUNT; ++i) {

        if (i == 0) usleep_range(500, 1500);
        else msleep(30);

        if (i > 0) pr_debug("retrying %d", i);

        rc = ugreen_led_change_state(client, led_id, command, param1, param2, param3, param4);
        if (rc == 0) {
            usleep_range(1500, 2500);
            if (ugreen_led_get_last_command_status(client)) {
                return 0;
            }
        }
    }

    return -1;
}

static int ugreen_led_get_state_robust(
        struct i2c_client *client, 
        u8 led_id, 
        struct ugreen_led_state *state
) {
    int rc = 0;
    for (int i = 0; i < UGREEN_LED_CHANGE_STATE_RETRY_COUNT; ++i) {

        if (i == 0) usleep_range(500, 1500);
        else msleep(30);

        rc = ugreen_led_get_state(client, led_id, state);
        if (rc == 0) return 0;
    }

    state->status = UGREEN_LED_STATE_INVALID;

    return -1;
}

static void ugreen_led_turn_on_or_off_unlock(struct ugreen_led_array *priv, u8 led_id, bool on) {

    if (priv->state[led_id].status == (on ? UGREEN_LED_STATE_ON : UGREEN_LED_STATE_OFF)) {
        return;
    }

    int rc = ugreen_led_change_state_robust(priv->client, led_id, 0x03, on ? 1 : 0, 0, 0, 0);
    if (rc == 0) {
        priv->state[led_id].status = on ? UGREEN_LED_STATE_ON : UGREEN_LED_STATE_OFF;
    } else {
        pr_err("failed to turn %d %s", led_id, on ? "on" : "off");
    }
}

static void ugreen_led_set_brightness_unlock(struct ugreen_led_array *priv, u8 led_id, enum led_brightness brightness) {

    struct ugreen_led_state *state = priv->state + led_id;

    if (brightness == 0) {
        ugreen_led_turn_on_or_off_unlock(priv, led_id, false);
    } else {
        if (state->brightness != brightness) {
            int rc = ugreen_led_change_state_robust(priv->client, led_id, 0x01, brightness, 0, 0, 0);
            if (rc == 0) {
                state->brightness = brightness;
            } else {
                pr_err("failed to set brightness of %d to %d", led_id, brightness);
            }
        }

        if (state->status == UGREEN_LED_STATE_OFF)
            ugreen_led_turn_on_or_off_unlock(priv, led_id, true);
    }
}

static void ugreen_led_set_color_unlock(struct ugreen_led_array *priv, u8 led_id, u8 r, u8 g, u8 b) {

    struct ugreen_led_state *state = priv->state + led_id;

    if (!r && !g && !b) {
        return ugreen_led_turn_on_or_off_unlock(priv, led_id, false);
    }

    if (state->r != r || state->g != g || state->b != b) {
        int rc = ugreen_led_change_state_robust(priv->client, led_id, 0x02, r, g, b, 0);
        if (rc == 0) {
            state->r = r;
            state->g = g;
            state->b = b;
        } else {
            pr_err("failed to set color of %d to 0x%02x%02x%02x", led_id, r, g, b);
        }
    }
}

static void ugreen_led_set_blink_or_breath_unlock(struct ugreen_led_array *priv, u8 led_id, u16 t_on, u16 t_cycle, bool is_blink) {

    int rc;
    struct ugreen_led_state *state = priv->state + led_id;
    u8 led_status = is_blink ? UGREEN_LED_STATE_BLINK : UGREEN_LED_STATE_BREATH;

    if (state->t_on == t_on && state->t_cycle == t_cycle && state->status == led_status) {
        rc = 0;
    } else {
        rc = ugreen_led_change_state_robust(priv->client, led_id, is_blink ? 0x04 : 0x05, 
            (u8)(t_cycle >> 8), (u8)(t_cycle & 0xff), 
            (u8)(t_on >> 8), (u8)(t_on & 0xff)
        );

        if (rc == 0) {
            state->t_on = t_on;
            state->t_cycle = t_cycle;
            state->status = led_status;
        } else {
            pr_err("failed to set %s of %d to %d %d", is_blink ? "blink" : "breath", led_id, t_on, t_cycle);
        }
    }
}

static int ugreen_led_set_brightness_blocking(struct led_classdev *cdev, enum led_brightness brightness) {

    struct ugreen_led_state *state = lcdev_to_ugreen_led_state(cdev);
    struct ugreen_led_array *priv = state->priv;
    int led_id = state->led_id;

    pr_debug("set brightness of %d to %d\n", led_id, brightness);

    mutex_lock(&priv->mutex);
    ugreen_led_set_brightness_unlock(priv, led_id, brightness);
    mutex_unlock(&priv->mutex);

    return 0;
}

static enum led_brightness ugreen_led_get_brightness(struct led_classdev *cdev) {

    struct ugreen_led_state *state = lcdev_to_ugreen_led_state(cdev);

    pr_debug("get brightness of %d\n", state->led_id);

    if (!state->r && !state->g && !state->b)
        return LED_OFF;

    return state->status == UGREEN_LED_STATE_OFF ? LED_OFF : state->brightness;
}

static void truncate_blink_delay_time(unsigned long *delay_on, unsigned long *delay_off) {

    if (*delay_on < 100) *delay_on = 100;
    else if (*delay_on > 0x7fff) *delay_on = 0x7fff;

    if (*delay_off < 100) *delay_off = 100;
    else if (*delay_off > 0x7fff) *delay_off = 0x7fff;
}

static int ugreen_led_set_blink(struct led_classdev *cdev, unsigned long *delay_on, unsigned long *delay_off) {

    struct ugreen_led_state *state = lcdev_to_ugreen_led_state(cdev);
    struct ugreen_led_array *priv = state->priv;
    int led_id = state->led_id;

    truncate_blink_delay_time(delay_on, delay_off);

    pr_debug("set blink of %d to %lu %lu\n", led_id, *delay_on, *delay_off);

    mutex_lock(&priv->mutex);

    ugreen_led_set_blink_or_breath_unlock(priv, led_id, *delay_on, *delay_on + *delay_off, true);
    *delay_on = state->t_on;
    *delay_off = state->t_cycle - state->t_on;

    mutex_unlock(&priv->mutex);

    return state->status == UGREEN_LED_STATE_BLINK ? 0 : -EINVAL;
}

static ssize_t color_store(struct device *dev, 
        struct device_attribute *attr, 
        const char *buf, size_t size)
{

    struct led_classdev *cdev = dev_get_drvdata(dev);
    struct ugreen_led_state *state = lcdev_to_ugreen_led_state(cdev);
    struct ugreen_led_array *priv = state->priv;
    int led_id = state->led_id;
    u8 r, g, b;

    int nrchars;

    if (sscanf(buf, "%hhu %hhu %hhu%n", &r, &g, &b, &nrchars) != 3) {
        return -EINVAL;
    }

    if (++nrchars < size) {
        return -EINVAL;
    }

    pr_debug("set color of %d to 0x%02x%02x%02x\n", led_id, r, g, b);

    mutex_lock(&priv->mutex);
    ugreen_led_set_color_unlock(priv, led_id, r, g, b);
    mutex_unlock(&priv->mutex);

    return size;
}

static ssize_t color_show(struct device *dev, struct device_attribute *attr, char *buf) {

    struct led_classdev *cdev = dev_get_drvdata(dev);
    struct ugreen_led_state *state = lcdev_to_ugreen_led_state(cdev);
    return sprintf(buf, "%d %d %d\n", state->r, state->g, state->b);
}

static DEVICE_ATTR_RW(color);

static ssize_t blink_type_store(struct device *dev, 
        struct device_attribute *attr, 
        const char *buf, size_t size)
{

    struct led_classdev *cdev = dev_get_drvdata(dev);
    struct ugreen_led_state *state = lcdev_to_ugreen_led_state(cdev);

    u8 blink_type;
    unsigned long delay_on, delay_off;
    int nrchars;

    if (sscanf(buf, "blink %lu %lu%n", &delay_on, &delay_off, &nrchars) == 2) {
        blink_type = UGREEN_LED_STATE_BLINK;
    } else if(sscanf(buf, "breath %lu %lu%n", &delay_on, &delay_off, &nrchars) == 2) {
        blink_type = UGREEN_LED_STATE_BREATH;
    } else if(strcmp(buf, "none\n") == 0) {
        blink_type = UGREEN_LED_STATE_ON;
        nrchars = size;
    } else return -EINVAL;

    if (++nrchars < size) {
        return -EINVAL;
    }

    mutex_lock(&state->priv->mutex);

    if (blink_type == UGREEN_LED_STATE_ON) {
        ugreen_led_turn_on_or_off_unlock(state->priv, state->led_id, true);
    } else {
        truncate_blink_delay_time(&delay_on, &delay_off);
        ugreen_led_set_blink_or_breath_unlock(state->priv, state->led_id, 
                (u16)delay_on, (u16)(delay_on + delay_off),
                blink_type == UGREEN_LED_STATE_BLINK ? true : false);
    }

    mutex_unlock(&state->priv->mutex);

    return size;
}

static ssize_t blink_type_show(struct device *dev, struct device_attribute *attr, char *buf) {

    struct led_classdev *cdev = dev_get_drvdata(dev);
    struct ugreen_led_state *state = lcdev_to_ugreen_led_state(cdev);

    ssize_t size = 0;

    mutex_lock(&state->priv->mutex);
    u8 status = state->status;
    int delay_on = state->t_on;
    int delay_off = state->t_cycle - state->t_on;
    mutex_unlock(&state->priv->mutex);

    if (status == UGREEN_LED_STATE_BLINK) {
        size += sprintf(buf, "none [blink] breath\n");
    } else if (status == UGREEN_LED_STATE_BREATH) {
        size += sprintf(buf, "none blink [breath]\n");
    } else {
        size += sprintf(buf, "[none] blink breath\n");
    }

    if (status == UGREEN_LED_STATE_BLINK || status == UGREEN_LED_STATE_BREATH) {
        size += sprintf(buf + size, "delay_on: %d, delay_off: %d\n", delay_on, delay_off);
    }

    size += sprintf(buf + size, "\nUsage: write \"blink <delay_on> <delay_off>\" to change the state.\n");

    return size;
}

static DEVICE_ATTR_RW(blink_type);

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf) {

    struct led_classdev *cdev = dev_get_drvdata(dev);
    struct ugreen_led_state state = *lcdev_to_ugreen_led_state(cdev);

    mutex_lock(&state.priv->mutex);
    int status = state.status;
    if (status >= ARRAY_SIZE(ugreen_led_state_name)) {
        status = UGREEN_LED_STATE_INVALID;
    }
    ssize_t size = sprintf(buf, "%s %d %d %d %d %d %d\n", 
            ugreen_led_state_name[state.status], (int)state.brightness, 
            (int)state.r, (int)state.g, (int)state.b,
            (int)state.t_on, (int)(state.t_cycle - state.t_on));
    mutex_unlock(&state.priv->mutex);

    return size;
}

static DEVICE_ATTR_RO(status);

static struct attribute *ugreen_led_attrs[] = {
	&dev_attr_color.attr,
	&dev_attr_status.attr,
	&dev_attr_blink_type.attr,
	NULL,
};

ATTRIBUTE_GROUPS(ugreen_led);

static int ugreen_led_probe(struct i2c_client *client) {

    pr_info ("i2c probed");

    struct ugreen_led_array *priv;
    
    priv = devm_kzalloc(&client->dev, sizeof(struct ugreen_led_array), GFP_KERNEL);
    if (!priv) {
        return -ENOMEM;
    }

    priv->client = client;

    mutex_init(&priv->mutex);

    // probe and initialize leds
    for (int i = 0; i < UGREEN_MAX_LED_NUMBER; ++i) {

        priv->state[i].priv = priv;
        priv->state[i].led_id = i;

        ugreen_led_get_state_robust(client, i, priv->state + i);

        struct ugreen_led_state *state = priv->state + i;
        if (state->status != UGREEN_LED_STATE_INVALID) {

            pr_info("probed led id %d, status %d, rgb 0x%02x%02x%02x, "
                    "brightness %d, t_on %d, t_cycle %d\n", i, 
                    state->status, state->r, state->g, state->b,
                    state->brightness, state->t_on, state->t_cycle);

            ugreen_led_set_brightness_unlock(priv, i, 128);
            ugreen_led_set_color_unlock(priv, i, 0xff, 0xff, 0xff);

        }
    }

    i2c_set_clientdata(client, priv);

    mutex_lock(&priv->mutex);

    // register leds class devices
    const char *led_name[] = {
        "power", "netdev", "disk1", "disk2", "disk3", "disk4", "disk5", "disk6", "disk7", "disk8"
    };

    for (int i = 0; i < UGREEN_MAX_LED_NUMBER; ++i) {

        struct ugreen_led_state *state = priv->state + i;
        if (state->status == UGREEN_LED_STATE_INVALID)
            continue;

        // register the brightness control
        if (i < ARRAY_SIZE(led_name))
            state->cdev.name = led_name[i];
        else state->cdev.name = "unknown";

        state->cdev.brightness = state->cdev.brightness;
        state->cdev.max_brightness = 0xff;
        state->cdev.brightness_set_blocking = ugreen_led_set_brightness_blocking;
        state->cdev.brightness_get = ugreen_led_get_brightness;
        state->cdev.groups = ugreen_led_groups;
        state->cdev.blink_set = ugreen_led_set_blink;

        led_classdev_register(&client->dev, &state->cdev);
    }

    mutex_unlock(&priv->mutex);

    return 0;
}

static void ugreen_led_remove(struct i2c_client *client) {   

    struct ugreen_led_array *priv = i2c_get_clientdata(client);

    for (int i = 0; i < UGREEN_MAX_LED_NUMBER; ++i) {

        struct ugreen_led_state *state = priv->state + i;
        if (state->status == UGREEN_LED_STATE_INVALID)
            continue;

        led_classdev_unregister(&state->cdev);
    }

    mutex_destroy(&priv->mutex);

    pr_info ("i2c removed");
}

static const struct i2c_device_id ugreen_led_id[] = {
        { UGREEN_LED_SLAVE_NAME, 0 },
        { }
};

MODULE_DEVICE_TABLE(i2c, ugreen_led_id);

static struct i2c_driver ugreen_led_driver = {
        .driver = {
            .name   = UGREEN_LED_SLAVE_NAME,
            .owner  = THIS_MODULE,
        },

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)
        .probe          = ugreen_led_probe,
#else
        .probe_new      = ugreen_led_probe,
#endif
        .remove         = ugreen_led_remove,
        .id_table       = ugreen_led_id,
};


static int __init ugreen_led_init(void) {
    pr_info ("initializing");
    i2c_add_driver(&ugreen_led_driver);
    return 0;
}

static void __exit ugreen_led_exit(void) {
    i2c_del_driver(&ugreen_led_driver);
    pr_info ("exited");
}

module_init(ugreen_led_init);
module_exit(ugreen_led_exit);


// Module metadata
MODULE_AUTHOR("Yuhao Zhou <miskcoo@gmail.com>");
MODULE_DESCRIPTION("UGREEN NAS LED driver");
MODULE_LICENSE("GPL v2");
