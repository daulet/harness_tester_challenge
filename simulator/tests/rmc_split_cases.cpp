#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "host_simulator/runtime.h"

extern void setup();
extern void loop();
extern bool time_fixed;
extern char utc_time[];
extern char date[];

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

std::string data_path(const std::string &relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

std::string nmea_sentence(const std::string &body) {
  std::uint8_t checksum = 0;
  for (const char value : body) {
    checksum ^= static_cast<std::uint8_t>(value);
  }
  std::ostringstream sentence;
  sentence << '$' << body << '*' << std::uppercase << std::hex
           << std::setw(2) << std::setfill('0')
           << static_cast<int>(checksum) << "\r\n";
  return sentence.str();
}

std::string corrupt_checksum(std::string sentence) {
  const auto separator = sentence.find('*');
  if (separator == std::string::npos) {
    throw std::runtime_error("generated NMEA sentence has no checksum");
  }
  sentence[separator + 1] = sentence[separator + 1] == '0' ? '1' : '0';
  return sentence;
}

host_sim::Runtime make_runtime() {
  return host_sim::Runtime(host_sim::BoardModel::load(
      data_path("kicad_files/hardware_challenge.kicad_pcb"),
      data_path("kicad_files/hardware_challenge.kicad_sch")));
}

bool require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

void inject_and_loop(host_sim::Runtime &runtime, const std::string &sentence) {
  runtime.inject_serial1_rx_bypass_capacity(sentence);
  loop();
  if (!time_fixed && host_sim::Serial1.available() != 0) {
    loop();
  }
}

bool run_status_only() {
  auto runtime = make_runtime();
  runtime.set_button_pressed(true);
  setup();

  inject_and_loop(
      runtime, nmea_sentence("GPRMC,123519,V,0,N,0,E,0,0,230394"));
  bool ok = require(!time_fixed,
                    "status-only repair accepted a checksum-valid V sentence") &&
      require(std::string(utc_time).empty() && std::string(date).empty(),
              "status-only repair mutated state for rejected status V");

  inject_and_loop(
      runtime,
      corrupt_checksum(
          nmea_sentence("GPRMC,235959,A,0,N,0,E,0,0,060626")));
  ok &= require(time_fixed,
                "status-only repair unexpectedly rejected bad checksum") &&
      require(std::string(utc_time) == "235959" &&
                  std::string(date) == "060626",
              "status-only repair did not commit the accepted sentence");
  return ok;
}

bool run_checksum_only() {
  auto runtime = make_runtime();
  runtime.set_button_pressed(true);
  setup();

  inject_and_loop(
      runtime,
      corrupt_checksum(
          nmea_sentence("GPRMC,123519,A,0,N,0,E,0,0,230394")));
  bool ok = require(!time_fixed,
                    "checksum-only repair accepted a corrupted sentence");

  inject_and_loop(
      runtime, nmea_sentence("GPRMC,123519,V,0,N,0,E,0,0,230394"));
  ok &= require(time_fixed,
                "checksum-only repair unexpectedly rejected status V");
  return ok;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: rmc_split_cases <status-only|checksum-only>\n";
    return EXIT_FAILURE;
  }
  const std::string name = argv[1];
  if (name == "status-only") {
    return run_status_only() ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if (name == "checksum-only") {
    return run_checksum_only() ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  throw std::runtime_error("unknown RMC split case: " + name);
}
