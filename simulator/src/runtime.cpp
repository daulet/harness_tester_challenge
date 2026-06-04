#include "host_simulator/runtime.h"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <sstream>
#include <stdexcept>

namespace host_sim {

namespace {

Runtime* g_runtime = nullptr;

std::string format_number(long long value) {
  return std::to_string(value);
}

std::string format_number(unsigned long long value) {
  return std::to_string(value);
}

std::string vformat(const char* format, va_list args) {
  std::array<char, 1024> buffer{};
  va_list copy;
  va_copy(copy, args);
  const auto count = std::vsnprintf(buffer.data(), buffer.size(), format, copy);
  va_end(copy);
  if (count < 0) {
    return {};
  }
  if (static_cast<std::size_t>(count) < buffer.size()) {
    return std::string(buffer.data(), static_cast<std::size_t>(count));
  }
  std::string result(static_cast<std::size_t>(count) + 1, '\0');
  std::vsnprintf(result.data(), result.size(), format, args);
  result.resize(static_cast<std::size_t>(count));
  return result;
}

}  // namespace

Runtime& active_runtime() {
  if (!g_runtime) {
    throw std::runtime_error("no active host simulator runtime");
  }
  return *g_runtime;
}

void set_active_runtime(Runtime* runtime) {
  g_runtime = runtime;
}

std::size_t PrintSink::print(const char* value) {
  return write_bytes(value ? value : "");
}

std::size_t PrintSink::print(const std::string& value) {
  return write_bytes(value);
}

std::size_t PrintSink::print(char value) {
  return write_bytes(std::string(1, value));
}

std::size_t PrintSink::print(int value) {
  return write_bytes(format_number(static_cast<long long>(value)));
}

std::size_t PrintSink::print(unsigned int value) {
  return write_bytes(format_number(static_cast<unsigned long long>(value)));
}

std::size_t PrintSink::print(long value) {
  return write_bytes(format_number(static_cast<long long>(value)));
}

std::size_t PrintSink::print(unsigned long value) {
  return write_bytes(format_number(static_cast<unsigned long long>(value)));
}

std::size_t PrintSink::print(long long value) {
  return write_bytes(format_number(value));
}

std::size_t PrintSink::print(unsigned long long value) {
  return write_bytes(format_number(value));
}

std::size_t PrintSink::print(bool value) {
  return write_bytes(value ? "1" : "0");
}

std::size_t PrintSink::println() {
  return write_bytes("\n");
}

std::size_t PrintSink::println(const char* value) {
  return print(value) + println();
}

std::size_t PrintSink::println(const std::string& value) {
  return print(value) + println();
}

std::size_t PrintSink::println(char value) {
  return print(value) + println();
}

std::size_t PrintSink::println(int value) {
  return print(value) + println();
}

std::size_t PrintSink::println(bool value) {
  return print(value) + println();
}

int PrintSink::printf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  const auto output = vformat(format, args);
  va_end(args);
  write_bytes(output);
  return static_cast<int>(output.size());
}

void HardwareSerial::begin(unsigned long baud) {
  baud_ = baud;
}

int HardwareSerial::available() const {
  return static_cast<int>(rx_.size() - rx_offset_);
}

int HardwareSerial::read() {
  if (!available()) {
    return -1;
  }
  return static_cast<unsigned char>(rx_[rx_offset_++]);
}

void HardwareSerial::push_rx(const std::string& bytes) {
  if (rx_offset_ == rx_.size()) {
    rx_.clear();
    rx_offset_ = 0;
  }
  rx_ += bytes;
}

void HardwareSerial::clear() {
  baud_ = 0;
  rx_.clear();
  tx_.clear();
  rx_offset_ = 0;
}

const std::string& HardwareSerial::output() const {
  return tx_;
}

bool HardwareSerial::begun() const {
  return baud_ != 0;
}

std::size_t HardwareSerial::write_bytes(const std::string& bytes) {
  tx_ += bytes;
  return bytes.size();
}

File::File(std::shared_ptr<std::string> content) : content_(std::move(content)), open_(true) {
}

File::operator bool() const {
  return open_ && content_ != nullptr;
}

void File::close() {
  open_ = false;
}

std::size_t File::write_bytes(const std::string& bytes) {
  if (!*this) {
    return 0;
  }
  *content_ += bytes;
  return bytes.size();
}

bool SDClass::begin(std::uint8_t) {
  return available_;
}

File SDClass::open(const char* path, std::uint8_t) {
  if (!available_ || !path) {
    return {};
  }
  auto& content = files_[path];
  if (!content) {
    content = std::make_shared<std::string>();
  }
  return File(content);
}

void SDClass::set_available(bool available) {
  available_ = available;
}

void SDClass::clear() {
  files_.clear();
  available_ = true;
}

const std::string& SDClass::content(const std::string& path) const {
  const auto iter = files_.find(path);
  if (iter == files_.end()) {
    return empty_;
  }
  return *iter->second;
}

void TwoWire::begin() {
  begun_ = true;
  active_runtime().mark_wire_begun();
}

void TwoWire::setClock(std::uint32_t clock_hz) {
  clock_hz_ = clock_hz;
}

void TwoWire::beginTransmission(std::uint8_t address) {
  address_ = address;
  tx_.clear();
}

std::size_t TwoWire::write(std::uint8_t value) {
  tx_.push_back(value);
  return 1;
}

std::uint8_t TwoWire::endTransmission(bool) {
  if (!begun_) {
    return 4;
  }
  return active_runtime().i2c_write(address_, tx_) ? 0 : 4;
}

std::uint8_t TwoWire::requestFrom(std::uint8_t address, std::uint8_t quantity) {
  rx_ = begun_ ? active_runtime().i2c_read(address, quantity) : std::vector<std::uint8_t>{};
  rx_offset_ = 0;
  return static_cast<std::uint8_t>(rx_.size());
}

int TwoWire::read() {
  if (rx_offset_ >= rx_.size()) {
    return -1;
  }
  return rx_[rx_offset_++];
}

void TwoWire::clear() {
  begun_ = false;
  clock_hz_ = 0;
  address_ = 0;
  tx_.clear();
  rx_.clear();
  rx_offset_ = 0;
}

bool TwoWire::begun() const {
  return begun_;
}

Runtime::Runtime(BoardModel model) : model_(std::move(model)) {
  set_active_runtime(this);
  reset_expander_state();
}

Runtime::~Runtime() {
  if (g_runtime == this) {
    set_active_runtime(nullptr);
  }
}

void Runtime::set_harness(Harness harness) {
  harness_ = std::move(harness);
}

void Runtime::set_button_pressed(bool pressed) {
  button_pressed_ = pressed;
}

void Runtime::inject_gps(const std::string& nmea) {
  Serial1.push_rx(nmea);
}

void Runtime::set_sd_available(bool available) {
  SD.set_available(available);
}

void Runtime::clear_peripherals() {
  pins_.clear();
  button_pressed_ = false;
  elapsed_ms_ = 0;
  Serial.clear();
  Serial1.clear();
  Wire2.clear();
  SD.clear();
  reset_expander_state();
}

void Runtime::pin_mode(std::uint8_t pin, std::uint8_t mode) {
  pins_[pin].mode = mode;
}

void Runtime::digital_write(std::uint8_t pin, std::uint8_t value) {
  pins_[pin].value = value ? 1 : 0;
  if (pin == model_.arduino_pin("CY_RST_N")) {
    if (value == 0) {
      reset_expander_state();
    } else {
      expander_.reset_asserted = false;
    }
  }
}

int Runtime::digital_read(std::uint8_t pin) const {
  if (pin == model_.arduino_pin("BTN_TEST")) {
    return button_pressed_ ? 1 : 0;
  }
  const auto iter = pins_.find(pin);
  return iter == pins_.end() ? 0 : iter->second.value;
}

void Runtime::delay(std::uint32_t milliseconds) {
  elapsed_ms_ += milliseconds;
}

bool Runtime::i2c_write(std::uint8_t address, const std::vector<std::uint8_t>& bytes) {
  if (address != 0x20 || !expander_available() || bytes.empty()) {
    return false;
  }
  expander_.accessed = true;
  expander_.register_pointer = bytes.front();
  for (std::size_t index = 1; index < bytes.size(); ++index) {
    write_expander_register(expander_.register_pointer++, bytes[index]);
  }
  return true;
}

std::vector<std::uint8_t> Runtime::i2c_read(std::uint8_t address, std::uint8_t quantity) {
  std::vector<std::uint8_t> result;
  if (address != 0x20 || !expander_available()) {
    return result;
  }
  expander_.accessed = true;
  for (std::uint8_t index = 0; index < quantity; ++index) {
    result.push_back(read_expander_register(expander_.register_pointer++));
  }
  return result;
}

void Runtime::mark_wire_begun() {
}

const BoardModel& Runtime::model() const {
  return model_;
}

const Harness& Runtime::harness() const {
  return harness_;
}

const std::string& Runtime::serial_output() const {
  return Serial.output();
}

const std::string& Runtime::serial1_output() const {
  return Serial1.output();
}

const std::string& Runtime::sd_content(const std::string& path) const {
  return SD.content(path);
}

LedState Runtime::led_state() const {
  LedState state;
  state.red = digital_read(static_cast<std::uint8_t>(model_.arduino_pin("LED_R"))) == 0;
  state.green = digital_read(static_cast<std::uint8_t>(model_.arduino_pin("LED_G"))) == 0;
  state.blue = digital_read(static_cast<std::uint8_t>(model_.arduino_pin("LED_B"))) == 0;
  return state;
}

bool Runtime::wire_begun() const {
  return Wire2.begun();
}

bool Runtime::expander_accessed() const {
  return expander_.accessed;
}

std::uint64_t Runtime::last_expander_inputs() const {
  return expander_.last_inputs;
}

std::uint32_t Runtime::elapsed_ms() const {
  return elapsed_ms_;
}

bool Runtime::expander_available() const {
  return Wire2.begun() && !expander_.reset_asserted;
}

void Runtime::reset_expander_state() {
  expander_ = {};
  expander_.reset_asserted = true;
  expander_.directions.fill(0x00);
  for (auto& bank : expander_.drive_modes) {
    bank.fill(0x00);
  }
  expander_.drive_modes[static_cast<std::size_t>(DriveMode::PullUp)].fill(0xFF);
}

std::uint8_t Runtime::read_expander_register(std::uint8_t reg) {
  if (reg < 8) {
    expander_.last_inputs = model_.resolve_inputs(harness_, expander_drives());
    return static_cast<std::uint8_t>(expander_.last_inputs >> (reg * 8));
  }
  if (reg >= 0x08 && reg < 0x10) {
    return expander_.outputs[reg - 0x08];
  }
  if (reg == 0x18) {
    return expander_.selected_port;
  }
  if (reg == 0x1C) {
    return expander_.directions[expander_.selected_port];
  }
  if (reg >= 0x1D && reg < 0x24) {
    return expander_.drive_modes[reg - 0x1D][expander_.selected_port];
  }
  if (reg == 0x2E) {
    return 0x60;
  }
  return 0;
}

void Runtime::write_expander_register(std::uint8_t reg, std::uint8_t value) {
  if (reg >= 0x08 && reg < 0x10) {
    expander_.outputs[reg - 0x08] = value;
    return;
  }
  if (reg == 0x18) {
    expander_.selected_port = value & 0x07;
    return;
  }
  if (reg == 0x1C) {
    expander_.directions[expander_.selected_port] = value;
    return;
  }
  if (reg >= 0x1D && reg < 0x24) {
    const auto mode = reg - 0x1D;
    for (auto& bank : expander_.drive_modes) {
      bank[expander_.selected_port] &= static_cast<std::uint8_t>(~value);
    }
    expander_.drive_modes[mode][expander_.selected_port] |= value;
  }
}

std::array<ExpanderPinDrive, kExpanderPins> Runtime::expander_drives() const {
  std::array<ExpanderPinDrive, kExpanderPins> drives{};
  for (std::size_t port = 0; port < 8; ++port) {
    for (std::size_t bit = 0; bit < 8; ++bit) {
      const auto index = port * 8 + bit;
      const auto mask = static_cast<std::uint8_t>(1U << bit);
      drives[index].is_input = (expander_.directions[port] & mask) != 0;
      drives[index].output_value = (expander_.outputs[port] & mask) != 0;
      for (std::size_t mode = 0; mode < expander_.drive_modes.size(); ++mode) {
        if (expander_.drive_modes[mode][port] & mask) {
          drives[index].drive_mode = static_cast<DriveMode>(mode);
          break;
        }
      }
    }
  }
  return drives;
}

HardwareSerial Serial;
HardwareSerial Serial1;
TwoWire Wire2;
SDClass SD;

}  // namespace host_sim
