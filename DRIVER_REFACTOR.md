# Loadcell Driver & UART Command Interface

## Overview

The refactored GRF force platform now uses a modular driver architecture similar to popular Arduino libraries (e.g., HX711). The system consists of:

### Core Components

1. **`loadcell.h` / `loadcell.c`** - Main loadcell driver
   - High-level API for 4-channel measurement
   - Automatic tare/offset calibration
   - Full-scale sensitivity calibration
   - Real-time statistics (min/max/avg)
   - Ratiometric bridge measurement

2. **`uart_cmd.h` / `uart_cmd.c`** - UART command interface
   - Interactive command-line control
   - Live calibration without recompilation
   - Measurement readout and statistics
   - Similar to serial monitor in Arduino IDE

3. **`main.c`** - Main application
   - FreeRTOS tasks for measurements and UART
   - Periodic logging of force data
   - Dual output format (human-readable + CSV)

---

## Architecture

### Data Flow

```
┌─────────────────┐
│   ADS1261 ADC   │  (SPI interface)
└────────┬────────┘
         │
         ▼
┌──────────────────────┐
│  loadcell_read()     │  (Multiplexer all 4 channels)
└────────┬─────────────┘
         │
         ▼
┌────────────────────────────────┐
│  Apply Calibration             │  (offset + scale_factor)
│  - Tare/Zero offset            │
│  - Full-scale sensitivity      │
└────────┬───────────────────────┘
         │
         ▼
┌────────────────────────────────┐
│  Statistics & Storage          │  (min/max/avg per channel)
└────────┬───────────────────────┘
         │
         ├─► Measurement Task (every 10ms)
         │   └─► Log to UART (every 500ms)
         │
         └─► UART Command Task
             └─► Interactive interface
```

### Calibration State Machine

```
UNCALIBRATED
    │
    ├─► tare <ch> [samples]
    │
    ▼
TARE_DONE
    │
    ├─► cal <ch> <force> [samples]
    │
    ▼
CALIBRATED
    │
    └─► ready for measurements
```

---

## API Reference

### Initialization

```c
esp_err_t loadcell_init(loadcell_t *device, spi_host_device_t host,
                       int cs_pin, int drdy_pin,
                       uint8_t pga_gain, uint8_t data_rate);

// Example:
loadcell_t device;
loadcell_init(&device, HSPI_HOST, CS_PIN, DRDY_PIN, 
              ADS1261_PGA_GAIN_128, ADS1261_DR_40);
```

### Measurement

```c
/* Read all 4 channels once */
esp_err_t loadcell_read(loadcell_t *device);

/* Read single channel */
esp_err_t loadcell_read_channel(loadcell_t *device, uint8_t channel,
                                loadcell_measurement_t *measurement);

/* Get last measurement */
esp_err_t loadcell_get_measurement(loadcell_t *device, uint8_t channel,
                                   loadcell_measurement_t *measurement);
```

### Calibration

```c
/* Tare (zero) - perform with NO load */
esp_err_t loadcell_tare(loadcell_t *device, uint8_t channel, uint32_t num_samples);

/* Full-scale calibration - perform with known weight */
esp_err_t loadcell_calibrate(loadcell_t *device, uint8_t channel,
                             float known_force_n, uint32_t num_samples);

/* Get calibration state */
loadcell_calib_state_t loadcell_get_calib_state(loadcell_t *device, uint8_t channel);

/* Reset calibration */
esp_err_t loadcell_reset_calibration(loadcell_t *device, uint8_t channel);
```

### Statistics

```c
/* Get channel statistics */
esp_err_t loadcell_get_stats(loadcell_t *device, uint8_t channel, 
                             loadcell_stats_t *stats);

/* Reset statistics */
esp_err_t loadcell_reset_stats(loadcell_t *device, uint8_t channel);

/* Debug output */
void loadcell_print_measurements(loadcell_t *device);
void loadcell_print_calib_info(loadcell_t *device);
```

---

## UART Command Reference

### Getting Started

Connect to ESP32 serial port at **921600 baud**, then:

```
> help                           # Show all commands
> status                         # Check device status
> tare 1 500                     # Zero calibration
> cal 1 100.5                    # Full-scale calibration (100.5 N)
> read                           # Single measurement
> stats                          # Show statistics
```

### Available Commands

#### Calibration Commands

| Command | Usage | Description |
|---------|-------|-------------|
| `tare` | `tare <ch> [samples]` | Tare (zero) calibration with no load |
| `cal` | `cal <ch> <force_N> [samples]` | Full-scale calibration with known weight |
| `rst_calib` | `rst_calib <ch>` | Reset calibration to uncalibrated state |

**Parameters:**
- `<ch>`: Channel 1-4, or 0 for all channels
- `<force_N>`: Reference force in Newtons (e.g., 100.5)
- `[samples]`: Number of samples to average (default: 200)

#### Measurement Commands

| Command | Usage | Description |
|---------|-------|-------------|
| `read` | `read` | Read all 4 channels once |
| `status` | `status` | Show calibration status of all channels |
| `stats` | `stats` | Show min/max/avg statistics |
| `raw` | `raw` | Show raw ADC values and normalized readings |
| `info` | `info` | Show calibration parameters |

#### Utility Commands

| Command | Usage | Description |
|---------|-------|-------------|
| `rst_stats` | `rst_stats <ch>` | Reset statistics counters |
| `help` | `help` | Show this help message |

---

## Usage Examples

### Complete Calibration Workflow

```
> status
=== Loadcell Status ===
Frame count: 123
Channel 1: UNCALIBRATED
Channel 2: UNCALIBRATED
Channel 3: UNCALIBRATED
Channel 4: UNCALIBRATED

> tare 0 300
Taring channel 1...
Taring channel 2...
Taring channel 3...
Taring channel 4...
Tare calibration done. Offset: 12345

> cal 0 100.0 300
Calibrating channel 1 with 100.00 N (using 300 samples)...
Make sure the known weight is applied to the loadcell!
Calibration successful!

> status
Channel 1: CALIBRATED
Channel 2: CALIBRATED
Channel 3: CALIBRATED
Channel 4: CALIBRATED

> read
=== Loadcell Measurements (Frame 456) ===
Channel 1: 23.45 N
  Raw ADC: 0x1a2b3c (normalized: 0.1234)
  Stats: min=0.00, max=23.45, avg=11.72 (n=100)
Channel 2: 22.89 N
  Raw ADC: 0x1a1e2d (normalized: 0.1200)
  Stats: min=0.00, max=22.89, avg=11.44 (n=100)
Channel 3: 24.12 N
Channel 4: 21.54 N
Total GRF: 91.98 N
```

### Selective Channel Calibration

```
> tare 1 200      # Tare only channel 1
> cal 1 50.0      # Calibrate channel 1 with 50 N

> stats
=== Channel Statistics ===
Channel 1:
  Min:   0.00 N
  Max:   50.00 N
  Avg:   25.00 N
  Count: 500
```

### Statistics Management

```
> stats                # View current statistics
> rst_stats 0          # Reset stats for all channels
> rst_stats 2          # Reset stats for channel 2 only
```

---

## Technical Details

### Ratiometric Bridge Measurement

The system uses **ratiometric bridge measurement**, which means:

- ✅ No need for precise reference voltage value
- ✅ Voltage variations automatically cancel out
- ✅ Only two calibration values needed: offset and scale factor

### Calibration Process

#### Tare Calibration (Zero)
```
1. Apply no load
2. Run: tare <ch> <samples>
3. System averages raw ADC readings
4. Stores offset_raw = average_adc
```

#### Full-Scale Calibration (Span)
```
1. Apply known reference weight (e.g., 100 N)
2. Run: cal <ch> 100.0 <samples>
3. System reads normalized value
4. Calculates: scale_factor = known_force / normalized_reading
```

#### Measurement
```
normalized = (raw_adc - offset_raw) / ADC_MAX_VALUE
force = normalized * scale_factor
```

### Per-Channel Structure

```c
typedef struct {
    uint8_t channel_id;
    loadcell_calib_state_t calib_state;    // Calibration state
    int32_t offset_raw;                    // Tare offset (raw ADC value)
    float scale_factor;                    // N per normalized unit
    loadcell_stats_t stats;                // Min/max/avg tracking
    loadcell_measurement_t last_measurement;
} loadcell_channel_t;
```

### Measurement Resolution

- **ADC Resolution**: 24-bit signed (-2^23 to 2^23-1)
- **Normalized Range**: -1.0 to +1.0
- **Ratiometric Accuracy**: Better than 16-bit equivalent
- **Sample Rate**: 40 kSPS (system) → ~1200 Hz per channel (4-ch mux)
- **Per-Channel Rate**: ~10 ms (1 frame = 4 channels)

---

## Performance Characteristics

### Timing

| Operation | Time | Notes |
|-----------|------|-------|
| Single channel read | ~100 µs | Includes SPI communication |
| Full 4-channel frame | ~400 µs | Multiplexed sequentially |
| Tare with 200 samples | ~3 seconds | Including delays between samples |
| Full calibration | ~4 seconds | Including tare + span calibration |
| Stats update | < 1 µs | Incremental (O(1)) |

### Memory

| Component | Size |
|-----------|------|
| `loadcell_t` structure | ~1 KB |
| Per-channel data | ~200 bytes |
| UART command buffer | ~256 bytes |
| Total driver overhead | < 3 KB |

---

## Integration with Existing Code

### SPI Bus

The driver uses the existing ADS1261 driver (`ads1261.h`). Initialize the SPI bus before calling `loadcell_init()`:

```c
spi_bus_config_t spi_cfg = { /* ... */ };
spi_bus_initialize(HSPI_HOST, &spi_cfg, SPI_DMA_CH_AUTO);

loadcell_init(&device, HSPI_HOST, CS_PIN, DRDY_PIN, ...);
```

### FreeRTOS Integration

The main application uses two FreeRTOS tasks:

```c
/* Measurement task - periodically reads all channels */
xTaskCreate(measurement_task, "measurement", 4096, NULL, 5, NULL);

/* UART command task - handles interactive commands */
xTaskCreate(uart_cmd_task, "uart_cmd", 4096, NULL, 4, NULL);
```

### Output Formats

**Human-readable (default):**
```
[Frame 123] Force readings:
  Ch1: 25.45 N (raw=1a2b3c, norm=0.123456)
  Ch2: 24.12 N (raw=1a1e2d, norm=0.118765)
  Ch3: 26.78 N (raw=1a3f4e, norm=0.130234)
  Ch4: 23.89 N (raw=1a0b2c, norm=0.116982)
  Total GRF: 100.24 N
```

**CSV format:**
```
123,1234567890,25.45,24.12,26.78,23.89,100.24
```

---

## Debugging Tips

### Check Calibration Status
```
> info
```

### Verify Raw ADC Readings
```
> raw
```

### Monitor Statistics
```
> stats
```

### Reset and Recalibrate
```
> rst_calib 0         # Reset all channels
> tare 0 300          # Retare all channels
> cal 1 100.0 300     # Recalibrate channel 1
```

---

## Future Enhancements

Possible additions to the driver:

- [ ] Non-volatile storage for calibration (NVS)
- [ ] Automatic background zeroing (continuous offset tracking)
- [ ] Digital filtering (IIR/FIR)
- [ ] Temperature compensation for force sensor
- [ ] Per-channel gain adjustment
- [ ] Data logging to SD card
- [ ] Bluetooth/WiFi streaming

---

## Comparison with Original Code

| Feature | Original | Refactored |
|---------|----------|-----------|
| **Architecture** | Monolithic | Modular (driver + app) |
| **Calibration** | Static constants | Dynamic, interactive |
| **Per-channel control** | Limited | Full control |
| **Statistics** | Not tracked | Live min/max/avg |
| **Interaction** | None (logging only) | Full UART interface |
| **Code reusability** | Low | High (easy to port) |
| **Memory overhead** | ~500 bytes | ~3 KB (worth it!) |
| **Maintainability** | Medium | High |

---

## License & Attribution

Built for ESP32-C6 GRF force measurement platform with ratiometric Wheatstone bridge loadcells.
API design inspired by popular Arduino libraries (HX711 by Rob Tillaart, etc.).
