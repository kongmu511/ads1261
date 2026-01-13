#ifndef COMMANDHANDLE_H
#define COMMANDHANDLE_H

#include "Arduino.h"
#include "ADS1261.h"
#include <EEPROM.h>

#define DEBUG 1

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

/********************************/
extern ADS1261 adc;
extern ADS1261_REGISTERS_Type reg_map;
/********************************/

class COMMANDHANDLE{
private:
	//校准和校零的地址信息
	int tareAddress = 0;
	int calibrationAddress = 4;

public:
	boolean capturing = false;
	float rightValue = 0;

	String get_command_argument(String inputString);
	void start_capture();
	void end_capture();
	void tare();
	void calibrate(String inputString);
	void get_calibration_factor();
	void set_calibration_factor(String inputString);
	void get_tare();
	void set_tare(String inputString);
	void processSerial();
};

#endif
