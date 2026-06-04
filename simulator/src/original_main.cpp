#include <iostream>

#include "host_simulator/runtime.h"

extern void setup();
extern void loop();

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

constexpr const char* kValidNmea =
    "$GPRMC,123519,A,0,N,0,E,0,0,230394\r\n";

std::string data_path(const std::string& relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

}  // namespace

int main() {
  host_sim::Runtime runtime(host_sim::BoardModel::load(
      data_path("simulator/data/schematic_harness_map.csv"),
      data_path("simulator/data/schematic_io_map.csv"),
      data_path("simulator/data/pcb_fault_overlay.csv")));
  runtime.set_button_pressed(true);
  runtime.inject_gps(kValidNmea);

  setup();
  loop();

  std::cout << runtime.serial_output();
  std::cout << runtime.sd_content("results.txt");
  return 0;
}
