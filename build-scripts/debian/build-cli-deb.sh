#!/usr/bin/env bash

set -e
set -x

pkgver="0.3"
pkgname="led-ugreen-cli"

mkdir -p $pkgname/DEBIAN

cat <<EOF > $pkgname/DEBIAN/control
Package: $pkgname
Version: $pkgver
Architecture: amd64
Maintainer: Yuhao Zhou <miskcoo@gmail.com>
Depends: libc6, libstdc++6
Conflicts: led-ugreen-utils (<< 0.3)
Homepage: https://github.com/miskcoo/ugreen_leds_controller
Description: UGREEN NAS LED command-line interface tool
 Command-line utility for direct control of UGREEN NAS LED lights.
 This tool allows manual manipulation of LED states, colors, brightness,
 and blink modes via I2C communication.
 .
 Note: This tool conflicts with the kernel module. Unload led-ugreen
 module before using: rmmod led-ugreen
EOF

# Build CLI tool
cd cli
make clean 2>/dev/null || true  # Clean if target exists, ignore if not
make -j$(nproc) || make -j4     # Try nproc, fallback to -j4
cd ..

# Install CLI binary
mkdir -p $pkgname/usr/bin
cp cli/ugreen_leds_cli $pkgname/usr/bin/
chmod +x $pkgname/usr/bin/ugreen_leds_cli

# Create man page directory
mkdir -p $pkgname/usr/share/man/man1

# Create basic man page
cat > $pkgname/usr/share/man/man1/ugreen_leds_cli.1 << 'MANEOF'
.TH UGREEN_LEDS_CLI 1 "2025" "ugreen_leds_cli 0.3" "User Commands"
.SH NAME
ugreen_leds_cli \- Control UGREEN NAS LED lights
.SH SYNOPSIS
.B ugreen_leds_cli
[\fILED-NAME\fR...] [\fIOPTIONS\fR]
.SH DESCRIPTION
Command-line tool for controlling LED lights on UGREEN NAS devices via I2C.
.PP
LED names: power, netdev, disk1-disk8, all
.PP
Options:
.TP
.B \-on
Turn on LED(s)
.TP
.B \-off
Turn off LED(s)
.TP
.B \-blink T_ON T_OFF
Set blink mode (milliseconds)
.TP
.B \-breath T_ON T_OFF
Set breath mode (milliseconds)
.TP
.B \-color R G B
Set RGB color (0-255 each)
.TP
.B \-brightness N
Set brightness (0-255)
.TP
.B \-status
Display current LED status
.SH EXAMPLES
.TP
ugreen_leds_cli all \-on
Turn on all LEDs
.TP
ugreen_leds_cli power \-color 255 0 0 \-brightness 128
Set power LED to red at half brightness
.SH NOTES
Requires root permissions. Conflicts with led-ugreen kernel module.
.SH SEE ALSO
https://github.com/miskcoo/ugreen_leds_controller
MANEOF

gzip -9 $pkgname/usr/share/man/man1/ugreen_leds_cli.1

# Set ownership
chown -R root:root $pkgname/

# Build package
dpkg -b $pkgname

# Cleanup
rm -rv $pkgname
