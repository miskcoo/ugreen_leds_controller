#!/usr/bin/env bash

set -e

# Test UGREEN LED package installation
# Run this in a Debian VM or container

echo "=== UGREEN LED Package Installation Test ==="
echo ""

# Check prerequisites
echo "Checking prerequisites..."
if ! which i2cdetect > /dev/null; then
    echo "Installing i2c-tools..."
    apt-get update
    apt-get install -y i2c-tools
fi

# Check for I2C device
echo "Checking for I2C device at 0x3a..."
modprobe i2c-dev
i2cdetect -l
if ! i2cdetect -y 1 2>/dev/null | grep -q "3a"; then
    echo "WARNING: I2C device at 0x3a not found. Tests may fail on non-UGREEN hardware."
    echo "Continuing anyway for package verification..."
fi

# Install packages in order
# Note: DKMS package is now Architecture: all (was amd64 in older versions)
echo ""
echo "Installing led-ugreen-dkms (v0.3-1, arch: all)..."
dpkg -i led-ugreen-dkms*.deb || apt-get install -f -y

echo ""
echo "Installing led-ugreen-cli..."
dpkg -i led-ugreen-cli*.deb || apt-get install -f -y

echo ""
echo "Installing led-ugreen-utils..."
dpkg -i led-ugreen-utils*.deb || apt-get install -f -y

echo ""
echo "Installing led-ugreen-systemd (interactive)..."
# Pre-seed debconf for non-interactive testing
debconf-set-selections <<EOF
led-ugreen-systemd led-ugreen-systemd/configure-services boolean true
led-ugreen-systemd led-ugreen-systemd/disk-mapping-method select ata
led-ugreen-systemd led-ugreen-systemd/network-mode select all
led-ugreen-systemd led-ugreen-systemd/enable-services boolean true
EOF

dpkg -i led-ugreen-systemd*.deb || apt-get install -f -y

# Verify installation
echo ""
echo "=== Verification ==="

echo ""
echo "0. Checking installed package metadata..."
if dpkg -l led-ugreen-dkms | grep -q "0.3-1"; then
    echo "  ✓ Package version 0.3-1 installed"
else
    echo "  ✗ Package version mismatch"
    dpkg -l led-ugreen-dkms
fi

# Verify architecture (should be 'all' not 'amd64')
arch=$(dpkg -s led-ugreen-dkms 2>/dev/null | grep "^Architecture:" | awk '{print $2}')
if [ "$arch" = "all" ]; then
    echo "  ✓ Package architecture: all (correct)"
elif [ "$arch" = "amd64" ]; then
    echo "  ⚠ Package architecture: amd64 (old format, should be 'all')"
else
    echo "  ? Package architecture: $arch"
fi

echo ""
echo "1. Checking DKMS module..."
if dkms status | grep "led-ugreen/0.3"; then
    echo "  ✓ DKMS module registered with version 0.3"
else
    echo "  ✗ DKMS module not found with version 0.3"
    dkms status | grep led-ugreen || echo "  No led-ugreen found in DKMS"
fi

echo ""
echo "2. Checking if module can load..."
if lsmod | grep led_ugreen > /dev/null; then
    echo "  ✓ Module already loaded"
else
    # Try to load the module
    if modprobe led-ugreen 2>/dev/null; then
        if lsmod | grep led_ugreen > /dev/null; then
            echo "  ✓ Module loaded successfully"
        else
            echo "  ✗ Module load reported success but not in lsmod"
        fi
    else
        echo "  ⚠ Module not loaded (expected if no UGREEN hardware)"
    fi
fi

echo ""
echo "3. Checking CLI tool..."
which ugreen_leds_cli
ugreen_leds_cli 2>&1 | head -5 || true

echo ""
echo "4. Checking utilities..."
for util in ugreen-probe-leds ugreen-diskiomon ugreen-power-led ugreen-netdevmon-multi ugreen-blink-disk ugreen-check-standby ugreen-detect-disks ugreen-detect-network; do
    if which $util > /dev/null; then
        echo "  ✓ $util"
    else
        echo "  ✗ $util MISSING"
    fi
done

echo ""
echo "5. Checking configuration..."
if [ -f /etc/ugreen-leds.conf ]; then
    echo "  ✓ /etc/ugreen-leds.conf exists"
    grep "^MAPPING_METHOD=" /etc/ugreen-leds.conf
else
    echo "  ✗ /etc/ugreen-leds.conf MISSING"
fi

echo ""
echo "6. Checking module autoload configuration..."
if [ -f /etc/modules-load.d/led-ugreen.conf ]; then
    echo "  ✓ /etc/modules-load.d/led-ugreen.conf exists"
    cat /etc/modules-load.d/led-ugreen.conf
elif [ -f /etc/modules-load.d/ugreen-led.conf ]; then
    echo "  ✓ /etc/modules-load.d/ugreen-led.conf exists (old naming)"
    cat /etc/modules-load.d/ugreen-led.conf
else
    echo "  ✗ Module autoload configuration file MISSING"
    echo "     Expected: /etc/modules-load.d/led-ugreen.conf"
fi

echo ""
echo "7. Checking systemd services..."
for service in ugreen-probe-leds ugreen-diskiomon ugreen-power-led ugreen-netdevmon-multi; do
    status=$(systemctl is-enabled $service 2>&1 || echo "not-enabled")
    echo "  $service: $status"
done

echo ""
echo "8. Checking service status..."
systemctl status 'ugreen-*' --no-pager || true

echo ""
echo "=== Test Summary ==="
echo ""

# Count checks
total_checks=8
passed_checks=0

# Re-verify key items for summary
dpkg -l led-ugreen-dkms | grep -q "0.3-1" && ((passed_checks++)) || echo "✗ Package version check failed"
[ "$(dpkg -s led-ugreen-dkms 2>/dev/null | grep "^Architecture:" | awk '{print $2}')" = "all" ] && ((passed_checks++)) || echo "✗ Architecture check failed"
dkms status | grep -q "led-ugreen/0.3" && ((passed_checks++)) || echo "✗ DKMS version check failed"
lsmod | grep -q led_ugreen && ((passed_checks++)) || echo "⚠ Module not loaded (may be expected)"
which ugreen_leds_cli > /dev/null && ((passed_checks++))
[ -f /etc/ugreen-leds.conf ] && ((passed_checks++))
[ -f /etc/modules-load.d/led-ugreen.conf ] || [ -f /etc/modules-load.d/ugreen-led.conf ] && ((passed_checks++))
systemctl is-enabled ugreen-probe-leds > /dev/null 2>&1 && ((passed_checks++))

echo "Verification: $passed_checks/$total_checks checks passed"
echo ""

if [ $passed_checks -ge 6 ]; then
    echo "✓ Installation appears successful!"
    echo ""
    echo "Key package details:"
    echo "  - Version: 0.3-1"
    echo "  - Architecture: all (source-only, DKMS-built)"
    echo "  - DKMS module: led-ugreen/0.3"
    echo ""
else
    echo "⚠ Some checks failed - review output above"
    echo ""
fi

echo "To test LED functionality (requires UGREEN hardware):"
echo "  - Check disk LEDs during I/O: dd if=/dev/sda of=/dev/null bs=1M count=100"
echo "  - Check network LED during traffic: ping -c 10 8.8.8.8"
echo "  - Test CLI: ugreen_leds_cli all -status"
echo "  - Check DKMS: dkms status led-ugreen"
echo "  - Check kernel module: modinfo led-ugreen"
echo ""
