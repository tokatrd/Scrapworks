#!/bin/bash
# Force a power-cycle on USB port 9 to recover bricked A4Tech mouse
# Usage: bash force-port-reset.sh

PORT_DISABLE="/sys/bus/usb/devices/1-0:1.0/usb1-port9/disable"
PORT_QUIRKS="/sys/bus/usb/devices/1-0:1.0/usb1-port9/quirks"

echo "=== Port 9 Force Reset ==="

echo "1. Setting port quirks (DELAY_INIT + RESET_MORPHS)..."
echo 0x2400 | sudo tee "$PORT_QUIRKS"
sleep 1

echo "2. Disabling port 9 (power off)..."
sudo sh -c 'echo 1 > /sys/bus/usb/devices/1-0:1.0/usb1-port9/disable'
sleep 3

echo "3. Re-enabling port 9..."
sudo sh -c 'echo 0 > /sys/bus/usb/devices/1-0:1.0/usb1-port9/disable'
sleep 3

echo ""
echo "4. Port state:"
cat /sys/bus/usb/devices/1-0:1.0/usb1-port9/state

echo ""
echo "5. Scanning USB bus..."
lsusb | grep -i "09da\|a4tech\|f3" || echo "   Mouse NOT detected"

echo ""
echo "6. Checking dmesg..."
dmesg | grep -i "usb.*9\|09da\|a4tech\|9066" | tail -5 || echo "   No relevant events"
