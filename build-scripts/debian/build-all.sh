#!/usr/bin/env bash

set -e
set -x

# Parse arguments for selective builds
BUILD_DKMS=true
BUILD_CLI=true
BUILD_UTILS=true
BUILD_SYSTEMD=true
LOCAL_BUILD=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --local)
            LOCAL_BUILD=true
            shift
            ;;
        --only-dkms)
            BUILD_CLI=false
            BUILD_UTILS=false
            BUILD_SYSTEMD=false
            shift
            ;;
        --only-cli)
            BUILD_DKMS=false
            BUILD_UTILS=false
            BUILD_SYSTEMD=false
            shift
            ;;
        --only-utils)
            BUILD_DKMS=false
            BUILD_CLI=false
            BUILD_SYSTEMD=false
            shift
            ;;
        --only-systemd)
            BUILD_DKMS=false
            BUILD_CLI=false
            BUILD_UTILS=false
            shift
            ;;
        --skip-dkms)
            BUILD_DKMS=false
            shift
            ;;
        --skip-cli)
            BUILD_CLI=false
            shift
            ;;
        --skip-utils)
            BUILD_UTILS=false
            shift
            ;;
        --skip-systemd)
            BUILD_SYSTEMD=false
            shift
            ;;
        *)
            GIT_REF=$1
            shift
            ;;
    esac
done

# Clone repository only if not building from local sources
if [ "$LOCAL_BUILD" = false ]; then
    git clone https://github.com/miskcoo/ugreen_leds_controller.git
    cd ugreen_leds_controller

    # Checkout specific ref if provided
    if [ ! -z "$GIT_REF" ]; then
        git checkout $GIT_REF
    fi
fi

# Build packages in dependency order
if [ "$BUILD_DKMS" = true ]; then
    echo "Building led-ugreen-dkms..."
    # Clean any previous build artifacts
    (cd kmod && debian/rules clean 2>/dev/null || true)
    bash build-scripts/debian/build-dkms-deb.sh
    dpkg-name led-ugreen-dkms.deb
fi

if [ "$BUILD_CLI" = true ]; then
    echo "Building led-ugreen-cli..."
    bash build-scripts/debian/build-cli-deb.sh
    dpkg-name led-ugreen-cli.deb
fi

if [ "$BUILD_UTILS" = true ]; then
    echo "Building led-ugreen-utils..."
    bash build-scripts/debian/build-utils-deb.sh
    dpkg-name led-ugreen-utils.deb
fi

if [ "$BUILD_SYSTEMD" = true ]; then
    echo "Building led-ugreen-systemd..."
    bash build-scripts/debian/build-systemd-deb.sh
    dpkg-name led-ugreen-systemd.deb
fi

# Move all packages to output (only for non-local builds)
if [ "$LOCAL_BUILD" = false ]; then
    mv *.deb ..
fi

echo "Build completed successfully!"
