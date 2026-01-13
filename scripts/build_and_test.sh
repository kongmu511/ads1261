#!/bin/bash

# Script to build, flash, and test the ESP32-C6 GRF Force Platform project
# Automatically detects the ESP32 port and performs build/flash/test cycle

set -e  # Exit on any error

echo "🔍 Detecting ESP32-C6 port..."

# Function to detect ESP32 port based on macOS device naming patterns
detect_port() {
    local ports=($(ls /dev/tty.usbmodem* 2>/dev/null || ls /dev/cu.SLAB_USBtoUART 2>/dev/null || ls /dev/cu.wchusbserial* 2>/dev/null))
    if [ ${#ports[@]} -eq 0 ]; then
        return 1
    elif [ ${#ports[@]} -eq 1 ]; then
        echo "${ports[0]}"
        return 0
    else
        echo "${ports[0]}"  # Return first match if multiple found
        return 0
    fi
}

# Try to detect the port
PORT=$(detect_port)

if [ -z "$PORT" ]; then
    echo "❌ No ESP32-C6 device found!"
    echo "🔌 Please connect your ESP32-C6 development board via USB."
    echo "💡 Common port patterns on macOS:"
    echo "   - /dev/tty.usbmodem* (Silicon Labs CP210x USB to UART Bridge)"
    echo "   - /dev/cu.SLAB_USBtoUART (SiLabs CP210x)"
    echo "   - /dev/cu.wchusbserial* (CH340/CH341)"
    echo ""
    echo "📝 You can also manually specify the port:"
    echo "   make flash PORT=/dev/tty.usbmodemXXXXX"
    exit 1
fi

echo "✅ Detected ESP32-C6 on port: $PORT"

# Check if the port is writable
if [ ! -w "$PORT" ]; then
    echo "❌ Insufficient permissions for $PORT"
    echo "💡 Try adding your user to dialout group or run with sudo"
    exit 1
fi

echo ""
echo "🔧 Building project..."
make build

echo ""
echo "🚀 Flashing to ESP32-C6 ($PORT)..."
make flash PORT="$PORT"

echo ""
echo "📊 Starting monitor..."
echo "💡 Press Ctrl+] to exit monitor"
make monitor PORT="$PORT"