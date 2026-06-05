#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "CY8C9560.h"
#include "host_simulator/runtime.h"

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

std::string data_path(const std::string &relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

host_sim::BoardModel board_model() {
  return host_sim::BoardModel::load(
      data_path("kicad_files/hardware_challenge.kicad_pcb"),
      data_path("kicad_files/hardware_challenge.kicad_sch"));
}

bool require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

} // namespace

int main() {
  using namespace std::chrono_literals;

  bool ok = true;
  {
    host_sim::Runtime runtime(board_model());
    CY8C9560 driver;
    ok &= require(driver.begin(),
                  "CY8C9560 driver did not read its ID after reset release");
    const auto &trace = runtime.expander_reset_trace();
    ok &= require(trace.size() == 2 && trace[0].time == 0ms &&
                      trace[0].asserted && trace[1].time == 10ms &&
                      !trace[1].asserted,
                  "CY8C9560 reset pulse did not match the firmware timeline");
    ok &= require(runtime.now() == 110ms && !runtime.expander_reset_asserted(),
                  "CY8C9560 begin did not preserve its 10 ms/100 ms delays");
  }

  {
    host_sim::Runtime runtime(board_model());
    runtime.schedule_button_state(true, 1ms);
    runtime.schedule_button_state(false, 1200us);
    runtime.schedule_button_state(true, 1500us);
    runtime.schedule_button_state(false, 11500us);

    runtime.advance_to(999us);
    ok &= require(!runtime.button_pressed(),
                  "button transition arrived before the scheduled press");
    runtime.advance_to(1ms);
    ok &= require(runtime.button_pressed(),
                  "button press did not arrive at its exact timestamp");
    runtime.advance_to(1200us);
    ok &=
        require(!runtime.button_pressed(),
                "button bounce release did not arrive at its exact timestamp");
    runtime.advance_to(1500us);
    ok &=
        require(runtime.button_pressed(),
                "button bounce repress did not arrive at its exact timestamp");
    runtime.advance_to(11500us);
    ok &= require(!runtime.button_pressed(),
                  "button press duration did not end at its exact timestamp");

    runtime.schedule_button_state(true, 1ms);
    runtime.clear_peripherals();
    runtime.advance_by(2ms);
    ok &= require(!runtime.button_pressed(),
                  "runtime reset retained a stale button transition");
  }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
