#pragma once
#include <Adafruit_MCP23X17.h>
extern Adafruit_MCP23X17 mcp;

void i2c_start();
void i2c_stop();
void i2c_write_bit(uint8_t bit);
uint8_t i2c_read_bit();
bool i2c_write_byte(uint8_t byte);
uint8_t i2c_read_byte(bool nack);