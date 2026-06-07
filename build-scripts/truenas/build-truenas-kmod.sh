set -x
set -euo pipefail

# $1 is the TrueNAS version path, e.g. "TrueNAS-SCALE-Goldeye/25.10.4".
version_path="${1:?missing TrueNAS version path}"
# Version number is the part after the slash, e.g. "25.10.4".
version="${version_path##*/}"

url_prefix="https://download.truenas.com/${version_path}/"
update_file="TrueNAS-SCALE-${version}.update"
output_root="$(pwd)"
workdir="$(mktemp -d)"

cleanup() {
    rm -rf "$workdir"
}
trap cleanup EXIT

cd "$workdir"

# Fetch the .update squashfs image (and its signature). It is published for
# every release, including ones without a packages/Packages.gz index (e.g.
# 25.10.4); the kernel headers are extracted from it below.
if ! wget -nv "${url_prefix}${update_file}"; then
    exit 0
fi
wget -nv "${url_prefix}${update_file}.sig"

# Build linux-headers-truenas-production-amd64.deb straight from the .update.
extract_log="${workdir}/extract-truenas-headers.log"
if ! bash /extract-truenas-headers.sh "${update_file}" . 2>&1 | tee "${extract_log}"; then
    if grep -q "Error: header files missing" "${extract_log}"; then
        headers_missing_file="${output_root}/headers-missing.txt"
        touch "${headers_missing_file}"
        if ! grep -Fxq -- "${version_path}" "${headers_missing_file}"; then
            printf '%s\n' "${version_path}" >> "${headers_missing_file}"
        fi
        echo "Header files missing for ${version_path}; skipping because headers match a previous release."
        exit 0
    fi

    exit 1
fi

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

mkdir -p "${output_root}/${version_path}"
cp ugreen_dx4600_leds_controller/kmod/*.ko "${output_root}/${version_path}"
