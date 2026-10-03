// SPDX-License-Identifier: GPL-2.0
#include <bpf/libbpf.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "./ugreen_led.bpf.o";
    struct bpf_object *object = bpf_object__open_file(path, NULL);
    int error;

    if (!object) {
        error = -(errno ? errno : EIO);
        fprintf(stderr, "open %s: %s\n", path, strerror(-error));
        return 1;
    }
    error = bpf_object__load(object);
    if (error < 0)
        fprintf(stderr, "load %s: %s\n", path, strerror(-error));
    bpf_object__close(object);
    return error < 0;
}
