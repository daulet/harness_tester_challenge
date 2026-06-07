#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "blocker_peeling_harness_fixture.h"
#include "host_simulator/runtime.h"

extern void setup();
extern void loop();
extern void process_nmea(char *buf, int len);
extern char utc_time[20];
extern char date[20];
extern bool time_fixed;
extern bool test_button_latched;
extern char nmea_buf[128];
extern int nmea_idx;
extern bool nmea_discarding;

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

#ifndef BLOCKER_VARIANT_DIR
#error BLOCKER_VARIANT_DIR must be defined by the build.
#endif

struct Case {
  std::string id;
  std::string category;
  std::string transport;
  std::string prelude;
  std::vector<std::string> chunks;
  bool expected_locked = false;
  std::string expected_time;
  std::string expected_date;
};

std::string root_path(const std::string &relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

std::string variant_path(const std::string &relative) {
  return std::string(BLOCKER_VARIANT_DIR) + "/" + relative;
}

std::vector<std::string> split(const std::string &value, char delimiter) {
  std::vector<std::string> fields;
  std::size_t begin = 0;
  while (true) {
    const auto end = value.find(delimiter, begin);
    fields.push_back(value.substr(begin, end - begin));
    if (end == std::string::npos) {
      return fields;
    }
    begin = end + 1;
  }
}

int hex_digit(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  throw std::runtime_error("invalid hexadecimal corpus byte");
}

std::string decode_hex(const std::string &value) {
  if (value.size() % 2 != 0) {
    throw std::runtime_error("odd-length hexadecimal corpus payload");
  }
  std::string decoded;
  decoded.reserve(value.size() / 2);
  for (std::size_t index = 0; index < value.size(); index += 2) {
    decoded.push_back(static_cast<char>((hex_digit(value[index]) << 4) |
                                        hex_digit(value[index + 1])));
  }
  return decoded;
}

std::vector<Case> load_cases(const std::string &path) {
  std::ifstream source(path);
  if (!source) {
    throw std::runtime_error("failed to open persisted corpus plan");
  }
  std::string line;
  if (!std::getline(source, line) ||
      line != "id\tcategory\ttransport\tprelude_hex\tchunks_hex\t"
              "expected_locked\texpected_time\texpected_date") {
    throw std::runtime_error("persisted corpus plan has an invalid header");
  }

  std::vector<Case> cases;
  while (std::getline(source, line)) {
    const auto fields = split(line, '\t');
    if (fields.size() != 8) {
      throw std::runtime_error("persisted corpus plan has a malformed row");
    }
    Case test_case;
    test_case.id = fields[0];
    test_case.category = fields[1];
    test_case.transport = fields[2];
    test_case.prelude = decode_hex(fields[3]);
    for (const auto &chunk : split(fields[4], ',')) {
      test_case.chunks.push_back(decode_hex(chunk));
    }
    if (fields[5] != "0" && fields[5] != "1") {
      throw std::runtime_error("persisted corpus lock expectation is invalid");
    }
    test_case.expected_locked = fields[5] == "1";
    test_case.expected_time = fields[6];
    test_case.expected_date = fields[7];
    cases.push_back(std::move(test_case));
  }
  return cases;
}

void reset_firmware_globals() {
  std::fill(std::begin(utc_time), std::end(utc_time), '\0');
  std::fill(std::begin(date), std::end(date), '\0');
  std::fill(std::begin(nmea_buf), std::end(nmea_buf), '\0');
  time_fixed = false;
  test_button_latched = false;
  nmea_idx = 0;
  nmea_discarding = false;
}

void process_direct(const std::string &sentence) {
  std::vector<char> mutable_sentence(sentence.begin(), sentence.end());
  mutable_sentence.push_back('\0');
  process_nmea(mutable_sentence.data(), static_cast<int>(sentence.size()));
}

void drain_serial() {
  std::size_t iterations = 0;
  while (host_sim::Serial1.available() > 0) {
    loop();
    if (++iterations > 16384) {
      throw std::runtime_error("persisted corpus serial drain exceeded bound");
    }
  }
}

std::unique_ptr<host_sim::Runtime> make_runtime() {
  auto board = host_sim::BoardModel::load(
      variant_path("kicad_files/hardware_challenge.kicad_pcb"),
      root_path("kicad_files/hardware_challenge.kicad_sch"),
      host_sim::HarnessRoutingMode::SchematicIdeal);
  auto runtime = std::make_unique<host_sim::Runtime>(std::move(board));
  runtime->set_harness(blocker_peeling_test::declared_harness());
  runtime->set_button_pressed(false);
  return runtime;
}

void run_case(host_sim::Runtime &runtime, const Case &test_case) {
  reset_firmware_globals();
  if (!test_case.prelude.empty()) {
    process_direct(test_case.prelude);
  }

  if (test_case.transport == "direct") {
    for (const auto &chunk : test_case.chunks) {
      process_direct(chunk);
    }
    return;
  }
  if (test_case.transport == "loop") {
    for (const auto &chunk : test_case.chunks) {
      runtime.inject_serial1_rx_bypass_capacity(chunk);
      drain_serial();
    }
    return;
  }
  throw std::runtime_error("unknown persisted corpus transport");
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: blocker_peeling_persisted_corpus <plan.tsv>\n";
    return EXIT_FAILURE;
  }

  try {
    const auto cases = load_cases(argv[1]);
    auto runtime = make_runtime();
    setup();
    std::cout << "id\tobserved_locked\tobserved_time\tobserved_date\n";
    std::size_t failures = 0;
    for (const auto &test_case : cases) {
      run_case(*runtime, test_case);
      std::cout << test_case.id << '\t' << (time_fixed ? 1 : 0) << '\t'
                << utc_time << '\t' << date << '\n';
      failures += time_fixed != test_case.expected_locked ||
                  utc_time != test_case.expected_time ||
                  date != test_case.expected_date;
    }
    if (failures != 0) {
      std::cerr << "persisted corpus mismatches=" << failures << '\n';
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  } catch (const std::exception &exception) {
    std::cerr << exception.what() << '\n';
    return EXIT_FAILURE;
  }
}
