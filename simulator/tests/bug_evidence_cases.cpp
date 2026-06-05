#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "CY8C9560.h"
#include "host_simulator/runtime.h"

extern void setup();
extern void loop();
extern bool time_fixed;
extern int nmea_idx;
extern std::uint64_t EXPECTED_CONNECTIONS[];

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

constexpr const char* kValidGprmc =
    "$GPRMC,123519,A,0,N,0,E,0,0,230394\r\n";
constexpr const char* kInvalidGprmc =
    "$GPRMC,123519,V,0,N,0,E,0,0,230394\r\n";
constexpr const char* kValidGnrmc =
    "$GNRMC,123519,A,0,N,0,E,0,0,230394\r\n";

std::string data_path(const std::string& relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

host_sim::BoardModel board_model() {
  return host_sim::BoardModel::load(
      data_path("kicad_files/hardware_challenge.kicad_pcb"),
      data_path("kicad_files/hardware_challenge.kicad_sch"));
}

host_sim::Harness expected_harness() {
  host_sim::Harness harness;
  for (std::size_t left = 0; left < host_sim::kHarnessPins; ++left) {
    for (std::size_t right = left + 1; right < host_sim::kHarnessPins; ++right) {
      if (EXPECTED_CONNECTIONS[left] & (1ULL << right)) {
        harness.connect(left, right);
      }
    }
  }
  return harness;
}

host_sim::Harness one_matching_row_harness() {
  host_sim::Harness harness;
  harness.connect(0, 13);
  harness.connect(0, 39);
  return harness;
}

host_sim::Harness one_exact_row_harness() {
  host_sim::Harness harness;
  // CBL_22 is raw expander bit 26 on the as-drawn board, so this gives row 13
  // exactly the oracle bits 0 and 26 while later rows still fail.
  harness.connect(0, 13);
  harness.connect(13, 22);
  return harness;
}

bool contains(const std::string& value, const std::string& needle) {
  return value.find(needle) != std::string::npos;
}

std::size_t count_lines_with(const std::string& value, const std::string& needle) {
  std::size_t count = 0;
  std::size_t offset = 0;
  while ((offset = value.find(needle, offset)) != std::string::npos) {
    ++count;
    offset += needle.size();
  }
  return count;
}

bool require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

std::unique_ptr<host_sim::Runtime> make_runtime(host_sim::Harness harness = {}) {
  auto runtime = std::make_unique<host_sim::Runtime>(board_model());
  runtime->set_harness(std::move(harness));
  return runtime;
}

void prepare_for_test(host_sim::Runtime& runtime, const std::string& nmea,
                      bool physically_pressed = false) {
  runtime.set_button_pressed(physically_pressed);
  runtime.inject_gps(nmea);
  setup();
  loop();
}

bool run_a01_missing_begin() {
  auto runtime = make_runtime(expected_harness());
  prepare_for_test(*runtime, kValidGprmc);
  return require(!runtime->wire_begun(),
                 "A01: original firmware unexpectedly initialized Wire2") &&
      require(!runtime->expander_accessed(),
              "A01: original firmware unexpectedly accessed the expander");
}

bool run_a02_missing_output_modes() {
  auto runtime = make_runtime();
  setup();
  const auto safeboot = static_cast<std::uint8_t>(runtime->model().arduino_pin("UBX_SAFEBOOT"));
  const auto reset = static_cast<std::uint8_t>(runtime->model().arduino_pin("UBX_RST_N"));
  return require(runtime->pin_mode(safeboot) == INPUT,
                 "A02: UBX_SAFEBOOT was unexpectedly configured as an output") &&
      require(runtime->pin_mode(reset) == INPUT,
              "A02: UBX_RST_N was unexpectedly configured as an output");
}

bool run_a03_nmea_buffer_reaches_end_without_guard() {
  auto runtime = make_runtime();
  runtime->inject_gps(std::string(64, 'x'));
  setup();
  loop();
  return require(nmea_idx == 64,
                 "A03: NMEA staging buffer did not reach its last valid index") &&
      require(!time_fixed,
              "A03: unterminated NMEA bytes unexpectedly produced a time lock");
}

bool run_a04_invalid_status_accepted() {
  auto runtime = make_runtime(expected_harness());
  prepare_for_test(*runtime, kInvalidGprmc);
  return require(time_fixed,
                 "A04: invalid-status GPRMC did not set time_fixed") &&
      require(contains(runtime->serial_output(), "Time received."),
              "A04: invalid-status GPRMC did not produce a time-lock message");
}

bool run_a06_all_outputs() {
  auto runtime = make_runtime();
  CY8C9560 driver;
  driver.begin();
  driver.set_output(1ULL << 3, 1ULL << 3);
  bool ok = true;
  for (std::size_t port = 0; port < 8; ++port) {
    ok &= require(runtime->expander_direction(port) == 0x00,
                  "A06: set_output did not make every port an output");
  }
  return ok;
}

bool run_a07_selected_output_undone() {
  auto runtime = make_runtime();
  CY8C9560 driver;
  driver.begin();
  const auto selected = 1ULL << 3;
  driver.set_output(selected, selected);
  driver.set_pd_inputs(~selected);
  bool ok = true;
  for (std::size_t port = 0; port < 8; ++port) {
    ok &= require(runtime->expander_direction(port) == 0xFF,
                  "A07: set_pd_inputs did not return every port to input mode");
  }
  return ok;
}

bool run_a08_one_matching_row_passes() {
  auto runtime = make_runtime(one_exact_row_harness());
  prepare_for_test(*runtime, kValidGprmc);
  const auto output = runtime->serial_output();
  return require(contains(output, "Pin 13: 1000000000000000000000000010000000000000"),
                 "A08: matching probe row was not visible") &&
      require(contains(output, "Pin 1: 0000000000000000000000000000000000000000"),
              "A08: later non-matching probe row was not visible") &&
      require(contains(output, "Harness passed!"),
              "A08: one matching probe row did not produce a pass");
}

bool run_a09_failed_status_overwritten() {
  auto runtime = make_runtime(one_matching_row_harness());
  prepare_for_test(*runtime, kValidGprmc);
  const auto red = static_cast<std::uint8_t>(runtime->model().arduino_pin("LED_R"));
  const auto green = static_cast<std::uint8_t>(runtime->model().arduino_pin("LED_G"));
  const bool failed_written = runtime->pin_value(red) == LOW &&
      runtime->pin_value(green) == HIGH;
  runtime->set_button_pressed(true);
  loop();
  return require(failed_written,
                 "A09: failed result did not write FAILED status") &&
      require(runtime->pin_value(red) == HIGH && runtime->pin_value(green) == LOW,
              "A09: next loop did not overwrite FAILED with GOOD before returning");
}

bool run_a17_pressed_button_does_not_run() {
  auto runtime = make_runtime(expected_harness());
  prepare_for_test(*runtime, kValidGprmc, true);
  return require(time_fixed,
                 "A17: valid GPS fix was not established") &&
      require(!contains(runtime->serial_output(), "Harness passed!"),
              "A17: physically pressed button unexpectedly ran a test") &&
      require(runtime->sd_content("results.txt").empty(),
              "A17: physically pressed button unexpectedly logged a result");
}

bool run_a19_gnrmc_ignored() {
  auto runtime = make_runtime(expected_harness());
  prepare_for_test(*runtime, kValidGnrmc);
  return require(!time_fixed,
                 "A19: GNRMC unexpectedly set time_fixed") &&
      require(!contains(runtime->serial_output(), "Time received."),
              "A19: GNRMC unexpectedly produced a time-lock message");
}

bool run_a20_logical_index_gap() {
  auto runtime = make_runtime(expected_harness());
  prepare_for_test(*runtime, kValidGprmc);
  return require(contains(runtime->serial_output(),
                          "Pin 20: 0000000000000000000000000000000000000000"),
                 "A20: logical pin 20 did not demonstrate the register-bit gap");
}

bool run_a21_leds_never_become_outputs() {
  auto runtime = make_runtime();
  setup();
  const auto leds = runtime->led_state();
  const auto red = static_cast<std::uint8_t>(runtime->model().arduino_pin("LED_R"));
  const auto green = static_cast<std::uint8_t>(runtime->model().arduino_pin("LED_G"));
  const auto blue = static_cast<std::uint8_t>(runtime->model().arduino_pin("LED_B"));
  return require(runtime->pin_mode(red) == INPUT,
                 "A21: LED_R was unexpectedly configured as an output") &&
      require(runtime->pin_mode(green) == INPUT,
                 "A21: LED_G was unexpectedly configured as an output") &&
      require(runtime->pin_mode(blue) == INPUT,
                 "A21: LED_B was unexpectedly configured as an output") &&
      require(!leds.red && !leds.green && !leds.blue,
              "A21: LED model showed active drive without output modes");
}

bool run_a23_held_gate_relogs() {
  auto runtime = make_runtime(one_matching_row_harness());
  prepare_for_test(*runtime, kValidGprmc);
  loop();
  const auto& log = runtime->sd_content("results.txt");
  return require(count_lines_with(log, " - ") == 2,
                 "A23: held valid gate did not append two result lines");
}

bool run_case(const std::string& name) {
  if (name == "a01_missing_begin") return run_a01_missing_begin();
  if (name == "a02_missing_output_modes") return run_a02_missing_output_modes();
  if (name == "a03_nmea_buffer_reaches_end_without_guard") {
    return run_a03_nmea_buffer_reaches_end_without_guard();
  }
  if (name == "a04_invalid_status_accepted") return run_a04_invalid_status_accepted();
  if (name == "a06_all_outputs") return run_a06_all_outputs();
  if (name == "a07_selected_output_undone") return run_a07_selected_output_undone();
  if (name == "a08_one_matching_row_passes") return run_a08_one_matching_row_passes();
  if (name == "a09_failed_status_overwritten") return run_a09_failed_status_overwritten();
  if (name == "a17_pressed_button_does_not_run") return run_a17_pressed_button_does_not_run();
  if (name == "a19_gnrmc_ignored") return run_a19_gnrmc_ignored();
  if (name == "a20_logical_index_gap") return run_a20_logical_index_gap();
  if (name == "a21_leds_never_become_outputs") return run_a21_leds_never_become_outputs();
  if (name == "a23_held_gate_relogs") return run_a23_held_gate_relogs();
  throw std::runtime_error("unknown bug evidence case: " + name);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: bug_evidence_cases <case>\n";
    return EXIT_FAILURE;
  }
  try {
    return run_case(argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return EXIT_FAILURE;
  }
}
