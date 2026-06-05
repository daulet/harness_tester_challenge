#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "host_simulator/runtime.h"

extern void setup();
extern void loop();

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

constexpr const char *kValidGprmc = "$GPRMC,123519,A,0,N,0,E,0,0,230394\r\n";

std::string data_path(const std::string &relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

host_sim::Runtime make_runtime() {
  return host_sim::Runtime(host_sim::BoardModel::load(
      data_path("kicad_files/hardware_challenge.kicad_pcb"),
      data_path("kicad_files/hardware_challenge.kicad_sch")));
}

void run_nmea_overflow() {
  auto runtime = make_runtime();
  runtime.inject_serial1_rx(std::string(65, 'x'));
  setup();
  loop();
}

void run_narrow_shift() {
  auto runtime = make_runtime();
  runtime.set_button_pressed(false);
  runtime.inject_serial1_rx(kValidGprmc);
  setup();
  loop();
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: sanitizer_cases <nmea-overflow|narrow-shift>\n";
    return EXIT_FAILURE;
  }

  const std::string name = argv[1];
  if (name == "nmea-overflow") {
    run_nmea_overflow();
  } else if (name == "narrow-shift") {
    run_narrow_shift();
  } else {
    throw std::runtime_error("unknown sanitizer case: " + name);
  }

  std::cerr << "sanitizer did not detect the expected firmware defect: " << name
            << "\n";
  return EXIT_SUCCESS;
}
