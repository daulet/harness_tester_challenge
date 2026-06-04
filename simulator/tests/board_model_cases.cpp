#include <cstdlib>
#include <iostream>
#include <string>

#include "host_simulator/board.h"

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

std::string data_path(const std::string& relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

host_sim::ExpanderPinDrive output_high() {
  host_sim::ExpanderPinDrive drive;
  drive.is_input = false;
  drive.output_value = true;
  drive.drive_mode = host_sim::DriveMode::Strong;
  return drive;
}

host_sim::ExpanderPinDrive input_pull_down() {
  host_sim::ExpanderPinDrive drive;
  drive.is_input = true;
  drive.drive_mode = host_sim::DriveMode::PullDown;
  return drive;
}

bool require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

}  // namespace

int main() {
  const auto map = data_path("simulator/data/schematic_harness_map.csv");
  const auto io = data_path("simulator/data/schematic_io_map.csv");
  const auto overlay = data_path("simulator/data/pcb_fault_overlay.csv");
  const auto schematic = host_sim::BoardModel::load(map, io);
  const auto as_drawn = host_sim::BoardModel::load(map, io, overlay);

  host_sim::Harness harness;
  harness.connect(3, 4);

  std::array<host_sim::ExpanderPinDrive, host_sim::kExpanderPins> drives;
  drives.fill(input_pull_down());
  drives[1] = output_high();
  const auto schematic_short_check = schematic.resolve_inputs(harness, drives);
  const auto as_drawn_short_check = as_drawn.resolve_inputs(harness, drives);

  bool ok = true;
  ok &= require((schematic_short_check & (1ULL << 2)) == 0,
                "schematic map unexpectedly shorts CBL_1 to CBL_2");
  ok &= require((as_drawn_short_check & (1ULL << 2)) != 0,
                "PCB overlay did not short CBL_1 to CBL_2");

  drives.fill(input_pull_down());
  drives[3] = output_high();
  const auto schematic_open_check = schematic.resolve_inputs(harness, drives);
  const auto as_drawn_open_check = as_drawn.resolve_inputs(harness, drives);
  ok &= require((schematic_open_check & (1ULL << 4)) != 0,
                "schematic map did not carry CBL_3 through J3");
  ok &= require((as_drawn_open_check & (1ULL << 4)) == 0,
                "PCB overlay did not leave J3 CBL_3 open");

  drives.fill(input_pull_down());
  drives[24] = output_high();
  const auto schematic_map_check = schematic.resolve_inputs(harness, drives);
  ok &= require((schematic_map_check & (1ULL << 24)) != 0,
                "schematic map did not report CBL_20 on physical bit 24");
  ok &= require((schematic_map_check & (1ULL << 20)) == 0,
                "schematic map incorrectly normalized CBL_20 onto logical bit 20");

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
