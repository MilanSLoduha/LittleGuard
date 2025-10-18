#include <select_pins.h>
#include <Adafruit_MCP23X17.h>
#include "bit_banging.h"

void i2c_start() {
  mcp.digitalWrite(MCP_SDA_PIN, HIGH);
  mcp.digitalWrite(MCP_SCL_PIN, HIGH);
  delayMicroseconds(5);
  mcp.digitalWrite(MCP_SDA_PIN, LOW);
  delayMicroseconds(5);
  mcp.digitalWrite(MCP_SCL_PIN, LOW);
}

void i2c_stop() {
  mcp.digitalWrite(MCP_SDA_PIN, LOW);
  mcp.digitalWrite(MCP_SCL_PIN, HIGH);
  delayMicroseconds(5);
  mcp.digitalWrite(MCP_SDA_PIN, HIGH);
  delayMicroseconds(5);
}

void i2c_write_bit(uint8_t bit) {
  mcp.digitalWrite(MCP_SDA_PIN, bit);
  delayMicroseconds(5);
  mcp.digitalWrite(MCP_SCL_PIN, HIGH);
  delayMicroseconds(5);
  mcp.digitalWrite(MCP_SCL_PIN, LOW);
}

uint8_t i2c_read_bit() {
  uint8_t bit;
  mcp.digitalWrite(MCP_SCL_PIN, HIGH);
  delayMicroseconds(5);
  bit = mcp.digitalRead(MCP_SDA_PIN);
  mcp.digitalWrite(MCP_SCL_PIN, LOW);
  return bit;
}

bool i2c_write_byte(uint8_t byte) {
  for (int i = 7; i >= 0; i--) {
    i2c_write_bit((byte >> i) & 1);
  }
  mcp.pinMode(MCP_SDA_PIN, INPUT); // Na čítanie ACK
  uint8_t ack = i2c_read_bit();
  mcp.pinMode(MCP_SDA_PIN, OUTPUT); // Späť na výstup
  return ack == 0;
}

uint8_t i2c_read_byte(bool nack) {
  uint8_t byte = 0;
  mcp.pinMode(MCP_SDA_PIN, INPUT);
  for (int i = 7; i >= 0; i--) {
    byte |= i2c_read_bit() << i;
  }
  mcp.pinMode(MCP_SDA_PIN, OUTPUT);
  i2c_write_bit(nack);
  return byte;
}