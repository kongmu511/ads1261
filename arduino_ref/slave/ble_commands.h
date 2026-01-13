/**
 * @file ble_commands.h
 * @brief BLE command protocol for force plate control
 * 
 * Mobile app sends commands to control force plate:
 * - START: Begin data acquisition
 * - STOP: End data acquisition
 * - TARE: Zero all channels
 * - CALIB: Set calibration factor
 */

#ifndef BLE_COMMANDS_H
#define BLE_COMMANDS_H

#include <Arduino.h>
#include "force_plate.h"

// Command bytes from mobile app
enum BleCommand : uint8_t {
  CMD_START = 0x01,
  CMD_STOP = 0x02,
  CMD_TARE = 0x03,
  CMD_SET_CALIB = 0x04,
  CMD_GET_STATUS = 0x05
};

// Response bytes to mobile app
enum BleResponse : uint8_t {
  RESP_OK = 0x80,
  RESP_ERROR = 0x81,
  RESP_STATUS = 0x82,
  RESP_DATA = 0xAA  // Force data header
};

class BleCommandHandler {
private:
  ForcePlate* plate;
  
public:
  BleCommandHandler(ForcePlate* forcePlate);
  
  // Process received command from BLE
  void processCommand(uint8_t* data, size_t length);
  
  // Build response packets
  static void buildDataPacket(uint8_t* buffer, const ForceData& force);
  static void buildStatusPacket(uint8_t* buffer, bool running, unsigned long samples);
};

#endif
