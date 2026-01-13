
#include "Arduino.h"
#include "ads1261.h"
#include <SPI.h>

#define ADS_SCK 6
#define ADS_MISO 2
#define ADS_MOSI 7

void ADS1261::begin() {
    /********* Configure SPI pins *********/
    // Note: CS pin is hardwired to ground on custom board
    _drdyPin = -1;  // Default: no DRDY pin
    _dataReady = false;
    pinMode(ADS_SCK, OUTPUT);
    pinMode(ADS_MISO, INPUT);
    pinMode(ADS_MOSI, OUTPUT);
    SPI.begin(ADS_SCK, ADS_MISO, ADS_MOSI);
    SPI.setFrequency(8000000);  // 8MHz max for ADS1261
    SPI.setDataMode(SPI_MODE1);
}

void ADS1261::attachDrdyInterrupt(void (*isr)(void)) {
    if (_drdyPin >= 0) {
        attachInterrupt(digitalPinToInterrupt(_drdyPin), isr, FALLING);
    }
}

int32_t ADS1261::readChannel(uint8_t pos, uint8_t neg) {
    int32_t result;

    inp.bit.MUXP = pos;
    inp.bit.MUXN = neg;
    pga.bit.GAIN = PGA_GAIN_128;

    // Pulse mode: configure MUX, then trigger single conversion
    writeConfigRegister(ADS1261_INPMUX, inp.reg);
    writeConfigRegister(ADS1261_PGA, pga.reg);
    
    // START triggers a single conversion in pulse mode
    writeCommand(ADS1261_COMMAND_START);
    
    result = readConversionData();
    return result;
}

int32_t ADS1261::readConversionData() {
    ThreeBytesTo24Bit value;
    
    // Wait for DRDY interrupt flag (CRITICAL for 40kSPS multiplexing)
    if (_drdyPin >= 0) {
        unsigned long currentMicros = micros();
        // Wait for interrupt flag set by DRDY ISR
        while (!_dataReady && ((micros() - currentMicros) < 2000)) {
            // Interrupt-driven: flag set in ~1µs when DRDY falls
        }
        _dataReady = false;  // Clear flag for next conversion
    }
    // Note: At 40kSPS SINC1, conversion = 25µs. Total overhead ~100µs for SPI
    
    writeCommand(ADS1261_COMMAND_RDATA);
    value.bytes[2] = SPI.transfer(0x00);
    value.bytes[1] = SPI.transfer(0x00);
    value.bytes[0] = SPI.transfer(0x00);
    int32_t signedValue = value.value;

    if (signedValue & 0x800000) {
        signedValue = signedValue - 0x1000000;
    }
    
    // Continuous mode - no STOP command
    // CS pin hardwired to ground - no digitalWrite needed
    return signedValue;
}

void ADS1261::readAllRegisters(ADS1261_REGISTERS_Type* args) {
    // CS pin hardwired to ground - always selected
    args->ID.reg = readRegister(ADS1261_ID);
    args->STATUS.reg = readRegister(ADS1261_STATUS);
    args->MODE0.reg = readRegister(ADS1261_MODE0);
    args->MODE1.reg = readRegister(ADS1261_MODE1);
    args->MODE2.reg = readRegister(ADS1261_MODE2);
    args->MODE3.reg = readRegister(ADS1261_MODE3);
    args->REF.reg = readRegister(ADS1261_REF);
    args->OFCAL.byte.OFC0 = readRegister(ADS1261_OFCAL0);
    args->OFCAL.byte.OFC1 = readRegister(ADS1261_OFCAL1);
    args->OFCAL.byte.OFC2 = readRegister(ADS1261_OFCAL2);

    args->FSCAL.byte.FSC0 = readRegister(ADS1261_FSCAL0);
    args->FSCAL.byte.FSC1 = readRegister(ADS1261_FSCAL1);
    args->FSCAL.byte.FSC2 = readRegister(ADS1261_FSCAL2);

    args->IMUX.reg = readRegister(ADS1261_IMUX);
    args->IMAG.reg = readRegister(ADS1261_IMAG);
    args->PGA.reg = readRegister(ADS1261_PGA);
    args->INPMUX.reg = readRegister(ADS1261_INPMUX);
    args->INPBIAS.reg = readRegister(ADS1261_INPBIAS);
    // CS pin hardwired to ground - no digitalWrite needed
}

void ADS1261::writeAllRegisters(ADS1261_REGISTERS_Type* args) {
    writeRegister(ADS1261_MODE0, args->MODE0.reg);
    writeRegister(ADS1261_MODE1, args->MODE1.reg);
    writeRegister(ADS1261_MODE2, args->MODE2.reg);
    writeRegister(ADS1261_MODE3, args->MODE3.reg);
    writeRegister(ADS1261_REF, args->REF.reg);
    writeRegister(ADS1261_OFCAL0, args->OFCAL.byte.OFC0);
    writeRegister(ADS1261_OFCAL1, args->OFCAL.byte.OFC1);
    writeRegister(ADS1261_OFCAL2, args->OFCAL.byte.OFC2);

    writeRegister(ADS1261_FSCAL0, args->FSCAL.byte.FSC0);
    writeRegister(ADS1261_FSCAL1, args->FSCAL.byte.FSC1);
    writeRegister(ADS1261_FSCAL2, args->FSCAL.byte.FSC2);

    writeRegister(ADS1261_IMUX, args->IMUX.reg);
    writeRegister(ADS1261_IMAG, args->IMAG.reg);
    writeRegister(ADS1261_PGA, args->PGA.reg);
    writeRegister(ADS1261_INPMUX, args->INPMUX.reg);
    writeRegister(ADS1261_INPBIAS, args->INPBIAS.reg);
}

uint8_t ADS1261::readConfigRegister(uint8_t add) {
    uint8_t data = readRegister(add);
    return data;
}

uint8_t ADS1261::writeConfigRegister(uint8_t add, uint8_t val) {
    uint8_t data = writeRegister(add, val);
    return data;
}

uint8_t ADS1261::sendCommand(uint8_t add) {
    uint8_t data = writeCommand(add);
    return data;
}

uint8_t ADS1261::writeRegister(uint8_t reg_add, uint8_t reg_val) {
    uint8_t reg = ADS1261_COMMAND_WREG | reg_add;
    SPI.transfer(reg);
    byte echo_byte = SPI.transfer(reg_val);
    return echo_byte;
}

ChannelData ADS1261::readFourChannel()
{
    ChannelData data;
    data.ch1 = readChannel(INPMUX_MUXP_AIN2, INPMUX_MUXN_AIN3);
    data.ch2 = readChannel(INPMUX_MUXP_AIN4, INPMUX_MUXN_AIN5);
    data.ch3 = readChannel(INPMUX_MUXP_AIN6, INPMUX_MUXN_AIN7);
    data.ch4 = readChannel(INPMUX_MUXP_AIN8, INPMUX_MUXN_AIN9);
    return data;
}

void ADS1261::sort(int32_t* array, uint8_t size)
{
    uint32_t max;
    uint8_t i, j, pos;

    for (i = 0; i < size; i++)
    {
        max = array[i];
        pos = i;
        for (j = i + 1; j < size; j++)
        {
            if (array[j] > max)
            {
                max = array[j];
                pos = j;
            }
        }
        array[pos] = array[i];
        array[i] = max;
    }
}

float ADS1261::read_mid()
{
    float buf[TIME];
    for (uint8_t i = 0; i < TIME; i++)
    {
        ChannelData data = readFourChannel();
        buf[i] = (float)(data.ch1 + data.ch2 + data.ch3 + data.ch4) * 0.25f;
    }
    std::sort(buf, buf + TIME);
    return buf[(uint8_t)TIME / 2];
}

void ADS1261::tare()
{
    double sum = (double)read_mid();
    set_offset(sum);
}

void ADS1261::set_offset(float offset)
{
    OFFSET = offset;
}

float ADS1261::get_offset()
{
    return OFFSET;
}

float ADS1261::get_value()
{
    ChannelData data = readFourChannel();
    float rawValue = (float)(data.ch1 + data.ch2 + data.ch3 + data.ch4) * 0.25f - OFFSET;

    buffer[index] = rawValue;
    index = (index + 1) % BUF_SIZE;

    float tempBuffer[BUF_SIZE];
    std::copy(buffer, buffer + BUF_SIZE, tempBuffer);
    std::sort(tempBuffer, tempBuffer + BUF_SIZE);

    return tempBuffer[BUF_SIZE / 2];
}

void ADS1261::set_scale(float scale)
{
    SCALE = scale;
}

float ADS1261::get_scale()
{
    return SCALE;
}

float ADS1261::get_units()
{
    return abs(get_value() / SCALE);
}

uint8_t ADS1261::readRegister(uint8_t reg_add) {
    uint8_t reg_val = ADS1261_COMMAND_PREG | reg_add;
    SPI.transfer(reg_val);
    byte echo_byte = SPI.transfer(0x00);
    byte reg_data = SPI.transfer(0x00);
    return reg_data;
}

uint8_t ADS1261::writeCommand(uint8_t command_add) {
    SPI.transfer(command_add);
    byte echo_byte = SPI.transfer(0x00);
    return echo_byte;
}
