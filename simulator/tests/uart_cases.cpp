#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "host_simulator/runtime.h"

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

std::string data_path(const std::string &relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

std::string read_file(const std::string &path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("failed to open source file: " + path);
  }
  std::ostringstream content;
  content << file.rdbuf();
  return content.str();
}

void write_file(const std::string &path, const std::string &content) {
  std::ofstream file(path);
  if (!file) {
    throw std::runtime_error("failed to write source file: " + path);
  }
  file << content;
}

void replace_after(std::string &source, std::size_t offset,
                   const std::string &from, const std::string &to) {
  const auto marker = source.find(from, offset);
  if (marker == std::string::npos) {
    throw std::runtime_error("UART mutation marker not found: " + from);
  }
  source.replace(marker, from.size(), to);
}

host_sim::BoardModel board_model(const std::string &pcb_path) {
  return host_sim::BoardModel::load(
      pcb_path, data_path("kicad_files/hardware_challenge.kicad_sch"));
}

std::string corrected_uart_pcb() {
  auto source =
      read_file(data_path("kicad_files/hardware_challenge.kicad_pcb"));
  const auto u2 = source.find("(property \"Reference\" \"U2\"");
  if (u2 == std::string::npos) {
    throw std::runtime_error("UART mutation did not find U2");
  }
  const auto rx_pad = source.find("(pad \"2\"", u2);
  const auto tx_pad = source.find("(pad \"3\"", rx_pad);
  if (rx_pad == std::string::npos || tx_pad == std::string::npos) {
    throw std::runtime_error("UART mutation did not find U2 serial pads");
  }
  replace_after(source, rx_pad, "(net 64 \"UBX-RXD\")", "(net 74 \"UBX-TXD\")");
  replace_after(source, tx_pad, "(net 74 \"UBX-TXD\")", "(net 64 \"UBX-RXD\")");
  const auto path =
      data_path("build/corrected_uart_hardware_challenge.kicad_pcb");
  write_file(path, source);
  return path;
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
  const auto corrected_pcb = corrected_uart_pcb();
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
    runtime.inject_serial1_rx("Z");
    ok &= require(
        host_sim::Serial1.available() == 1 && host_sim::Serial1.read() == 'Z',
        "direct parser injection unexpectedly used physical UART timing");
  }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
