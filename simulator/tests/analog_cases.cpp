#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "host_simulator/analog.h"
#include "host_simulator/board.h"
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

bool require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

host_sim::AnalogFixture fixture(const std::string &name) {
  return host_sim::AnalogFixture::load(
      root_path("simulator/ngspice/fixtures/" + name + ".fixture"));
}

host_sim::NgSpiceSimulator simulator() {
  return host_sim::NgSpiceSimulator(root_path("simulator"), HOST_SIM_NGSPICE);
}

bool run_nominal() {
  const auto result = simulator().run(fixture("nominal"));
  return require(result.measurement("input_nominal_v") > 11.0,
                 "nominal: 12V input did not propagate") &&
         require(result.measurement("protected_nominal_v") > 11.0,
                 "nominal: reverse-protected rail did not propagate") &&
         require(result.measurement("rail5_nominal_v") > 4.8 &&
                     result.measurement("rail5_nominal_v") < 5.2,
                 "nominal: 5V rail was outside first-pass bounds") &&
         require(result.measurement("rail33_nominal_v") > 3.15 &&
                     result.measurement("rail33_nominal_v") < 3.45,
                 "nominal: 3.3V rail was outside first-pass bounds") &&
         require(
             result.measurement("harness_before_v") > 3.0 &&
                 result.measurement("harness_during_v") > 3.0 &&
                 result.measurement("harness_after_v") > 3.0,
             "nominal: harness R/L/C path did not hold a valid high level") &&
         require(result.level("i2c_sda_high_v") ==
                         host_sim::ElectricalLevel::High &&
                     result.level("i2c_scl_high_v") ==
                         host_sim::ElectricalLevel::High,
                 "nominal: I2C pull-ups did not produce high levels") &&
         require(result.level("i2c_sda_low_v") ==
                         host_sim::ElectricalLevel::Low &&
                     result.level("i2c_scl_low_v") ==
                         host_sim::ElectricalLevel::Low,
                 "nominal: I2C open-drain sinks did not produce low levels") &&
         require(result.measurement("led_red_current_a") > 0.001 &&
                     result.measurement("led_red_current_a") < 0.02,
                 "nominal: LED current was outside the representative range") &&
         require(result.level("uart_level_v") ==
                     host_sim::ElectricalLevel::High,
                 "nominal: UART did not produce a valid 3.3V high level");
}

bool run_reverse_polarity() {
  const auto result = simulator().run(fixture("reverse_polarity"));
  return require(std::abs(result.measurement("protected_nominal_v")) < 0.2,
                 "reverse polarity: protected 12V rail was energized") &&
         require(std::abs(result.measurement("rail5_nominal_v")) < 0.2,
                 "reverse polarity: 5V rail was energized") &&
         require(std::abs(result.measurement("rail33_nominal_v")) < 0.2,
                 "reverse polarity: 3.3V rail was energized");
}

bool run_transient_clamp() {
  const auto result = simulator().run(fixture("transient_clamp"));
  return require(result.measurement("input_source_peak_v") > 45.0,
                 "transient clamp: source surge was not applied") &&
         require(result.measurement("input_clamped_peak_v") > 15.0 &&
                     result.measurement("input_clamped_peak_v") < 25.0,
                 "transient clamp: TVS did not bound the board input") &&
         require(
             result.measurement("rail5_nominal_v") > 4.8 &&
                 result.measurement("rail33_nominal_v") > 3.15,
             "transient clamp: downstream rails were not nominal before surge");
}

bool run_open_circuit() {
  const auto result = simulator().run(fixture("open_circuit"));
  return require(result.measurement("harness_before_v") < 0.2,
                 "open circuit: receiver still observed a driven high");
}

bool run_unexpected_short() {
  const auto result = simulator().run(fixture("unexpected_short"));
  return require(result.measurement("harness_before_v") < 0.8,
                 "unexpected short: signal was not forced low") &&
         require(std::abs(result.measurement("harness_source_current_a")) > 0.5,
                 "unexpected short: source current did not expose the short");
}

bool run_leakage() {
  const auto result = simulator().run(fixture("leakage"));
  return require(std::abs(result.measurement("harness_source_current_a")) >
                     0.01,
                 "leakage: source current did not expose low insulation "
                 "resistance") &&
         require(
             result.measurement("harness_before_v") > 2.5,
             "leakage: fixture collapsed into a hard short instead of leakage");
}

bool run_elevated_contact_resistance() {
  const auto result = simulator().run(fixture("elevated_contact_resistance"));
  return require(
      result.measurement("harness_before_v") > 0.8 &&
          result.measurement("harness_before_v") < 2.0,
      "contact resistance: loaded signal did not enter the invalid band");
}

bool run_intermittent_connection() {
  const auto result = simulator().run(fixture("intermittent_connection"));
  return require(result.measurement("harness_before_v") > 3.0,
                 "intermittent: signal was not high before the interruption") &&
         require(result.measurement("harness_during_v") < 0.2,
                 "intermittent: signal did not drop during the interruption") &&
         require(result.measurement("harness_after_v") > 3.0,
                 "intermittent: signal did not recover after the interruption");
}

bool run_bad_pull_network() {
  const auto result = simulator().run(fixture("bad_pull_network"));
  return require(
      result.level("i2c_sda_high_v") ==
              host_sim::ElectricalLevel::Indeterminate &&
          result.level("i2c_scl_high_v") ==
              host_sim::ElectricalLevel::Indeterminate,
      "bad pull network: opposing pulls did not create invalid levels");
}

bool run_led_overcurrent() {
  const auto result = simulator().run(fixture("led_overcurrent"));
  return require(
      result.measurement("led_red_current_a") > 0.05,
      "LED overcurrent: missing series resistance was not observable");
}

bool run_led_no_current() {
  const auto result = simulator().run(fixture("led_no_current"));
  return require(std::abs(result.measurement("led_red_current_a")) < 1e-6,
                 "LED no-current: open LED path still conducted");
}

bool run_uart_level_mismatch() {
  const auto result = simulator().run(fixture("uart_level_mismatch"));
  return require(result.level("uart_level_v") ==
                     host_sim::ElectricalLevel::Overvoltage,
                 "UART mismatch: 5V peer did not create an overvoltage level");
}

bool run_i2c_pull_failure() {
  const auto result = simulator().run(fixture("i2c_pull_failure"));
  return require(
      result.level("i2c_sda_high_v") == host_sim::ElectricalLevel::Low &&
          result.level("i2c_scl_high_v") == host_sim::ElectricalLevel::Low,
      "I2C pull failure: released lines did not remain low");
}

bool run_runtime_control() {
  auto model = host_sim::BoardModel::load(
      root_path("kicad_files/hardware_challenge.kicad_pcb"),
      root_path("kicad_files/hardware_challenge.kicad_sch"));
  host_sim::Runtime runtime(std::move(model));
  const auto red =
      static_cast<std::uint8_t>(runtime.model().arduino_pin("LED_R"));
  runtime.pin_mode(red, 1);
  runtime.digital_write(red, 0);
  host_sim::Serial1.begin(9600);
  const auto result = runtime.simulate_analog(simulator(), fixture("nominal"));
  return require(result.measurement("led_red_current_a") > 0.001,
                 "runtime control: active-low red LED did not reach ngspice") &&
         require(
             std::abs(result.measurement("led_green_current_a")) < 1e-6 &&
                 std::abs(result.measurement("led_blue_current_a")) < 1e-6,
             "runtime control: inactive firmware LEDs conducted in ngspice") &&
         require(result.level("uart_level_v") ==
                     host_sim::ElectricalLevel::High,
                 "runtime control: UART idle-high state did not reach ngspice");
}

bool run_case(const std::string &name) {
  if (name == "nominal")
    return run_nominal();
  if (name == "reverse_polarity")
    return run_reverse_polarity();
  if (name == "transient_clamp")
    return run_transient_clamp();
  if (name == "open_circuit")
    return run_open_circuit();
  if (name == "unexpected_short")
    return run_unexpected_short();
  if (name == "leakage")
    return run_leakage();
  if (name == "elevated_contact_resistance")
    return run_elevated_contact_resistance();
  if (name == "intermittent_connection")
    return run_intermittent_connection();
  if (name == "bad_pull_network")
    return run_bad_pull_network();
  if (name == "led_overcurrent")
    return run_led_overcurrent();
  if (name == "led_no_current")
    return run_led_no_current();
  if (name == "uart_level_mismatch")
    return run_uart_level_mismatch();
  if (name == "i2c_pull_failure")
    return run_i2c_pull_failure();
  if (name == "runtime_control")
    return run_runtime_control();
  throw std::runtime_error("unknown analog case: " + name);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: analog_cases <case>\n";
    return EXIT_FAILURE;
  }
  try {
    const auto ngspice = simulator();
    if (!ngspice.available()) {
      std::cerr << "ngspice unavailable; analog case skipped\n";
      return 77;
    }
    return run_case(argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n";
    return EXIT_FAILURE;
  }
}
