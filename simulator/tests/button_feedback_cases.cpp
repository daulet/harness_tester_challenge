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

host_sim::BoardModel
board_model(const std::string &pcb_path =
                root_path("kicad_files/hardware_challenge.kicad_pcb")) {
  return host_sim::BoardModel::load(
      pcb_path, root_path("kicad_files/hardware_challenge.kicad_sch"));
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

host_sim::ElectricalSnapshot button_snapshot(host_sim::ElectricalLevel level) {
  return {host_sim::ElectricalLevel::High, host_sim::ElectricalLevel::High,
          level, {}};
}

class FixedButtonFeedback final : public host_sim::ElectricalFeedback {
public:
  explicit FixedButtonFeedback(host_sim::ElectricalLevel level)
      : snapshot_(button_snapshot(level)) {}

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

class ContactButtonFeedback final : public host_sim::ElectricalFeedback {
public:
  host_sim::ElectricalSnapshot
  solve(const host_sim::AnalogStimulus &stimulus) const override {
    ++solve_count_;
    return button_snapshot(stimulus.button_contact_closed.value_or(false)
                               ? host_sim::ElectricalLevel::Low
                               : host_sim::ElectricalLevel::High);
  }

  std::size_t solve_count() const { return solve_count_; }

private:
  mutable std::size_t solve_count_ = 0;
};

bool run_source_topology() {
  auto single_switch_leg_source = test_support::read_file(
      root_path("kicad_files/hardware_challenge.kicad_pcb"));
  test_support::remove_segment_by_uuid(single_switch_leg_source,
                                       "8847a5b8-bca2-4574-8f3b-4522130a9379");
  const auto single_switch_leg_pcb =
      root_path("build/mutated_button_feedback_one_switch_leg.kicad_pcb");
  test_support::write_file(single_switch_leg_pcb, single_switch_leg_source);
  const auto single_switch_leg_model = board_model(single_switch_leg_pcb);
  const auto single_switch_leg_config =
      host_sim::BoardElectricalConfig::button_from_board(
          single_switch_leg_model);

  auto open_switch_source = test_support::read_file(
      root_path("kicad_files/hardware_challenge.kicad_pcb"));
  test_support::remove_segment_by_uuid(open_switch_source,
                                       "b750739f-e20b-4a9e-87bd-6388b81bb0c4");
  const auto open_switch_pcb =
      root_path("build/mutated_button_feedback_open_switch.kicad_pcb");
  test_support::write_file(open_switch_pcb, open_switch_source);
  const auto open_switch_model = board_model(open_switch_pcb);
  const auto open_switch_config =
      host_sim::BoardElectricalConfig::button_from_board(open_switch_model);

  return require(single_switch_leg_model.pcb_connected("BTN_TEST", "SW1", "2",
                                                       "U2", "10") &&
                     single_switch_leg_config.button_switch_path_ohm < 0.01,
                 "one surviving duplicate SW1 land was not recognized") &&
         require(!open_switch_model.pcb_connected("BTN_TEST", "SW1", "2", "U2",
                                                  "10") &&
                     open_switch_config.button_switch_path_ohm > 1e11,
                 "removed SW1 common route still produced a switch path");
}

bool throws_button_level(host_sim::ElectricalLevel level,
                         const std::string &expected, bool pressed) {
  host_sim::Runtime runtime(board_model());
  runtime.set_button_pressed(pressed);
  runtime.set_electrical_feedback(std::make_shared<FixedButtonFeedback>(level));
  const auto button =
      static_cast<std::uint8_t>(runtime.model().arduino_pin("BTN_TEST"));
  try {
    static_cast<void>(runtime.digital_read(button));
  } catch (const std::runtime_error &error) {
    return require(std::string(error.what()).find(expected) !=
                       std::string::npos,
                   "button electrical error did not identify its cause") &&
           require(runtime.now() == host_sim::SimTime::zero(),
                   "invalid button read advanced simulated time") &&
           require(runtime.button_pressed() == pressed,
                   "invalid button read changed contact intent");
  }
  return require(false, "unsupported button level did not fail");
}

bool run_runtime_feedback() {
  using namespace std::chrono_literals;

  const auto button_pin = [](const host_sim::Runtime &runtime) {
    return static_cast<std::uint8_t>(runtime.model().arduino_pin("BTN_TEST"));
  };

  {
    host_sim::Runtime runtime(board_model());
    const auto button = button_pin(runtime);
    runtime.set_button_pressed(false);
    if (!require(runtime.digital_read(button) == 1,
                 "fast button fallback did not report released High")) {
      return false;
    }
    runtime.set_button_pressed(true);
    if (!require(runtime.digital_read(button) == 0,
                 "fast button fallback did not report pressed Low")) {
      return false;
    }
  }

  {
    host_sim::Runtime runtime(board_model());
    const auto button = button_pin(runtime);
    const auto low =
        std::make_shared<FixedButtonFeedback>(host_sim::ElectricalLevel::Low);
    runtime.set_button_pressed(false);
    runtime.set_electrical_feedback(low);
    if (!require(runtime.digital_read(button) == 0 && low->solve_count() == 1 &&
                     !runtime.button_pressed() && runtime.now() == 0us,
                 "solved Low did not override released mechanical intent")) {
      return false;
    }
  }

  {
    host_sim::Runtime runtime(board_model());
    const auto button = button_pin(runtime);
    const auto high =
        std::make_shared<FixedButtonFeedback>(host_sim::ElectricalLevel::High);
    runtime.set_button_pressed(true);
    runtime.set_electrical_feedback(high);
    if (!require(runtime.digital_read(button) == 1 &&
                     high->solve_count() == 1 && runtime.button_pressed() &&
                     runtime.now() == 0us,
                 "solved High did not override pressed mechanical intent")) {
      return false;
    }
  }

  {
    host_sim::Runtime runtime(board_model());
    const auto button = button_pin(runtime);
    const auto contact = std::make_shared<ContactButtonFeedback>();
    runtime.set_electrical_feedback(contact);
    runtime.schedule_button_state(true, 1ms);
    runtime.schedule_button_state(false, 2ms);
    if (!require(runtime.digital_read(button) == 1,
                 "released contact did not solve High")) {
      return false;
    }
    runtime.advance_to(1ms);
    if (!require(runtime.digital_read(button) == 0,
                 "scheduled press did not solve Low")) {
      return false;
    }
    runtime.advance_to(2ms);
    if (!require(runtime.digital_read(button) == 1 &&
                     contact->solve_count() == 3,
                 "scheduled release did not solve High")) {
      return false;
    }
  }

  {
    host_sim::Runtime runtime(board_model());
    const auto button = button_pin(runtime);
    runtime.pin_mode(button, 2);
    runtime.set_electrical_feedback(
        std::make_shared<FixedButtonFeedback>(host_sim::ElectricalLevel::High));
    if (!require(runtime.digital_read(button) == 1,
                 "INPUT_PULLUP button read was rejected")) {
      return false;
    }
    runtime.pin_mode(button, 1);
    bool rejected = false;
    try {
      static_cast<void>(runtime.digital_read(button));
    } catch (const std::runtime_error &error) {
      rejected = require(std::string(error.what()).find("input mode") !=
                             std::string::npos,
                         "button output-mode diagnostic was imprecise");
    }
    if (!require(rejected, "button output mode was accepted by feedback")) {
      return false;
    }
  }
  return throws_button_level(host_sim::ElectricalLevel::Indeterminate,
                             "BTN_TEST", false) &&
         throws_button_level(host_sim::ElectricalLevel::Overvoltage, "BTN_TEST",
                             true);
}

bool run_ngspice_feedback() {
  const auto model = board_model();
  const auto config = host_sim::BoardElectricalConfig::from_board(model);
  bool ok =
      require(std::abs(config.button_pullup_ohm - 10000.0) < 1.0,
              "R4 value did not reach the button electrical config") &&
      require(config.button_switch_path_ohm < 0.01,
              "SW1 physical path was not derived") &&
      require(model.pcb_connected("BTN_TEST", "R4", "2", "U2", "10") &&
                  model.pcb_connected("+3.3V", "R4", "1", "U2", "15") &&
                  model.pcb_connected("BTN_TEST", "SW1", "2", "U2", "10") &&
                  model.pcb_connected("GND", "SW1", "1", "U2", "1"),
              "button source endpoints were not physically connected");

  {
    host_sim::Runtime runtime(board_model());
    const auto button =
        static_cast<std::uint8_t>(runtime.model().arduino_pin("BTN_TEST"));
    runtime.configure_electrical_feedback(simulator(), fixture());
    runtime.set_button_pressed(false);
    ok &= require(runtime.digital_read(button) == 1,
                  "source-derived released button did not solve High");
    runtime.set_button_pressed(true);
    ok &= require(runtime.digital_read(button) == 0,
                  "source-derived pressed button did not solve Low");
  }

  {
    auto shorted = fixture();
    shorted.parameters["button_short_ground_ohm"] = 0.2;
    host_sim::Runtime runtime(board_model());
    const auto button =
        static_cast<std::uint8_t>(runtime.model().arduino_pin("BTN_TEST"));
    runtime.configure_electrical_feedback(simulator(), std::move(shorted));
    runtime.set_button_pressed(false);
    ok &= require(runtime.digital_read(button) == 0,
                  "external button short did not override released contact");
  }

  auto missing_pull_source = test_support::read_file(
      root_path("kicad_files/hardware_challenge.kicad_pcb"));
  test_support::remove_segment_by_uuid(missing_pull_source,
                                       "d2d83f25-74e7-42e1-89dc-a5ed4edc1a76");
  const auto missing_pull_pcb =
      root_path("build/mutated_button_feedback_missing_pull.kicad_pcb");
  test_support::write_file(missing_pull_pcb, missing_pull_source);
  const auto missing_pull_model = board_model(missing_pull_pcb);
  const auto missing_pull_config =
      host_sim::BoardElectricalConfig::button_from_board(missing_pull_model);
  ok &= require(missing_pull_config.button_pullup_ohm > 1e11,
                "removed R4 rail copper still produced a pull-up");
  host_sim::Runtime weak_pull(missing_pull_model);
  const auto weak_button =
      static_cast<std::uint8_t>(weak_pull.model().arduino_pin("BTN_TEST"));
  weak_pull.configure_electrical_feedback(simulator(), fixture());
  bool weak_pull_rejected = false;
  try {
    static_cast<void>(weak_pull.digital_read(weak_button));
  } catch (const std::runtime_error &error) {
    weak_pull_rejected =
        require(std::string(error.what()).find("BTN_TEST") != std::string::npos,
                "weak source-derived pull diagnostic missed BTN_TEST");
  }
  ok &= require(weak_pull_rejected,
                "missing source-derived pull did not enter invalid band");
  if (weak_pull.i2c_trace().size() != 0) {
    return require(false, "button solve unexpectedly created I2C traffic");
  }

  auto open_switch_source = test_support::read_file(
      root_path("kicad_files/hardware_challenge.kicad_pcb"));
  test_support::remove_segment_by_uuid(open_switch_source,
                                       "b750739f-e20b-4a9e-87bd-6388b81bb0c4");
  const auto open_switch_pcb =
      root_path("build/mutated_button_feedback_open_switch.kicad_pcb");
  test_support::write_file(open_switch_pcb, open_switch_source);
  const auto open_switch_model = board_model(open_switch_pcb);
  const auto open_switch_config =
      host_sim::BoardElectricalConfig::button_from_board(open_switch_model);
  ok &= require(open_switch_config.button_switch_path_ohm > 1e11,
                "removed SW1 signal copper still produced a switch path");
  host_sim::Runtime open_switch(open_switch_model);
  const auto open_button =
      static_cast<std::uint8_t>(open_switch.model().arduino_pin("BTN_TEST"));
  open_switch.configure_electrical_feedback(simulator(), fixture());
  open_switch.set_button_pressed(true);
  ok &= require(open_switch.digital_read(open_button) == 1,
                "open SW1 route still pulled the MCU input Low");
  return ok;
}

} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 2) {
      throw std::invalid_argument(
          "usage: button_feedback_cases runtime|ngspice");
    }
    const std::string test_case = argv[1];
    if (test_case == "runtime") {
      const bool ok = run_source_topology() && run_runtime_feedback();
      return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (test_case == "ngspice") {
      if (!simulator().available()) {
        std::cerr << "ngspice unavailable; button feedback case skipped\n";
        return 77;
      }
      return run_ngspice_feedback() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    throw std::invalid_argument("unknown button feedback case: " + test_case);
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n";
    return EXIT_FAILURE;
  }
}
