#!/bin/bash
# Description: Disk activity & standby LED indicator for UGREEN DXP6800 Pro on Unraid
# Hardware: UGREEN DXP6800 Pro (6-Bay)

# Initialize all disk LEDs to off
for led in /sys/class/leds/disk*; do
    echo none > "$led/trigger" 2>/dev/null
    echo 0 > "$led/brightness" 2>/dev/null
done

declare -A LAST_IO
declare -A LAST_STATE

COUNTER=0

while true; do
    CHECK_STANDBY=0
    # Query standby status every ~5s to avoid excessive overhead
    if [ $((COUNTER % 25)) -eq 0 ]; then
        CHECK_STANDBY=1
    fi

    for devpath in /sys/block/sd*; do
        [ -e "$devpath" ] || continue
        dev=$(basename "$devpath")
        realpath=$(readlink -f "$devpath")

        if [[ "$realpath" =~ ata([0-9]+) ]]; then
            atanum="${BASH_REMATCH[1]}"

            # Hardware slot mapping for UGREEN DXP6800 Pro
            case "$atanum" in
                2) slot=1 ;;
                3) slot=2 ;;
                4) slot=3 ;;
                5) slot=4 ;;
                1) slot=5 ;;
                6) slot=6 ;;
                *) slot="" ;;
            esac

            if [ -n "$slot" ]; then
                led="/sys/class/leds/disk${slot}"

                if [ -d "$led" ]; then
                    read_io=$(awk '{print $1 + $5}' "/sys/block/${dev}/stat" 2>/dev/null || echo 0)
                    prev_io=${LAST_IO[$dev]:-0}

                    if [ "$read_io" -gt "$prev_io" ] && [ "$prev_io" -ne 0 ]; then
                        LAST_STATE[$dev]="active"
                        base_bright=255
                        # Flash on I/O activity
                        echo 0 > "$led/brightness"
                        (sleep 0.05 && echo "$base_bright" > "$led/brightness") &
                    else
                        if [ "$CHECK_STANDBY" -eq 1 ] || [ -z "${LAST_STATE[$dev]}" ]; then
                            # Non-invasive standby check using hdparm -C
                            if hdparm -C "/dev/$dev" 2>/dev/null | grep -iq "standby"; then
                                LAST_STATE[$dev]="standby"
                                base_bright=20
                            else
                                LAST_STATE[$dev]="active"
                                base_bright=255
                            fi
                            echo "$base_bright" > "$led/brightness"
                        fi
                    fi

                    LAST_IO[$dev]=$read_io
                fi
            fi
        fi
    done

    COUNTER=$((COUNTER + 1))
    sleep 0.2
done
