// SPDX-License-Identifier: GPL-2.0
#include "network_led.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>

static uint64_t saturating_add(uint64_t a, uint64_t b)
{
    return UINT64_MAX - a < b ? UINT64_MAX : a + b;
}

static int arm_timer(struct network_led *network)
{
    struct itimerspec timer = {0};

    timer.it_value.tv_sec = network->config->network_pulse_ms / 1000;
    timer.it_value.tv_nsec =
        (long)(network->config->network_pulse_ms % 1000) * 1000000L;
    if (!timer.it_value.tv_sec && !timer.it_value.tv_nsec)
        timer.it_value.tv_nsec = 1;
    return timerfd_settime(network->timer_fd, 0, &timer, NULL) < 0
        ? -errno : 0;
}

int network_led_init(struct network_led *network,
                     struct led_controller *controller,
                     const struct config *config)
{
    int error;

    memset(network, 0, sizeof(*network));
    network->controller = controller;
    network->config = config;
    network->timer_fd = timerfd_create(CLOCK_MONOTONIC,
                                       TFD_CLOEXEC | TFD_NONBLOCK);
    if (network->timer_fd < 0)
        return -errno;

    error = led_controller_set_brightness(controller, UGREEN_LED_NETWORK,
                                           (uint8_t)config->network_brightness);
    if (!error)
        error = led_controller_set_on(controller, UGREEN_LED_NETWORK, false);
    if (error < 0) {
        close(network->timer_fd);
        network->timer_fd = -1;
    }
    return error;
}

void network_led_add_sample(struct network_led *network,
                            uint64_t rx_bytes, uint64_t tx_bytes)
{
    network->pending_rx_bytes =
        saturating_add(network->pending_rx_bytes, rx_bytes);
    network->pending_tx_bytes =
        saturating_add(network->pending_tx_bytes, tx_bytes);

    if (network->config->verbose)
        fprintf(stderr, "network sample rx=%llu tx=%llu\n",
                (unsigned long long)rx_bytes,
                (unsigned long long)tx_bytes);
}

int network_led_render(struct network_led *network)
{
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    struct rgb color;
    bool rx;
    int error;

    if (network->led_on)
        return 0;
    rx_bytes = network->pending_rx_bytes;
    tx_bytes = network->pending_tx_bytes;
    if (!rx_bytes && !tx_bytes)
        return 0;

    network->pending_rx_bytes = 0;
    network->pending_tx_bytes = 0;
    rx = scheduler_choose_rx(&network->scheduler, rx_bytes, tx_bytes);
    color = rx ? network->config->network_rx_color
               : network->config->network_tx_color;

    if (network->config->verbose)
        fprintf(stderr,
                "network pulse sample-rx=%llu sample-tx=%llu "
                "pulses-rx=%llu pulses-tx=%llu -> %s\n",
                (unsigned long long)rx_bytes,
                (unsigned long long)tx_bytes,
                (unsigned long long)network->scheduler.rx_pulses,
                (unsigned long long)network->scheduler.tx_pulses,
                rx ? "RX" : "TX");

    error = led_controller_set_color(network->controller,
                                     UGREEN_LED_NETWORK, color);
    if (!error)
        error = led_controller_set_on(network->controller,
                                      UGREEN_LED_NETWORK, true);
    if (error < 0) {
        fprintf(stderr, "failed to start network LED pulse: %s; dropping pulse\n",
                strerror(-error));
        led_controller_invalidate(network->controller, UGREEN_LED_NETWORK);
        network->led_on = false;
        return 0;
    }
    network->led_on = true;

    error = arm_timer(network);
    if (error < 0) {
        (void)led_controller_set_on(network->controller,
                                    UGREEN_LED_NETWORK, false);
        network->led_on = false;
    }
    return error;
}

int network_led_handle_timer(struct network_led *network)
{
    uint64_t expirations;
    ssize_t size = read(network->timer_fd, &expirations, sizeof(expirations));
    int error;

    if (size < 0 && errno != EAGAIN)
        return -errno;
    if (!network->led_on)
        return 0;

    error = led_controller_set_on(network->controller,
                                  UGREEN_LED_NETWORK, false);
    if (error < 0) {
        fprintf(stderr, "failed to finish network LED pulse: %s; continuing\n",
                strerror(-error));
        led_controller_invalidate(network->controller, UGREEN_LED_NETWORK);
    }
    network->led_on = false;
    return network_led_render(network);
}

void network_led_cleanup(struct network_led *network)
{
    if (network->controller)
        (void)led_controller_set_on(network->controller,
                                    UGREEN_LED_NETWORK, false);
    if (network->timer_fd >= 0)
        close(network->timer_fd);
    network->timer_fd = -1;
}
