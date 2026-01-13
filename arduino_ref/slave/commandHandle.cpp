#include "commandHandle.h"

ADS1261 adc;
ADS1261_REGISTERS_Type reg_map;

String COMMANDHANDLE::get_command_argument(String inputString) {
	return (inputString.substring(inputString.lastIndexOf(":") + 1, inputString.lastIndexOf(";")));
}

void COMMANDHANDLE::start_capture() {
	capturing = true;
  rightValue = 0;
	debugln("Starting capture...");
}

void COMMANDHANDLE::end_capture() {
	capturing = false;
	debugln("Capture ended...");
}

void COMMANDHANDLE::tare() {
	adc.tare();
	// EEPROM.put(tareAddress, adc.get_offset());
	debug("Taring OK.");
	debugln(adc.get_offset());
}

void COMMANDHANDLE::calibrate(String inputString) {
	String weightString = get_command_argument(inputString);
	float weight = weightString.toFloat();
	double offsetted_data = adc.get_value();

	float calibration_factor = offsetted_data / weight / 9.81;
	adc.set_scale(calibration_factor);
	EEPROM.put(calibrationAddress, calibration_factor);
	debug("Calibrating OK:");
	debugln(calibration_factor);
}

void COMMANDHANDLE::get_calibration_factor() {
	debugln(adc.get_scale());
}

void COMMANDHANDLE::set_calibration_factor(String inputString) {
	String calibration_factor = get_command_argument(inputString);
	adc.set_scale(calibration_factor.toFloat());
	float stored_calibration = 0.0f;
	EEPROM.get(calibrationAddress, stored_calibration);
	if (stored_calibration != calibration_factor.toFloat()) {
		EEPROM.put(calibrationAddress, calibration_factor.toFloat());
	}
	debugln("Calibration factor set");
}

void COMMANDHANDLE::get_tare() {
	debugln(adc.get_offset());
}

void COMMANDHANDLE::set_tare(String inputString) {
	String tare = get_command_argument(inputString);
	long value = tare.toInt();
	adc.set_offset(value);
	long stored_tare = 0;
	EEPROM.get(tareAddress, stored_tare);
	if (stored_tare != value) {
		EEPROM.put(tareAddress, value);
		debugln("updated");
	}
	debugln("Tare set");
}

void COMMANDHANDLE::processSerial() {
	String inputString;
	int colonIndex, colonLastIndex;

  if(Serial0.available()){
    inputString = Serial0.readString();
  } else if(Serial.available()){
    inputString = Serial.readString();
  } else {
    return;
  }

  colonLastIndex = inputString.lastIndexOf(":");
  if (colonLastIndex <= 0) return;
  
  colonIndex = inputString.indexOf(":");
  String commandString = inputString.substring(0, colonIndex);

	if (commandString == "start_capture") {
		start_capture();
	}
	else if(commandString == "end_capture") {
		end_capture();
	}
	else if (commandString == "tare") {
		tare();
	}
	else if (commandString == "calibrate") {
		calibrate(inputString);
	}
	else if (commandString == "get_calibration_factor") {
		get_calibration_factor();
	}
	else if (commandString == "set_calibration_factor") {
		set_calibration_factor(inputString);
	}
	else if (commandString == "get_tare") {
		get_tare();
	}
	else if (commandString == "set_tare") {
		set_tare(inputString);
	}
}
