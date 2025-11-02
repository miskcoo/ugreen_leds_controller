#!/usr/bin/env bash

set -e
set -x

# Default Debian version
DEBIAN_VERSION=${DEBIAN_VERSION:-bookworm}

# Create build directory
if [[ ! -d ./build ]]; then
    mkdir -p build
fi

# Get absolute path to repository root (two levels up from this script)
REPO_ROOT=$(cd $(dirname $0)/../.. && pwd)

# Build the Docker image with specified Debian version
docker build \
    --platform linux/amd64 \
    --build-arg DEBIAN_VERSION=${DEBIAN_VERSION} \
    --tag ugreen-build:${DEBIAN_VERSION} \
    .

# Run the build - mount the entire repository as read-only, and build dir as read-write
docker run \
    --rm \
    --platform linux/amd64 \
    --mount type=bind,source=${REPO_ROOT},target=/source,readonly \
    --mount type=bind,source=$(pwd)/build,target=/output \
    ugreen-build:${DEBIAN_VERSION} \
    bash -c "cp -r /source /build/ugreen_leds_controller && cd /build/ugreen_leds_controller && bash build-scripts/debian/build-all.sh --local $@ && mv *.deb /output/ 2>/dev/null || true"

echo "Build complete. Packages available in: ./build/"
ls -lh ./build/*.deb 2>/dev/null || echo "No packages built yet"
