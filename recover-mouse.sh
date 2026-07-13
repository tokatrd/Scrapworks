#!/bin/bash
# Recover A4Tech X7 F3 V-Track mouse (09da:9066) from USB reset loop
# Usage: bash recover-mouse.sh

set -e

MOUSE_PATH="/sys/bus/usb/devices/1-9"

echo "=== X7 Mouse Recovery ==="

# Check if the stale device entry exists
if [ -d "$MOUSE_PATH" ]; then
    echo "Removing stale device entry..."
    echo "1-9" | sudo tee /sys/bus/usb/drivers/usb/unbind 2>/dev/null || true
    sleep 1
    echo 1 | sudo tee "$MOUSE_PATH/remove" 2>/dev/null || true
    sleep 2
    if [ -d "$MOUSE_PATH" ]; then
        echo "Still present — rebinding..."
        echo "1-9" | sudo tee /sys/bus/usb/drivers/usb/bind 2>/dev/null || true
    else
        echo "Device entry removed OK"
    fi
else
    echo "No stale device entry found (clean state)"
fi

echo ""
echo "=== Scanning for mouse ==="
lsusb | grep -i "09da\|a4tech\|f3 v-track" || echo "Mouse not detected"

echo ""
echo "If mouse still doesn't appear:"
echo "  1. Unplug the mouse completely"
echo "  2. Count to 30 (let caps drain)"
echo "  3. Plug into a USB 2.0 port"
echo "  4. Run this script again"
