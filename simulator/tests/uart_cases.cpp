#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "host_simulator/runtime.h"
#include "board_fixture.h"

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

std::string data_path(const std::string &relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

host_sim::BoardModel board_model(const std::string &pcb_path) {
  return host_sim::BoardModel::load(
      pcb_path, data_path("kicad_files/hardware_challenge.kicad_sch"));
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

  const auto original_pcb =
      data_path("kicad_files/hardware_challenge.kicad_pcb");
  const auto corrected_pcb = test_support::corrected_uart_pcb(
      HOST_SIM_ROOT, "corrected_uart_hardware_challenge.kicad_pcb");
  bool ok = true;

  {
    host_sim::Runtime runtime(board_model(original_pcb));
    host_sim::Serial1.begin(9600);
    runtime.transmit_gps("A");
    runtime.advance_to(1041us);
    ok &= require(host_sim::Serial1.available() == 0,
                  "misrouted GPS byte arrived before frame completion");
    runtime.advance_to(1042us);
    ok &= require(host_sim::Serial1.available() == 0 &&
                      runtime.uart_unrouted_frames() == 1 &&
                      runtime.uart_contention_frames() == 1,
                  "same-direction board UART unexpectedly reached MCU RX");
  }

  {
    host_sim::Runtime runtime(board_model(corrected_pcb));
    host_sim::Serial1.begin(9600);
    runtime.transmit_gps("AB");
    runtime.advance_to(1041us);
    ok &= require(host_sim::Serial1.available() == 0,
                  "UART byte became available before its stop bit completed");
    runtime.advance_to(1042us);
    ok &= require(host_sim::Serial1.available() == 1 &&
                      host_sim::Serial1.read() == 'A',
                  "first 9600-baud frame did not arrive at 1042 us");
    runtime.advance_to(2083us);
    ok &=
        require(host_sim::Serial1.available() == 0,
                "second UART byte arrived before its rational frame boundary");
    runtime.advance_to(2084us);
    ok &= require(host_sim::Serial1.available() == 1 &&
                      host_sim::Serial1.read() == 'B',
                  "second 9600-baud frame did not arrive at 2084 us");
  }

  {
    host_sim::Runtime runtime(board_model(corrected_pcb));
    host_sim::Serial1.begin(9600);
    runtime.transmit_gps(std::string(70, 'R'));
    runtime.advance_to(73ms);
    ok &= require(runtime.serial1_rx_capacity() == 63 &&
                      host_sim::Serial1.available() == 63 &&
                      runtime.serial1_rx_overruns() == 7,
                  "Teensy Serial1 did not drop new bytes after 63 buffered bytes");
    while (host_sim::Serial1.available()) {
      ok &= require(host_sim::Serial1.read() == 'R',
                    "Serial1 overrun corrupted an already buffered byte");
    }
    runtime.transmit_gps("S");
    runtime.advance_by(1042us);
    ok &= require(host_sim::Serial1.available() == 1 &&
                      host_sim::Serial1.read() == 'S' &&
                      runtime.serial1_rx_overruns() == 7,
                  "draining Serial1 did not free receive capacity");
  }

  {
    host_sim::Runtime runtime(board_model(corrected_pcb));
    host_sim::Serial1.begin(115200);
    runtime.transmit_gps("A", 9600);
    runtime.advance_to(1042us);
    ok &= require(host_sim::Serial1.available() == 0 &&
                      runtime.uart_framing_errors() == 1,
                  "incompatible UART baud did not produce a framing failure");
  }

  {
    host_sim::Runtime runtime(board_model(original_pcb));
    host_sim::Serial1.begin(9600);
    host_sim::Serial1.print("M");
    runtime.advance_to(1042us);
    ok &= require(runtime.uart_contention_frames() == 1,
                  "MCU transmit did not contend with the GPS idle driver");
    ok &= require(host_sim::Serial1.output() == "M",
                  "timed MCU UART transmission lost its TX transcript");
  }

  {
    host_sim::Runtime runtime(board_model(original_pcb));
    host_sim::Serial1.begin(9600);
    host_sim::Serial1.print("M");
    runtime.transmit_gps("G");
    runtime.advance_to(1042us);
    ok &= require(runtime.uart_contention_frames() == 2,
                  "overlapping MCU and GPS transmitters did not contend");
  }

  {
    host_sim::Runtime runtime(board_model(original_pcb));
    runtime.configure_serial1_rx_capacity(1);
    runtime.inject_serial1_rx_bypass_capacity("XYZ");
    ok &= require(
        host_sim::Serial1.available() == 3 && host_sim::Serial1.read() == 'X' &&
            runtime.serial1_rx_overruns() == 0,
        "direct parser injection unexpectedly used physical UART buffering");
  }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
