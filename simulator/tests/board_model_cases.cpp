#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

#include "host_simulator/board.h"

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

std::string data_path(const std::string& relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

std::string read_file(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("failed to open source file: " + path);
  }
  std::ostringstream content;
  content << file.rdbuf();
  return content.str();
}

void write_file(const std::string& path, const std::string& content) {
  std::ofstream file(path);
  if (!file) {
    throw std::runtime_error("failed to write source file: " + path);
  }
  file << content;
}

bool require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
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

}  // namespace

int main() {
  const auto pcb = data_path("kicad_files/hardware_challenge.kicad_pcb");
  const auto schematic = data_path("kicad_files/hardware_challenge.kicad_sch");
  const auto model = host_sim::BoardModel::load(pcb, schematic);

  bool ok = true;
  for (std::size_t index = 0; index < host_sim::kHarnessPins; ++index) {
    ok &= require(model.channel(index).net == "CBL_" + std::to_string(index),
                  "J3 source parity failed for harness channel " + std::to_string(index));
  }

  ok &= require(model.channel(20).expander_port == 3 &&
                    model.channel(20).expander_bit == 0,
                "non-J3 routed net CBL_20 did not resolve to U4 GPort3_Bit0");
  ok &= require(model.arduino_pin("CY_SCL") == 24 &&
                    model.arduino_pin("CY_SDA") == 25,
                "firmware-visible I2C nets did not resolve from source");
  ok &= require(model.pcb_connected("CY_SCL", "U2", "16", "U4", "24") &&
                    model.pcb_connected("CY_SDA", "U2", "17", "U4", "28"),
                "non-J3 I2C routes did not resolve through the PCB graph");
  const auto& u4 = model.component("U4");
  ok &= require(u4.schematic_lib_id == "CY8C9560A:CY8C9560A-24AXIT" &&
                    u4.schematic_value == "CY8C9560A-24AXIT" &&
                    u4.schematic_footprint ==
                        "Package_QFP:TQFP-100_12x12mm_P0.4mm" &&
                    u4.pcb_value == "CY8C9560A-24AXIT" &&
                    u4.pcb_footprint ==
                        "Package_QFP:TQFP-100_12x12mm_P0.4mm",
                "U4 component metadata did not resolve structurally");
  ok &= require(model.connectivity_mismatches().empty(),
                "source-matched digital connectivity unexpectedly reported mismatches");

  const auto gnd = model.physical_nets().find("GND");
  ok &= require(gnd != model.physical_nets().end(),
                "PCB source did not expose GND physical net");
  if (gnd != model.physical_nets().end()) {
    ok &= require(gnd->second.zone_count > 0,
                  "PCB source did not account for copper zones");
    ok &= require(gnd->second.via_count > 0,
                  "PCB source did not account for vias");
  }
  ok &= require(model.physical_stats().keepouts > 0,
                "PCB source did not account for keepouts");
  ok &= require(model.has_copper_at("GND", "F.Cu", 150.0, 50.0),
                "PCB source did not expose copper outside the keepout");
  ok &= require(!model.has_copper_at("GND", "F.Cu", 130.0, 70.0),
                "PCB source did not suppress copper inside the keepout");

  const auto source = read_file(schematic);
  const auto marker = source.find("(global_label \"CBL_0\"");
  ok &= require(marker != std::string::npos,
                "schematic source did not contain the mismatch test marker");
  if (marker != std::string::npos) {
    auto mutated = source;
    mutated.replace(marker + std::string("(global_label \"").size(), 5, "CBL_99");
    const auto mutated_path = data_path("build/mutated_hardware_challenge.kicad_sch");
    write_file(mutated_path, mutated);
    const auto mutated_model = host_sim::BoardModel::load(pcb, mutated_path);
    ok &= require(!mutated_model.connectivity_mismatches().empty(),
                  "schematic-vs-PCB mismatch did not produce evidence");
    ok &= require(mutated_model.channel(0).net == "CBL_0",
                  "schematic mismatch incorrectly replaced PCB runtime connectivity");
  }

  const auto u4_schematic_marker = source.find("(property \"Reference\" \"U4\"");
  ok &= require(u4_schematic_marker != std::string::npos,
                "schematic source did not contain the U4 metadata marker");
  if (u4_schematic_marker != std::string::npos) {
    constexpr auto value_property =
        "(property \"Value\" \"CY8C9560A-24AXIT\"";
    const auto value_marker = source.find(value_property, u4_schematic_marker);
    ok &= require(value_marker != std::string::npos,
                  "schematic source did not contain the U4 value property");
    if (value_marker != std::string::npos) {
      auto mutated = source;
      constexpr auto value_prefix = "(property \"Value\" \"";
      constexpr auto original_value = "CY8C9560A-24AXIT";
      constexpr auto mutated_value = "CY8C9560A-MUTATED";
      mutated.replace(value_marker + std::string(value_prefix).size(),
                      std::string(original_value).size(),
                      mutated_value);
      const auto mutated_path =
          data_path("build/mutated_hardware_challenge_u4_value.kicad_sch");
      write_file(mutated_path, mutated);
      const auto mutated_model = host_sim::BoardModel::load(pcb, mutated_path);
      const auto& mutated_u4 = mutated_model.component("U4");
      ok &= require(mutated_u4.schematic_value == mutated_value,
                    "U4 schematic value mutation did not reach structured metadata");
      ok &= require(mutated_u4.pcb_value == original_value,
                    "U4 schematic value mutation contaminated PCB metadata");
    }
  }

  const auto pcb_source = read_file(pcb);
  const auto j3_marker = pcb_source.find("(property \"Reference\" \"J3\"");
  ok &= require(j3_marker != std::string::npos,
                "PCB source did not contain the J3 mismatch test marker");
  if (j3_marker != std::string::npos) {
    const auto net_marker = pcb_source.find("(net 26 \"CBL_0\")", j3_marker);
    ok &= require(net_marker != std::string::npos,
                  "PCB source did not contain the J3 CBL_0 net marker");
    if (net_marker != std::string::npos) {
      auto mutated = pcb_source;
      mutated.replace(net_marker,
                      std::string("(net 26 \"CBL_0\")").size(),
                      "(net 0 \"\")");
      const auto mutated_path = data_path("build/mutated_hardware_challenge.kicad_pcb");
      write_file(mutated_path, mutated);
      const auto mutated_model = host_sim::BoardModel::load(mutated_path, schematic);
      ok &= require(!mutated_model.connectivity_mismatches().empty(),
                    "PCB mismatch did not produce evidence");
      ok &= require(mutated_model.channel(0).net.empty(),
                    "PCB mismatch incorrectly retained the schematic net as runtime connectivity");
      ok &= require(!mutated_model.pcb_connected("CBL_0", "J3", "1", "U4", "24"),
                    "PCB mismatch incorrectly retained the disconnected route");
    }
  }

  const auto u2_marker = pcb_source.find("(property \"Reference\" \"U2\"");
  ok &= require(u2_marker != std::string::npos,
                "PCB source did not contain the U2 route mismatch test marker");
  if (u2_marker != std::string::npos) {
    const auto pad_marker = pcb_source.find("(pad \"16\"", u2_marker);
    ok &= require(pad_marker != std::string::npos,
                  "PCB source did not contain the U2 CY_SCL pad marker");
    const auto net_marker = pad_marker == std::string::npos
        ? std::string::npos
        : pcb_source.find("(net 55)", pad_marker);
    ok &= require(net_marker != std::string::npos,
                  "PCB source did not contain the U2 CY_SCL route net marker");
    if (net_marker != std::string::npos) {
      auto mutated = pcb_source;
      mutated.replace(net_marker, std::string("(net 55)").size(), "(net 0)");
      const auto mutated_path = data_path("build/mutated_hardware_challenge_u2_route.kicad_pcb");
      write_file(mutated_path, mutated);
      const auto mutated_model = host_sim::BoardModel::load(mutated_path, schematic);
      ok &= require(!mutated_model.connectivity_mismatches().empty(),
                    "route mismatch did not produce evidence");
      ok &= require(!mutated_model.pcb_connected("CY_SCL", "U2", "16", "U4", "24"),
                    "route mismatch incorrectly retained physical connectivity");
    }
  }

  const auto u4_pcb_marker = pcb_source.find("(property \"Reference\" \"U4\"");
  ok &= require(u4_pcb_marker != std::string::npos,
                "PCB source did not contain the U4 metadata marker");
  if (u4_pcb_marker != std::string::npos) {
    constexpr auto footprint_atom =
        "(footprint \"Package_QFP:TQFP-100_12x12mm_P0.4mm\"";
    const auto footprint_marker = pcb_source.rfind(footprint_atom, u4_pcb_marker);
    ok &= require(footprint_marker != std::string::npos,
                  "PCB source did not contain the U4 footprint atom");
    if (footprint_marker != std::string::npos) {
      auto mutated = pcb_source;
      constexpr auto footprint_prefix = "(footprint \"";
      constexpr auto original_footprint =
          "Package_QFP:TQFP-100_12x12mm_P0.4mm";
      constexpr auto mutated_footprint =
          "Package_QFP:TQFP-100_MUTATED";
      mutated.replace(footprint_marker + std::string(footprint_prefix).size(),
                      std::string(original_footprint).size(),
                      mutated_footprint);
      const auto mutated_path =
          data_path("build/mutated_hardware_challenge_u4_footprint.kicad_pcb");
      write_file(mutated_path, mutated);
      const auto mutated_model = host_sim::BoardModel::load(mutated_path, schematic);
      const auto& mutated_u4 = mutated_model.component("U4");
      ok &= require(mutated_u4.pcb_footprint == mutated_footprint,
                    "U4 PCB footprint mutation did not reach structured metadata");
      ok &= require(mutated_u4.schematic_footprint == original_footprint,
                    "U4 PCB footprint mutation contaminated schematic metadata");
    }
  }

  host_sim::Harness harness;
  harness.connect(3, 4);
  std::array<host_sim::ExpanderPinDrive, host_sim::kExpanderPins> drives;
  drives.fill(input_pull_down());
  drives[3] = output_high();
  const auto resolved = model.resolve_inputs(harness, drives);
  ok &= require((resolved & (1ULL << 4)) != 0,
                "physical runtime graph did not preserve CBL_3 connectivity");

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
