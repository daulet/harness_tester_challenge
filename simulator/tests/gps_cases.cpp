#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "board_fixture.h"
#include "host_simulator/gps.h"

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

std::string data_path(const std::string &relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

host_sim::BoardModel corrected_board_model() {
  const auto pcb = test_support::corrected_uart_pcb(
      HOST_SIM_ROOT, "corrected_gps_hardware_challenge.kicad_pcb");
  return host_sim::BoardModel::load(
      pcb, data_path("kicad_files/hardware_challenge.kicad_sch"));
}

bool require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

std::string read_serial1() {
  std::string bytes;
  while (host_sim::Serial1.available()) {
    bytes.push_back(static_cast<char>(host_sim::Serial1.read()));
  }
  return bytes;
}

std::vector<std::string> nmea_lines(const std::string &bytes) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start < bytes.size()) {
    const auto end = bytes.find("\r\n", start);
    if (end == std::string::npos) {
      break;
    }
    lines.push_back(bytes.substr(start, end - start));
    start = end + 2;
  }
  return lines;
}

int hex_value(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

bool checksum_valid(const std::string &line) {
  const auto separator = line.find('*');
  if (line.empty() || line.front() != '$' || separator == std::string::npos ||
      separator + 2 >= line.size()) {
    return false;
  }
  std::uint8_t checksum = 0;
  for (std::size_t index = 1; index < separator; ++index) {
    checksum ^= static_cast<std::uint8_t>(line[index]);
  }
  const auto high = hex_value(line[separator + 1]);
  const auto low = hex_value(line[separator + 2]);
  return high >= 0 && low >= 0 &&
         checksum == static_cast<std::uint8_t>((high << 4) | low);
}

std::size_t count_occurrences(const std::string &value,
                              const std::string &needle) {
  std::size_t count = 0;
  std::size_t offset = 0;
  while ((offset = value.find(needle, offset)) != std::string::npos) {
    ++count;
    offset += needle.size();
  }
  return count;
}

} // namespace

int main() {
  using namespace std::chrono_literals;

  bool ok = true;
  const host_sim::GpsReceiverConfig defaults;
  ok &=
      require(defaults.acquisition_time == 26s && defaults.epoch_period == 1s &&
                  defaults.constellation ==
                      host_sim::GpsConstellationMode::Concurrent &&
                  defaults.checksum == host_sim::GpsChecksumMode::Valid,
              "GPS defaults diverged from the NEO-M8 source profile");
  {
    host_sim::Runtime runtime(corrected_board_model());
    host_sim::Serial1.begin(9600);
    host_sim::GpsReceiver receiver(runtime);
    host_sim::GpsReceiverConfig config;
    config.acquisition_time = 2s;
    receiver.start(config);

    ok &= require(receiver.output().find("$GPTXT") == 0 &&
                      receiver.epochs_emitted() == 0,
                  "GPS startup did not emit one immediate TXT notice");
    runtime.advance_to(999999us);
    ok &= require(receiver.epochs_emitted() == 0,
                  "GPS navigation epoch arrived before one second");
    runtime.advance_to(1s);
    ok &= require(receiver.epochs_emitted() == 1,
                  "GPS first navigation epoch did not start at one second");
    runtime.advance_to(1700ms);

    auto received = read_serial1();
    const auto first_output = receiver.output();
    ok &= require(received == first_output,
                  "GPS output did not traverse corrected timed UART intact");
    const auto first_lines = nmea_lines(first_output);
    ok &= require(first_lines.size() == 8,
                  "GPS startup/default sentence set was incomplete");
    for (const auto &line : first_lines) {
      ok &= require(checksum_valid(line),
                    "GPS receiver generated an invalid NMEA checksum");
    }
    ok &=
        require(first_output.find("$GNRMC,123519.00,V") != std::string::npos &&
                    first_output.find("$GPGSV") != std::string::npos &&
                    first_output.find("$GLGSV") != std::string::npos,
                "pre-fix concurrent-GNSS epoch did not use source-backed IDs");

    runtime.advance_to(2700ms);
    received += read_serial1();
    ok &= require(
        receiver.epochs_emitted() == 2 && received == receiver.output() &&
            receiver.output().find("$GNRMC,123520.00,A") != std::string::npos &&
            count_occurrences(receiver.output(), "$GPTXT") == 1,
        "GPS fix transition or startup-only TXT cadence was wrong");

    receiver.stop();
    runtime.advance_by(2s);
    ok &= require(!receiver.running() && receiver.epochs_emitted() == 2,
                  "stopped GPS receiver emitted another epoch");
  }

  {
    host_sim::Runtime runtime(corrected_board_model());
    host_sim::Serial1.begin(9600);
    host_sim::GpsReceiver receiver(runtime);
    host_sim::GpsReceiverConfig config;
    config.acquisition_time = 0s;
    config.constellation = host_sim::GpsConstellationMode::GpsOnly;
    config.checksum = host_sim::GpsChecksumMode::Corrupt;
    receiver.start(config);
    runtime.advance_to(1700ms);

    const auto lines = nmea_lines(receiver.output());
    ok &= require(lines.size() == 7 &&
                      receiver.output().find("$GPRMC,123519.00,A") !=
                          std::string::npos,
                  "GPS-only receiver did not emit its configured sentence set");
    for (const auto &line : lines) {
      ok &= require(!checksum_valid(line),
                    "checksum fault injection left a valid NMEA sentence");
    }
  }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
