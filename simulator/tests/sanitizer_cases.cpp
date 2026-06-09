#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "board_fixture.h"
#include "host_simulator/gps.h"
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

host_sim::Runtime make_corrected_uart_runtime() {
  const auto pcb = test_support::corrected_uart_pcb(
      HOST_SIM_ROOT, "corrected_gps_sanitizer_hardware_challenge.kicad_pcb");
  return host_sim::Runtime(host_sim::BoardModel::load(
      pcb, data_path("kicad_files/hardware_challenge.kicad_sch")));
}

void run_nmea_append_overflow() {
  auto runtime = make_runtime();
  runtime.inject_serial1_rx_bypass_capacity(std::string(65, 'x'));
  setup();
  loop();
}

void run_nmea_terminator_overflow() {
  auto runtime = make_runtime();
  runtime.inject_serial1_rx_bypass_capacity(std::string(63, 'x') + "\n");
  setup();
  loop();
}

void run_narrow_shift() {
  auto runtime = make_runtime();
  runtime.set_button_pressed(false);
  runtime.inject_serial1_rx_bypass_capacity(kValidGprmc);
  setup();
  loop();
}

void run_default_gps_overflow() {
  auto runtime = make_corrected_uart_runtime();
  runtime.set_button_pressed(true);
  setup();

  host_sim::GpsReceiver receiver(runtime);
  host_sim::GpsReceiverConfig config;
  config.acquisition_time = host_sim::SimTime::zero();
  receiver.start(config);
  runtime.advance_by(std::chrono::milliseconds(1700));
  loop();
  loop();
  loop();
  runtime.advance_by(std::chrono::seconds(1));
  loop();
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: sanitizer_cases "
                 "<nmea-append-overflow|nmea-terminator-overflow|"
                 "narrow-shift|default-gps-overflow>\n";
    return EXIT_FAILURE;
  }

  const std::string name = argv[1];
  if (name == "nmea-append-overflow") {
    run_nmea_append_overflow();
  } else if (name == "nmea-terminator-overflow") {
    run_nmea_terminator_overflow();
  } else if (name == "narrow-shift") {
    run_narrow_shift();
  } else if (name == "default-gps-overflow") {
    run_default_gps_overflow();
  } else {
    throw std::runtime_error("unknown sanitizer case: " + name);
  }

  std::cerr << "sanitizer did not detect the expected firmware defect: " << name
            << "\n";
  return EXIT_SUCCESS;
}
