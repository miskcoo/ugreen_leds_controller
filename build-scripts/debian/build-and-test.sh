#!/usr/bin/env bash

set -e

# Build all packages and run installation test in Docker

DEBIAN_VERSION=${DEBIAN_VERSION:-bookworm}

echo "Building packages for Debian $DEBIAN_VERSION..."
./docker-run.sh

echo ""
echo "Running installation test..."

docker run \
    --rm \
    --platform linux/amd64 \
    --privileged \
    -v $(pwd)/build:/packages \
    -v $(pwd)/test-install.sh:/test-install.sh \
    debian:${DEBIAN_VERSION} \
    bash -c "cd /packages && bash /test-install.sh"

echo ""
echo "Test completed successfully!"
