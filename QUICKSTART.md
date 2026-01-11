# Quick Start Guide - GRF Force Platform

**Status**: Software Complete ✅ | Ready for Hardware Deployment

---

## What You Have

A complete ESP32-C6 + ADS1261 software platform for measuring ground reaction forces with 4 strain gauge loadcells at **1000+ Hz per channel**.

## What You Need to Do

### 1. Hardware Assembly (30 minutes)
Follow [HARDWARE_SETUP.md](HARDWARE_SETUP.md):
- Wire ESP32-C6 to ADS1261 via SPI (6 wires: MOSI, MISO, CLK, CS, DRDY, GND)
- Connect 4 loadcells to differential input pairs (AIN0-1, AIN2-3, AIN4-5, AIN6-7)
- Add RC filters (1kΩ + 100nF) on each input
- Install power supply decoupling capacitors

### 2. Build & Flash (5 minutes)
```bash
cd /Users/zhaojia/github/ads1261
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 3. Calibration (10 minutes)
**Zero Calibration:**
- Remove all load from loadcells
- Record the raw ADC value shown in serial output (labeled "raw=")

**Full-Scale Calibration:**
- Apply known weight (e.g., 100 N reference load)
- Record the new raw ADC value
- Calculate: `FORCE_SENSITIVITY = 100 / (normalized_adc_full_scale - normalized_adc_zero)`

### 4. Update Code & Redeploy
Edit `main/main.c`:
```c
static const float ZERO_OFFSET_RAW = <your_zero_value>;
static const float FORCE_SENSITIVITY = <your_calculated_value>;
```
Rebuild and flash.

### 5. Verify (Testing)
- Watch serial output for 4 channels of force readings in Newtons
- Test linearity with intermediate weights
- Confirm per-channel rate shows ~1000-1200 Hz

---

## Key System Specs

| Parameter | Value |
|-----------|-------|
| **Sampling Rate** | 40 kSPS (1000-1200 Hz per channel) |
| **Resolution** | 24-bit (high precision) |
| **Latency** | ~130 µs per sample |
| **Channels** | 4 differential loadcells |
| **Measurement Mode** | Ratiometric (no reference voltage value needed) |

---

## File Reference

| File | Purpose |
|------|---------|
| [readme.md](readme.md) | Project overview |
| [HARDWARE_SETUP.md](HARDWARE_SETUP.md) | Wiring & pin assignment guide |
| [DATA_RATE_ANALYSIS.md](DATA_RATE_ANALYSIS.md) | Data rate calculations |
| [LATENCY_ANALYSIS_GRF.md](LATENCY_ANALYSIS_GRF.md) | Detailed latency analysis |
| [PROJECT_STATUS.md](PROJECT_STATUS.md) | Complete project status report |
| `main/main.c` | Application code (edit calibration constants here) |
| `components/ads1261/` | ADS1261 driver |

---

## Common Issues

**Problem**: ADC reads zeros
- **Fix**: Check SPI wiring (MOSI, MISO, CLK pins)

**Problem**: Noisy readings  
- **Fix**: Add RC filters, improve power supply grounding

**Problem**: DRDY timeout error
- **Fix**: Verify ADS1261_DR_40 is set in main.c, check DRDY pin connection

---

## What's Different from Standard Load Cells?

**Ratiometric measurement** means:
- ✅ You DON'T need to know the exact reference voltage
- ✅ Voltage variations automatically cancel out
- ✅ Only need to calibrate zero offset and sensitivity
- ✅ More accurate and simpler than traditional approaches

---

## Next Steps

1. **Order components** (if not already purchased)
2. **Assemble hardware** per HARDWARE_SETUP.md
3. **Run calibration** procedure with known reference loads
4. **Update calibration constants** in main/main.c
5. **Deploy and test** on force platform

**Estimated Total Time**: 1-2 hours for complete setup and first measurement

See [PROJECT_STATUS.md](PROJECT_STATUS.md) for comprehensive documentation.
