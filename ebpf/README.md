# Event-driven UGREEN LED daemon

This backend controls the power, network, and disk LEDs through one userspace
daemon and one direct I2C connection to the UGREEN LED MCU. It does not require
the `led-ugreen` DKMS module.

## Architecture

The sources are deliberately separated by responsibility:

```text
bpf/       eBPF maps, TC network collectors, and block-I/O collector
include/   event ABI shared by eBPF and userspace
src/       configuration, I2C, discovery, policy, and event-loop modules
tests/     userspace regression tests
systemd/   example per-interface service unit
```

The three BPF translation units are linked into `ugreen_led.bpf.o`:

- TC ingress and egress programs aggregate network RX/TX bytes.
- `block:block_rq_issue` observes requests sent to configured physical disks.
- Both collectors rate-limit events in-kernel and publish them through one
  ring buffer. Idle devices cause no userspace wakeups.

`ugreen-ledd` is the sole MCU owner. It serializes commands and performs the
MCU acknowledgement/retry sequence before caching state.

## LED behavior

### Power

At startup the daemon applies `COLOR_POWER`, `BRIGHTNESS_POWER`, and
`BLINK_TYPE_POWER`. Blink and breath effects run in the MCU without periodic
userspace work.

### Network

Network behavior preserves the tested standalone daemon defaults:

- interval: 100 ms
- pulse: 45 ms
- RX: `0,96,255`
- TX: `255,0,0`

Mixed traffic is scheduled according to the current coalesced RX/TX byte ratio
without allowing old traffic to pin the color.

### Disks

The daemon discovers physical SATA disks through sysfs and supports the
existing `ata`, `hctl`, and `serial` mapping modes. Known model layouts include
the reordered six-bay DXP6800 mapping. Serial mapping reads
`ID_SERIAL_SHORT` from the udev database.

Both `COLOR_DISK_HEALTH` and the existing
`COLOR_DISK_HEALTH_PER_DISK[diskN]` overrides are honored.

The BPF program filters requests to mapped physical devices, so logical RAID,
device-mapper, and filesystem activity is represented by the bays that
actually receive I/O. At the first event in a burst the MCU starts hardware
blinking for that disk. Further events extend an idle deadline; after the
deadline the LED returns to its steady state. This avoids issuing two I2C
commands for every pulse when several RAID members are busy.

Disk presence is updated from block-device uevents. Empty slots are turned
off. `LED_INVERT=1` keeps healthy disks steadily lit and makes activity the dark
part of the blink cycle; `LED_INVERT=0` uses light-on-activity behavior.

SMART, ZFS health, and ATA standby policies are not yet part of this backend.
They remain separate from activity collection because they require slow
userspace queries rather than eBPF hooks.

## Dependencies

On Debian:

```sh
sudo apt install build-essential clang llvm libbpf-dev libelf-dev zlib1g-dev \
    pkg-config i2c-tools
sudo modprobe i2c-dev
```

The kernel must provide BPF ring buffers, TC BPF, and the
`block:block_rq_issue` tracepoint.

## Build and test

```sh
cd ebpf
make
make test
make ugreen-bpf-load-test
sudo ./ugreen-bpf-load-test
```

The build produces:

```text
ugreen_led.bpf.o
ugreen-ledd
```

## Configuration

The daemon safely parses the relevant scalar settings from
`/etc/ugreen-leds.conf`; it does not source the file as a shell script.

Important settings and defaults are:

```sh
NETDEV_INTERFACE=""
NETDEV_EBPF_INTERVAL_MS=100
NETDEV_EBPF_PULSE_MS=45
COLOR_NETDEV_RX="0 96 255"
COLOR_NETDEV_TX="255 0 0"
BRIGHTNESS_NETDEV_LED="255"

MAPPING_METHOD=ata
DISK_SERIAL="SN1 SN2 SN3 SN4"
DISK_EBPF_INTERVAL_MS=100
DISK_EBPF_HOLD_MS=200
DISK_EBPF_PULSE_MS=45
LED_INVERT=0
COLOR_DISK_HEALTH="255 255 255"
BRIGHTNESS_DISK_LEDS="255"

BLINK_TYPE_POWER="none"
BRIGHTNESS_POWER=255
COLOR_POWER="255 255 255"
```

`BLINK_TYPE_POWER` also accepts `blink ON_MS OFF_MS` and
`breath ON_MS OFF_MS`.

Command-line options override the configuration file. For example:

```sh
sudo ./ugreen-ledd -i "$IF" -v
sudo ./ugreen-ledd --no-power --no-disks -i "$IF"
sudo ./ugreen-ledd --no-network
```

## Direct-I2C ownership

The DKMS module and the direct-I2C daemon cannot own the MCU simultaneously.
Before testing, stop the old policy services and unload the module:

```sh
sudo systemctl stop 'ugreen-netdevmon@*.service' 2>/dev/null || true
sudo systemctl stop ugreen-diskiomon.service ugreen-power-led.service \
    2>/dev/null || true
sudo modprobe -r led_ugreen
sudo modprobe i2c-dev
```

The daemon detaches only the TC filters it installed and deliberately leaves
an existing `clsact` qdisc in place.
