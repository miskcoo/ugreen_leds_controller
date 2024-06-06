#!/usr/bin/bash

if ! lsmod | grep ledtrig_netdev; then
    modprobe -v ledtrig_netdev
fi

led="netdev"
echo netdev > /sys/class/leds/$led/trigger
echo enp2s0 > /sys/class/leds/$led/device_name
echo 1 > /sys/class/leds/$led/link
echo 1 > /sys/class/leds/$led/tx
echo 1 > /sys/class/leds/$led/rx
echo 100 > /sys/class/leds/$led/interval
