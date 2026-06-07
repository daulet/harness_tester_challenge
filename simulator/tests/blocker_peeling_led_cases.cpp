#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "host_simulator/analog.h"
#include "host_simulator/runtime.h"

extern void setup();
extern void loop();
extern bool time_fixed;

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

#ifndef HOST_SIM_NGSPICE
#define HOST_SIM_NGSPICE "ngspice"
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

std::string root_path(const std::string &relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

std::string variant_path(const std::string &relative) {
  return std::string(BLOCKER_VARIANT_DIR) + "/" + relative;
}

host_sim::NgSpiceSimulator simulator() {
  return host_sim::NgSpiceSimulator(root_path("simulator"),
                                    HOST_SIM_NGSPICE);
}

struct Workflow {
  host_sim::BoardElectricalConfig source_config;
  host_sim::BoardElectricalConfig repaired_config;
  host_sim::AnalogFixture fixture;
  host_sim::Runtime runtime;

  Workflow()
      : source_config(host_sim::BoardElectricalConfig::from_board(
            host_sim::BoardModel::load(
                variant_path("kicad_files/hardware_challenge.kicad_pcb"),
                root_path("kicad_files/hardware_challenge.kicad_sch")))),
        repaired_config(source_config),
        fixture(host_sim::AnalogFixture::load(
            root_path("simulator/ngspice/fixtures/nominal.fixture"))),
        runtime(host_sim::BoardModel::load(
            variant_path("kicad_files/hardware_challenge.kicad_pcb"),
            root_path("kicad_files/hardware_challenge.kicad_sch"))) {
    const auto overlay = host_sim::BoardElectricalOverlay::load(variant_path(
        "simulator/blocker_peeling/board_electrical_overlay.fixture"));
    overlay.apply_to(repaired_config);
    repaired_config.apply_to(fixture);
  }

  host_sim::AnalogObservation observe() const {
    return runtime.simulate_analog(simulator(), fixture, repaired_config);
  }
};

bool source_facts_match(const Workflow &workflow) {
  return require(workflow.source_config.led_anode_path_ohm > 1e11,
                 "P8 source config did not preserve the isolated D3 anode") &&
         require(workflow.source_config.led_red_series_ohm < 2.0 &&
                     workflow.source_config.led_green_series_ohm < 2.0 &&
                     workflow.source_config.led_blue_series_ohm < 2.0,
                 "P8 source config did not preserve the missing resistors") &&
         require(
             workflow.source_config.physical_red_driven_by ==
                     host_sim::LedChannel::Blue &&
                 workflow.source_config.physical_green_driven_by ==
                     host_sim::LedChannel::Green &&
                 workflow.source_config.physical_blue_driven_by ==
                     host_sim::LedChannel::Red,
             "P8 source config did not preserve the D3 red/blue cathode swap");
}

bool expect_dark(const host_sim::AnalogObservation &observation,
                 const std::string &context) {
  return require(std::abs(observation.measurement("led_red_current_a")) < 1e-6 &&
                     std::abs(observation.measurement("led_green_current_a")) <
                         1e-6 &&
                     std::abs(observation.measurement("led_blue_current_a")) <
                         1e-6,
                 context + ": an LED conducted without a complete drive path");
}

bool expect_color(const host_sim::AnalogObservation &observation,
                  host_sim::LedChannel expected,
                  const std::string &context) {
  const auto red = std::abs(observation.measurement("led_red_current_a"));
  const auto green = std::abs(observation.measurement("led_green_current_a"));
  const auto blue = std::abs(observation.measurement("led_blue_current_a"));
  const auto active = [&](host_sim::LedChannel channel) {
    switch (channel) {
    case host_sim::LedChannel::Red:
      return red;
    case host_sim::LedChannel::Green:
      return green;
    case host_sim::LedChannel::Blue:
      return blue;
    }
    throw std::logic_error("unknown LED channel");
  };
  bool ok = require(active(expected) > 0.001 && active(expected) < 0.02,
                    context + ": expected LED current was outside bounds");
  if (expected != host_sim::LedChannel::Red) {
    ok &= require(red < 1e-6, context + ": red die conducted unexpectedly");
  }
  if (expected != host_sim::LedChannel::Green) {
    ok &= require(green < 1e-6,
                  context + ": green die conducted unexpectedly");
  }
  if (expected != host_sim::LedChannel::Blue) {
    ok &= require(blue < 1e-6, context + ": blue die conducted unexpectedly");
  }
  return ok;
}

bool acquire_time(Workflow &workflow) {
  workflow.runtime.set_button_pressed(false);
  workflow.runtime.inject_serial1_rx_bypass_capacity(kValidGnrmc);
  loop();
  return require(time_fixed, "P8 did not acquire the injected GNRMC fix");
}

bool run_repaired() {
  Workflow workflow;
  bool ok = source_facts_match(workflow);
  ok &= require(workflow.repaired_config.led_anode_path_ohm < 2.0 &&
                    std::abs(workflow.repaired_config.led_red_series_ohm -
                             330.0) <
                        0.1 &&
                    workflow.repaired_config.physical_red_driven_by ==
                        host_sim::LedChannel::Red &&
                    workflow.repaired_config.physical_blue_driven_by ==
                        host_sim::LedChannel::Blue,
                "P8 repaired overlay did not fix power, resistance, and mapping");

  setup();
  ok &= expect_color(workflow.observe(), host_sim::LedChannel::Blue,
                     "P8 BUSY state");
  ok &= acquire_time(workflow);
  ok &= expect_color(workflow.observe(), host_sim::LedChannel::Green,
                     "P8 GOOD state");
  workflow.runtime.set_button_pressed(true);
  loop();
  ok &= expect_color(workflow.observe(), host_sim::LedChannel::Red,
                     "P8 FAILED state");
  return ok;
}

bool run_without_modes() {
  Workflow workflow;
  bool ok = source_facts_match(workflow);
  setup();
  ok &= expect_dark(workflow.observe(), "P8 firmware counterfactual");
  return ok;
}

bool run_without_power() {
  Workflow workflow;
  bool ok = source_facts_match(workflow);
  ok &= require(workflow.repaired_config.led_anode_path_ohm > 1e11,
                "P8 board counterfactual unexpectedly repaired the anode");
  setup();
  ok &= expect_dark(workflow.observe(), "P8 board counterfactual BUSY state");
  ok &= acquire_time(workflow);
  ok &= expect_dark(workflow.observe(), "P8 board counterfactual GOOD state");
  workflow.runtime.set_button_pressed(true);
  loop();
  ok &= expect_dark(workflow.observe(), "P8 board counterfactual FAILED state");
  return ok;
}

bool run_without_mapping() {
  Workflow workflow;
  bool ok = source_facts_match(workflow);
  setup();
  ok &= expect_color(workflow.observe(), host_sim::LedChannel::Red,
                     "P8 color-map counterfactual BUSY state");
  ok &= acquire_time(workflow);
  ok &= expect_color(workflow.observe(), host_sim::LedChannel::Green,
                     "P8 color-map counterfactual GOOD state");
  workflow.runtime.set_button_pressed(true);
  loop();
  ok &= expect_color(workflow.observe(), host_sim::LedChannel::Blue,
                     "P8 color-map counterfactual FAILED state");
  return ok;
}

bool run_case(const std::string &name) {
  if (name == "repaired") return run_repaired();
  if (name == "without-modes") return run_without_modes();
  if (name == "without-power") return run_without_power();
  if (name == "without-mapping") return run_without_mapping();
  throw std::runtime_error("unknown blocker-peeling LED case: " + name);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: blocker_peeling_led_cases <case>\n";
    return EXIT_FAILURE;
  }
  try {
    const auto ngspice = simulator();
    if (!ngspice.available()) {
      std::cerr << "ngspice unavailable; P8 LED case skipped\n";
      return 77;
    }
    return run_case(argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n";
    return EXIT_FAILURE;
  }
}
