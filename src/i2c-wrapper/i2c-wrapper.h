#ifndef __I2C_WRAPPER_H_
#define __I2C_WRAPPER_H_

#include <Arduino.h>
#include <Wire.h>

void i2cWriteByte(uint8_t device, TwoWire *tw, uint8_t addr, uint8_t value);
uint8_t* i2cReadByte(uint8_t device, TwoWire *tw, uint8_t addr, size_t length);
uint8_t* i2cReadWord(uint8_t device, TwoWire *tw, uint16_t addr, size_t length);

#endif
