#!/bin/sh

whoami | grep root > /dev/null 2>&1 || exit

rm -rf    /root/rpmbuild/SOURCES/led-ugreen-0.1.tar.gz /root/rpmbuild/SOURCES/led-ugreen-0.1

if [ ! -d /root/rpmbuild/SOURCES/led-ugreen-0.1 ] ; then
    mkdir -p  /root/rpmbuild/SOURCES
    git clone https://github.com/miskcoo/ugreen_dx4600_leds_controller.git /root/rpmbuild/SOURCES/led-ugreen-0.1

    sed -i -e s/grep\ i2c-dev/grep\ i2c_dev/g -e s/grep\ led-ugreen/grep\ led_ugreen/g /root/rpmbuild/SOURCES/led-ugreen-0.1/scripts/ugreen-probe-leds

    cp /root/rpmbuild/SOURCES/led-ugreen-0.1/kmod/led-ugreen.c /root/rpmbuild/SOURCES/led-ugreen-0.1/led-ugreen.c
    cp /root/rpmbuild/SOURCES/led-ugreen-0.1/kmod/led-ugreen.h /root/rpmbuild/SOURCES/led-ugreen-0.1/led-ugreen.h

    
    cp ./Makefile                /root/rpmbuild/SOURCES/led-ugreen-0.1/Makefile
    cat >                        /root/rpmbuild/SOURCES/led-ugreen-0.1/ugreen-led.conf <<EOF
i2c-dev
led-ugreen
ledtrig-oneshot
ledtrig-netdev
EOF
    cp ./GPL-v2.0.txt            /root/rpmbuild/SOURCES
fi

if [ ! -f /root/rpmbuild/SOURCES/led-ugreen-0.1.tar.gz ] ; then
    (cd   /root/rpmbuild/SOURCES && tar -czf led-ugreen-0.1.tar.gz led-ugreen-0.1)
fi

# The spec file is a modified version of:
# https://elrepo.org/linux/testing/el9/SRPMS/kmod-hello-1-0.0-13.el9_2.elrepo.src.rpm

rpmbuild -ba kmod-led-ugreen.spec

cp -up /root/rpmbuild/SRPMS/kmod-led-ugreen-0.1-*.el9.src.rpm /root/rpmbuild/RPMS/x86_64/kmod-led-ugreen-0.1-*.el9.x86_64.rpm .

echo to install:
echo rpm -i kmod-led-ugreen-0.1-\*.el9.x86_64.rpm
echo systemctl start  ugreen-diskiomon.service
echo ls /sys/class/leds
echo Make sure you can see disk1, netdev, power, etc.
echo systemctl enable ugreen-diskiomon.service
echo
echo to uninstall:
echo systemctl stop    ugreen-diskiomon.service
echo systemctl disable ugreen-diskiomon.service
echo rpm -e kmod-led-ugreen
