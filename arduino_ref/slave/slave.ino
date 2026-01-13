
/*
* 
* 无蓝牙Slave
* 
*/

#include "ADS1261.h"
#include <SPI.h>
#include "OTA.h"
#include "commandHandle.h"

#define PWDN 18
#define RSTA 19
#define START 20

#define CS -1

#define RX_PIN 17
#define TX_PIN 16

uint8_t rightBuffer[5];

COMMANDHANDLE commandhandle;

void setup() {
  Serial.begin(115200);
  Serial0.begin(256000, SERIAL_8N1, RX_PIN, TX_PIN, false, 1000);

  choseSpiffsOrFfat();

  adc.begin();
  adc.sendCommand(ADS1261_COMMAND_RESET);  // reset adc
  
  pinMode(PWDN, OUTPUT);
  pinMode(RSTA, OUTPUT);
  pinMode(START, INPUT);
  digitalWrite(RSTA, HIGH);
  digitalWrite(PWDN, HIGH);
  digitalWrite(START, LOW);

  adc.readAllRegisters(&reg_map);  // this function read all ADS1261 register value and print in serial
  debugln();
  reg_map.MODE0.bit.DR = ADS1261_DR_40000_SPS;
  reg_map.MODE1.bit.DELAY = ADS1261_DELAY_50_US;
  reg_map.MODE1.bit.CONVRT = ADS1261_CONVRT_CONTINUOUS_CONVERSION;
  reg_map.MODE3.bit.SPITIM = SPITIM_AUTO_ENABLE;

  // 写入 MODE0 寄存器
  adc.writeConfigRegister(ADS1261_MODE0, reg_map.MODE0.reg);
  // 写入 MODE1 寄存器
  adc.writeConfigRegister(ADS1261_MODE1, reg_map.MODE1.reg);
  // 写入 MODE3 寄存器
  adc.writeConfigRegister(ADS1261_MODE3, reg_map.MODE3.reg);
  adc.readAllRegisters(&reg_map);
  
  debugln();
  adc.set_scale(FACTOR);
  adc.tare();
  debugln(adc.get_offset());
}

void loop() {
  switch (MODE) {
    case NORMAL_MODE:
      fun_NORMAL_MODE();

      if (commandhandle.capturing) {
        ChannelData channelData = adc.readFourChannel();
        float units[4] = {
          (float)channelData.ch1 / FACTOR,
          (float)channelData.ch2 / FACTOR,
          (float)channelData.ch3 / FACTOR,
          (float)channelData.ch4 / FACTOR
        };
        
        uint8_t buffer[17];
        buffer[0] = 0xAA;
        memcpy(&buffer[1], &units[0], 4);
        memcpy(&buffer[5], &units[1], 4);
        memcpy(&buffer[9], &units[2], 4);
        memcpy(&buffer[13], &units[3], 4);
        Serial0.write(buffer, 17);
        
        debugln(String(units[0]) + ", " + String(units[1]) + ", " + String(units[2]) + ", " + String(units[3]));
      }
      if (Serial0.available() > 0 || Serial.available() > 0) {
        commandhandle.processSerial();
      }
      
      break;

    case UPDATE_MODE:
      fun_UPDATE_MODE();
      break;

    case OTA_MODE:
      fun_OTA_MODE();
      break;
  }
}