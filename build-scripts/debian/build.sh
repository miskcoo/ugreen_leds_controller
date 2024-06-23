#!/usr/bin/bash

git clone https://github.com/miskcoo/ugreen_dx4600_leds_controller.git 

cp build-dkms-deb.sh ugreen_dx4600_leds_controller
cp build-utils-deb.sh ugreen_dx4600_leds_controller

cd ugreen_dx4600_leds_controller 
bash build-dkms-deb.sh
bash build-utils-deb.sh

dpkg-name led-ugreen-dkms.deb
dpkg-name led-ugreen-utils.deb
mv *.deb ../
