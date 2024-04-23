#include "i2c-wrapper.h"

void i2cWriteByte(uint8_t device, TwoWire *tw, uint8_t addr, uint8_t value) {
  tw->beginTransmission(device);
  tw->write(addr);
  tw->write(value);
  tw->endTransmission(true);
}

uint8_t* i2cReadByte(uint8_t device, TwoWire *tw, uint8_t addr, size_t length) {
  uint8_t *buf = NULL;
  
  // Write the command
  tw->beginTransmission(device);
  tw->write(addr);
  // Do a restart if false, otherwise stop if true
  tw->endTransmission(true);

  if (length > 0) {
    buf = (uint8_t *) malloc(length * sizeof(uint8_t));

    // For RCWL-9620
    delay(65);

    // Read back bytes
    tw->requestFrom(device, length);
    for (int x = 0; x < length; x++) {
      buf[x] = tw->read();
    }
  }
  return buf;
}

uint8_t* i2cReadWord(uint8_t device, TwoWire *tw, uint16_t addr, size_t length) {
  uint8_t *buf = NULL;
  
  // Write the command
  tw->beginTransmission(device);
  tw->write((uint8_t)(addr >> 8));
  tw->write((uint8_t)(addr & 0xFF));
  // Do a restart if false, otherwise stop if true
  tw->endTransmission(true);

  if (length > 0) {
    buf = (uint8_t *) malloc(length * sizeof(uint8_t));

    // For SHT30
    delay(15);

    // Read back bytes
    tw->requestFrom(device, length);
    for (int x = 0; x < length; x++) {
      buf[x] = tw->read();
    }
    return buf;
  } else {
    return NULL;
  }
}
