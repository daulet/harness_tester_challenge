#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "host_simulator/gps.h"
#include "host_simulator/oracle.h"
#include "host_simulator/runtime.h"

extern void setup();
extern void loop();
extern bool time_fixed;

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

#ifndef BLOCKER_VARIANT_DIR
#error BLOCKER_VARIANT_DIR must be defined by the build.
#endif

#ifndef BLOCKER_VARIANT_ID
#error BLOCKER_VARIANT_ID must be defined by the build.
#endif

bool require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

host_sim::ReachabilityExpectation expectation_for(const std::string &variant) {
  if (variant == "P0" || variant == "P1_without_FW_EXPANDER_BEGIN") {
    return {false, false, false, false};
  }
  if (variant == "P1" || variant == "P2_without_PCB_I2C_SDA_PULLUP") {
    return {true, false, false, false};
  }
  if (variant == "P2") {
    return {true, true, false, false};
  }
  if (variant == "P3") {
    return {true, true, true, true};
  }
  if (variant == "P3_without_FW_EXPANDER_BEGIN") {
    return {false, false, true, true};
  }
  if (variant == "P3_without_PCB_I2C_SDA_PULLUP") {
    return {true, false, true, true};
  }
  if (variant == "P3_without_PCB_UART_CROSS") {
    return {true, true, false, false};
  }
  if (variant == "P3_without_FW_ACCEPT_GNRMC") {
    return {true, true, false, true};
  }
  if (variant == "P3_without_FW_NMEA_BOUNDS") {
    throw std::runtime_error(
        "the no-bounds variant is valid only as a sanitizer witness");
  }
  throw std::runtime_error("unknown blocker-peeling variant: " + variant);
}

host_sim::ReachabilityExpectation
stage_requirement(const std::string &variant) {
  if (variant == "P0") {
    return {false, false, false, false};
  }
  if (variant == "P1" || variant == "P1_without_FW_EXPANDER_BEGIN") {
    return {true, false, false, false};
  }
  if (variant == "P2" || variant == "P2_without_PCB_I2C_SDA_PULLUP") {
    return {true, true, false, false};
  }
  return {true, true, true, true};
}

std::vector<std::string> required_mismatches(const std::string &variant) {
  if (variant == "P0" || variant == "P1" || variant == "P2" ||
      variant == "P3") {
    return {};
  }
  if (variant == "P1_without_FW_EXPANDER_BEGIN") {
    return {"wire_begun"};
  }
  if (variant == "P2_without_PCB_I2C_SDA_PULLUP") {
    return {"expander_accessed"};
  }
  if (variant == "P3_without_FW_EXPANDER_BEGIN") {
    return {"wire_begun", "expander_accessed"};
  }
  if (variant == "P3_without_PCB_I2C_SDA_PULLUP") {
    return {"expander_accessed"};
  }
  if (variant == "P3_without_PCB_UART_CROSS") {
    return {"time_locked", "uart_routed"};
  }
  if (variant == "P3_without_FW_ACCEPT_GNRMC") {
    return {"time_locked"};
  }
  return {};
}

host_sim::ReachabilityObservation run_default_workflow() {
  using namespace std::chrono_literals;

  const auto pcb = std::string(BLOCKER_VARIANT_DIR) +
                   "/kicad_files/hardware_challenge.kicad_pcb";
  const auto schematic =
      std::string(HOST_SIM_ROOT) + "/kicad_files/hardware_challenge.kicad_sch";
  host_sim::Runtime runtime(host_sim::BoardModel::load(pcb, schematic));
  runtime.set_button_pressed(true);
  setup();

  host_sim::GpsReceiver receiver(runtime);
  host_sim::GpsReceiverConfig config;
  config.acquisition_time = host_sim::SimTime::zero();
  receiver.start(config);
  for (int poll = 0; poll < 500; ++poll) {
    runtime.advance_by(5ms);
    loop();
  }

  return {
      runtime.wire_begun(),
      runtime.expander_accessed(),
      time_fixed,
      // epochs_emitted counts bytes placed on the wire, independently of
      // whether the firmware recognizes the sentence talker ID.
      receiver.epochs_emitted() > 0 && runtime.uart_unrouted_frames() == 0 &&
          runtime.uart_framing_errors() == 0 &&
          runtime.uart_contention_frames() == 0,
  };
}

} // namespace

int main(int argc, char **argv) {
  try {
    if (argc > 2 ||
        (argc == 2 && std::string(argv[1]) != "normal-default-gps")) {
      std::cerr << "usage: blocker_peeling_cases [normal-default-gps]\n";
      return EXIT_FAILURE;
    }
    const std::string variant = BLOCKER_VARIANT_ID;
    const auto observation = run_default_workflow();
    const auto exact =
        host_sim::compare_reachability(observation, expectation_for(variant));
    bool ok = require(exact.empty(),
                      variant + ": observed reachability did not match its "
                                "declared counterfactual state");

    const auto stage =
        host_sim::compare_reachability(observation, stage_requirement(variant));
    ok &= require(stage == required_mismatches(variant),
                  variant + ": leave-one-out control did not isolate the "
                            "expected reachability invariant");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n";
    return EXIT_FAILURE;
  }
}
