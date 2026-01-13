/**
 * @file force_plate.h
 * @brief Force plate class wrapping 4 loadcells with ADS1261 ADC
 * 
 * Separates high-level force plate logic from low-level ADC operations.
 * Meets ISO force platform standard: 1000+ SPS per channel.
 */

#ifndef FORCE_PLATE_H
#define FORCE_PLATE_H

#include "ads1261.h"

// Force data structure for 4 loadcells
struct ForceData {
  float ch1;  // Front-left loadcell (grams or newtons)
  float ch2;  // Front-right loadcell
  float ch3;  // Rear-left loadcell
  float ch4;  // Rear-right loadcell
  
  // Total vertical force
  float total() const { return ch1 + ch2 + ch3 + ch4; }
  
  // Center of pressure (CoP) in normalized coordinates [-1, 1]
  float copX() const { 
    float total_force = total();
    if (total_force < 0.1f) return 0.0f;
    return ((ch2 + ch4) - (ch1 + ch3)) / total_force;
  }
  
  float copY() const {
    float total_force = total();
    if (total_force < 0.1f) return 0.0f;
    return ((ch3 + ch4) - (ch1 + ch2)) / total_force;
  }
};

class ForcePlate {
private:
  ADS1261* adc;
  float calibrationFactor;
  bool isCalibrated;
  
  // Timing for ISO compliance (1000 SPS minimum)
  unsigned long lastReadUs;
  static const unsigned long READ_INTERVAL_US = 1000;  // 1ms = 1000 Hz
  
  // Running state
  bool isRunning;
  unsigned long sampleCount;

public:
  ForcePlate(ADS1261* adcInstance, float calFactor = 2231.19f);
  
  // Initialization
  void begin();
  void configure(uint8_t dataRate = ADS1261_DR_4800_SPS);
  
  // Calibration
  void tare();
  void setCalibrationFactor(float factor);
  float getCalibrationFactor() const { return calibrationFactor; }
  
  // Data acquisition
  void start();
  void stop();
  bool isActive() const { return isRunning; }
  
  // Read force data (non-blocking, respects 1000 Hz timing)
  bool readIfReady(ForceData& data);
  
  // Read force data (blocking)
  ForceData read();
  
  // Statistics
  unsigned long getSampleCount() const { return sampleCount; }
  float getActualSampleRate() const;
};

#endif
