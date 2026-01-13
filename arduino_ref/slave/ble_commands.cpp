/**
 * @file ble_commands.cpp
 * @brief Implementation of BLE command handling
 */

#include "ble_commands.h"

BleCommandHandler::BleCommandHandler(ForcePlate* forcePlate)
  : plate(forcePlate)
{
}

void BleCommandHandler::processCommand(uint8_t* data, size_t length) {
  if (length < 1) return;
  
  BleCommand cmd = static_cast<BleCommand>(data[0]);
  
  switch (cmd) {
    case CMD_START:
      plate->start();
      Serial.println("BLE: START command received");
      break;
      
    case CMD_STOP:
      plate->stop();
      Serial.println("BLE: STOP command received");
      break;
      
    case CMD_TARE:
      plate->tare();
      Serial.println("BLE: TARE command received");
      break;
      
    case CMD_SET_CALIB:
      if (length >= 5) {
        float factor;
        memcpy(&factor, &data[1], 4);
        plate->setCalibrationFactor(factor);
        Serial.print("BLE: CALIB set to ");
        Serial.println(factor);
      }
      break;
      
    case CMD_GET_STATUS:
      Serial.println("BLE: STATUS request received");
      break;
      
    default:
      Serial.print("BLE: Unknown command: 0x");
      Serial.println(cmd, HEX);
      break;
  }
}

void BleCommandHandler::buildDataPacket(uint8_t* buffer, const ForceData& force) {
  buffer[0] = RESP_DATA;  // 0xAA header
  memcpy(&buffer[1], &force.ch1, 4);
  memcpy(&buffer[5], &force.ch2, 4);
  memcpy(&buffer[9], &force.ch3, 4);
  memcpy(&buffer[13], &force.ch4, 4);
  // Total 17 bytes
}

void BleCommandHandler::buildStatusPacket(uint8_t* buffer, bool running, unsigned long samples) {
  buffer[0] = RESP_STATUS;
  buffer[1] = running ? 1 : 0;
  memcpy(&buffer[2], &samples, 4);
  // Total 6 bytes
}
