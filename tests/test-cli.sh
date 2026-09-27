#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror -Icli tests/dx4600.cpp cli/i2c.cpp \
  -Wl,--wrap=open,--wrap=close,--wrap=ioctl,--wrap=usleep -o "$build_dir/dx4600-test"
"$build_dir/dx4600-test"
make -C cli CFLAGS='-std=c++17 -I. -O2 -Wall -Wextra -Werror -static'
sudo python3 tests/cli_lock.py cli/ugreen_leds_cli
