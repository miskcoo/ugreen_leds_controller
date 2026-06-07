set -x
set -euo pipefail

# $1 is the TrueNAS version path, e.g. "TrueNAS-SCALE-Goldeye/25.10.4".
version_path="$1"
# Version number is the part after the slash, e.g. "25.10.4".
version="${version_path##*/}"

url_prefix="https://download.truenas.com/${version_path}/"
update_file="TrueNAS-SCALE-${version}.update"

mkdir truenas_working
cd truenas_working

# Fetch the .update squashfs image (and its signature). It is published for
# every release, including ones without a packages/Packages.gz index (e.g.
# 25.10.4); the kernel headers are extracted from it below.
if ! wget -nv "${url_prefix}${update_file}"; then
    cd ..
    rm -rf truenas_working
    exit 0
fi
wget -nv "${url_prefix}${update_file}.sig"

# Build linux-headers-truenas-production-amd64.deb straight from the .update.
bash /extract-truenas-headers.sh "${update_file}" .

mkdir tmp
dpkg-deb -R linux-headers-truenas-production-amd64_*.deb tmp

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
mkdir -p "$1"
cp truenas_working/ugreen_dx4600_leds_controller/kmod/*.ko "$1"
rm -fr truenas_working
