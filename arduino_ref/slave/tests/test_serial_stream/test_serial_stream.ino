/*
 * Serial Streaming - 1000 SPS per channel
 * Streams 4-channel ADC data to PC via Serial
 * Binary format for speed: timestamp + 4x int32
 */

#include "ADS1261.h"

// Hardware Pins
const int DRDY_PIN = 10;
const int PWDN_PIN = 18;
const int RESET_PIN = 19;
const int START_PIN = 20;
const int SCK = 6;
const int MISO = 2;
const int MOSI = 7;

// Timer Configuration
hw_timer_t* timer = NULL;
volatile bool sampleReady = false;
volatile uint32_t sampleCount = 0;

// ADC
ADS1261 adc;
ADS1261_REGISTERS_Type regMap;

// Data packet structure (20 bytes)
struct __attribute__((packed)) DataPacket {
  uint32_t timestamp_us;  // 4 bytes
  int32_t ch1;           // 4 bytes
  int32_t ch2;           // 4 bytes
  int32_t ch3;           // 4 bytes
  int32_t ch4;           // 4 bytes
};

// DRDY interrupt
void IRAM_ATTR drdyISR() {
  adc.setDataReady();
}

// Timer interrupt - triggers at 1000 Hz
void IRAM_ATTR onTimer() {
  sampleReady = true;
  sampleCount++;
}

void configureADC() {
  Serial.println("Initializing ADS1261...");
  
  pinMode(DRDY_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(DRDY_PIN), drdyISR, FALLING);
  
  adc.begin(SCK, MISO, MOSI, PWDN_PIN, RESET_PIN, START_PIN);
  adc.readRegisters(&regMap);
  
  // Optimized settings for 1125 SPS
  regMap.MODE0.bit.FILTER = ADS1261_FILTER_SINC1;
  regMap.MODE0.bit.DR = ADS1261_DR_14400;
  regMap.MODE0.bit.DELAY = ADS1261_DELAY_0us;
  regMap.MODE1.bit.CHOP = ADS1261_CHOP_OFF;
  
  adc.writeRegister(ADS1261_ADDR_MODE0, regMap.MODE0.reg);
  adc.writeRegister(ADS1261_ADDR_MODE1, regMap.MODE1.reg);
  
  Serial.println("✓ ADC configured for 1125 SPS");
}

void setupTimer() {
  timer = timerBegin(1000000);  // 1 MHz = 1 microsecond resolution
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 1000, true, 0);  // 1000 us = 1ms = 1000 Hz
  Serial.println("✓ Timer configured: 1000 Hz");
}

void setup() {
  Serial.begin(2000000);  // 2 Mbaud for high-speed data
  delay(1000);
  
  Serial.println("\n=== ZPlate Serial Streaming ===");
  Serial.println("Baud: 2000000");
  Serial.println("Format: Binary 20-byte packets");
  Serial.println("Structure: uint32 timestamp + 4x int32 channels\n");
  
  configureADC();
  setupTimer();
  
  Serial.println("\n✓ Ready! Streaming in 2 seconds...");
  Serial.println("Close serial monitor and use Python receiver\n");
  delay(2000);
}

void loop() {
  if (sampleReady) {
    sampleReady = false;
    
    DataPacket packet;
    packet.timestamp_us = micros();
    
    // Read all 4 channels
    packet.ch1 = adc.readChannel(INPMUX_MUXP_AIN2, INPMUX_MUXN_AIN3);
    packet.ch2 = adc.readChannel(INPMUX_MUXP_AIN4, INPMUX_MUXN_AIN5);
    packet.ch3 = adc.readChannel(INPMUX_MUXP_AIN6, INPMUX_MUXN_AIN7);
    packet.ch4 = adc.readChannel(INPMUX_MUXP_AIN8, INPMUX_MUXN_AIN9);
    
    // Send binary packet
    Serial.write((uint8_t*)&packet, sizeof(packet));
    
    // Debug every 1000 samples (once per second)
    if (sampleCount % 1000 == 0) {
      // Use stderr for debug so it doesn't interfere with binary data
      // (Serial is stdout in Arduino, but we'll just send less frequently)
      Serial.flush();
    }
  }
}
