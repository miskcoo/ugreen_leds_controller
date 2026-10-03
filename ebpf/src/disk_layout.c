// SPDX-License-Identifier: GPL-2.0
#include "disk_layout.h"

#include <stdio.h>
#include <string.h>

#include "types.h"

static bool product_starts_with(const char *product_name, const char *prefix)
{
    return !strncmp(product_name, prefix, strlen(prefix));
}

unsigned disk_layout_slot_count(const char *product_name)
{
    if (product_starts_with(product_name, "DXP2800"))
        return 2;
    if (product_starts_with(product_name, "DX4600") ||
        product_starts_with(product_name, "DX4700") ||
        product_starts_with(product_name, "DXP4800"))
        return 4;
    if (product_starts_with(product_name, "DXP6800"))
        return 6;
    return UGREEN_DISK_COUNT;
}

int disk_layout_hctl_slot(const char *product_name, unsigned slot_count,
                          const char *device_path)
{
    const char *base = strrchr(device_path, '/');
    unsigned host, channel, target, lun;
    char tail;

    base = base ? base + 1 : device_path;
    if (sscanf(base, "%u:%u:%u:%u%c", &host, &channel, &target, &lun,
               &tail) != 4 || channel || target || lun)
        return -1;

    if (product_starts_with(product_name, "DXP6800")) {
        static const int slots[] = {4, 5, 0, 1, 2, 3};
        return host < sizeof(slots) / sizeof(slots[0]) ? slots[host] : -1;
    }
    return host < slot_count ? (int)host : -1;
}

int disk_layout_ata_slot(const char *product_name, unsigned slot_count,
                         const char *block_path)
{
    const char *cursor = block_path;
    unsigned ata;

    while ((cursor = strstr(cursor, "/ata")) != NULL) {
        char tail;
        if (sscanf(cursor, "/ata%u%c", &ata, &tail) >= 1) {
            if (product_starts_with(product_name, "DXP6800")) {
                static const int slots[] = {-1, 4, 5, 0, 1, 2, 3};
                return ata < sizeof(slots) / sizeof(slots[0])
                    ? slots[ata] : -1;
            }
            return ata >= 1 && ata <= slot_count ? (int)ata - 1 : -1;
        }
        cursor += 4;
    }
    return -1;
}
