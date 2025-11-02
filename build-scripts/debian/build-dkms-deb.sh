#!/usr/bin/env bash

set -e
set -x

# Build using dpkg-buildpackage from kmod/ directory
cd kmod

# Install build dependencies (in case not in Docker)
# mk-build-deps -i -r -t "apt-get -y" debian/control 2>/dev/null || true

# Build binary package only (no source changes)
dpkg-buildpackage -b -us -uc -tc

# Move package up to root directory for collection
mv ../led-ugreen-dkms_*.deb ../led-ugreen-dkms.deb

echo "DKMS package built successfully: led-ugreen-dkms.deb"
