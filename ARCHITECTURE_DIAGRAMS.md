# Loadcell System Architecture & Flow Diagrams

## System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    GRF Force Platform                       │
│                  (ESP32-C6 + ADS1261)                       │
└─────────────────────────────────────────────────────────────┘

┌──────────────────────┐
│   4x Loadcells       │  (Wheatstone bridge strain gauges)
│   (Differential      │
│    configuration)    │
└──────────┬───────────┘
           │ (Analog voltages)
           │ AIN0-AIN7
           ▼
┌──────────────────────────────────────────┐
│         ADS1261 ADC                      │
│  (24-bit Δ-Σ, 40 kSPS)                   │
│  • SPI Interface                         │
│  • PGA Gain: 128×                        │
│  • Reference: External (ratiometric)     │
└──────────┬───────────────────────────────┘
           │ (SPI: MOSI/MISO/CLK/CS/DRDY)
           │
           ▼
┌──────────────────────────────────────────┐
│      loadcell.c Driver                   │
│  • Multiplexer sequencing                │
│  • ADC data conversion                   │
│  • Calibration management                │
│  • Per-channel statistics                │
└──────────┬───────────────────────────────┘
           │
      ┌────┴────────────┬──────────────────┐
      │                 │                  │
      ▼                 ▼                  ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│  Measure     │  │   UART Cmd   │  │  Statistics  │
│  (every 10ms)│  │  Interface   │  │  Tracking    │
└──────────┬───┘  └──────────┬───┘  └──────────────┘
           │                 │
           └────────┬────────┘
                    │
                    ▼
           ┌─────────────────┐
           │   FreeRTOS      │
           │   Tasks         │
           └────────┬────────┘
                    │
      ┌─────────────┼─────────────┐
      │             │             │
      ▼             ▼             ▼
┌──────────┐  ┌──────────┐  ┌──────────┐
│Measure   │  │UART CMD  │  │ Logging  │
│Task      │  │Task      │  │(periodic)│
│(5ms)     │  │(4ms)     │  │          │
└──────────┘  └──────────┘  └──────────┘
```

---

## Data Flow: Complete Measurement

```
┌─────────────────────────────────────────┐
│  loadcell_read(&device)                 │
│  Main measurement function              │
└─────────────┬───────────────────────────┘
              │
              ▼
    ┌─────────────────────┐
    │ For each of 4 chs:  │
    └─────────┬───────────┘
              │
              ▼
    ┌──────────────────────────────┐
    │ 1. Set multiplexer           │
    │    (select differential pair) │
    └─────────┬────────────────────┘
              │
              ▼
    ┌──────────────────────────────┐
    │ 2. Start ADC conversion      │
    │    (ads1261_start_conversion)│
    └─────────┬────────────────────┘
              │
              ▼
    ┌──────────────────────────────┐
    │ 3. Read 24-bit result        │
    │    (ads1261_read_adc)        │
    │    raw_adc = 0x±XXXXXX       │
    └─────────┬────────────────────┘
              │
              ▼
    ┌──────────────────────────────┐
    │ 4. Normalize to ±1.0         │
    │    normalized =              │
    │    raw_adc / ADC_MAX_VALUE    │
    └─────────┬────────────────────┘
              │
              ▼
    ┌──────────────────────────────┐
    │ 5. Apply offset correction   │
    │    normalized -=              │
    │    offset_raw / ADC_MAX       │
    └─────────┬────────────────────┘
              │
              ▼
    ┌──────────────────────────────┐
    │ 6. Apply scale factor        │
    │    force =                   │
    │    normalized * scale_factor │
    └─────────┬────────────────────┘
              │
              ▼
    ┌──────────────────────────────┐
    │ 7. Store measurement         │
    │    measurements[ch] = value  │
    └─────────┬────────────────────┘
              │
              ▼
    ┌──────────────────────────────┐
    │ 8. Update statistics         │
    │    min/max/avg tracking      │
    └──────────────────────────────┘
              │
              └─ (repeat for next channel)
```

---

## Calibration State Machine

```
                    ┌──────────────────────┐
                    │   UNCALIBRATED       │
                    │  (Initial state)     │
                    │  offset_raw = 0      │
                    │  scale_factor = 1.0  │
                    └──────────┬───────────┘
                               │
                               │ User runs:
                               │ > tare <ch>
                               │
                               ▼
                    ┌──────────────────────┐
                    │   TARE_READY         │
                    │ (after first tare)   │
                    │  offset_raw = 12345  │
                    │  scale_factor = 1.0  │
                    └──────────┬───────────┘
                               │
                               │ Tare completed
                               │
                               ▼
                    ┌──────────────────────┐
                    │   TARE_DONE          │
                    │ (ready for span cal) │
                    │  offset_raw = 12345  │
                    │  scale_factor = 1.0  │
                    └──────────┬───────────┘
                               │
                               │ User runs:
                               │ > cal <ch> 100.0
                               │
                               ▼
                    ┌──────────────────────┐
                    │   SPAN_READY         │
                    │ (before calibration) │
                    │  offset_raw = 12345  │
                    │  scale_factor = 1.0  │
                    └──────────┬───────────┘
                               │
                               │ Calibration completed
                               │
                               ▼
                    ┌──────────────────────┐
                    │   CALIBRATED         │
                    │ ✓ Ready for use      │
                    │  offset_raw = 12345  │
                    │  scale_factor = 784.2│
                    └──────────────────────┘
                               │
                               │ User can run:
                               │ > rst_calib <ch>
                               │
                               └─→ Back to UNCALIBRATED
```

---

## Calibration Process: Tare

```
USER COMMAND: > tare 0 300

┌─────────────────────────────────┐
│ loadcell_tare(&device, ch, 300) │
└──────────────┬──────────────────┘
               │
               ▼
    ┌──────────────────────────┐
    │ Loop 300 times:          │
    │  sum += raw_adc_reading  │
    │  delay(10ms)             │
    └──────────────┬───────────┘
                   │
                   │ (Total time: ~3 seconds)
                   │
                   ▼
    ┌──────────────────────────┐
    │ offset_raw = sum / 300   │
    │        = 12345 (average) │
    └──────────────┬───────────┘
                   │
                   ▼
    ┌──────────────────────────┐
    │ calib_state =            │
    │   TARE_DONE              │
    └──────────────────────────┘

RESULT: Channel ready for full-scale calibration
```

---

## Calibration Process: Full-Scale (Span)

```
USER COMMAND: > cal 1 100.0 300
(Apply 100 N reference weight to channel 1)

┌─────────────────────────────────┐
│ loadcell_calibrate(&device,     │
│    channel=1, force=100.0,      │
│    samples=300)                 │
└──────────────┬──────────────────┘
               │
               ▼
    ┌──────────────────────────┐
    │ Loop 300 times:          │
    │  1. Read ADC             │
    │  2. Subtract offset      │
    │  3. Normalize            │
    │  4. sum += normalized    │
    │  5. delay(10ms)          │
    └──────────────┬───────────┘
                   │
                   │ (Total time: ~3 seconds)
                   │
                   ▼
    ┌──────────────────────────┐
    │ avg_normalized =         │
    │   sum / 300 = 0.1274     │
    └──────────────┬───────────┘
                   │
                   ▼
    ┌──────────────────────────┐
    │ scale_factor =           │
    │   100.0 / 0.1274         │
    │   = 784.5 N/unit         │
    └──────────────┬───────────┘
                   │
                   ▼
    ┌──────────────────────────┐
    │ calib_state =            │
    │   CALIBRATED             │
    └──────────────────────────┘

RESULT: Channel ready for measurement
        scale_factor saved in device.channels[1]
```

---

## Measurement Formula

```
┌─────────────────────────────────────────────────┐
│            RATIOMETRIC BRIDGE FORMULA           │
└─────────────────────────────────────────────────┘

INPUT: raw_adc (24-bit signed value from ADC)
PARAMETERS:
  • offset_raw: Tare offset (from calibration)
  • scale_factor: Sensitivity (from calibration)
  • ADC_MAX_VALUE: 0x7FFFFF (2^23 - 1)

CALCULATION:

Step 1: Normalize to ±1.0 range
    normalized = raw_adc / ADC_MAX_VALUE
    
    Range: -1.0 to +1.0

Step 2: Apply zero offset (tare) correction
    normalized_adjusted = normalized - (offset_raw / ADC_MAX_VALUE)
    
    Removes DC offset from unloaded condition

Step 3: Convert to force using scale factor
    force_newtons = normalized_adjusted * scale_factor
    
    Result: Force in Newtons

WHY RATIOMETRIC?
• Excitation voltage not explicitly needed
• If excitation voltage changes, bridge output changes proportionally
• Normalized value automatically compensates
• Offset and scale calibration captures the effect
• Error cancellation!
```

---

## FreeRTOS Task Layout

```
┌──────────────────────────────────────────────────┐
│         FreeRTOS Scheduler (CORE 1)              │
└──────────────────────────────────────────────────┘

Task Priority Stack    Function        Period  Status
────────────────────────────────────────────────────
5     4KB  Measurement every 10ms  continuous
           • Calls loadcell_read()
           • Updates measurements[4]
           • Logs periodically (every 50 frames)

4     4KB  UART_CMD    continuous  Blocking on serial
           • Reads character from stdin
           • Parses commands
           • Executes handlers
           • Returns to blocking

Other                  FreeRTOS kernel
             idle task, timers, etc.
```

---

## UART Command Handler Flow

```
USER INPUT: "> tare 0 300"

┌────────────────────────────────┐
│ uart_cmd_process()             │
│ (called from UART task)        │
└──────────────┬─────────────────┘
               │
               ▼
    ┌──────────────────────────┐
    │ Read character from UART │
    │ > tare 0 300<LF>        │
    └──────────────┬───────────┘
                   │
                   ▼
    ┌──────────────────────────┐
    │ Build command buffer:    │
    │ "tare 0 300"             │
    └──────────────┬───────────┘
                   │
                   │ (on newline)
                   │
                   ▼
    ┌──────────────────────────┐
    │ parse_and_execute()      │
    │                          │
    │ strtok("tare 0 300")     │
    │ → argv[0] = "tare"       │
    │ → argv[1] = "0"          │
    │ → argv[2] = "300"        │
    │ → argc = 3               │
    └──────────────┬───────────┘
                   │
                   ▼
    ┌──────────────────────────┐
    │ Find command in table:   │
    │ if (strcmp("tare", ...) ==0)
    │   call cmd_tare()        │
    └──────────────┬───────────┘
                   │
                   ▼
    ┌──────────────────────────┐
    │ cmd_tare(argc=3, argv[])  │
    │                          │
    │ ch = atoi("0") = 0       │
    │ samples = atoi("300")300 │
    │                          │
    │ loadcell_tare(&device,   │
    │    0, 300)               │
    │                          │
    │ → Tares all 4 channels   │
    │ → ~3 second operation    │
    │ → Prints progress        │
    └──────────────────────────┘
```

---

## Memory Layout

```
┌────────────────────────────────────────────────┐
│  loadcell_t Device Structure (~3 KB)           │
├────────────────────────────────────────────────┤
│                                                │
│  SPI/Hardware:                                 │
│    spi_device_handle_t                         │
│    cs_pin, drdy_pin                            │
│                                                │
│  Per-Channel (4 channels × ~400 bytes):        │
│    [0]: loadcell_channel_t                     │
│      • calib_state                             │
│      • offset_raw                              │
│      • scale_factor                            │
│      • stats (min/max/avg/count)              │
│      • last_measurement                        │
│    [1]: ... (repeat)                           │
│    [2]: ... (repeat)                           │
│    [3]: ... (repeat)                           │
│                                                │
│  Measurements:                                 │
│    measurements[4]  (4 × 32 bytes)            │
│      • raw_adc                                 │
│      • normalized                              │
│      • force_newtons                           │
│      • timestamp_us                            │
│                                                │
│  Frame counter:                                │
│    frame_count (4 bytes)                       │
│                                                │
└────────────────────────────────────────────────┘

Plus UART command buffer (~256 bytes)
Total driver overhead: < 3 KB
```

---

## Statistics Calculation

```
Per-Channel Real-Time Statistics

On each measurement:

IF first_sample:
    stats.min_force = reading
    stats.max_force = reading
    stats.avg_force = reading
    stats.sample_count = 1

ELSE:
    IF reading < stats.min_force:
        stats.min_force = reading
    
    IF reading > stats.max_force:
        stats.max_force = reading
    
    old_avg = stats.avg_force
    stats.avg_force = 
        (old_avg * count + reading) / (count + 1)
    
    stats.sample_count++

TIME COMPLEXITY: O(1) per measurement
MEMORY: Fixed (min/max/avg/count per channel)
```

---

## Example: Full Lifecycle

```
STEP 1: HARDWARE INITIALIZATION
  ├─ SPI bus configure
  ├─ ADS1261 init (PGA=128, Rate=40kSPS)
  └─ loadcell_init()
     └─ Creates 4 channel structures (UNCALIBRATED)

STEP 2: TASKS START
  ├─ Measurement task: Read every 10ms
  └─ UART task: Wait for commands

STEP 3: USER CALIBRATION
  ├─ > tare 0 300
  │  ├─ Ch1: offset_raw = 12340
  │  ├─ Ch2: offset_raw = 12342
  │  ├─ Ch3: offset_raw = 12338
  │  └─ Ch4: offset_raw = 12344
  │     → All TARE_DONE
  │
  ├─ > cal 1 100.0 300
  │  ├─ Ch1: scale_factor = 784.5 N/unit
  │  └─ Ch1: → CALIBRATED
  │
  ├─ > cal 2 100.0 300
  │  ├─ Ch2: scale_factor = 785.1 N/unit
  │  └─ Ch2: → CALIBRATED
  │
  └─ (repeat for Ch3, Ch4)

STEP 4: MEASUREMENT
  ├─ Every 10ms: loadcell_read()
  │  └─ Reads all 4 channels, updates statistics
  │
  ├─ Every 500ms (50 frames): Log to UART
  │  └─ Print: Ch1: 99.87 N, Ch2: 100.12 N, ...
  │
  └─ User can query:
     ├─ > read          → Single measurement
     ├─ > stats         → Min/max/avg
     └─ > raw           → ADC values

STEP 5: STATISTICS
  After 1 minute of measurement:
    ├─ Ch1: min=0.0, max=100.0, avg=50.2, n=6000
    ├─ Ch2: min=0.1, max=99.8, avg=50.1, n=6000
    ├─ Ch3: min=0.2, max=100.2, avg=50.3, n=6000
    └─ Ch4: min=0.0, max=99.9, avg=50.0, n=6000
```

---

**Complete system architecture documented above!** ✓
