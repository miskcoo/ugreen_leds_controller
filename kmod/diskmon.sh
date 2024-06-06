#!/usr/bin/bash

devices=(sda sdb sdc sdd)
led_map=(disk1 disk2 disk3 disk4)

diskio_data_r=()
diskio_data_w=()

if ! lsmod | grep ledtrig_ondshot; then
    modprobe -v ledtrig_oneshot
fi

for led in ${led_map[@]}; do 
    echo oneshot > /sys/class/leds/$led/trigger
    echo 1 > /sys/class/leds/$led/invert
    echo 100 > /sys/class/leds/$led/delay_on
    echo 100 > /sys/class/leds/$led/delay_off
done

while true; do

    for i in "${!devices[@]}"; do
        dev=${devices[$i]}
        diskio_old_r=${diskio_data_r[$i]}
        diskio_old_w=${diskio_data_w[$i]}

        diskio_new_r=$(cat /sys/block/${dev}/stat 2>/dev/null | awk '{ print $1 }')
        diskio_new_w=$(cat /sys/block/${dev}/stat 2>/dev/null | awk '{ print $4 }')

        if [ $? != 0 ]; then
            continue
        fi

        if [ "${diskio_old_r}" != "${diskio_new_r}" ] || [ "${diskio_old_w}" != "${diskio_new_w}" ]; then
            echo 1 > /sys/class/leds/${led_map[$i]}/shot
        fi

        diskio_data_r[$i]=$diskio_new_r
        diskio_data_w[$i]=$diskio_new_w
    done

    sleep 0.1

done
