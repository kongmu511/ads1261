#!/bin/bash
# build_and_flash.sh - ESP32-C6 GRF Platform Build & Flash Script (macOS)

set -e

# Configuration
IDF_PATH=~/idf/v5.5.2/esp-idf
IDF_TOOLS_PATH=~/idf_tool
IDF_TARGET=esp32c6
PORT=${1:-/dev/tty.usbmodem1301}
BAUD=921600

echo "╔════════════════════════════════════════╗"
echo "║  ESP32-C6 GRF Platform Build & Flash  ║"
echo "║           (macOS Version)             ║"
echo "╚════════════════════════════════════════╝"
echo ""

# Check if the device exists
if [ ! -e "$PORT" ]; then
    echo "❌ Device $PORT not found!"
    echo "🔌 Please connect your ESP32-C6 development board via USB."
    echo "💡 Available serial devices:"
    ls /dev/tty.usbmodem* 2>/dev/null || echo "   No USB modems found"
    exit 1
fi

# Check if the port is writable
if [ ! -w "$PORT" ]; then
    echo "❌ Insufficient permissions for $PORT"
    echo "💡 Try running with sudo or adding your user to dialout group"
    exit 1
fi

echo "✅ Found ESP32-C6 on port: $PORT"

# Setup environment
echo ""
echo "🔧 Setting up ESP-IDF environment..."
export IDF_PATH IDF_TOOLS_PATH IDF_TARGET
source $IDF_PATH/export.sh >/dev/null 2>&1
echo "✅ Environment ready"
echo "   IDF_PATH: $IDF_PATH"
echo "   TARGET: $IDF_TARGET"
echo "   PORT: $PORT"
echo ""

# Build
echo "🔨 Building project..."
idf.py build
echo "✅ Build complete"
echo ""

# Check for binary
if [ ! -f build/esp32c6_loadcell_adc.bin ]; then
    echo "❌ Error: Binary not found at build/esp32c6_loadcell_adc.bin"
    exit 1
fi

echo "📦 Binary ready: build/esp32c6_loadcell_adc.bin"
echo "   Size: $(du -h build/esp32c6_loadcell_adc.bin | cut -f1)"
echo ""

# Flash
echo "🔌 Flashing to ESP32-C6 at $PORT ($BAUD baud)..."
echo "   Make sure device is connected!"
echo ""

idf.py -p $PORT -b $BAUD flash

echo ""
echo "✅ Flash complete!"
echo ""

# Monitor
echo "📊 Starting monitor session..."
echo "💡 Press Ctrl+] to exit monitor"
echo ""

idf.py -p $PORT -b $BAUD monitor

echo ""
echo "🎉 Done! Your ESP32-C6 is now running the GRF Force Platform firmware."
echo ""
echo "🔧 Calibration tips:"
echo "   • Tare calibration: > tare 0 300"
echo "   • Weight calibration: > cal 1 100.0"
echo ""
