// SPDX-License-Identifier: GPL-2.0
#ifndef UGREEN_DISK_LAYOUT_H
#define UGREEN_DISK_LAYOUT_H

unsigned disk_layout_slot_count(const char *product_name);
int disk_layout_hctl_slot(const char *product_name, unsigned slot_count,
                          const char *device_path);
int disk_layout_ata_slot(const char *product_name, unsigned slot_count,
                         const char *block_path);

#endif
