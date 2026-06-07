#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "Arduino.h"
#include "CY8C9560.h"
#include "host_simulator/oracle.h"
#include "host_simulator/runtime.h"
#include "blocker_peeling_harness_fixture.h"

extern void setup();
extern void loop();
extern bool time_fixed;
extern std::uint64_t EXPECTED_CONNECTIONS[];

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

#ifndef BLOCKER_VARIANT_DIR
#error BLOCKER_VARIANT_DIR must be defined by the build.
#endif

constexpr const char *kValidGnrmc =
    "$GNRMC,123519,A,0,N,0,E,0,0,230394\r\n";

bool require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

bool contains(const std::string &value, const std::string &needle) {
  return value.find(needle) != std::string::npos;
}

std::size_t count_occurrences(const std::string &value,
                              const std::string &needle) {
  std::size_t count = 0;
  std::size_t offset = 0;
  while ((offset = value.find(needle, offset)) != std::string::npos) {
    ++count;
    offset += needle.size();
  }
  return count;
}

host_sim::BoardModel board_model(host_sim::HarnessRoutingMode routing) {
  const auto pcb = std::string(BLOCKER_VARIANT_DIR) +
                   "/kicad_files/hardware_challenge.kicad_pcb";
  const auto schematic =
      std::string(HOST_SIM_ROOT) + "/kicad_files/hardware_challenge.kicad_sch";
  return host_sim::BoardModel::load(pcb, schematic, routing);
}

std::unique_ptr<host_sim::Runtime>
make_runtime(host_sim::Harness harness = {},
             host_sim::HarnessRoutingMode routing =
                 host_sim::HarnessRoutingMode::AsDrawn) {
  auto runtime =
      std::make_unique<host_sim::Runtime>(board_model(routing));
  runtime->set_harness(std::move(harness));
  return runtime;
}

bool lock_time(host_sim::Runtime &runtime, bool repaired_button) {
  // The unchanged gate runs while released; the repaired gate runs while
  // pressed. Hold the opposite state while consuming the time-fix sentence.
  runtime.set_button_pressed(!repaired_button);
  runtime.inject_serial1_rx_bypass_capacity(kValidGnrmc);
  setup();
  loop();
  return require(time_fixed, "workflow did not acquire the injected GNRMC fix");
}

std::string probe_bits(const std::string &output, int pin) {
  const auto prefix = "Pin " + std::to_string(pin) + ": ";
  const auto start = output.find(prefix);
  if (start == std::string::npos) {
    throw std::runtime_error("probe line not found for pin " +
                             std::to_string(pin));
  }
  const auto bits = output.substr(start + prefix.size(), host_sim::kHarnessPins);
  if (bits.size() != host_sim::kHarnessPins) {
    throw std::runtime_error("probe line is truncated");
  }
  return bits;
}

std::uint64_t probe_value(const std::string &output, int pin) {
  const auto bits = probe_bits(output, pin);
  std::uint64_t value = 0;
  for (std::size_t bit = 0; bit < bits.size(); ++bit) {
    if (bits[bit] == '1') {
      value |= std::uint64_t{1} << bit;
    } else if (bits[bit] != '0') {
      throw std::runtime_error("probe line contains a non-binary value");
    }
  }
  return value;
}

bool run_probe_directions(bool repaired) {
  auto runtime = make_runtime();
  bool ok = lock_time(*runtime, false);
  runtime->set_button_pressed(false);
  loop();

  const auto &output = runtime->serial_output();
  ok &= require(count_occurrences(output, "Pin ") == host_sim::kHarnessPins,
                "P4 did not execute all 40 probe rows");
  ok &= require(contains(output, "Pin 39:"),
                "P4 did not reach the final logical probe row");

  for (std::size_t port = 0; port < 8; ++port) {
    const auto expected =
        repaired && port == 4 ? std::uint8_t{0x7F} : std::uint8_t{0xFF};
    ok &= require(runtime->expander_direction(port) == expected,
                  "P4 final expander direction did not isolate raw bit 39");
  }
  return ok;
}

bool run_set_output_directions(bool repaired) {
  auto runtime = make_runtime();
  CY8C9560 driver;
  bool ok = require(driver.begin(), "P4 driver setup did not reach the expander");
  driver.set_output(1ULL << 39, 1ULL << 39);
  for (std::size_t port = 0; port < 8; ++port) {
    const auto expected = repaired
                              ? (port == 4 ? std::uint8_t{0x7F}
                                           : std::uint8_t{0xFF})
                              : std::uint8_t{0x00};
    ok &= require(runtime->expander_direction(port) == expected,
                  "P4 set_output direction did not isolate raw bit 39");
  }
  return ok;
}

bool run_mapping(bool repaired) {
  host_sim::Harness harness;
  harness.connect(20, 21);
  auto runtime = make_runtime(
      std::move(harness), host_sim::HarnessRoutingMode::SchematicIdeal);

  bool ok = true;
  ok &= require(runtime->model().channel(20).expander_port == 3 &&
                    runtime->model().channel(20).expander_bit == 0,
                "P5 source map did not place logical 20 at raw bit 24");
  ok &= require(runtime->model().channel(39).expander_port == 5 &&
                    runtime->model().channel(39).expander_bit == 3,
                "P5 source map did not place logical 39 at raw bit 43");
  ok &= lock_time(*runtime, false);
  runtime->set_button_pressed(false);
  loop();

  const auto bits = probe_bits(runtime->serial_output(), 20);
  const auto ones = count_occurrences(bits, "1");
  if (repaired) {
    ok &= require(bits[21] == '1' && ones == 1,
                  "P5 did not map raw bit 25 back to logical bit 21");
  } else {
    ok &= require(ones == 0,
                  "P5 counterfactual unexpectedly reached logical pair 20-21");
  }
  return ok;
}

host_sim::Harness one_matching_row_harness() {
  host_sim::Harness harness;
  harness.connect(0, 13);
  harness.connect(0, 39);
  return harness;
}

bool run_one_row_verdict(bool repaired) {
  const auto harness = one_matching_row_harness();
  const auto observations = host_sim::HarnessOracle::observations(harness);
  constexpr std::size_t matching_row = 7;
  constexpr std::uint64_t logical_mask =
      (std::uint64_t{1} << host_sim::kHarnessPins) - 1;
  for (std::size_t row = 0; row < host_sim::kHarnessPins; ++row) {
    EXPECTED_CONNECTIONS[row] =
        observations[row] ^ (std::uint64_t{1} << ((row + 1) % 40));
    EXPECTED_CONNECTIONS[row] &= logical_mask;
  }
  EXPECTED_CONNECTIONS[matching_row] = observations[matching_row];
  std::size_t matching_rows = 0;
  for (std::size_t row = 0; row < host_sim::kHarnessPins; ++row) {
    if (observations[row] == EXPECTED_CONNECTIONS[row]) {
      ++matching_rows;
    }
  }

  auto runtime = make_runtime(
      harness, host_sim::HarnessRoutingMode::SchematicIdeal);
  bool ok = require(matching_rows == 1,
                    "P6 negative witness does not have exactly one matching row");
  ok &= lock_time(*runtime, false);
  runtime->set_button_pressed(false);
  loop();

  const auto &output = runtime->serial_output();
  ok &= require(contains(output, repaired ? "Harness failed!"
                                         : "Harness passed!"),
                "P6 one-row verdict did not match the selected counterfactual");
  return ok;
}

bool run_matrix_closure(bool repaired) {
  const auto harness = blocker_peeling_test::declared_harness();
  const auto sparse = blocker_peeling_test::declared_sparse_rows();
  const auto closed = host_sim::HarnessOracle::observations(harness);
  std::size_t changed_rows = 0;
  std::size_t compiled_mismatches = 0;
  for (std::size_t row = 0; row < host_sim::kHarnessPins; ++row) {
    changed_rows += sparse[row] != closed[row];
    compiled_mismatches += EXPECTED_CONNECTIONS[row] != closed[row];
  }

  auto runtime = make_runtime(
      harness, host_sim::HarnessRoutingMode::SchematicIdeal);
  bool ok = require(changed_rows == 36,
                    "P6 matrix fixture did not preserve 36 closure changes");
  ok &= require(compiled_mismatches == (repaired ? 0 : 36),
                "P6 generated matrix did not match the selected closure state");
  ok &= lock_time(*runtime, false);
  runtime->set_button_pressed(false);
  loop();

  for (std::size_t row = 0; row < host_sim::kHarnessPins; ++row) {
    ok &= require(probe_value(runtime->serial_output(),
                              static_cast<int>(row)) == closed[row],
                  "P6 firmware observation disagreed with passive closure");
  }
  ok &= require(contains(runtime->serial_output(),
                         repaired ? "Harness passed!" : "Harness failed!"),
                "P6 matrix closure verdict did not match the counterfactual");
  return ok;
}

bool run_all_rows_verdict() {
  host_sim::Harness harness;
  harness.connect(0, 1);
  harness.connect(20, 21);
  const auto observations = host_sim::HarnessOracle::observations(harness);
  for (std::size_t row = 0; row < host_sim::kHarnessPins; ++row) {
    EXPECTED_CONNECTIONS[row] = observations[row];
  }

  auto runtime = make_runtime(
      harness, host_sim::HarnessRoutingMode::SchematicIdeal);
  bool ok = lock_time(*runtime, false);
  runtime->set_button_pressed(false);
  loop();
  ok &= require(contains(runtime->serial_output(), "Harness passed!"),
                "P6 rejected an oracle-matching 40-row harness");
  return ok;
}

bool run_button_session(bool repaired) {
  auto runtime = make_runtime(
      {}, host_sim::HarnessRoutingMode::SchematicIdeal);
  bool ok = lock_time(*runtime, repaired);
  const auto button =
      static_cast<std::uint8_t>(runtime->model().arduino_pin("BTN_TEST"));

  if (repaired) {
    ok &= require(runtime->pin_mode(button) == INPUT_PULLUP,
                  "P7 did not enable the button pull-up");
    ok &= require(runtime->sd_content("results.txt").empty(),
                  "P7 ran before the first press");
    runtime->set_button_pressed(true);
    loop();
    ok &= require(count_occurrences(runtime->sd_content("results.txt"), " - ") ==
                      1,
                  "P7 first press did not run exactly one session");
    loop();
    ok &= require(count_occurrences(runtime->sd_content("results.txt"), " - ") ==
                      1,
                  "P7 held press repeated the session");
    runtime->set_button_pressed(false);
    loop();
    runtime->set_button_pressed(true);
    loop();
    ok &= require(count_occurrences(runtime->sd_content("results.txt"), " - ") ==
                      2,
                  "P7 release and repress did not run a second session");
  } else {
    ok &= require(runtime->pin_mode(button) == INPUT,
                  "P7 counterfactual unexpectedly enabled the pull-up");
    runtime->set_button_pressed(false);
    loop();
    loop();
    ok &= require(count_occurrences(runtime->sd_content("results.txt"), " - ") ==
                      2,
                  "P7 counterfactual did not repeat while physically released");
  }
  return ok;
}

bool run_sd_write(bool repaired, std::size_t capacity,
                  const std::string &expected_content,
                  bool expect_diagnostic) {
  auto runtime = make_runtime(
      {}, host_sim::HarnessRoutingMode::SchematicIdeal);
  host_sim::SdCardConfig config;
  config.capacity_bytes = capacity;
  runtime->configure_sd(config);

  bool ok = lock_time(*runtime, true);
  runtime->set_button_pressed(true);
  loop();
  ok &= require(runtime->sd_content("results.txt") == expected_content,
                "P9 stored record did not match the configured capacity");
  ok &= require(
      contains(runtime->serial_output(),
               "Failed to write complete log entry") ==
          (repaired && expect_diagnostic),
      "P9 partial-write diagnostic did not match the selected counterfactual");
  return ok;
}

bool run_case(const std::string &name) {
  if (name == "probe-repaired") return run_probe_directions(true);
  if (name == "probe-counterfactual") return run_probe_directions(false);
  if (name == "set-output-repaired") return run_set_output_directions(true);
  if (name == "set-output-counterfactual") {
    return run_set_output_directions(false);
  }
  if (name == "mapping-repaired") return run_mapping(true);
  if (name == "mapping-counterfactual") return run_mapping(false);
  if (name == "verdict-one-repaired") return run_one_row_verdict(true);
  if (name == "verdict-one-counterfactual") {
    return run_one_row_verdict(false);
  }
  if (name == "verdict-all-repaired") return run_all_rows_verdict();
  if (name == "matrix-closure-repaired") return run_matrix_closure(true);
  if (name == "matrix-closure-counterfactual") {
    return run_matrix_closure(false);
  }
  if (name == "button-repaired") return run_button_session(true);
  if (name == "button-counterfactual") return run_button_session(false);
  if (name == "sd-repaired") {
    return run_sd_write(true, 12, "230394 - 123", true);
  }
  if (name == "sd-counterfactual") {
    return run_sd_write(false, 12, "230394 - 123", true);
  }
  if (name == "sd-line-ending-repaired") {
    return run_sd_write(true, 24, "230394 - 123519: Failed\r", true);
  }
  if (name == "sd-full-repaired") {
    return run_sd_write(true, 25, "230394 - 123519: Failed\r\n", false);
  }
  throw std::runtime_error("unknown blocker-peeling workflow case: " + name);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: blocker_peeling_workflow_cases <case>\n";
    return EXIT_FAILURE;
  }
  try {
    return run_case(argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n";
    return EXIT_FAILURE;
  }
}
