#include "host_simulator/runtime.h"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace host_sim {

namespace {

Runtime *g_runtime = nullptr;
constexpr std::uint8_t kInputMode = 0;
constexpr std::uint8_t kOutputMode = 1;
constexpr std::uint64_t kMicrosecondsPerSecond = 1000000;
constexpr std::uint64_t kUartBitsPerFrame = 10;
constexpr unsigned long kMaximumModeledBaud = 10000000;

std::string format_number(long long value) { return std::to_string(value); }

std::string format_number(unsigned long long value) {
  return std::to_string(value);
}

std::string vformat(const char *format, va_list args) {
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

SimTime uart_boundary(std::size_t frame_count, unsigned long baud) {
  if (baud == 0 || baud > kMaximumModeledBaud) {
    throw std::invalid_argument("UART baud is outside modeled range");
  }
  if (frame_count >
      std::numeric_limits<std::uint64_t>::max() / kUartBitsPerFrame) {
    throw std::overflow_error("UART frame count overflow");
  }

  const auto bits = static_cast<std::uint64_t>(frame_count) * kUartBitsPerFrame;
  const auto whole_seconds = bits / baud;
  const auto remaining_bits = bits % baud;
  const auto max_microseconds =
      static_cast<std::uint64_t>(SimTime::max().count());
  if (whole_seconds > max_microseconds / kMicrosecondsPerSecond) {
    throw std::overflow_error("UART frame time overflow");
  }

  auto microseconds = whole_seconds * kMicrosecondsPerSecond;
  const auto remainder_numerator = remaining_bits * kMicrosecondsPerSecond;
  const auto remainder =
      (remainder_numerator + baud - 1) / static_cast<std::uint64_t>(baud);
  if (microseconds > max_microseconds - remainder) {
    throw std::overflow_error("UART frame time overflow");
  }
  microseconds += remainder;
  return SimTime(static_cast<SimTime::rep>(microseconds));
}

bool baud_compatible(unsigned long transmitter, unsigned long receiver) {
  if (transmitter == 0 || receiver == 0 ||
      transmitter > kMaximumModeledBaud || receiver > kMaximumModeledBaud) {
    return false;
  }
  const auto high = std::max(transmitter, receiver);
  const auto low = std::min(transmitter, receiver);
  return static_cast<std::uint64_t>(high - low) * 100 <=
      static_cast<std::uint64_t>(high) * 2;
}

} // namespace

Runtime &active_runtime() {
  if (!g_runtime) {
    throw std::runtime_error("no active host simulator runtime");
  }
  return *g_runtime;
}

void set_active_runtime(Runtime *runtime) { g_runtime = runtime; }

std::size_t PrintSink::print(const char *value) {
  return write_bytes(value ? value : "");
}

std::size_t PrintSink::print(const std::string &value) {
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

std::size_t PrintSink::println() { return write_bytes("\n"); }

std::size_t PrintSink::println(const char *value) {
  return print(value) + println();
}

std::size_t PrintSink::println(const std::string &value) {
  return print(value) + println();
}

std::size_t PrintSink::println(char value) { return print(value) + println(); }

std::size_t PrintSink::println(int value) { return print(value) + println(); }

std::size_t PrintSink::println(bool value) { return print(value) + println(); }

int PrintSink::printf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  const auto output = vformat(format, args);
  va_end(args);
  write_bytes(output);
  return static_cast<int>(output.size());
}

HardwareSerial::HardwareSerial(Port port) : port_(port) {}

void HardwareSerial::begin(unsigned long baud) { baud_ = baud; }

int HardwareSerial::available() const {
  return static_cast<int>(rx_.size() - rx_offset_);
}

int HardwareSerial::read() {
  if (!available()) {
    return -1;
  }
  return static_cast<unsigned char>(rx_[rx_offset_++]);
}

void HardwareSerial::push_rx(const std::string &bytes) {
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

const std::string &HardwareSerial::output() const { return tx_; }

bool HardwareSerial::begun() const { return baud_ != 0; }

unsigned long HardwareSerial::baud() const { return baud_; }

std::size_t HardwareSerial::write_bytes(const std::string &bytes) {
  tx_ += bytes;
  if (port_ == Port::Uart1 && baud_ != 0 && !bytes.empty()) {
    active_runtime().transmit_serial1(bytes, baud_);
  }
  return bytes.size();
}

File::File(std::shared_ptr<std::string> content)
    : content_(std::move(content)), open_(true) {}

File::operator bool() const { return open_ && content_ != nullptr; }

void File::close() { open_ = false; }

std::size_t File::write_bytes(const std::string &bytes) {
  if (!*this) {
    return 0;
  }
  *content_ += bytes;
  return bytes.size();
}

bool SDClass::begin(std::uint8_t) { return available_; }

File SDClass::open(const char *path, std::uint8_t) {
  if (!available_ || !path) {
    return {};
  }
  auto &content = files_[path];
  if (!content) {
    content = std::make_shared<std::string>();
  }
  return File(content);
}

void SDClass::set_available(bool available) { available_ = available; }

void SDClass::clear() {
  files_.clear();
  available_ = true;
}

const std::string &SDClass::content(const std::string &path) const {
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

void TwoWire::setClock(std::uint32_t clock_hz) { clock_hz_ = clock_hz; }

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
  rx_ = begun_ ? active_runtime().i2c_read(address, quantity)
               : std::vector<std::uint8_t>{};
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

bool TwoWire::begun() const { return begun_; }

Runtime::Runtime(BoardModel model) : model_(std::move(model)) {
  set_active_runtime(this);
  clear_peripherals();
}

Runtime::~Runtime() {
  if (g_runtime == this) {
    set_active_runtime(nullptr);
  }
}

bool Runtime::EventLater::operator()(const Event &left,
                                     const Event &right) const {
  if (left.due != right.due) {
    return left.due > right.due;
  }
  return left.sequence > right.sequence;
}

void Runtime::set_harness(Harness harness) { harness_ = std::move(harness); }

void Runtime::set_button_pressed(bool pressed) { button_pressed_ = pressed; }

void Runtime::inject_serial1_rx(const std::string &bytes) {
  Serial1.push_rx(bytes);
}

void Runtime::transmit_gps(const std::string &bytes,
                           unsigned long baud,
                           SimTime start_delay) {
  schedule_uart(UartDriver::Gps, bytes, baud, start_delay);
}

void Runtime::set_sd_available(bool available) { SD.set_available(available); }

void Runtime::clear_peripherals() {
  if (advancing_) {
    throw std::logic_error("cannot clear runtime while advancing time");
  }
  pins_.clear();
  button_pressed_ = false;
  now_ = SimTime::zero();
  next_event_sequence_ = 0;
  events_ = {};
  active_uart_frames_.clear();
  mcu_uart_ready_ = SimTime::zero();
  gps_uart_ready_ = SimTime::zero();
  gps_uart_enabled_ = true;
  uart_unrouted_frames_ = 0;
  uart_framing_errors_ = 0;
  uart_contention_frames_ = 0;
  Serial.clear();
  Serial1.clear();
  Wire2.clear();
  SD.clear();
  reset_expander_state();
}

void Runtime::schedule_at(SimTime due, EventAction action) {
  if (!action) {
    throw std::invalid_argument("event action must not be empty");
  }
  if (due < now_) {
    throw std::invalid_argument("cannot schedule an event in the past");
  }
  if (next_event_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error("event sequence exhausted");
  }
  events_.push(Event{due, next_event_sequence_++, std::move(action)});
}

void Runtime::schedule_after(SimTime delay, EventAction action) {
  if (delay < SimTime::zero()) {
    throw std::invalid_argument("event delay must not be negative");
  }
  if (delay > SimTime::max() - now_) {
    throw std::overflow_error("event time overflow");
  }
  schedule_at(now_ + delay, std::move(action));
}

void Runtime::advance_to(SimTime target) {
  if (advancing_) {
    throw std::logic_error("simulation time advancement is not reentrant");
  }
  if (target < now_) {
    throw std::invalid_argument("cannot move simulation time backwards");
  }

  advancing_ = true;
  try {
    while (!events_.empty() && events_.top().due <= target) {
      auto event = events_.top();
      events_.pop();
      now_ = event.due;
      event.action();
    }
    now_ = target;
    advancing_ = false;
  } catch (...) {
    advancing_ = false;
    throw;
  }
}

void Runtime::advance_by(SimTime duration) {
  if (duration < SimTime::zero()) {
    throw std::invalid_argument("simulation duration must not be negative");
  }
  if (duration > SimTime::max() - now_) {
    throw std::overflow_error("simulation time overflow");
  }
  advance_to(now_ + duration);
}

SimTime Runtime::now() const { return now_; }

void Runtime::transmit_serial1(const std::string &bytes, unsigned long baud) {
  schedule_uart(UartDriver::Mcu, bytes, baud, SimTime::zero());
}

void Runtime::schedule_uart(UartDriver driver,
                            const std::string &bytes,
                            unsigned long baud,
                            SimTime start_delay) {
  if (bytes.empty()) {
    return;
  }
  if (start_delay < SimTime::zero()) {
    throw std::invalid_argument("UART start delay must not be negative");
  }
  if (start_delay > SimTime::max() - now_) {
    throw std::overflow_error("UART start time overflow");
  }

  const auto earliest_start = now_ + start_delay;
  auto &driver_ready =
      driver == UartDriver::Mcu ? mcu_uart_ready_ : gps_uart_ready_;
  const auto transmission_start = std::max(earliest_start, driver_ready);
  const auto source_net = driver == UartDriver::Mcu
      ? model_.pad("U2", "3").net
      : model_.pad("U3", "20").net;
  const auto mcu_rx_net = model_.pad("U2", "2").net;
  const auto mcu_tx_net = model_.pad("U2", "3").net;
  const auto gps_tx_net = model_.pad("U3", "20").net;

  for (std::size_t index = 0; index < bytes.size(); ++index) {
    const auto start_offset = uart_boundary(index, baud);
    const auto end_offset = uart_boundary(index + 1, baud);
    if (start_offset > SimTime::max() - transmission_start ||
        end_offset > SimTime::max() - transmission_start) {
      throw std::overflow_error("UART transmission time overflow");
    }

    auto frame = std::make_shared<UartFrame>();
    frame->driver = driver;
    frame->net = source_net;
    frame->start = transmission_start + start_offset;
    frame->end = transmission_start + end_offset;
    frame->baud = baud;
    frame->value = bytes[index];
    frame->routed_to_mcu_rx =
        driver == UartDriver::Gps && source_net == mcu_rx_net;
    frame->shares_mcu_tx =
        driver == UartDriver::Gps && source_net == mcu_tx_net;
    frame->shares_gps_tx =
        driver == UartDriver::Mcu && source_net == gps_tx_net;

    for (const auto &active : active_uart_frames_) {
      if (active->driver != frame->driver && active->net == frame->net &&
          active->start < frame->end && frame->start < active->end) {
        active->collided = true;
        frame->collided = true;
      }
    }
    active_uart_frames_.push_back(frame);

    schedule_at(frame->start, [this, frame] {
      if (frame->driver == UartDriver::Gps) {
        if (frame->shares_mcu_tx && Serial1.begun()) {
          frame->collided = true;
        }
        if (frame->routed_to_mcu_rx) {
          frame->receiver_ready =
              Serial1.begun() && baud_compatible(frame->baud, Serial1.baud());
        }
      } else if (frame->shares_gps_tx && gps_uart_enabled_) {
        frame->collided = true;
      }
    });
    schedule_at(frame->end, [this, frame] { finish_uart_frame(frame); });
  }
  driver_ready = transmission_start + uart_boundary(bytes.size(), baud);
}

void Runtime::finish_uart_frame(const std::shared_ptr<UartFrame> &frame) {
  if (frame->collided) {
    ++uart_contention_frames_;
  }
  if (frame->driver == UartDriver::Gps) {
    if (!frame->routed_to_mcu_rx) {
      ++uart_unrouted_frames_;
    } else if (!frame->collided && frame->receiver_ready && Serial1.begun() &&
               baud_compatible(frame->baud, Serial1.baud())) {
      Serial1.push_rx(std::string(1, frame->value));
    } else if (!frame->collided) {
      ++uart_framing_errors_;
    }
  }

  const auto iter = std::find(active_uart_frames_.begin(),
                              active_uart_frames_.end(), frame);
  if (iter != active_uart_frames_.end()) {
    active_uart_frames_.erase(iter);
  }
}

void Runtime::pin_mode(std::uint8_t pin, std::uint8_t mode) {
  pins_[pin].mode = mode;
}

void Runtime::digital_write(std::uint8_t pin, std::uint8_t value) {
  pins_[pin].value = value ? 1 : 0;
  if (pin == model_.arduino_pin("CY_RST_N")) {
    // The board net is named CY_RST_N, but the actual CY8C9560A XRES pin is
    // active high and has an internal pull-down.
    if (value != 0) {
      reset_expander_state();
    } else {
      expander_.reset_asserted = false;
    }
  }
}

int Runtime::digital_read(std::uint8_t pin) const {
  if (pin == model_.arduino_pin("BTN_TEST")) {
    return button_pressed_ ? 0 : 1;
  }
  const auto iter = pins_.find(pin);
  return iter == pins_.end() ? 0 : iter->second.value;
}

std::uint8_t Runtime::pin_mode(std::uint8_t pin) const {
  const auto iter = pins_.find(pin);
  return iter == pins_.end() ? kInputMode : iter->second.mode;
}

std::uint8_t Runtime::pin_value(std::uint8_t pin) const {
  const auto iter = pins_.find(pin);
  return iter == pins_.end() ? 0 : iter->second.value;
}

void Runtime::delay(std::uint32_t milliseconds) {
  advance_by(std::chrono::duration_cast<SimTime>(
      std::chrono::milliseconds(milliseconds)));
}

bool Runtime::i2c_write(std::uint8_t address,
                        const std::vector<std::uint8_t> &bytes) {
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

std::vector<std::uint8_t> Runtime::i2c_read(std::uint8_t address,
                                            std::uint8_t quantity) {
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

void Runtime::mark_wire_begun() {}

const BoardModel &Runtime::model() const { return model_; }

const Harness &Runtime::harness() const { return harness_; }

const std::string &Runtime::serial_output() const { return Serial.output(); }

const std::string &Runtime::serial1_output() const { return Serial1.output(); }

const std::string &Runtime::sd_content(const std::string &path) const {
  return SD.content(path);
}

LedState Runtime::led_state() const {
  LedState state;
  const auto red = static_cast<std::uint8_t>(model_.arduino_pin("LED_R"));
  const auto green = static_cast<std::uint8_t>(model_.arduino_pin("LED_G"));
  const auto blue = static_cast<std::uint8_t>(model_.arduino_pin("LED_B"));
  state.red = pin_mode(red) == kOutputMode && pin_value(red) == 0;
  state.green = pin_mode(green) == kOutputMode && pin_value(green) == 0;
  state.blue = pin_mode(blue) == kOutputMode && pin_value(blue) == 0;
  return state;
}

bool Runtime::wire_begun() const { return Wire2.begun(); }

bool Runtime::expander_accessed() const { return expander_.accessed; }

std::size_t Runtime::uart_unrouted_frames() const {
  return uart_unrouted_frames_;
}

std::size_t Runtime::uart_framing_errors() const {
  return uart_framing_errors_;
}

std::size_t Runtime::uart_contention_frames() const {
  return uart_contention_frames_;
}

std::uint8_t Runtime::expander_direction(std::size_t port) const {
  if (port >= expander_.directions.size()) {
    throw std::out_of_range("expander port index");
  }
  return expander_.directions[port];
}

std::uint64_t Runtime::last_expander_inputs() const {
  return expander_.last_inputs;
}

std::uint64_t Runtime::elapsed_ms() const {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now_).count());
}

AnalogStimulus Runtime::analog_stimulus() const {
  const auto leds = led_state();
  AnalogStimulus stimulus;
  stimulus.led_red_on = leds.red;
  stimulus.led_green_on = leds.green;
  stimulus.led_blue_on = leds.blue;
  stimulus.i2c_sda_low = false;
  stimulus.i2c_scl_low = false;
  if (Serial1.begun()) {
    stimulus.uart_tx_voltage = 3.3;
  }
  return stimulus;
}

AnalogObservation Runtime::simulate_analog(const NgSpiceSimulator &simulator,
                                           const AnalogFixture &fixture) const {
  return simulator.run(fixture, analog_stimulus());
}

bool Runtime::expander_available() const {
  return Wire2.begun() && !expander_.reset_asserted;
}

void Runtime::reset_expander_state() {
  expander_ = {};
  expander_.reset_asserted = true;
  expander_.directions.fill(0xFF);
  for (auto &bank : expander_.drive_modes) {
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
    for (auto &bank : expander_.drive_modes) {
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

HardwareSerial Serial(HardwareSerial::Port::Debug);
HardwareSerial Serial1(HardwareSerial::Port::Uart1);
TwoWire Wire2;
SDClass SD;

} // namespace host_sim
