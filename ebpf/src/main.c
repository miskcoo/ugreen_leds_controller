// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "bpf_runtime.h"
#include "config.h"
#include "disk_led.h"
#include "led_controller.h"
#include "network_led.h"
#include "power_led.h"
#include "ugreen_bpf_shared.h"

struct app {
    struct config config;
    struct led_controller controller;
    struct network_led network;
    struct disk_leds disks;
    struct bpf_runtime bpf;
    int signal_fd;
    bool network_initialized;
    bool disks_initialized;
    bool bpf_started;
};

static int set_memlock_rlimit(void)
{
    struct rlimit limit = {RLIM_INFINITY, RLIM_INFINITY};

    if (setrlimit(RLIMIT_MEMLOCK, &limit) == 0 || errno == EPERM)
        return 0;
    return -errno;
}

static int open_signal_fd(void)
{
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0)
        return -errno;
    return signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
}

static int consume_signal(int fd)
{
    struct signalfd_siginfo signal;
    ssize_t size = read(fd, &signal, sizeof(signal));

    if (size < 0)
        return errno == EAGAIN ? 0 : -errno;
    if ((size_t)size != sizeof(signal))
        return -EIO;
    return signal.ssi_signo == SIGINT || signal.ssi_signo == SIGTERM ? 1 : 0;
}

static int handle_bpf_event(void *context, void *data, size_t size)
{
    struct app *app = context;
    const struct ugreen_led_event *event = data;

    if (size < sizeof(*event)) {
        fprintf(stderr, "short BPF event (%zu bytes)\n", size);
        return 0;
    }
    switch (event->kind) {
    case UGREEN_EVENT_NETWORK:
        if (app->network_initialized)
            network_led_add_sample(&app->network, event->first_bytes,
                                   event->second_bytes);
        break;
    case UGREEN_EVENT_DISK:
        if (app->disks_initialized)
            (void)disk_leds_handle_activity(&app->disks, event->led_id);
        break;
    default:
        fprintf(stderr, "unknown BPF event kind %u\n", event->kind);
        break;
    }
    return 0;
}

static int run_event_loop(struct app *app)
{
    enum { SIGNAL_FD, NETWORK_FD, DISK_TIMER_FD, UEVENT_FD, BPF_FD, FD_COUNT };
    struct pollfd fds[FD_COUNT];

    for (;;) {
        int result;
        int error;

        fds[SIGNAL_FD] = (struct pollfd){app->signal_fd, POLLIN, 0};
        fds[NETWORK_FD] = (struct pollfd){
            app->network_initialized ? app->network.timer_fd : -1, POLLIN, 0};
        fds[DISK_TIMER_FD] = (struct pollfd){
            app->disks_initialized ? app->disks.timer_fd : -1, POLLIN, 0};
        fds[UEVENT_FD] = (struct pollfd){
            app->disks_initialized ? app->disks.uevent_fd : -1, POLLIN, 0};
        fds[BPF_FD] = (struct pollfd){
            app->bpf_started ? bpf_runtime_epoll_fd(&app->bpf) : -1,
            POLLIN, 0};

        result = poll(fds, FD_COUNT, -1);
        if (result < 0) {
            if (errno == EINTR)
                continue;
            return -errno;
        }

        if (fds[SIGNAL_FD].revents & POLLIN) {
            error = consume_signal(app->signal_fd);
            if (error != 0)
                return error < 0 ? error : 0;
        }
        if (fds[NETWORK_FD].revents & POLLIN) {
            error = network_led_handle_timer(&app->network);
            if (error < 0)
                return error;
        }
        if (fds[DISK_TIMER_FD].revents & POLLIN) {
            error = disk_leds_handle_timer(&app->disks);
            if (error < 0)
                return error;
        }
        if (fds[UEVENT_FD].revents & POLLIN) {
            error = disk_leds_handle_uevent(&app->disks, &app->bpf);
            if (error < 0)
                return error;
        }
        if (fds[BPF_FD].revents & POLLIN) {
            error = bpf_runtime_consume(&app->bpf);
            if (error < 0)
                return error;
            if (app->network_initialized) {
                error = network_led_render(&app->network);
                if (error < 0)
                    return error;
            }
        }

        for (unsigned i = 0; i < FD_COUNT; ++i) {
            if (fds[i].fd >= 0 &&
                (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)))
                return -EIO;
        }
    }
}

int main(int argc, char **argv)
{
    struct app app;
    int error = 0;

    memset(&app, 0, sizeof(app));
    app.controller.fd = -1;
    app.signal_fd = -1;

    if (config_parse_options(argc, argv, &app.config) < 0) {
        config_usage(argv[0]);
        return EXIT_FAILURE;
    }
    error = set_memlock_rlimit();
    if (error < 0) {
        fprintf(stderr, "failed to raise memlock limit: %s\n", strerror(-error));
        goto cleanup;
    }
    app.signal_fd = open_signal_fd();
    if (app.signal_fd < 0) {
        error = app.signal_fd;
        fprintf(stderr, "failed to create signal fd: %s\n", strerror(-error));
        goto cleanup;
    }
    error = led_controller_open(&app.controller, app.config.i2c_device);
    if (error < 0) {
        fprintf(stderr,
                "could not open UGREEN LED MCU: %s\n"
                "Is i2c-dev loaded and led_ugreen unloaded?\n",
                strerror(-error));
        goto cleanup;
    }

    if (app.config.power_enabled) {
        error = power_led_apply(&app.controller, &app.config);
        if (error < 0) {
            fprintf(stderr, "failed to initialize power LED: %s\n",
                    strerror(-error));
            goto cleanup;
        }
    }
    if (app.config.disks_enabled) {
        error = disk_leds_init(&app.disks, &app.controller, &app.config);
        if (error < 0) {
            fprintf(stderr, "failed to initialize disk LEDs: %s\n",
                    strerror(-error));
            goto cleanup;
        }
        app.disks_initialized = true;
    }
    if (app.config.network_enabled) {
        error = network_led_init(&app.network, &app.controller, &app.config);
        if (error < 0) {
            fprintf(stderr, "failed to initialize network LED: %s\n",
                    strerror(-error));
            goto cleanup;
        }
        app.network_initialized = true;
    }

    if (app.config.network_enabled || app.config.disks_enabled) {
        error = bpf_runtime_start(&app.bpf, &app.config,
                                  handle_bpf_event, &app);
        if (error < 0) {
            fprintf(stderr, "failed to start BPF collectors from '%s': %s\n",
                    app.config.bpf_object, strerror(-error));
            goto cleanup;
        }
        app.bpf_started = true;
        if (app.disks_initialized) {
            error = disk_leds_sync_bpf(&app.disks, &app.bpf);
            if (error < 0) {
                fprintf(stderr, "failed to configure disk BPF map: %s\n",
                        strerror(-error));
                goto cleanup;
            }
        }
    }

    fprintf(stderr, "ugreen-ledd started: power=%s network=%s disks=%s "
                    "i2c=%s config=%s\n",
            app.config.power_enabled ? "on" : "off",
            app.config.network_enabled ? app.config.interface : "off",
            app.config.disks_enabled ? "on" : "off",
            app.controller.path, app.config.config_path);
    error = run_event_loop(&app);
    if (error < 0)
        fprintf(stderr, "event loop failed: %s\n", strerror(-error));

cleanup:
    if (app.bpf_started)
        bpf_runtime_stop(&app.bpf);
    if (app.network_initialized)
        network_led_cleanup(&app.network);
    if (app.disks_initialized)
        disk_leds_cleanup(&app.disks);
    if (app.signal_fd >= 0)
        close(app.signal_fd);
    led_controller_close(&app.controller);
    return error < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
