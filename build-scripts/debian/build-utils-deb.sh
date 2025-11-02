#!/usr/bin/env bash

set -e
set -x

pkgver="0.3"
pkgname="led-ugreen-utils"
drivername="led-ugreen"

mkdir -p $pkgname/DEBIAN

cat <<EOF > $pkgname/DEBIAN/control
Package: $pkgname
Version: $pkgver
Architecture: amd64
Maintainer: Yuhao Zhou <miskcoo@gmail.com>
Depends: led-ugreen-dkms (>= 0.3), dmidecode, smartmontools, i2c-tools
Recommends: led-ugreen-systemd
Homepage: https://github.com/miskcoo/ugreen_leds_controller
Description: UGREEN NAS LED monitoring utilities
 Monitoring scripts and utilities for UGREEN NAS LED control.
 Includes disk activity monitoring, network activity monitoring,
 SMART health checking, and power LED control.
EOF

cat <<EOF > $pkgname/DEBIAN/postinst
#!/usr/bin/bash

set -e

# Install default configuration if not exists
if [ ! -f /etc/ugreen-leds.conf ]; then
    cp /etc/ugreen-leds.conf.default /etc/ugreen-leds.conf
    echo "Installed default configuration to /etc/ugreen-leds.conf"
else
    echo "Preserving existing /etc/ugreen-leds.conf"
fi

echo "UGREEN LED utilities installed successfully."
echo "To configure services, install led-ugreen-systemd package."

exit 0
EOF

cat <<EOF > $pkgname/DEBIAN/conffiles
/etc/ugreen-leds.conf
EOF

chmod +x $pkgname/DEBIAN/postinst

# Install monitoring scripts
mkdir -p $pkgname/usr/bin/

script_files=(ugreen-probe-leds ugreen-diskiomon ugreen-power-led ugreen-netdevmon-multi ugreen-detect-disks ugreen-detect-network)
for f in ${script_files[@]}; do
    cp scripts/$f $pkgname/usr/bin/
    chmod +x $pkgname/usr/bin/$f
done

# Install configuration files
mkdir -p $pkgname/etc
cp scripts/ugreen-leds.conf $pkgname/etc/ugreen-leds.conf.default
cp scripts/ugreen-leds.conf $pkgname/etc/ugreen-leds.conf

# Compile disk activities monitor
g++ -std=c++17 -O2 scripts/blink-disk.cpp -o ugreen-blink-disk
cp ugreen-blink-disk $pkgname/usr/bin/
chmod +x $pkgname/usr/bin/ugreen-blink-disk

# Compile disk standby checker (FIXED - now included)
g++ -std=c++17 -O2 scripts/check-standby.cpp -o ugreen-check-standby
cp ugreen-check-standby $pkgname/usr/bin/
chmod +x $pkgname/usr/bin/ugreen-check-standby

# Set ownership
chown -R root:root $pkgname/

# Build package
dpkg -b $pkgname

# Cleanup
rm -rv $pkgname

