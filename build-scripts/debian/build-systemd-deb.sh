#!/usr/bin/env bash

set -e
set -x

pkgver="0.3"
pkgname="led-ugreen-systemd"

mkdir -p $pkgname/DEBIAN

cat <<EOF > $pkgname/DEBIAN/control
Package: $pkgname
Version: $pkgver
Architecture: all
Maintainer: Yuhao Zhou <miskcoo@gmail.com>
Depends: led-ugreen-utils (>= 0.3), led-ugreen-dkms (>= 0.3), debconf (>= 0.5)
Homepage: https://github.com/miskcoo/ugreen_leds_controller
Description: UGREEN NAS LED systemd services
 Systemd service files for automatic UGREEN NAS LED monitoring.
 Provides services for disk activity monitoring, network activity
 monitoring, LED hardware probing, and power LED control.
 .
 Includes interactive configuration for disk mapping and network
 interface selection during installation.
EOF

# Debconf templates for interactive configuration
mkdir -p $pkgname/DEBIAN

cat <<'EOF' > $pkgname/DEBIAN/templates
Template: led-ugreen-systemd/configure-services
Type: boolean
Default: true
Description: Configure UGREEN LED services automatically?
 The UGREEN LED systemd services can be automatically configured
 based on your hardware. This includes:
 - Auto-detecting disk mapping (ATA/HCTL/Serial)
 - Selecting network interface(s) to monitor
 - Enabling services to start at boot
 .
 If you select 'No', you will need to manually configure
 /etc/ugreen-leds.conf and enable services.

Template: led-ugreen-systemd/disk-mapping-method
Type: select
Choices: ata, hctl, serial
Default: ata
Description: Disk mapping method:
 Select how to map physical disk slots to LEDs:
 .
 ata: Map by ATA port number (recommended for most devices)
 hctl: Map by SCSI HCTL address
 serial: Map by disk serial number (requires manual configuration)
 .
 The 'ata' method is used by UGOS and works on most UGREEN devices.

Template: led-ugreen-systemd/network-mode
Type: select
Choices: single, multi, all
Default: single
Description: Network interface monitoring mode:
 Select which network interfaces to monitor:
 .
 single: Monitor one primary interface (you will be prompted to select)
 multi: Monitor multiple selected interfaces
 all: Monitor all physical ethernet interfaces automatically
 .
 Note: Only one network LED is available, it will show activity
 from any monitored interface.

Template: led-ugreen-systemd/network-interface
Type: string
Description: Network interface to monitor:
 Enter the name of the primary network interface to monitor
 (e.g., eth0, enp2s0, eno1).
 .
 Leave blank to auto-detect the first active interface.

Template: led-ugreen-systemd/led-invert
Type: boolean
Default: false
Description: Invert LED behavior?
 Should the LEDs (disk and network) be lit when inactive instead of when active?
 .
 Normal behavior (No): LEDs are dark when inactive, light up on activity
 Inverted behavior (Yes): LEDs are lit when inactive, go dark on activity
 .
 Most users should choose 'No' for normal behavior.

Template: led-ugreen-systemd/enable-services
Type: boolean
Default: true
Description: Enable services to start at boot?
 Should the UGREEN LED services be enabled to start automatically
 at boot time?
EOF

cat <<'EOF' > $pkgname/DEBIAN/config
#!/usr/bin/bash

set -e

. /usr/share/debconf/confmodule

# Ask if user wants automatic configuration
db_input high led-ugreen-systemd/configure-services || true
db_go || true

db_get led-ugreen-systemd/configure-services
if [ "$RET" = "true" ]; then
    # Detect device model to suggest mapping method
    if which dmidecode > /dev/null 2>&1; then
        product_name=$(dmidecode --string system-product-name 2>/dev/null || echo "Unknown")
        case "$product_name" in
            DXP6800*)
                db_set led-ugreen-systemd/disk-mapping-method "hctl"
                ;;
            *)
                db_set led-ugreen-systemd/disk-mapping-method "ata"
                ;;
        esac
    fi

    # Ask for disk mapping method
    db_input high led-ugreen-systemd/disk-mapping-method || true
    db_go || true

    # Detect network interfaces
    interfaces=$(ls /sys/class/net/ | grep -E '^(eth|enp|eno)' | head -1 || echo "")
    if [ ! -z "$interfaces" ]; then
        db_set led-ugreen-systemd/network-interface "$interfaces"
    fi

    # Ask for network monitoring mode
    db_input high led-ugreen-systemd/network-mode || true
    db_go || true

    db_get led-ugreen-systemd/network-mode
    if [ "$RET" = "single" ]; then
        # Ask which interface to monitor
        db_input high led-ugreen-systemd/network-interface || true
        db_go || true
    fi

    # Ask about LED invert behavior
    db_input medium led-ugreen-systemd/led-invert || true
    db_go || true

    # Ask about auto-enabling services
    db_input high led-ugreen-systemd/enable-services || true
    db_go || true
fi

exit 0
EOF

cat <<'EOF' > $pkgname/DEBIAN/postinst
#!/usr/bin/bash

set -e

. /usr/share/debconf/confmodule

# Load current configuration
config_file="/etc/ugreen-leds.conf"

db_get led-ugreen-systemd/configure-services
if [ "$RET" = "true" ]; then
    echo "Configuring UGREEN LED services..."

    # Update disk mapping method
    db_get led-ugreen-systemd/disk-mapping-method
    mapping_method="$RET"
    sed -i "s/^MAPPING_METHOD=.*/MAPPING_METHOD=$mapping_method/" "$config_file"
    echo "Set disk mapping method to: $mapping_method"

    # Update LED invert behavior
    db_get led-ugreen-systemd/led-invert
    if [ "$RET" = "true" ]; then
        led_invert="1"
    else
        led_invert="0"
    fi
    sed -i "s/^LED_INVERT=.*/LED_INVERT=$led_invert/" "$config_file"
    echo "Set LED invert to: $led_invert"

    # Configure network monitoring
    db_get led-ugreen-systemd/network-mode
    network_mode="$RET"

    # Enable ugreen-probe-leds (always needed)
    systemctl enable ugreen-probe-leds.service || true

    # Enable disk monitoring
    systemctl enable ugreen-diskiomon.service || true

    # Enable power LED
    systemctl enable ugreen-power-led.service || true

    # Configure network monitoring based on mode
    case "$network_mode" in
        single)
            db_get led-ugreen-systemd/network-interface
            interface="$RET"
            if [ ! -z "$interface" ]; then
                systemctl enable ugreen-netdevmon@${interface}.service || true
                echo "Enabled network monitoring for: $interface"
            fi
            ;;
        multi)
            # For now, default to primary interface
            # TODO: Implement multi-interface selection
            interfaces=$(ls /sys/class/net/ | grep -E '^(eth|enp|eno)')
            for iface in $interfaces; do
                echo "To monitor $iface, run: systemctl enable ugreen-netdevmon@${iface}.service"
            done
            ;;
        all)
            systemctl enable ugreen-netdevmon-multi.service || true
            echo "Enabled multi-interface network monitoring"
            ;;
    esac

    db_get led-ugreen-systemd/enable-services
    if [ "$RET" = "true" ]; then
        echo "Services enabled. Starting services..."
        systemctl daemon-reload
        systemctl start ugreen-probe-leds.service || true
        systemctl start ugreen-power-led.service || true
        systemctl start ugreen-diskiomon.service || true

        case "$network_mode" in
            single)
                db_get led-ugreen-systemd/network-interface
                interface="$RET"
                if [ ! -z "$interface" ]; then
                    systemctl start ugreen-netdevmon@${interface}.service || true
                fi
                ;;
            all)
                systemctl start ugreen-netdevmon-multi.service || true
                ;;
        esac

        echo "UGREEN LED services are now running."
    else
        echo "Services enabled but not started. Start them with:"
        echo "  systemctl start ugreen-probe-leds.service"
        echo "  systemctl start ugreen-diskiomon.service"
        echo "  systemctl start ugreen-power-led.service"
        echo "  systemctl start ugreen-netdevmon@<interface>.service"
    fi

    # Show disk mapping information
    echo ""
    echo "Disk mapping configured with method: $mapping_method"
    echo "To verify disk mapping, check the output of:"
    case "$mapping_method" in
        ata)
            echo "  ls -ahl /sys/block | grep ata"
            ;;
        hctl)
            echo "  lsblk -S -x hctl -o hctl,serial,name"
            ;;
        serial)
            echo "  Edit /etc/ugreen-leds.conf and set DISK_SERIAL array"
            ;;
    esac
fi

db_stop

exit 0
EOF

cat <<'EOF' > $pkgname/DEBIAN/prerm
#!/usr/bin/bash

set -e

# Stop and disable all services
services="ugreen-probe-leds ugreen-diskiomon ugreen-power-led"
for service in $services; do
    systemctl stop ${service}.service 2>/dev/null || true
    systemctl disable ${service}.service 2>/dev/null || true
done

# Stop all netdevmon instances
systemctl stop 'ugreen-netdevmon@*.service' 2>/dev/null || true
systemctl disable 'ugreen-netdevmon@*.service' 2>/dev/null || true

exit 0
EOF

chmod +x $pkgname/DEBIAN/config
chmod +x $pkgname/DEBIAN/postinst
chmod +x $pkgname/DEBIAN/prerm

# Install systemd service files
mkdir -p $pkgname/etc/systemd/system
cp scripts/systemd/*.service $pkgname/etc/systemd/system/

# Set ownership
chown -R root:root $pkgname/

# Build package
dpkg -b $pkgname

# Cleanup
rm -rv $pkgname
