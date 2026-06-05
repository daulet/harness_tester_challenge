#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
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

bool require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

host_sim::BoardModel board_model() {
  return host_sim::BoardModel::load(
      data_path("kicad_files/hardware_challenge.kicad_pcb"),
      data_path("kicad_files/hardware_challenge.kicad_sch"));
}

bool has_exact_references(const host_sim::PhysicalNet& net,
                          const std::set<std::string>& expected) {
  return net.references == expected;
}

bool run_a05_narrow_masks() {
  const auto source = read_file(data_path("firmware/firmware.ino"));
  return require(source.find("#define NUM_HARNESS_PINS 40") != std::string::npos,
                 "A05: source did not declare the 40-pin harness") &&
      require(source.find("uint64_t output_mask = 1 << i;") != std::string::npos,
              "A05: source did not use the narrow output shift") &&
      require(source.find("(values & (1 << j))") != std::string::npos,
              "A05: source did not use the narrow readback shift");
}

bool run_a10_uart_same_direction() {
  const auto model = board_model();
  return require(model.pad("U2", "2").net == "UBX-RXD" &&
                     model.pad("U2", "2").pinfunction.find("RX1") != std::string::npos,
                 "A10: MCU RX1 did not land on UBX-RXD") &&
      require(model.pad("U3", "21").net == "UBX-RXD" &&
                     model.pad("U3", "21").pinfunction.find("RXD") != std::string::npos,
                 "A10: GPS RXD did not land on UBX-RXD") &&
      require(model.pad("U2", "3").net == "UBX-TXD" &&
                     model.pad("U2", "3").pinfunction.find("TX1") != std::string::npos,
                 "A10: MCU TX1 did not land on UBX-TXD") &&
      require(model.pad("U3", "20").net == "UBX-TXD" &&
                     model.pad("U3", "20").pinfunction.find("TXD") != std::string::npos,
                 "A10: GPS TXD did not land on UBX-TXD");
}

bool run_a11_sda_pull_down() {
  const auto model = board_model();
  return require(model.pad("R3", "1").net == "GND" ||
                     model.pad("R3", "2").net == "GND",
                 "A11: R3 did not connect one side to GND") &&
      require(model.pad("R3", "1").net == "CY_SDA" ||
                     model.pad("R3", "2").net == "CY_SDA",
                 "A11: R3 did not connect the other side to CY_SDA");
}

bool run_a12_led_no_series_resistors() {
  const auto model = board_model();
  return require(has_exact_references(model.physical_nets().at("LED_R"), {"D3", "U2"}),
                 "A12: LED_R had an unexpected in-line reference") &&
      require(has_exact_references(model.physical_nets().at("LED_G"), {"D3", "U2"}),
                 "A12: LED_G had an unexpected in-line reference") &&
      require(has_exact_references(model.physical_nets().at("LED_B"), {"D3", "U2"}),
                 "A12: LED_B had an unexpected in-line reference");
}

bool run_a16_d3_anode_isolated() {
  const auto model = board_model();
  return require(model.pcb_connected("+3.3V", "U2", "15", "U4", "32"),
                 "A16: main +3.3V path was not connected") &&
      require(!model.pcb_connected("+3.3V", "D3", "1", "U4", "32"),
                 "A16: D3 anode unexpectedly joined the main +3.3V path");
}

bool run_a18_lna_vcc_rf() {
  const auto model = board_model();
  return require(model.pad("U5", "A1").net == "Net-(U3-VCC_RF)",
                 "A18: U5 Vcc did not land on U3 VCC_RF") &&
      require(model.pad("U3", "9").net == "Net-(U3-VCC_RF)",
                 "A18: U3 VCC_RF did not feed the LNA rail") &&
      require(model.pad("U3", "23").net == "+3.3V",
                 "A18: U3 VCC did not come from +3.3V");
}

bool run_a25_reverse_polarity_path() {
  const auto model = board_model();
  const auto raw_input = model.pad("J1", "1").net;
  return require(raw_input == model.pad("D1", "1").net,
                 "A25: D1 cathode did not share the raw input net") &&
      require(raw_input == model.pad("Q1", "5").net,
              "A25: Q1 drain did not share the raw input net") &&
      require(model.pad("D1", "2").net == "GND",
              "A25: D1 anode did not land on GND") &&
      require(model.pad("Q1", "1").net == "+12V",
              "A25: Q1 source did not land on the protected +12V rail");
}

bool run_a29_u4_wrong_footprint() {
  const auto model = board_model();
  const auto& u4 = model.component("U4");
  return require(u4.schematic_value == "CY8C9560A-24AXIT",
                 "A29: schematic U4 did not select CY8C9560A-24AXIT") &&
      require(u4.schematic_footprint ==
                  "Package_QFP:TQFP-100_12x12mm_P0.4mm",
              "A29: schematic U4 did not assign the 12 mm TQFP footprint") &&
      require(u4.pcb_value == "CY8C9560A-24AXIT",
              "A29: PCB U4 value did not match the selected part") &&
      require(u4.pcb_footprint ==
                  "Package_QFP:TQFP-100_12x12mm_P0.4mm",
              "A29: PCB U4 did not use the 12 mm TQFP footprint");
}

bool run_case(const std::string& name) {
  if (name == "a05_narrow_masks") return run_a05_narrow_masks();
  if (name == "a10_uart_same_direction") return run_a10_uart_same_direction();
  if (name == "a11_sda_pull_down") return run_a11_sda_pull_down();
  if (name == "a12_led_no_series_resistors") return run_a12_led_no_series_resistors();
  if (name == "a16_d3_anode_isolated") return run_a16_d3_anode_isolated();
  if (name == "a18_lna_vcc_rf") return run_a18_lna_vcc_rf();
  if (name == "a25_reverse_polarity_path") return run_a25_reverse_polarity_path();
  if (name == "a29_u4_wrong_footprint") return run_a29_u4_wrong_footprint();
  throw std::runtime_error("unknown scenario case: " + name);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: scenario_cases <case>\n";
    return EXIT_FAILURE;
  }
  try {
    return run_case(argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return EXIT_FAILURE;
  }
}
