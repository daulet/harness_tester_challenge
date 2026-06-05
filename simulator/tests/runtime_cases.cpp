#include <cstdlib>
#include <iostream>
#include <string>

#include "host_simulator/runtime.h"

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

std::string data_path(const std::string& relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

bool require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

}  // namespace

int main() {
  host_sim::Runtime runtime(host_sim::BoardModel::load(
      data_path("kicad_files/hardware_challenge.kicad_pcb"),
      data_path("kicad_files/hardware_challenge.kicad_sch")));
  host_sim::Wire2.begin();

  bool ok = true;
  ok &= require(runtime.i2c_read(0x20, 1).empty(),
                "expander responded while reset was asserted");

  runtime.digital_write(22, 0);
  ok &= require(runtime.i2c_write(0x20, {0x1D}),
                "expander did not accept pull-up register select");
  const auto pull_up = runtime.i2c_read(0x20, 1);
  ok &= require(pull_up.size() == 1 && pull_up[0] == 0xFF,
                "expander pull-up reset default did not match");

  ok &= require(runtime.i2c_write(0x20, {0x1E, 0x01}),
                "expander did not accept pull-down mode write");
  ok &= require(runtime.i2c_write(0x20, {0x1D}),
                "expander did not accept pull-up register reread select");
  const auto updated_pull_up = runtime.i2c_read(0x20, 1);
  ok &= require(runtime.i2c_write(0x20, {0x1E}),
                "expander did not accept pull-down register reread select");
  const auto pull_down = runtime.i2c_read(0x20, 1);
  ok &= require(updated_pull_up.size() == 1 && updated_pull_up[0] == 0xFE,
                "expander did not clear the old mode on mode transition");
  ok &= require(pull_down.size() == 1 && pull_down[0] == 0x01,
                "expander did not set the new mode on mode transition");

  runtime.digital_write(22, 1);
  runtime.digital_write(22, 0);
  ok &= require(runtime.i2c_write(0x20, {0x1C}),
                "expander did not accept direction register select after reset");
  const auto direction = runtime.i2c_read(0x20, 1);
  ok &= require(direction.size() == 1 && direction[0] == 0x00,
                "expander direction reset default did not match");

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
