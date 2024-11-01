#!/usr/bin/bash

git clone https://github.com/miskcoo/ugreen_leds_controller.git 
cd ugreen_leds_controller 

if [[ $1 ]]; then 
    git checkout $1
fi

bash build-scripts/debian/build-dkms-deb.sh
bash build-scripts/debian/build-utils-deb.sh
dpkg-name led-ugreen-dkms.deb
dpkg-name led-ugreen-utils.deb
mv *.deb ..
