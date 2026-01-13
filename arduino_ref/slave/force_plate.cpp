/**
 * @file force_plate.cpp
 * @brief Implementation of force plate with 4 loadcells
 */

#include "force_plate.h"

ForcePlate::ForcePlate(ADS1261* adcInstance, float calFactor)
  : adc(adcInstance)
  , calibrationFactor(calFactor)
  , isCalibrated(false)
  , lastReadUs(0)
  , isRunning(false)
  , sampleCount(0)
{
}

void ForcePlate::begin() {
  adc->begin();
  adc->sendCommand(ADS1261_COMMAND_RESET);
  delay(100);
}

void ForcePlate::configure(uint8_t dataRate) {
  ADS1261_REGISTERS_Type reg_map;
  adc->readAllRegisters(&reg_map);
  
  // Configure for high-speed continuous conversion
  reg_map.MODE0.bit.DR = dataRate;
  reg_map.MODE1.bit.DELAY = ADS1261_DELAY_50_US;
  reg_map.MODE1.bit.CONVRT = ADS1261_CONVRT_CONTINUOUS_CONVERSION;
  reg_map.MODE3.bit.SPITIM = SPITIM_AUTO_ENABLE;
  
  adc->writeConfigRegister(ADS1261_MODE0, reg_map.MODE0.reg);
  adc->writeConfigRegister(ADS1261_MODE1, reg_map.MODE1.reg);
  adc->writeConfigRegister(ADS1261_MODE3, reg_map.MODE3.reg);
  
  delay(100);
}

void ForcePlate::tare() {
  adc->set_scale(calibrationFactor);
  adc->tare();
  isCalibrated = true;
}

void ForcePlate::setCalibrationFactor(float factor) {
  calibrationFactor = factor;
  adc->set_scale(factor);
}

void ForcePlate::start() {
  isRunning = true;
  sampleCount = 0;
  lastReadUs = micros();
}

void ForcePlate::stop() {
  isRunning = false;
}

bool ForcePlate::readIfReady(ForceData& data) {
  if (!isRunning) return false;
  
  unsigned long now = micros();
  if (now - lastReadUs < READ_INTERVAL_US) {
    return false;  // Not time yet
  }
  
  lastReadUs = now;
  data = read();
  sampleCount++;
  return true;
}

ForceData ForcePlate::read() {
  ChannelData rawData = adc->readFourChannel();
  
  const float scale = 1.0f / calibrationFactor;
  ForceData force;
  force.ch1 = rawData.ch1 * scale;
  force.ch2 = rawData.ch2 * scale;
  force.ch3 = rawData.ch3 * scale;
  force.ch4 = rawData.ch4 * scale;
  
  return force;
}

float ForcePlate::getActualSampleRate() const {
  if (sampleCount == 0) return 0.0f;
  unsigned long elapsedUs = micros() - lastReadUs;
  return 1000000.0f * sampleCount / elapsedUs;
}
