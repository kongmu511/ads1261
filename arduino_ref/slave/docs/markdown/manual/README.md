# ZPlate - WiFi Force Plate System

High-performance 4-channel force plate with 1000 SPS per channel, WiFi streaming, and real-time visualization.

## Features

- ✅ **1000 SPS per channel** - Hardware timer-based sampling
- ✅ **WiFi AP mode** - ESP32-C6 creates "ZPlate" network (no router needed)
- ✅ **Real-time streaming** - UDP packets to PC Python GUI
- ✅ **Low latency** - Hardware DRDY interrupt + SINC1 filter
- ✅ **CSV recording** - Save timestamped data for analysis

## Hardware

- **MCU**: ESP32-C6 DevKitC-1
- **ADC**: ADS1261 24-bit, 40kSPS data rate
- **Channels**: 4 differential inputs (load cells)
- **DRDY**: GPIO 10 (interrupt-driven)
- **WiFi**: 802.11ax (WiFi 6), Access Point mode

## Quick Start

### 1. Upload Firmware

```powershell
cd d:\github\ads1261\arduino_ref\slave
arduino-cli compile --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc .
arduino-cli upload -p COM10 --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc .
```

### 2. Connect to ZPlate WiFi

- **Network**: ZPlate
- **Password**: zplate2026
- **ESP32 IP**: 192.168.4.1

### 3. Run Python GUI

Install dependencies:
```bash
pip install PyQt5 pyqtgraph numpy
```

Launch GUI:
```bash
cd python
python wifi_receiver_gui.py
```

### 4. Start Streaming

1. Click **Connect** (ESP32 IP: 192.168.4.1)
2. Click **▶ Start Capture**
3. Watch real-time plots
4. Click **■ Stop Capture** to end

## Project Structure

```
slave/
├── slave.ino              # Main WiFi streaming firmware
├── ads1261.h/cpp          # Optimized ADC driver (interrupt-driven)
├── force_plate.h/cpp      # Force plate calculation (unused in WiFi mode)
├── ble_commands.h/cpp     # BLE protocol (unused in WiFi mode)
├── ota.h/cpp              # OTA updates (unused in WiFi mode)
├── tests/                 # Test firmwares
│   ├── test_wifi_stream.ino  # WiFi streaming (same as main)
│   ├── test_speed.ino         # Speed validation
│   ├── test_tare.ino          # Calibration test
│   └── test_ble.ino           # BLE test
├── python/
│   └── wifi_receiver_gui.py   # Real-time visualization GUI
├── docs/
│   └── ZForce_ESP32C6.pdf     # Hardware schematic
└── README.md              # This file
```

## Performance

- **Sample rate**: Exactly 1000 Hz (hardware timer)
- **Per-channel timing**: 221µs (40kSPS + DRDY interrupt)
- **Total cycle**: 885µs for 4 channels
- **Jitter**: ±35µs typical
- **WiFi throughput**: ~20 KB/sec (WiFi capacity: 5-10 Mbps)

## Configuration

Edit `slave.ino` if you want to change WiFi settings:

```cpp
const char* AP_SSID = "ZPlate";          // WiFi network name
const char* AP_PASSWORD = "zplate2026";   // WiFi password
const IPAddress AP_IP(192, 168, 4, 1);   // ESP32 IP
```

## Commands

Send UDP commands to ESP32:

- `'S'` - Start streaming
- `'E'` - End streaming  
- `'T'` - Tare (future feature)

## Data Format

Each UDP packet (20 bytes):
```c
struct DataPacket {
  uint32_t timestamp_us;  // Microseconds
  int32_t ch1;           // Channel 1 ADC value
  int32_t ch2;           // Channel 2 ADC value
  int32_t ch3;           // Channel 3 ADC value
  int32_t ch4;           // Channel 4 ADC value
};
```

## Troubleshooting

### Can't see "ZPlate" WiFi
- Power cycle ESP32 (unplug/replug USB)
- Check serial monitor: should show "✓ WiFi AP started!"
- Try moving closer to ESP32

### GUI shows no data
- Verify PC connected to "ZPlate" WiFi
- Ping ESP32: `ping 192.168.4.1`
- Check firewall allows UDP port 5555

### Sample rate not 1000 Hz
- Verify DRDY pin connected (GPIO 10)
- Check serial output for errors
- Monitor with oscilloscope on GPIO 1 (VERIFY_PIN)

## License

Open source project for force plate applications.
