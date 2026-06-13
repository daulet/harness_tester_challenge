#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "board_fixture.h"
#include "host_simulator/analog.h"
#include "host_simulator/runtime.h"

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

#ifndef HOST_SIM_NGSPICE
#define HOST_SIM_NGSPICE "ngspice"
#endif

std::string root_path(const std::string &relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

host_sim::BoardModel board_model(
    const std::string &pcb_path =
        root_path("kicad_files/hardware_challenge.kicad_pcb")) {
  return host_sim::BoardModel::load(
      pcb_path,
      root_path("kicad_files/hardware_challenge.kicad_sch"));
}

host_sim::AnalogFixture fixture() {
  return host_sim::AnalogFixture::load(
      root_path("simulator/ngspice/fixtures/nominal.fixture"));
}

host_sim::NgSpiceSimulator simulator() {
  return host_sim::NgSpiceSimulator(root_path("simulator"), HOST_SIM_NGSPICE);
}

bool require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

std::uint8_t select_device_id() {
  host_sim::Wire2.begin();
  host_sim::Wire2.setClock(100000);
  host_sim::Wire2.beginTransmission(0x20);
  host_sim::Wire2.write(0x2E);
  return host_sim::Wire2.endTransmission(false);
}

class FixedElectricalFeedback final : public host_sim::ElectricalFeedback {
public:
  explicit FixedElectricalFeedback(host_sim::ElectricalSnapshot snapshot)
      : snapshot_(snapshot) {}

  host_sim::ElectricalSnapshot
  solve(const host_sim::AnalogStimulus &) const override {
    ++solve_count_;
    return snapshot_;
  }

  std::size_t solve_count() const { return solve_count_; }

private:
  host_sim::ElectricalSnapshot snapshot_;
  mutable std::size_t solve_count_ = 0;
};

bool run_actual_sda_fault() {
  using namespace std::chrono_literals;

  host_sim::Runtime runtime(board_model());
  runtime.configure_electrical_feedback(simulator(), fixture());

  const auto result = select_device_id();
  const auto &trace = runtime.i2c_trace();
  return require(result == 4, "solved SDA pull-down did not block I2C START") &&
         require(runtime.now() == 0us,
                 "electrical solve unexpectedly advanced runtime time") &&
         require(trace.size() == 1 && trace[0].sda_stuck_low &&
                     !trace[0].scl_stuck_low,
                 "solved SDA fault was not independently attributed");
}

bool run_source_derived_thresholds() {
  const auto indeterminate_pcb = test_support::component_value_pcb(
      HOST_SIM_ROOT, "R2", "4k7", "1e12",
      "mutated_electrical_feedback_r2_1e12.kicad_pcb");
  const auto low_pcb = test_support::component_value_pcb(
      HOST_SIM_ROOT, "R2", "4k7", "1e15",
      "mutated_electrical_feedback_r2_1e15.kicad_pcb");

  host_sim::NgSpiceElectricalFeedback actual(board_model(), simulator(),
                                             fixture());
  host_sim::NgSpiceElectricalFeedback indeterminate(
      board_model(indeterminate_pcb), simulator(), fixture());
  host_sim::NgSpiceElectricalFeedback low(board_model(low_pcb), simulator(),
                                          fixture());
  const auto actual_snapshot = actual.solve({});
  const auto indeterminate_snapshot = indeterminate.solve({});
  const auto low_snapshot = low.solve({});
  bool ok =
      require(actual_snapshot.i2c_scl == host_sim::ElectricalLevel::High,
              "physical R2 value did not solve SCL High") &&
      require(indeterminate_snapshot.i2c_scl ==
                  host_sim::ElectricalLevel::Indeterminate,
              "1e12-ohm physical R2 did not cross into the invalid band") &&
      require(low_snapshot.i2c_scl == host_sim::ElectricalLevel::Low,
              "1e15-ohm physical R2 did not solve SCL Low");

  host_sim::Runtime runtime(board_model(indeterminate_pcb));
  runtime.configure_electrical_feedback(simulator(), fixture());
  try {
    static_cast<void>(select_device_id());
  } catch (const std::runtime_error &error) {
    ok &= require(std::string(error.what()).find("SCL") != std::string::npos,
                  "source-derived invalid SCL diagnostic missed the line");
    ok &= require(runtime.i2c_trace().empty(),
                  "source-derived invalid SCL produced an I2C transaction");
    return ok;
  }
  return require(false,
                 "source-derived invalid SCL did not reach Wire START gating");
}

bool run_feedback_override_and_scl_attribution() {
  using namespace std::chrono_literals;

  {
    host_sim::Runtime runtime(board_model());
    if (!require(select_device_id() == 4,
                 "parsed-board control did not reproduce the SDA fault")) {
      return false;
    }
  }

  {
    host_sim::Runtime runtime(board_model());
    const auto high =
        std::make_shared<FixedElectricalFeedback>(host_sim::ElectricalSnapshot{
            host_sim::ElectricalLevel::High, host_sim::ElectricalLevel::High,
            host_sim::ElectricalLevel::Indeterminate, {}});
    runtime.set_electrical_feedback(high);
    bool ok = require(select_device_id() == 0 && runtime.now() == 190us,
                      "High/High feedback did not permit I2C START");
    ok &= require(host_sim::Wire2.requestFrom(0x20, 1) == 1 &&
                      host_sim::Wire2.read() == 0x60 && runtime.now() == 390us,
                  "High/High feedback did not expose the device ID");
    ok &= require(high->solve_count() == 2,
                  "runtime did not solve before both I2C transactions");
    if (!ok) {
      return false;
    }
  }

  host_sim::Runtime runtime(board_model());
  const auto scl_low =
      std::make_shared<FixedElectricalFeedback>(host_sim::ElectricalSnapshot{
          host_sim::ElectricalLevel::High, host_sim::ElectricalLevel::Low,
          host_sim::ElectricalLevel::Indeterminate, {}});
  runtime.set_electrical_feedback(scl_low);
  const auto result = select_device_id();
  const auto &trace = runtime.i2c_trace();
  return require(result == 4, "solved SCL pull-down did not block I2C START") &&
         require(runtime.now() == 0us,
                 "SCL electrical solve unexpectedly advanced runtime time") &&
         require(trace.size() == 1 && !trace[0].sda_stuck_low &&
                     trace[0].scl_stuck_low,
                 "solved SCL fault was not independently attributed") &&
         require(scl_low->solve_count() == 1,
                 "SCL fault did not use the feedback solver");
}

bool run_peripheral_open_is_address_nack() {
  using namespace std::chrono_literals;

  auto source = test_support::read_file(
      root_path("kicad_files/hardware_challenge.kicad_pcb"));
  test_support::remove_segment_by_uuid(
      source, "4616ad31-8b09-41dc-b2a3-b246ea7cba68");
  const auto pcb = root_path("build/mutated_electrical_feedback_u4_scl.kicad_pcb");
  test_support::write_file(pcb, source);
  const auto model = board_model(pcb);
  const auto config = host_sim::BoardElectricalConfig::i2c_from_board(model);
  if (!require(std::abs(config.i2c_scl_pullup_ohm - 4700.0) < 1.0,
               "peripheral-side open incorrectly removed controller pull-up")) {
    return false;
  }

  host_sim::Runtime runtime(model);
  runtime.set_electrical_feedback(std::make_shared<FixedElectricalFeedback>(
      host_sim::ElectricalSnapshot{host_sim::ElectricalLevel::High,
                                   host_sim::ElectricalLevel::High,
                                   host_sim::ElectricalLevel::Indeterminate,
                                   {}}));
  const auto result = select_device_id();
  const auto &trace = runtime.i2c_trace();
  return require(result == 2 && runtime.now() == 100us,
                 "peripheral-side open did not produce address NACK timing") &&
         require(trace.size() == 1 &&
                     trace[0].status == host_sim::I2cStatus::AddressNack &&
                     !trace[0].sda_stuck_low && !trace[0].scl_stuck_low,
                 "peripheral-side open was misclassified as a line fault");
}

bool run_invalid_line_not_masked_by_low_peer() {
  host_sim::Runtime runtime(board_model());
  runtime.set_electrical_feedback(std::make_shared<FixedElectricalFeedback>(
      host_sim::ElectricalSnapshot{host_sim::ElectricalLevel::Indeterminate,
                                   host_sim::ElectricalLevel::Low,
                                   host_sim::ElectricalLevel::Indeterminate,
                                   {}}));
  try {
    static_cast<void>(select_device_id());
  } catch (const std::runtime_error &error) {
    return require(std::string(error.what()).find("SDA") != std::string::npos,
                   "invalid SDA diagnostic did not identify the line") &&
           require(runtime.i2c_trace().empty(),
                   "invalid electrical level produced an I2C transaction");
  }
  return require(false, "SCL Low masked an invalid solved SDA level");
}

} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 2) {
      throw std::invalid_argument(
          "usage: electrical_feedback_cases runtime|ngspice");
    }
    const std::string test_case = argv[1];
    if (test_case == "runtime") {
      const bool ok = run_feedback_override_and_scl_attribution() &&
                      run_peripheral_open_is_address_nack() &&
                      run_invalid_line_not_masked_by_low_peer();
      return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (test_case == "ngspice") {
      if (!simulator().available()) {
        std::cerr << "ngspice unavailable; electrical feedback case skipped\n";
        return 77;
      }
      const bool ok = run_actual_sda_fault() && run_source_derived_thresholds();
      return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    throw std::invalid_argument("unknown electrical feedback case: " +
                                test_case);
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n";
    return EXIT_FAILURE;
  }
}
