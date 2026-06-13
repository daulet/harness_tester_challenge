#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "host_simulator/analog.h"
#include "host_simulator/runtime.h"

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

using namespace std::chrono_literals;

std::string root_path(const std::string &relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

host_sim::BoardModel board_model() {
  return host_sim::BoardModel::load(
      root_path("kicad_files/hardware_challenge.kicad_pcb"),
      root_path("kicad_files/hardware_challenge.kicad_sch"));
}

bool require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

class FixedExpanderFeedback final : public host_sim::ElectricalFeedback {
public:
  FixedExpanderFeedback(std::size_t raw_bit,
                        host_sim::ElectricalLevel level)
      : raw_bit_(raw_bit), level_(level) {}

  host_sim::ElectricalSnapshot
  solve(const host_sim::AnalogStimulus &stimulus) const override {
    ++solve_count_;
    stimuli_.push_back(stimulus);
    host_sim::ElectricalSnapshot snapshot{
        host_sim::ElectricalLevel::High,
        host_sim::ElectricalLevel::High,
        host_sim::ElectricalLevel::High,
        {}};
    snapshot.expander_inputs[raw_bit_] = level_;
    return snapshot;
  }

  std::size_t solve_count() const { return solve_count_; }

  const host_sim::AnalogStimulus &last_stimulus() const {
    return stimuli_.back();
  }

private:
  std::size_t raw_bit_;
  host_sim::ElectricalLevel level_;
  mutable std::size_t solve_count_ = 0;
  mutable std::vector<host_sim::AnalogStimulus> stimuli_;
};

std::size_t raw_bit(const host_sim::Runtime &runtime,
                    std::size_t logical_channel) {
  const auto &channel = runtime.model().channel(logical_channel);
  return channel.expander_port * 8 + channel.expander_bit;
}

void configure_representative_pair(host_sim::Runtime &runtime,
                                   bool driver_high = true) {
  host_sim::Wire2.begin();
  runtime.set_i2c_bus_mode(host_sim::I2cBusMode::Ideal);

  const auto driver = runtime.model().channel(8);
  const auto receiver = runtime.model().channel(31);
  const auto driver_mask =
      static_cast<std::uint8_t>(1U << driver.expander_bit);
  const auto receiver_mask =
      static_cast<std::uint8_t>(1U << receiver.expander_bit);

  runtime.i2c_write(0x20, {0x08, 0, 0, 0, 0, 0, 0, 0, 0});
  for (std::uint8_t port = 0; port < 8; ++port) {
    runtime.i2c_write(0x20, {0x18, port});
    runtime.i2c_write(0x20, {0x1C, 0xFF});
    runtime.i2c_write(0x20, {0x1E, 0xFF});
  }

  runtime.i2c_write(
      0x20, {static_cast<std::uint8_t>(0x08 + driver.expander_port),
             static_cast<std::uint8_t>(driver_high ? driver_mask : 0)});
  runtime.i2c_write(
      0x20, {0x18, static_cast<std::uint8_t>(driver.expander_port)});
  runtime.i2c_write(0x20, {0x1C, static_cast<std::uint8_t>(~driver_mask)});
  runtime.i2c_write(0x20, {0x21, driver_mask});

  runtime.i2c_write(
      0x20, {0x18, static_cast<std::uint8_t>(receiver.expander_port)});
  runtime.i2c_write(0x20, {0x1C, 0xFF});
  runtime.i2c_write(0x20, {0x1E, receiver_mask});
}

std::vector<std::uint8_t> read_registers(host_sim::Runtime &runtime,
                                         std::uint8_t first,
                                         std::uint8_t quantity) {
  runtime.i2c_write(0x20, {first});
  return runtime.i2c_read(0x20, quantity);
}

bool run_transaction_snapshot() {
  host_sim::Runtime runtime(board_model());
  host_sim::Harness harness;
  harness.connect(8, 31);
  runtime.set_harness(std::move(harness));
  configure_representative_pair(runtime);

  const auto receiver = raw_bit(runtime, 31);
  const auto feedback = std::make_shared<FixedExpanderFeedback>(
      receiver, host_sim::ElectricalLevel::High);
  runtime.set_electrical_feedback(feedback);

  const auto inputs = read_registers(runtime, 0, 8);
  const auto &stimulus = feedback->last_stimulus();
  bool ok =
      require(feedback->solve_count() == 1,
              "eight-byte input read did not use one electrical snapshot") &&
      require(inputs.size() == 8 && receiver == 35 &&
                  (inputs[4] & (1U << 3)) != 0 &&
                  (inputs[3] & (1U << 7)) == 0,
              "logical channel 31 did not override raw expander bit 35") &&
      require(stimulus.expander_harness_drive_enabled.value_or(false) &&
                  stimulus.harness_drive_voltage.value_or(-1.0) == 3.3 &&
                  stimulus.expander_harness_receiver_pulldown.value_or(false) &&
                  stimulus.expander_harness_connected.value_or(false) &&
                  stimulus.expander_harness_receiver_bit == receiver,
              "runtime did not export the representative harness state");

  runtime.i2c_write(0x20, {0x09, 0xA5});
  const auto straddled = read_registers(runtime, 5, 5);
  ok &= require(feedback->solve_count() == 2 && straddled.size() == 5 &&
                    straddled[3] == 0 && straddled[4] == 0xA5,
                "straddling input/output read did not preserve register data");

  const auto device_id = read_registers(runtime, 0x2E, 1);
  ok &= require(feedback->solve_count() == 2 && device_id.size() == 1 &&
                    device_id[0] == 0x60,
                "non-input register read triggered electrical sampling");
  return ok;
}

bool run_low_override() {
  host_sim::Runtime runtime(board_model());
  host_sim::Harness harness;
  harness.connect(8, 31);
  runtime.set_harness(std::move(harness));
  configure_representative_pair(runtime);

  const auto receiver = raw_bit(runtime, 31);
  runtime.set_electrical_feedback(std::make_shared<FixedExpanderFeedback>(
      receiver, host_sim::ElectricalLevel::Low));
  const auto inputs = read_registers(runtime, 0, 8);
  return require(inputs.size() == 8 &&
                     (inputs[receiver / 8] & (1U << (receiver % 8))) == 0 &&
                     (runtime.last_expander_inputs() &
                      (std::uint64_t{1} << receiver)) == 0,
                 "solved Low did not override the digital harness fallback");
}

bool run_disabled_driver_state() {
  host_sim::Runtime runtime(board_model());
  host_sim::Harness harness;
  harness.connect(8, 31);
  runtime.set_harness(std::move(harness));
  configure_representative_pair(runtime);

  const auto driver = runtime.model().channel(8);
  const auto driver_mask =
      static_cast<std::uint8_t>(1U << driver.expander_bit);
  runtime.i2c_write(
      0x20, {0x18, static_cast<std::uint8_t>(driver.expander_port)});
  runtime.i2c_write(0x20, {0x1C, 0xFF});
  runtime.i2c_write(0x20, {0x1E, driver_mask});

  const auto receiver = raw_bit(runtime, 31);
  const auto feedback = std::make_shared<FixedExpanderFeedback>(
      receiver, host_sim::ElectricalLevel::Low);
  runtime.set_electrical_feedback(feedback);
  static_cast<void>(read_registers(runtime, 0, 8));
  return require(
      !feedback->last_stimulus().expander_harness_drive_enabled.value_or(true),
      "pull-down driver state was incorrectly exported as an active drive");
}

bool throws_level(host_sim::ElectricalLevel level) {
  host_sim::Runtime runtime(board_model());
  host_sim::Harness harness;
  harness.connect(8, 31);
  runtime.set_harness(std::move(harness));
  configure_representative_pair(runtime);
  runtime.set_electrical_feedback(std::make_shared<FixedExpanderFeedback>(
      raw_bit(runtime, 31), level));
  const auto before = runtime.now();
  try {
    static_cast<void>(read_registers(runtime, 0, 8));
  } catch (const std::runtime_error &error) {
    return require(std::string(error.what()).find("expander input") !=
                       std::string::npos,
                   "invalid expander level diagnostic was imprecise") &&
           require(runtime.now() == before,
                   "direct expander solve advanced simulated time");
  }
  return require(false, "invalid expander level did not fail");
}

bool run_invalid_feedback() {
  return throws_level(host_sim::ElectricalLevel::Indeterminate) &&
         throws_level(host_sim::ElectricalLevel::Overvoltage);
}

} // namespace

int main() {
  try {
    const bool ok = run_transaction_snapshot() && run_low_override() &&
                    run_disabled_driver_state() && run_invalid_feedback();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n";
    return EXIT_FAILURE;
  }
}
