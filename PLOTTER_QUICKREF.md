# Quick Reference - macOS Serial Plotter Setup

**Problem**: SerialPlot doesn't have macOS version  
**Solution**: Use `serial_plotter.py` - native Python alternative

---

## ⚡ Get Started in 2 Minutes

### 1. Install Dependencies (First Time Only)
```bash
pip3 install pyserial matplotlib
```

### 2. Flash ESP32-C6
```bash
cd /Users/zhaojia/github/ads1261
make flash-monitor
# Wait for "Ground Reaction Force Measurement Starting..."
# Press Ctrl+C to exit
```

### 3. Start Plotter (New Terminal)
```bash
make plot
# Or: python3 serial_plotter.py --port /dev/ttyUSB0
```

**Result**: Real-time 4-channel force plot appears automatically

---

## 🎯 Common Tasks

| Task | Command |
|------|---------|
| Build & flash | `make flash-monitor` |
| View serial output | `make monitor` |
| Start plotter | `make plot` |
| Calibrate | `make calibrate` |
| Use different port | `make plot PORT=/dev/ttyACM0` |
| Custom baud rate | `make plot BAUD=115200` |

---

## 📊 What You See

The plotter window shows:
- **4 subplots** - One for each force channel
- **Real-time plots** - Updated every 200ms
- **Color coding** - Red (Ch1), Blue (Ch2), Green (Ch3), Purple (Ch4)
- **Statistics** - Min/max/average for each channel
- **Clear button** - Reset and restart

---

## 🔧 Troubleshooting

| Issue | Solution |
|-------|----------|
| "Failed to connect" | Wrong port - use `ls /dev/tty.*` to find it |
| "ModuleNotFoundError: pyserial" | Run `pip3 install pyserial` |
| "ModuleNotFoundError: matplotlib" | Run `pip3 install matplotlib` |
| Plots not updating | Check ESP32-C6 is running, verify port |
| Wrong port showing data | Stop plotter, check `make monitor` works |

---

## 📍 Finding Your Port on macOS

```bash
# List all serial ports
ls /dev/tty.* /dev/cu.*

# Connected board usually shows:
# /dev/tty.usbserial-XXXXX      (USB serial adapter)
# /dev/tty.SLAB_USBtoUART        (CP2102 chip)
# /dev/tty.Bluetooth-Incoming-Port  (NOT this one)

# Use the port with USB or serialized name:
make plot PORT=/dev/tty.usbserial-1410
```

---

## 💡 Tips

1. **First Time Setup**: Install dependencies once with `pip3 install pyserial matplotlib`
2. **Multiple Ports**: If you have multiple USB devices, use `ls /dev/tty.*` to find the right one
3. **Keep Running**: Plotter stays open until you close the window
4. **Data Export**: Serial output can be piped to file: `idf.py -p /dev/ttyUSB0 monitor > data.log`
5. **CSV Export**: Edit `main.c` line 28 to use CSV format for automated analysis

---

## 🚀 Full Workflow

```bash
# 1. Terminal 1: Build and flash
make flash-monitor

# 2. Terminal 2: Start plotter when ESP32 is ready
make plot

# 3. Watch 4-channel force plot in real-time
# 4. Apply test weights and observe response
# 5. Close plotter window when done
```

---

## 📚 More Info

- **Full Guide**: [SERIAL_PLOTTER_GUIDE.md](SERIAL_PLOTTER_GUIDE.md)
- **Project Index**: [INDEX.md](INDEX.md)
- **Deployment**: [DEPLOYMENT_CHECKLIST.md](DEPLOYMENT_CHECKLIST.md)

---

**Works on macOS, Linux, and Windows!** 🎉
