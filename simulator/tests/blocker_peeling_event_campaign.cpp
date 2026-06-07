#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
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

#ifndef BLOCKER_VARIANT_ID
#error BLOCKER_VARIANT_ID must be defined by the build.
#endif

#ifndef BLOCKER_RMC_VALIDATED
#error BLOCKER_RMC_VALIDATED must be defined by the build.
#endif

using namespace std::chrono_literals;

constexpr std::size_t kExpectedScenarioCount = 32;
constexpr std::uint32_t kFuzzSeed = 0x45564E20U;

enum class Relation {
  Match,
  KnownDifferential,
  ObserveOnly,
};

std::string root_path(const std::string &relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

std::string variant_path(const std::string &relative) {
  return std::string(BLOCKER_VARIANT_DIR) + "/" + relative;
}

std::string csv_field(const std::string &value) {
  if (value.find_first_of(",\"\r\n") == std::string::npos) {
    return value;
  }
  std::string escaped = "\"";
  for (const char character : value) {
    escaped += character;
    if (character == '"') {
      escaped += '"';
    }
  }
  escaped += '"';
  return escaped;
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

bool contains(const std::string &value, const std::string &needle) {
  return value.find(needle) != std::string::npos;
}

std::string bool_field(bool value) { return value ? "1" : "0"; }

std::string nmea_sentence(const std::string &body, bool corrupt = false,
                          bool lowercase = false) {
  std::uint8_t checksum = 0;
  for (const char value : body) {
    checksum ^= static_cast<std::uint8_t>(value);
  }
  if (corrupt) {
    checksum ^= 0xFF;
  }

  std::ostringstream sentence;
  sentence << '$' << body << '*' << std::hex << std::setw(2)
           << std::setfill('0');
  if (!lowercase) {
    sentence << std::uppercase;
  }
  sentence << static_cast<unsigned int>(checksum) << "\r\n";
  return sentence.str();
}

std::string valid_rmc(const std::string &talker = "GN",
                      const std::string &time = "123519.00",
                      const std::string &day = "230394", char status = 'A',
                      bool corrupt_checksum = false,
                      bool lowercase_checksum = false) {
  return nmea_sentence(talker + "RMC," + time + "," + status +
                           ",4807.03800,N,01131.00000,E,0.004,77.52," + day +
                           ",,,A,V",
                       corrupt_checksum, lowercase_checksum);
}

std::string state_observation() {
  return "lock=" + bool_field(time_fixed) + ";time=" + utc_time +
         ";date=" + date;
}

std::uint32_t next_random(std::uint32_t &state) {
  state = state * 1664525U + 1013904223U;
  return state;
}

void process_direct(const std::string &sentence) {
  std::vector<char> mutable_sentence(sentence.begin(), sentence.end());
  mutable_sentence.push_back('\0');
  process_nmea(mutable_sentence.data(), static_cast<int>(sentence.size()));
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

host_sim::BoardModel board_model() {
  return host_sim::BoardModel::load(
      variant_path("kicad_files/hardware_challenge.kicad_pcb"),
      root_path("kicad_files/hardware_challenge.kicad_sch"),
      host_sim::HarnessRoutingMode::SchematicIdeal);
}

std::unique_ptr<host_sim::Runtime> make_runtime() {
  reset_firmware_globals();
  auto runtime = std::make_unique<host_sim::Runtime>(board_model());
  runtime->set_harness(blocker_peeling_test::declared_harness());
  runtime->set_button_pressed(false);
  return runtime;
}

void drain_serial() {
  std::size_t iterations = 0;
  while (host_sim::Serial1.available() > 0) {
    loop();
    if (++iterations > 4096) {
      throw std::runtime_error("serial drain exceeded deterministic bound");
    }
  }
}

void inject_and_drain(host_sim::Runtime &runtime, const std::string &message) {
  runtime.inject_serial1_rx_bypass_capacity(message);
  drain_serial();
}

void lock_time(host_sim::Runtime &runtime) {
  inject_and_drain(runtime, valid_rmc());
  if (!time_fixed) {
    throw std::runtime_error("valid RMC did not acquire a time lock");
  }
}

std::size_t result_lines(const host_sim::Runtime &runtime) {
  return count_occurrences(runtime.sd_content("results.txt"), "\r\n");
}

class Campaign {
public:
  explicit Campaign(const std::filesystem::path &output) : output_(output) {
    if (output_.has_parent_path()) {
      std::filesystem::create_directories(output_.parent_path());
    }
    matrix_.open(output_);
    if (!matrix_) {
      throw std::runtime_error("failed to open peripheral matrix: " +
                               output_.string());
    }
    matrix_ << "variant,scenario_id,category,oracle_kind,expected,observed,"
               "differential,classification,admission_eligible,error\n";
  }

  void check(const std::string &id, const std::string &category,
             const std::string &oracle_kind, const std::string &expected,
             Relation relation, const std::string &classification,
             const std::function<std::string()> &run) {
    ++scenarios_;
    std::string observed;
    std::string error;
    bool differential = false;
    bool failed = false;
    try {
      observed = run();
      differential = observed != expected;
      if (relation == Relation::Match && differential) {
        failed = true;
        error = "unexplained differential";
      } else if (relation == Relation::KnownDifferential && !differential) {
        failed = true;
        error = "expected counterfactual differential disappeared";
      }
    } catch (const std::exception &exception) {
      failed = true;
      error = exception.what();
    }
    failures_ += failed;

    const auto final_classification =
        failed && relation == Relation::Match ? "UNEXPLAINED" : classification;
    matrix_ << csv_field(BLOCKER_VARIANT_ID) << ',' << csv_field(id) << ','
            << csv_field(category) << ',' << csv_field(oracle_kind) << ','
            << csv_field(expected) << ',' << csv_field(observed) << ',';
    if (relation == Relation::ObserveOnly) {
      matrix_ << "not_applicable";
    } else {
      matrix_ << (differential ? 1 : 0);
    }
    matrix_ << ',' << csv_field(final_classification) << ",0,"
            << csv_field(error) << '\n';
  }

  std::size_t scenarios() const { return scenarios_; }
  std::size_t failures() const { return failures_; }

private:
  std::filesystem::path output_;
  std::ofstream matrix_;
  std::size_t scenarios_ = 0;
  std::size_t failures_ = 0;
};

Relation rmc_relation() {
  return BLOCKER_RMC_VALIDATED ? Relation::Match : Relation::KnownDifferential;
}

std::string rmc_classification(const std::string &classification) {
  return BLOCKER_RMC_VALIDATED ? "NONE" : classification;
}

void add_nmea_cases(Campaign &campaign) {
  const auto expected_unlocked = "lock=0;time=;date=";
  const auto expected_initial = "lock=1;time=123519.00;date=230394";

  campaign.check("nmea-valid-gprmc", "nmea_single", "u_blox_rmc_spec",
                 expected_initial, Relation::Match, "NONE", [] {
                   auto runtime = make_runtime();
                   setup();
                   inject_and_drain(*runtime, valid_rmc("GP"));
                   return state_observation();
                 });

  campaign.check("nmea-valid-gnrmc", "nmea_single", "u_blox_rmc_spec",
                 expected_initial, Relation::Match, "NONE", [] {
                   auto runtime = make_runtime();
                   setup();
                   inject_and_drain(*runtime, valid_rmc());
                   return state_observation();
                 });

  campaign.check("nmea-invalid-status", "nmea_single", "u_blox_rmc_spec",
                 expected_unlocked, rmc_relation(),
                 rmc_classification("A04_RMC_VALIDATION"), [] {
                   auto runtime = make_runtime();
                   setup();
                   inject_and_drain(
                       *runtime, valid_rmc("GN", "123519.00", "230394", 'V'));
                   return state_observation();
                 });

  campaign.check("nmea-corrupt-checksum", "nmea_single", "u_blox_rmc_spec",
                 expected_unlocked, rmc_relation(),
                 rmc_classification("A04_RMC_VALIDATION"), [] {
                   auto runtime = make_runtime();
                   setup();
                   inject_and_drain(*runtime, valid_rmc("GN", "123519.00",
                                                        "230394", 'A', true));
                   return state_observation();
                 });

  campaign.check("nmea-trailing-checksum-junk", "nmea_single",
                 "u_blox_rmc_spec", expected_unlocked, rmc_relation(),
                 rmc_classification("A04_RMC_VALIDATION"), [] {
                   auto runtime = make_runtime();
                   setup();
                   auto sentence = valid_rmc();
                   sentence.insert(sentence.size() - 2, "X");
                   inject_and_drain(*runtime, sentence);
                   return state_observation();
                 });

  campaign.check("nmea-invalid-hour", "nmea_single", "u_blox_rmc_spec",
                 expected_unlocked, rmc_relation(),
                 rmc_classification("A04_RMC_VALIDATION"), [] {
                   auto runtime = make_runtime();
                   setup();
                   inject_and_drain(*runtime, valid_rmc("GN", "246000.00"));
                   return state_observation();
                 });

  campaign.check(
      "nmea-invalid-date", "nmea_single", "u_blox_rmc_spec", expected_unlocked,
      rmc_relation(), rmc_classification("A04_RMC_VALIDATION"), [] {
        auto runtime = make_runtime();
        setup();
        inject_and_drain(*runtime, valid_rmc("GN", "123519.00", "310299"));
        return state_observation();
      });

  campaign.check("nmea-truncated-after-time", "nmea_single", "u_blox_rmc_spec",
                 expected_unlocked, rmc_relation(),
                 rmc_classification("BP_M002_ATOMICITY"), [] {
                   auto runtime = make_runtime();
                   setup();
                   inject_and_drain(*runtime,
                                    nmea_sentence("GNRMC,235959.00,V,"));
                   return state_observation();
                 });

  campaign.check(
      "nmea-non-rmc", "nmea_single", "u_blox_rmc_spec", expected_unlocked,
      Relation::Match, "NONE", [] {
        auto runtime = make_runtime();
        setup();
        inject_and_drain(
            *runtime,
            nmea_sentence(
                "GNGGA,123519.00,4807.03800,N,01131.00000,E,1,08,0.9"));
        return state_observation();
      });

  campaign.check(
      "nmea-oversized-valid-suffix", "nmea_single", "bounded_sentence_contract",
      expected_unlocked, Relation::Match, "NONE", [] {
        auto runtime = make_runtime();
        setup();
        inject_and_drain(*runtime, std::string(127, 'X') + valid_rmc());
        return state_observation();
      });

  campaign.check("nmea-lowercase-checksum", "nmea_single", "u_blox_rmc_spec",
                 expected_initial, Relation::Match, "NONE", [] {
                   auto runtime = make_runtime();
                   setup();
                   inject_and_drain(*runtime,
                                    valid_rmc("GN", "123519.00", "230394", 'A',
                                              false, true));
                   return state_observation();
                 });

  campaign.check(
      "nmea-valid-update-after-lock", "nmea_sequence",
      "current_timestamp_workflow", "lock=1;time=123520.00;date=240394",
      Relation::Match, "NONE", [] {
        auto runtime = make_runtime();
        setup();
        lock_time(*runtime);
        inject_and_drain(*runtime, valid_rmc("GN", "123520.00", "240394"));
        return state_observation();
      });

  campaign.check("nmea-partial-after-lock", "nmea_sequence", "u_blox_rmc_spec",
                 expected_initial, rmc_relation(),
                 rmc_classification("BP_M002_ATOMICITY"), [] {
                   auto runtime = make_runtime();
                   setup();
                   lock_time(*runtime);
                   inject_and_drain(*runtime,
                                    nmea_sentence("GNRMC,235959.00,V,"));
                   return state_observation();
                 });

  campaign.check("nmea-void-after-lock", "nmea_sequence", "u_blox_rmc_spec",
                 expected_initial, rmc_relation(),
                 rmc_classification("A04_RMC_VALIDATION"), [] {
                   auto runtime = make_runtime();
                   setup();
                   lock_time(*runtime);
                   inject_and_drain(
                       *runtime, valid_rmc("GN", "235959.00", "240394", 'V'));
                   return state_observation();
                 });

  campaign.check(
      "nmea-fragmented-valid", "nmea_sequence", "uart_stream_contract",
      expected_initial, Relation::Match, "NONE", [] {
        auto runtime = make_runtime();
        setup();
        const auto sentence = valid_rmc();
        const auto split = sentence.size() / 2;
        runtime->inject_serial1_rx_bypass_capacity(sentence.substr(0, split));
        loop();
        if (time_fixed) {
          throw std::runtime_error("partial frame acquired a time lock");
        }
        runtime->inject_serial1_rx_bypass_capacity(sentence.substr(split));
        drain_serial();
        return state_observation();
      });
}

void add_nmea_fuzz_cases(Campaign &campaign) {
  campaign.check(
      "nmea-invalid-mutation-corpus", "nmea_fuzz",
      "u_blox_rmc_spec_seed_1163284000", "cases=256;accepted=0;mutated=0",
      rmc_relation(), rmc_classification("A04_RMC_VALIDATION"), [] {
        auto runtime = make_runtime();
        setup();
        std::uint32_t random = kFuzzSeed;
        std::size_t accepted = 0;
        std::size_t mutated = 0;
        for (std::size_t index = 0; index < 256; ++index) {
          reset_firmware_globals();
          const auto mutation = next_random(random) % 7;
          std::string sentence;
          switch (mutation) {
          case 0:
            sentence = valid_rmc("GN", "123519.00", "230394", 'V');
            break;
          case 1:
            sentence = valid_rmc("GN", "123519.00", "230394", 'A', true);
            break;
          case 2:
            sentence = valid_rmc("GN", "246000.00");
            break;
          case 3:
            sentence = valid_rmc("GN", "123519.00", "310299");
            break;
          case 4:
            sentence = valid_rmc("GN", "1X3519.00");
            break;
          case 5:
            sentence = valid_rmc("GN", "123519.00", "23A394");
            break;
          default:
            sentence = nmea_sentence("GNRMC,235959.00,V,");
            break;
          }
          process_direct(sentence);
          accepted += time_fixed;
          mutated += utc_time[0] != '\0' || date[0] != '\0';
        }
        return "cases=256;accepted=" + std::to_string(accepted) +
               ";mutated=" + std::to_string(mutated);
      });

  campaign.check(
      "nmea-valid-generated-corpus", "nmea_fuzz",
      "u_blox_rmc_spec_seed_1163284000", "cases=128;accepted=128",
      Relation::Match, "NONE", [] {
        auto runtime = make_runtime();
        setup();
        std::uint32_t random = kFuzzSeed;
        std::size_t accepted = 0;
        constexpr std::array<int, 12> month_days{31, 29, 31, 30, 31, 30,
                                                 31, 31, 30, 31, 30, 31};
        for (std::size_t index = 0; index < 128; ++index) {
          reset_firmware_globals();
          const auto hour = next_random(random) % 24;
          const auto minute = next_random(random) % 60;
          const auto second = next_random(random) % 60;
          const auto month = next_random(random) % 12 + 1;
          const auto day = next_random(random) % month_days[month - 1] + 1;
          std::array<char, 11> time{};
          std::array<char, 7> day_string{};
          std::snprintf(time.data(), time.size(), "%02u%02u%02u.00", hour,
                        minute, second);
          std::snprintf(day_string.data(), day_string.size(), "%02u%02u24", day,
                        month);
          process_direct(valid_rmc("GN", time.data(), day_string.data()));
          accepted += time_fixed;
        }
        return "cases=128;accepted=" + std::to_string(accepted);
      });
}

host_sim::SimTime measure_scan_duration() {
  auto runtime = make_runtime();
  setup();
  lock_time(*runtime);
  const auto before = runtime->now();
  runtime->set_button_pressed(true);
  loop();
  return runtime->now() - before;
}

void add_button_cases(Campaign &campaign) {
  campaign.check("button-press-hold", "button_sequence",
                 "start_button_workflow", "sessions=1", Relation::Match, "NONE",
                 [] {
                   auto runtime = make_runtime();
                   setup();
                   lock_time(*runtime);
                   runtime->set_button_pressed(true);
                   loop();
                   loop();
                   return "sessions=" + std::to_string(result_lines(*runtime));
                 });

  campaign.check("button-release-repress", "button_sequence",
                 "start_button_workflow", "sessions=2", Relation::Match, "NONE",
                 [] {
                   auto runtime = make_runtime();
                   setup();
                   lock_time(*runtime);
                   runtime->set_button_pressed(true);
                   loop();
                   runtime->set_button_pressed(false);
                   loop();
                   runtime->set_button_pressed(true);
                   loop();
                   return "sessions=" + std::to_string(result_lines(*runtime));
                 });

  campaign.check("button-press-bounce-during-scan", "button_sequence",
                 "implementation_observation", "not_admission_oracle",
                 Relation::ObserveOnly, "BUTTON_BOUNCE_OBSERVATION", [] {
                   auto runtime = make_runtime();
                   setup();
                   lock_time(*runtime);
                   runtime->schedule_button_state(true, 0us);
                   runtime->schedule_button_state(false, 2ms);
                   runtime->schedule_button_state(true, 4ms);
                   runtime->advance_by(0us);
                   loop();
                   loop();
                   return "sessions=" + std::to_string(result_lines(*runtime)) +
                          ";pressed=" + bool_field(runtime->button_pressed());
                 });

  campaign.check("button-release-bounce", "button_sequence",
                 "implementation_observation", "not_admission_oracle",
                 Relation::ObserveOnly, "BUTTON_BOUNCE_OBSERVATION", [] {
                   auto runtime = make_runtime();
                   setup();
                   lock_time(*runtime);
                   runtime->set_button_pressed(true);
                   loop();
                   runtime->schedule_button_state(false, 0us);
                   runtime->schedule_button_state(true, 2ms);
                   runtime->schedule_button_state(false, 4ms);
                   runtime->advance_by(0us);
                   loop();
                   runtime->advance_by(2ms);
                   loop();
                   return "sessions=" + std::to_string(result_lines(*runtime)) +
                          ";pressed=" + bool_field(runtime->button_pressed());
                 });
}

void poll_uart(host_sim::Runtime &runtime, host_sim::SimTime duration,
               host_sim::SimTime period) {
  for (host_sim::SimTime elapsed{}; elapsed < duration; elapsed += period) {
    runtime.advance_by(period);
    loop();
  }
}

void add_uart_cases(Campaign &campaign) {
  campaign.check(
      "uart-9600-valid-rmc", "uart_sequence", "u_blox_uart_profile",
      "lock=1;framing=0;overruns=0", Relation::Match, "NONE", [] {
        auto runtime = make_runtime();
        setup();
        runtime->transmit_gps(valid_rmc(), 9600);
        poll_uart(*runtime, 100ms, 2ms);
        return "lock=" + bool_field(time_fixed) +
               ";framing=" + std::to_string(runtime->uart_framing_errors()) +
               ";overruns=" + std::to_string(runtime->serial1_rx_overruns());
      });

  campaign.check("uart-baud-mismatch", "uart_sequence", "u_blox_uart_profile",
                 "lock=0;framing_nonzero=1", Relation::Match, "NONE", [] {
                   auto runtime = make_runtime();
                   setup();
                   runtime->transmit_gps(valid_rmc(), 4800);
                   poll_uart(*runtime, 180ms, 2ms);
                   return "lock=" + bool_field(time_fixed) +
                          ";framing_nonzero=" +
                          bool_field(runtime->uart_framing_errors() > 0);
                 });

  campaign.check(
      "uart-unpolled-overrun", "uart_sequence", "implementation_observation",
      "not_admission_oracle", Relation::ObserveOnly, "A03_UART_CAPACITY_PATH",
      [] {
        auto runtime = make_runtime();
        setup();
        runtime->transmit_gps(std::string(80, 'X') + valid_rmc(), 9600);
        runtime->advance_by(200ms);
        drain_serial();
        return "lock=" + bool_field(time_fixed) +
               ";overruns=" + std::to_string(runtime->serial1_rx_overruns()) +
               ";index=" + std::to_string(nmea_idx);
      });
}

void add_i2c_cases(Campaign &campaign) {
  campaign.check("i2c-normal-startup", "i2c_sequence", "runtime_trace_contract",
                 "wire=1;accessed=1;faults=0", Relation::Match, "NONE", [] {
                   auto runtime = make_runtime();
                   setup();
                   std::size_t faults = 0;
                   for (const auto &transaction : runtime->i2c_trace()) {
                     faults += transaction.status != host_sim::I2cStatus::Ok;
                   }
                   return "wire=" + bool_field(runtime->wire_begun()) +
                          ";accessed=" +
                          bool_field(runtime->expander_accessed()) +
                          ";faults=" + std::to_string(faults);
                 });

  campaign.check("i2c-address-nack-at-startup", "i2c_sequence",
                 "implementation_observation", "not_admission_oracle",
                 Relation::ObserveOnly, "I2C_FAULT_POLICY_UNSPECIFIED", [] {
                   auto runtime = make_runtime();
                   runtime->set_i2c_next_nack(host_sim::I2cNack::Address);
                   setup();
                   const auto &trace = runtime->i2c_trace();
                   return "transactions=" + std::to_string(trace.size()) +
                          ";first_address_nack=" +
                          bool_field(!trace.empty() &&
                                     trace.front().status ==
                                         host_sim::I2cStatus::AddressNack) +
                          ";continued=" +
                          bool_field(contains(runtime->serial_output(),
                                              "Waiting for GPS time lock"));
                 });

  campaign.check("i2c-stuck-low-during-scan", "i2c_sequence",
                 "implementation_observation", "not_admission_oracle",
                 Relation::ObserveOnly, "I2C_FAULT_POLICY_UNSPECIFIED", [] {
                   auto runtime = make_runtime();
                   setup();
                   lock_time(*runtime);
                   runtime->set_i2c_line_faults(true, false);
                   runtime->set_button_pressed(true);
                   loop();
                   std::size_t faults = 0;
                   for (const auto &transaction : runtime->i2c_trace()) {
                     faults +=
                         transaction.status == host_sim::I2cStatus::BusStuckLow;
                   }
                   return "harness_failed=" +
                          bool_field(contains(runtime->serial_output(),
                                              "Harness failed!")) +
                          ";bus_faults=" + std::to_string(faults);
                 });
}

void add_sd_cases(Campaign &campaign) {
  campaign.check("sd-two-session-append", "sd_sequence",
                 "persistent_result_workflow", "sessions=2;diagnostics=0",
                 Relation::Match, "NONE", [] {
                   auto runtime = make_runtime();
                   setup();
                   lock_time(*runtime);
                   runtime->set_button_pressed(true);
                   loop();
                   runtime->set_button_pressed(false);
                   loop();
                   runtime->set_button_pressed(true);
                   loop();
                   return "sessions=" + std::to_string(result_lines(*runtime)) +
                          ";diagnostics=" +
                          std::to_string(count_occurrences(
                              runtime->serial_output(), "Failed to"));
                 });

  campaign.check(
      "sd-capacity-write-failure", "sd_sequence", "teensy_sdfat_contract",
      "content=230394 - 123519.00: ;write_diagnostic=1", Relation::Match,
      "NONE", [] {
        auto runtime = make_runtime();
        host_sim::SdCardConfig config;
        config.capacity_bytes = 20;
        runtime->configure_sd(config);
        setup();
        lock_time(*runtime);
        runtime->set_button_pressed(true);
        loop();
        return "content=" + runtime->sd_content("results.txt") +
               ";write_diagnostic=" +
               bool_field(contains(runtime->serial_output(),
                                   "Failed to write complete log entry"));
      });

  campaign.check("sd-removed-before-open", "sd_sequence",
                 "firmware_open_contract", "sessions=0;open_diagnostic=1",
                 Relation::Match, "NONE", [] {
                   auto runtime = make_runtime();
                   setup();
                   lock_time(*runtime);
                   runtime->set_sd_available(false);
                   runtime->set_button_pressed(true);
                   loop();
                   return "sessions=" + std::to_string(result_lines(*runtime)) +
                          ";open_diagnostic=" +
                          bool_field(contains(runtime->serial_output(),
                                              "Failed to open log file"));
                 });

  campaign.check(
      "sd-removed-during-write", "sd_sequence", "teensy_sdfat_contract",
      "sessions=0;write_diagnostic=1", Relation::Match, "NONE", [] {
        const auto scan_duration = measure_scan_duration();
        auto runtime = make_runtime();
        host_sim::SdCardConfig config;
        config.write_byte_time = 100us;
        runtime->configure_sd(config);
        setup();
        lock_time(*runtime);
        runtime->schedule_sd_available(false, scan_duration + 250us);
        runtime->set_button_pressed(true);
        loop();
        return "sessions=" + std::to_string(result_lines(*runtime)) +
               ";write_diagnostic=" +
               bool_field(contains(runtime->serial_output(),
                                   "Failed to write complete log entry"));
      });

  campaign.check("sd-reinsert-without-begin", "sd_sequence",
                 "implementation_observation", "not_admission_oracle",
                 Relation::ObserveOnly, "SD_REINSERT_POLICY_UNSPECIFIED", [] {
                   auto runtime = make_runtime();
                   setup();
                   lock_time(*runtime);
                   runtime->set_sd_available(false);
                   runtime->set_button_pressed(true);
                   loop();
                   runtime->set_button_pressed(false);
                   loop();
                   runtime->set_sd_available(true);
                   runtime->set_button_pressed(true);
                   loop();
                   return "sessions=" + std::to_string(result_lines(*runtime)) +
                          ";open_diagnostics=" +
                          std::to_string(
                              count_occurrences(runtime->serial_output(),
                                                "Failed to open log file"));
                 });
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: blocker_peeling_event_campaign <matrix.csv>\n";
    return EXIT_FAILURE;
  }

  try {
    Campaign campaign(argv[1]);
    add_nmea_cases(campaign);
    add_nmea_fuzz_cases(campaign);
    add_button_cases(campaign);
    add_uart_cases(campaign);
    add_i2c_cases(campaign);
    add_sd_cases(campaign);

    std::cout << "variant=" << BLOCKER_VARIANT_ID
              << " scenarios=" << campaign.scenarios()
              << " failures=" << campaign.failures() << '\n';
    if (campaign.scenarios() != kExpectedScenarioCount) {
      std::cerr << "unexpected peripheral scenario count\n";
      return EXIT_FAILURE;
    }
    return campaign.failures() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::exception &exception) {
    std::cerr << exception.what() << '\n';
    return EXIT_FAILURE;
  }
}
