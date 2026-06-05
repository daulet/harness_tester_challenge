#pragma once

#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "host_simulator/analog.h"
#include "host_simulator/board.h"

namespace host_sim {

using SimTime = std::chrono::microseconds;
using EventAction = std::function<void()>;

class Runtime;

Runtime &active_runtime();
void set_active_runtime(Runtime *runtime);

class PrintSink {
public:
  virtual ~PrintSink() = default;

  std::size_t print(const char *value);
  std::size_t print(const std::string &value);
  std::size_t print(char value);
  std::size_t print(int value);
  std::size_t print(unsigned int value);
  std::size_t print(long value);
  std::size_t print(unsigned long value);
  std::size_t print(long long value);
  std::size_t print(unsigned long long value);
  std::size_t print(bool value);

  std::size_t println();
  std::size_t println(const char *value);
  std::size_t println(const std::string &value);
  std::size_t println(char value);
  std::size_t println(int value);
  std::size_t println(bool value);
  int printf(const char *format, ...);

protected:
  virtual std::size_t write_bytes(const std::string &bytes) = 0;
};

class HardwareSerial : public PrintSink {
public:
  enum class Port {
    Debug,
    Uart1,
  };

  explicit HardwareSerial(Port port = Port::Debug);
  void begin(unsigned long baud);
  int available() const;
  int read();
  void push_rx(const std::string &bytes);
  void clear();
  const std::string &output() const;
  bool begun() const;
  unsigned long baud() const;

protected:
  std::size_t write_bytes(const std::string &bytes) override;

private:
  Port port_;
  unsigned long baud_ = 0;
  std::string rx_;
  std::string tx_;
  std::size_t rx_offset_ = 0;
};

class File : public PrintSink {
public:
  File() = default;
  explicit File(std::shared_ptr<std::string> content);

  explicit operator bool() const;
  void close();

protected:
  std::size_t write_bytes(const std::string &bytes) override;

private:
  std::shared_ptr<std::string> content_;
  bool open_ = false;
};

class SDClass {
public:
  bool begin(std::uint8_t cs);
  File open(const char *path, std::uint8_t mode);
  void set_available(bool available);
  void clear();
  const std::string &content(const std::string &path) const;

private:
  bool available_ = true;
  std::unordered_map<std::string, std::shared_ptr<std::string>> files_;
  std::string empty_;
};

class TwoWire {
public:
  void begin();
  void setClock(std::uint32_t clock_hz);
  void beginTransmission(std::uint8_t address);
  std::size_t write(std::uint8_t value);
  std::uint8_t endTransmission(bool send_stop = true);
  std::uint8_t requestFrom(std::uint8_t address, std::uint8_t quantity);
  int read();
  void clear();
  bool begun() const;

private:
  bool begun_ = false;
  std::uint32_t clock_hz_ = 0;
  std::uint8_t address_ = 0;
  std::vector<std::uint8_t> tx_;
  std::vector<std::uint8_t> rx_;
  std::size_t rx_offset_ = 0;
};

struct LedState {
  bool red = false;
  bool green = false;
  bool blue = false;
};

class Runtime {
public:
  explicit Runtime(BoardModel model);
  ~Runtime();

  Runtime(const Runtime &) = delete;
  Runtime &operator=(const Runtime &) = delete;

  void set_harness(Harness harness);
  void set_button_pressed(bool pressed);
  void inject_serial1_rx(const std::string &bytes);
  void transmit_gps(const std::string &bytes,
                    unsigned long baud = 9600,
                    SimTime start_delay = SimTime::zero());
  void set_sd_available(bool available);
  void clear_peripherals();

  void schedule_at(SimTime due, EventAction action);
  void schedule_after(SimTime delay, EventAction action);
  void advance_to(SimTime target);
  void advance_by(SimTime duration);
  SimTime now() const;

  void pin_mode(std::uint8_t pin, std::uint8_t mode);
  void digital_write(std::uint8_t pin, std::uint8_t value);
  int digital_read(std::uint8_t pin) const;
  std::uint8_t pin_mode(std::uint8_t pin) const;
  std::uint8_t pin_value(std::uint8_t pin) const;
  void delay(std::uint32_t milliseconds);

  bool i2c_write(std::uint8_t address, const std::vector<std::uint8_t> &bytes);
  std::vector<std::uint8_t> i2c_read(std::uint8_t address,
                                     std::uint8_t quantity);
  void mark_wire_begun();

  const BoardModel &model() const;
  const Harness &harness() const;
  const std::string &serial_output() const;
  const std::string &serial1_output() const;
  const std::string &sd_content(const std::string &path) const;
  LedState led_state() const;
  bool wire_begun() const;
  bool expander_accessed() const;
  std::size_t uart_unrouted_frames() const;
  std::size_t uart_framing_errors() const;
  std::size_t uart_contention_frames() const;
  std::uint8_t expander_direction(std::size_t port) const;
  std::uint64_t last_expander_inputs() const;
  std::uint64_t elapsed_ms() const;
  AnalogStimulus analog_stimulus() const;
  AnalogObservation simulate_analog(const NgSpiceSimulator &simulator,
                                    const AnalogFixture &fixture) const;

private:
  struct PinState {
    std::uint8_t mode = 0;
    std::uint8_t value = 0;
  };

  struct ExpanderState {
    bool reset_asserted = true;
    bool accessed = false;
    std::uint8_t register_pointer = 0;
    std::uint8_t selected_port = 0;
    std::array<std::uint8_t, 8> outputs{};
    std::array<std::uint8_t, 8> directions{};
    std::array<std::array<std::uint8_t, 8>, 7> drive_modes{};
    std::uint64_t last_inputs = 0;
  };

  struct Event {
    SimTime due{};
    std::uint64_t sequence = 0;
    EventAction action;
  };

  struct EventLater {
    bool operator()(const Event &left, const Event &right) const;
  };

  enum class UartDriver {
    Mcu,
    Gps,
  };

  struct UartFrame {
    UartDriver driver = UartDriver::Gps;
    std::string net;
    SimTime start{};
    SimTime end{};
    unsigned long baud = 0;
    char value = 0;
    bool routed_to_mcu_rx = false;
    bool shares_mcu_tx = false;
    bool shares_gps_tx = false;
    bool receiver_ready = false;
    bool collided = false;
  };

  bool expander_available() const;
  void reset_expander_state();
  std::uint8_t read_expander_register(std::uint8_t reg);
  void write_expander_register(std::uint8_t reg, std::uint8_t value);
  std::array<ExpanderPinDrive, kExpanderPins> expander_drives() const;
  void transmit_serial1(const std::string &bytes, unsigned long baud);
  void schedule_uart(UartDriver driver,
                     const std::string &bytes,
                     unsigned long baud,
                     SimTime start_delay);
  void finish_uart_frame(const std::shared_ptr<UartFrame> &frame);

  friend class HardwareSerial;

  BoardModel model_;
  Harness harness_;
  std::unordered_map<std::uint8_t, PinState> pins_;
  bool button_pressed_ = false;
  SimTime now_{};
  std::uint64_t next_event_sequence_ = 0;
  std::priority_queue<Event, std::vector<Event>, EventLater> events_;
  bool advancing_ = false;
  std::vector<std::shared_ptr<UartFrame>> active_uart_frames_;
  SimTime mcu_uart_ready_{};
  SimTime gps_uart_ready_{};
  bool gps_uart_enabled_ = true;
  std::size_t uart_unrouted_frames_ = 0;
  std::size_t uart_framing_errors_ = 0;
  std::size_t uart_contention_frames_ = 0;
  ExpanderState expander_;
};

extern HardwareSerial Serial;
extern HardwareSerial Serial1;
extern TwoWire Wire2;
extern SDClass SD;

} // namespace host_sim
