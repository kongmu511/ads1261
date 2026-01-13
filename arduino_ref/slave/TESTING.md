# Testing Tools for ADS1261 4-Channel Data

Two Python scripts for monitoring and displaying real-time serial data from the ESP32-C6:

## 1. serial_monitor.py (Simple Terminal Display)

### Features
- Simple console output with real-time values
- Auto-detects ESP32 serial port on macOS
- Shows timestamp and all 4 channel values
- Frame counter and error handling

### Usage
```bash
python3 serial_monitor.py
```

Or specify port and baudrate:
```bash
python3 serial_monitor.py --port /dev/tty.usbserial-xxx --baud 115200
```

### Output Example
```
=== ADS1261 4-Channel Serial Monitor ===

Connected to /dev/tty.usbserial-1410 at 115200 baud
Waiting for data (0xAA header)...

Time         CH1          CH2          CH3          CH4
------------------------------------------------------------
10:45:23.123 0.1234       0.5678       0.9012       0.3456
10:45:23.124 0.1235       0.5679       0.9013       0.3457
10:45:23.125 0.1236       0.5680       0.9014       0.3458
```

## 2. serial_plot.py (Live Real-time Plot)

### Features
- Beautiful real-time matplotlib visualization
- Two subplots:
  - Time-series plot of all 4 channels
  - Bar chart of current values
- Background thread for non-blocking reading
- Console output every 10 frames
- Auto-scaling axes

### Installation
```bash
# Install matplotlib if not already installed
pip3 install matplotlib pyserial
```

### Usage
```bash
python3 serial_plot.py
```

## Data Format

The device sends data in this format:
- **Header**: 0xAA (1 byte)
- **CH1**: 4-byte float (IEEE 754)
- **CH2**: 4-byte float (IEEE 754)
- **CH3**: 4-byte float (IEEE 754)
- **CH4**: 4-byte float (IEEE 754)

**Total: 17 bytes per frame**

## Troubleshooting

### Port not found
- List available ports:
  ```bash
  ls /dev/tty.*
  ```
- Common ESP32 patterns: `/dev/tty.usbserial-*`, `/dev/tty.SLAB*`, `/dev/tty.wchusbserial*`

### Permission denied
```bash
sudo chmod 666 /dev/tty.usbserial-xxx
```

### No data received
- Check serial connection and baudrate (default: 115200)
- Verify ESP32 is in NORMAL_MODE (not UPDATE_MODE or OTA_MODE)
- Check USB cable quality

### matplotlib errors
Install with: `pip3 install matplotlib`

## Tips

1. **For quick testing**: Use `serial_monitor.py` for simple terminal view
2. **For analysis**: Use `serial_plot.py` to visualize trends
3. **Copy-paste data**: Terminal output can be easily logged and analyzed
4. **Adjust refresh rate**: Edit `interval=100` in `serial_plot.py` (milliseconds)

## Integration with Arduino IDE Serial Monitor

You can also use Arduino IDE's built-in Serial Monitor, but these scripts provide better visualization for multi-channel data.
