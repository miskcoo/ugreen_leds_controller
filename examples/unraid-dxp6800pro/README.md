# Unraid Disk LED & Standby Indicator for UGREEN DXP6800 Pro

This script provides LED management for the **UGREEN DXP6800 Pro (6-Bay)** running Unraid OS.

### Features
- **Accurate Slot Mapping**: Maps SATA controller ports (`ata2->1`, `ata3->2`, `ata4->3`, `ata5->4`, `ata1->5`, `ata6->6`) to front panel LEDs.
- **Standby Detection (Dim Glow)**: Drops brightness to `20` when drives are spun down (`hdparm -C` non-invasive query).
- **Active State (Full Brightness)**: Sets brightness to `255` when drives are active/idle.
- **I/O Activity**: Flashes during disk read/write.
- **Empty Slot Handling**: Unpopulated bays remain unlit.

### Installation on Unraid
1. Place `disk_led_activity.sh` into `/boot/config/led/`.
2. Add the following to `/boot/config/go`:
   ```bash
   cp /boot/config/led/disk_led_activity.sh /usr/local/bin/disk_led_activity.sh
   chmod +x /usr/local/bin/disk_led_activity.sh
   /usr/local/bin/disk_led_activity.sh &
