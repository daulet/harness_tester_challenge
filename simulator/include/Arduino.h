#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "host_simulator/runtime.h"

using host_sim::File;
using host_sim::HardwareSerial;
using host_sim::SD;
using host_sim::SDClass;
using host_sim::Serial;
using host_sim::Serial1;
using host_sim::TwoWire;
using host_sim::Wire2;

constexpr std::uint8_t LOW = 0;
constexpr std::uint8_t HIGH = 1;
constexpr std::uint8_t INPUT = 0;
constexpr std::uint8_t OUTPUT = 1;
constexpr std::uint8_t INPUT_PULLUP = 2;

inline void pinMode(std::uint8_t pin, std::uint8_t mode) {
  host_sim::active_runtime().pin_mode(pin, mode);
}

inline void digitalWrite(std::uint8_t pin, std::uint8_t value) {
  host_sim::active_runtime().digital_write(pin, value);
}

inline int digitalRead(std::uint8_t pin) {
  return host_sim::active_runtime().digital_read(pin);
}

inline void delay(std::uint32_t milliseconds) {
  host_sim::active_runtime().delay(milliseconds);
}

#define BUILTIN_SDCARD 254
#define FILE_WRITE 0x01
