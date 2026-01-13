/*
 * Simple Serial Print Test
 * Prints ADC values as text to serial monitor
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

const float CALIB_FACTOR = 2231.19;  // ADC counts per Newton

// Timer
hw_timer_t* timer = NULL;
volatile bool sampleReady = false;

// ADC
ADS1261 adc;
ADS1261_REGISTERS_Type regMap;

// DRDY interrupt
void IRAM_ATTR drdyISR() {
  adc.setDataReady();
}

// Timer - 10 Hz for easy reading
void IRAM_ATTR onTimer() {
  sampleReady = true;
}

void configureADC() {
  Serial.println("Initializing ADS1261...");
  
  pinMode(DRDY_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(DRDY_PIN), drdyISR, FALLING);
  
  adc.begin(SCK, MISO, MOSI, PWDN_PIN, RESET_PIN, START_PIN);
  adc.readRegisters(&regMap);
  
  regMap.MODE0.bit.FILTER = ADS1261_FILTER_SINC1;
  regMap.MODE0.bit.DR = ADS1261_DR_14400;
  regMap.MODE0.bit.DELAY = ADS1261_DELAY_0us;
  regMap.MODE1.bit.CHOP = ADS1261_CHOP_OFF;
  
  adc.writeRegister(ADS1261_ADDR_MODE0, regMap.MODE0.reg);
  adc.writeRegister(ADS1261_ADDR_MODE1, regMap.MODE1.reg);
  
  Serial.println("OK: ADC configured");
}

void setupTimer() {
  timer = timerBegin(1000000);  // 1 MHz  
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 100000, true, 0);  // 100ms = 10 Hz
  Serial.println("OK: Timer at 10 Hz for easy reading");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== ZPlate Load Cell Test ===\n");
  
  configureADC();
  setupTimer();
  
  Serial.println("\nReading 4 channels every 100ms...");
  Serial.println("Format: CH1(ADC/N) CH2(ADC/N) CH3(ADC/N) CH4(ADC/N)\n");
  delay(1000);
}

void loop() {
  if (sampleReady) {
    sampleReady = false;
    
    // Read all 4 channels
    int32_t ch1 = adc.readChannel(INPMUX_MUXP_AIN2, INPMUX_MUXN_AIN3);
    int32_t ch2 = adc.readChannel(INPMUX_MUXP_AIN4, INPMUX_MUXN_AIN5);
    int32_t ch3 = adc.readChannel(INPMUX_MUXP_AIN6, INPMUX_MUXN_AIN7);
    int32_t ch4 = adc.readChannel(INPMUX_MUXP_AIN8, INPMUX_MUXN_AIN9);
    
    // Convert to Newtons
    float n1 = ch1 / CALIB_FACTOR;
    float n2 = ch2 / CALIB_FACTOR;
    float n3 = ch3 / CALIB_FACTOR;
    float n4 = ch4 / CALIB_FACTOR;
    
    // Print as table
    Serial.print("CH1: ");
    Serial.print(ch1);
    Serial.print(" (");
    Serial.print(n1, 2);
    Serial.print("N)  CH2: ");
    Serial.print(ch2);
    Serial.print(" (");
    Serial.print(n2, 2);
    Serial.print("N)  CH3: ");
    Serial.print(ch3);
    Serial.print(" (");
    Serial.print(n3, 2);
    Serial.print("N)  CH4: ");
    Serial.print(ch4);
    Serial.print(" (");
    Serial.print(n4, 2);
    Serial.println("N)");
  }
}
