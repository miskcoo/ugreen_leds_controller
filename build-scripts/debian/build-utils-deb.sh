#!/usr/bin/bash

set -e
set -x

pkgver="0.2"
pkgname="led-ugreen-utils"
drivername="led-ugreen"

mkdir -p $pkgname/DEBIAN

cat <<EOF > $pkgname/DEBIAN/control 
Package: $pkgname
Version: $pkgver
Architecture: amd64
Maintainer: Yuhao Zhou <miskcoo@gmail.com>
Depends: dmidecode, smartmontools
Homepage: https://github.com/miskcoo/ugreen_dx4600_leds_controller
Description: UGREEN NAS LED tools
 A reverse-engineered LED tools of UGREEN NAS.
EOF


# scripts
mkdir -p $pkgname/usr/bin/

script_files=(ugreen-probe-leds ugreen-netdevmon ugreen-diskiomon)

for f in ${script_files[@]}; do
    cp scripts/$f $pkgname/usr/bin/
    chmod +x $pkgname/usr/bin/$f
done

# systemd file
mkdir -p $pkgname/etc/systemd/system
cp scripts/*.service $pkgname/etc/systemd/system/
# cp scripts/ugreen-ledmon@.service $pkgname/etc/systemd/system/
#

# example config file
cp scripts/ugreen-leds.conf $pkgname/etc/ugreen-leds.example.conf

# change to root 
chown -R root:root $pkgname/

# cli 
cd cli && make  -j 4
cd ..
cp cli/ugreen_leds_cli $pkgname/usr/bin
# cp cli/ugreen_daemon $pkgname/usr/bin
chmod +x $pkgname/usr/bin

dpkg -b $pkgname

rm -rv $pkgname

