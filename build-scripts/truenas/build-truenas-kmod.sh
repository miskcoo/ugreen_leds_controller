

url_prefix="https://download.truenas.com/$1/packages/"

mkdir truenas_working
cd truenas_working

wget "${url_prefix}Packages.gz"
gzip -d Packages.gz
deb_name=$(grep linux-headers-truenas-production Packages | grep -Po "(?<=Filename: ./).*deb")
wget "${url_prefix}${deb_name}"
mkdir tmp
dpkg-deb -R ${deb_name} tmp

git clone https://github.com/miskcoo/ugreen_dx4600_leds_controller.git
cd ugreen_dx4600_leds_controller/kmod

cat <<EOF > Makefile
TARGET = led-ugreen
obj-m += led-ugreen.o
ccflags-y := -std=gnu11

all:
	make -C ../../tmp/usr/src/$(ls ../../tmp/usr/src/) M=$(pwd) modules

EOF

make 

cd ../../../
mkdir -p $1
cp truenas_working/ugreen_dx4600_leds_controller/kmod/*.ko $1
rm -fr truenas_working
