#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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
  using namespace std::chrono_literals;

  host_sim::Runtime runtime(host_sim::BoardModel::load(
      data_path("kicad_files/hardware_challenge.kicad_pcb"),
      data_path("kicad_files/hardware_challenge.kicad_sch")));
  host_sim::Wire2.begin();

  bool ok = true;
  ok &= require(!runtime.expander_reset_asserted(),
                "expander powered on with active-high XRES asserted");
  ok &= require(runtime.i2c_write(0x20, {0x2E}),
                "power-on expander did not accept the device ID register select");
  const auto power_on_id = runtime.i2c_read(0x20, 1);
  ok &= require(power_on_id.size() == 1 && power_on_id[0] == 0x60,
                "power-on expander did not expose POR register state");
  ok &= require(runtime.i2c_write(0x20, {0x08}),
                "power-on expander did not accept output register select");
  const auto power_on_output = runtime.i2c_read(0x20, 1);
  ok &= require(power_on_output.size() == 1 && power_on_output[0] == 0xFF,
                "expander output reset default did not match");
  ok &= require(runtime.i2c_write(0x20, {0x00}),
                "power-on expander did not accept input register select");
  const auto power_on_input = runtime.i2c_read(0x20, 1);
  ok &= require(power_on_input.size() == 1 && power_on_input[0] == 0xFF,
                "power-on pull-up pins did not resolve high");
  ok &= require(runtime.expander_pin_drive(0).drive_mode ==
                    host_sim::DriveMode::PullUp,
                "power-on expander did not restore the factory pull-up mode");

  runtime.digital_write(22, 1);
  ok &= require(runtime.expander_reset_asserted() &&
                    runtime.i2c_read(0x20, 1).empty(),
                "expander responded while active-high XRES was asserted");
  ok &= require(runtime.expander_pin_drive(0).drive_mode ==
                    host_sim::DriveMode::HighImpedance,
                "held-reset expander pin was not high impedance");

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

  std::vector<int> order;
  std::vector<host_sim::SimTime> timestamps;
  runtime.schedule_at(500us, [&] {
    order.push_back(1);
    timestamps.push_back(runtime.now());
    runtime.schedule_at(runtime.now(), [&] {
      order.push_back(3);
      timestamps.push_back(runtime.now());
    });
  });
  runtime.schedule_at(500us, [&] {
    order.push_back(2);
    timestamps.push_back(runtime.now());
  });
  runtime.schedule_after(2ms, [&] {
    order.push_back(4);
    timestamps.push_back(runtime.now());
  });

  runtime.advance_to(499us);
  ok &= require(order.empty(), "event queue delivered an event before its due time");
  runtime.advance_by(1us);
  ok &= require(order == std::vector<int>({1, 2, 3}),
                "same-time events did not preserve stable scheduling order");
  ok &= require(timestamps.size() == 3 && timestamps[0] == 500us &&
                    timestamps[1] == 500us && timestamps[2] == 500us,
                "same-time callbacks did not observe their exact due time");

  runtime.delay(1);
  ok &= require(order.size() == 3 && runtime.now() == 1500us,
                "Arduino delay did not advance deterministic simulation time");
  runtime.delay(1);
  ok &= require(order == std::vector<int>({1, 2, 3, 4}) &&
                    timestamps.back() == 2000us && runtime.elapsed_ms() == 2,
                "Arduino delay did not deliver the due event at its exact timestamp");

  bool rejected_backwards = false;
  try {
    runtime.advance_to(1999us);
  } catch (const std::invalid_argument&) {
    rejected_backwards = true;
  }
  ok &= require(rejected_backwards,
                "event queue allowed simulation time to move backwards");

  bool rejected_reentrant_advance = false;
  runtime.schedule_at(runtime.now(), [&] {
    try {
      runtime.advance_by(1us);
    } catch (const std::logic_error&) {
      rejected_reentrant_advance = true;
    }
  });
  runtime.advance_by(0us);
  ok &= require(rejected_reentrant_advance,
                "event callback recursively advanced simulation time");

  bool stale_event_ran = false;
  runtime.schedule_after(1ms, [&] { stale_event_ran = true; });
  runtime.clear_peripherals();
  runtime.advance_by(2ms);
  ok &= require(!stale_event_ran && runtime.now() == 2ms,
                "runtime reset retained a stale scheduled event");

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
