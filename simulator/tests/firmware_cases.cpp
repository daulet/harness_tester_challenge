#include <cstdlib>
#include <iostream>
#include <string>

#include "host_simulator/runtime.h"

extern void setup();
extern void loop();

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

constexpr const char* kValidNmea =
    "$GPRMC,123519,A,0,N,0,E,0,0,230394\r\n";

constexpr std::uint64_t kExpectedConnections[40] = {
    0b1000000000000000000000000010000000000000ULL,
    0b0100001000000000000000000000000000000000ULL,
    0b0010000000000000000000000000000000000000ULL,
    0b0001100000001000000000000000000010000000ULL,
    0b0000100000100000000000000001000000000000ULL,
    0b0000010100000000000000000000000000000000ULL,
    0b0000001000000000100000000000000000000000ULL,
    0b0000000100000000000000000000001000001000ULL,
    0b0000000010000000000000000000000000000000ULL,
    0b0000000001000000000000000000000010000000ULL,
    0b0010000000100000000000000000000000000000ULL,
    0b0000000000010000000010000000000000000000ULL,
    0b0000000000001000000000000000000000010000ULL,
    0b0000000000000100000000000000000000000001ULL,
    0b0000000000000010000000001000000000000000ULL,
    0b0000000000001001000000000100000000000000ULL,
    0b0000000000000000100000000000000000000000ULL,
    0b0000010000000000010000000000000000000000ULL,
    0b0000000000000000001000000000000000000000ULL,
    0b0000000000000000000100000000100000000000ULL,
    0b0000000000000000000010000000000000000000ULL,
    0b0000000000000000000001000000000000000000ULL,
    0b0000000000000000000000100000000000000000ULL,
    0b0000000000000000000000010000000001000000ULL,
    0b0000000000000000000000001000000000000000ULL,
    0b0000000000001000000000000100000000000000ULL,
    0b0000000000000000000000000010000000000000ULL,
    0b0000000000100010000000001001000000001000ULL,
    0b0000000000000000000000000000100000000000ULL,
    0b0000000000001000000000000000010000010000ULL,
    0b0000000000000000000000000000001000000000ULL,
    0b0000000000000000000000000000000100000000ULL,
    0b0000000000000000000000000000000010100000ULL,
    0b0000000000000000000000000000000001000010ULL,
    0b0000100000000000000000100000000000100000ULL,
    0b0000010000000000000000000000000000011000ULL,
    0b0000000000000000000000000000000000001000ULL,
    0b0000000000000000000000000000010000000100ULL,
    0b0000000000000000000000000000000000000010ULL,
    0b0000000000000000000000000000000000000001ULL,
};

std::string data_path(const std::string& relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

host_sim::Harness known_good_harness() {
  // A physical harness turns the expected pair list into connected components.
  // That is intentionally different from treating every matrix row as an
  // independent directed observation, and it exposes the firmware's
  // passed-on-any-match behavior once the init/probe blockers are removed.
  host_sim::Harness harness;
  for (std::size_t left = 0; left < 40; ++left) {
    for (std::size_t right = left + 1; right < 40; ++right) {
      if (kExpectedConnections[left] & (1ULL << right)) {
        harness.connect(left, right);
      }
    }
  }
  return harness;
}

host_sim::Harness broken_harness() {
  host_sim::Harness harness;
  harness.connect(0, 39);
  return harness;
}

bool contains(const std::string& value, const std::string& needle) {
  return value.find(needle) != std::string::npos;
}

bool require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

struct Case {
  std::string name;
  bool use_broken_harness = false;
  bool expect_pass = false;
  bool expect_wire = false;
  std::string expected_probe_line;
};

bool run_case(const Case& test_case) {
  const auto pcb = data_path("kicad_files/hardware_challenge.kicad_pcb");
  const auto schematic = data_path("kicad_files/hardware_challenge.kicad_sch");
  host_sim::Runtime runtime(host_sim::BoardModel::load(pcb, schematic));
  runtime.set_i2c_bus_mode(host_sim::I2cBusMode::Ideal);
  runtime.set_harness(test_case.use_broken_harness ? broken_harness() : known_good_harness());
  runtime.set_button_pressed(false);
  runtime.inject_serial1_rx(kValidNmea);

  setup();
  loop();

  const auto output = runtime.serial_output();
  const auto log = runtime.sd_content("results.txt");
  const auto expected_message = test_case.expect_pass ? "Harness passed!" : "Harness failed!";
  const auto expected_log = test_case.expect_pass ? "Passed" : "Failed";

  bool ok = true;
  ok &= require(contains(output, "Time received. Ready to test some harnesses!"),
                test_case.name + ": GPS time was not consumed");
  ok &= require(contains(output, expected_message),
                test_case.name + ": final harness result did not match");
  ok &= require(contains(log, expected_log),
                test_case.name + ": SD result did not match");
  ok &= require(runtime.wire_begun() == test_case.expect_wire,
                test_case.name + ": Wire2 initialization did not match");
  ok &= require(runtime.expander_accessed() == test_case.expect_wire,
                test_case.name + ": CY8C9560 access did not match");
  if (!test_case.expected_probe_line.empty()) {
    ok &= require(contains(output, test_case.expected_probe_line),
                  test_case.name + ": expected probe observation was not visible");
  }
  return ok;
}

Case parse_case(const std::string& name) {
  if (name == "original-good") {
    return {name, false, false, false, {}};
  }
  if (name == "original-broken") {
    return {name, true, false, false, {}};
  }
  if (name == "patched-good-schematic") {
    return {name, false, false, true,
            "Pin 20: 0000000000000000000000000000000000000000"};
  }
  if (name == "patched-broken-schematic") {
    return {name, true, false, true,
            "Pin 20: 0000000000000000000000000000000000000000"};
  }
  throw std::runtime_error("unknown case: " + name);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: firmware_cases <case>\n";
    return EXIT_FAILURE;
  }
  try {
    return run_case(parse_case(argv[1])) ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return EXIT_FAILURE;
  }
}
