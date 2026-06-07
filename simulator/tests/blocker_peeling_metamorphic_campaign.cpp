#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "blocker_peeling_harness_fixture.h"
#include "host_simulator/runtime.h"

extern void setup();
extern void loop();
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

using namespace std::chrono_literals;

constexpr std::size_t kCasesPerCategory = 64;
constexpr std::size_t kExpectedPairCount = 6 * kCasesPerCategory;
constexpr std::size_t kExpectedControlCount = 6;

struct Outcome {
  bool locked = false;
  std::string time;
  std::string day;
  std::size_t sessions = 0;
  std::string log;
  std::string status;
  std::size_t framing_errors = 0;
  std::size_t overruns = 0;
  std::size_t i2c_transactions = 0;
  std::size_t i2c_failures = 0;
  std::uint64_t elapsed_us = 0;
};

std::string root_path(const std::string &relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

std::string variant_path(const std::string &relative) {
  return std::string(BLOCKER_VARIANT_DIR) + "/" + relative;
}

const host_sim::BoardModel &campaign_board() {
  static const auto model = host_sim::BoardModel::load(
      variant_path("kicad_files/hardware_challenge.kicad_pcb"),
      root_path("kicad_files/hardware_challenge.kicad_sch"),
      host_sim::HarnessRoutingMode::SchematicIdeal);
  return model;
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

std::uint32_t next_random(std::uint32_t &state) {
  state = state * 1664525U + 1013904223U;
  return state;
}

std::string nmea_sentence(const std::string &body) {
  std::uint8_t checksum = 0;
  for (const char value : body) {
    checksum ^= static_cast<std::uint8_t>(value);
  }
  std::ostringstream sentence;
  sentence << '$' << body << '*' << std::hex << std::uppercase << std::setw(2)
           << std::setfill('0') << static_cast<unsigned int>(checksum)
           << "\r\n";
  return sentence.str();
}

std::string valid_rmc(const std::string &time, const std::string &day) {
  return nmea_sentence("GNRMC," + time +
                       ",A,4807.03800,N,01131.00000,E,0.004,77.52," + day +
                       ",,,A,V");
}

std::string checksum_invalid_rmc(const std::string &time,
                                 const std::string &day) {
  auto message = valid_rmc(time, day);
  const auto separator = message.find('*');
  if (separator == std::string::npos || separator + 1 >= message.size()) {
    throw std::runtime_error("generated RMC has no checksum");
  }
  message[separator + 1] = message[separator + 1] == '0' ? '1' : '0';
  return message;
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

std::unique_ptr<host_sim::Runtime>
make_runtime(const host_sim::SdCardConfig *sd_config = nullptr,
             host_sim::SimTime i2c_stretch = host_sim::SimTime::zero(),
             bool i2c_stuck_low = false) {
  reset_firmware_globals();
  auto runtime = std::make_unique<host_sim::Runtime>(campaign_board());
  runtime->set_harness(blocker_peeling_test::declared_harness());
  runtime->set_button_pressed(false);
  if (sd_config) {
    runtime->configure_sd(*sd_config);
  }
  runtime->set_i2c_clock_stretch(i2c_stretch);
  runtime->set_i2c_line_faults(i2c_stuck_low, false);
  return runtime;
}

void drain_serial() {
  std::size_t iterations = 0;
  while (host_sim::Serial1.available() > 0) {
    loop();
    if (++iterations > 16384) {
      throw std::runtime_error("metamorphic serial drain exceeded bound");
    }
  }
}

void inject_chunks(host_sim::Runtime &runtime,
                   const std::vector<std::string> &chunks) {
  for (const auto &chunk : chunks) {
    runtime.inject_serial1_rx_bypass_capacity(chunk);
    drain_serial();
  }
}

void lock_time(host_sim::Runtime &runtime,
               const std::string &time = "123519.00",
               const std::string &day = "230394") {
  inject_chunks(runtime, {valid_rmc(time, day)});
  if (!time_fixed) {
    throw std::runtime_error("metamorphic setup did not acquire time lock");
  }
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

std::string led_status(const host_sim::LedState &state) {
  if (state.red && !state.green && !state.blue) {
    return "red";
  }
  if (!state.red && state.green && !state.blue) {
    return "green";
  }
  if (!state.red && !state.green && state.blue) {
    return "blue";
  }
  return "other";
}

Outcome observe(const host_sim::Runtime &runtime) {
  const auto &i2c_trace = runtime.i2c_trace();
  return {
      time_fixed,
      utc_time,
      date,
      count_occurrences(runtime.sd_content("results.txt"), "\r\n"),
      runtime.sd_content("results.txt"),
      led_status(runtime.led_state()),
      runtime.uart_framing_errors(),
      runtime.serial1_rx_overruns(),
      i2c_trace.size(),
      static_cast<std::size_t>(std::count_if(i2c_trace.begin(), i2c_trace.end(),
                                             [](const auto &transaction) {
                                               return transaction.status !=
                                                      host_sim::I2cStatus::Ok;
                                             })),
      static_cast<std::uint64_t>(runtime.now().count()),
  };
}

std::string parser_projection(const Outcome &outcome) {
  return "lock=" + std::to_string(outcome.locked) + ";time=" + outcome.time +
         ";date=" + outcome.day;
}

std::string workflow_projection(const Outcome &outcome) {
  return "sessions=" + std::to_string(outcome.sessions) +
         ";status=" + outcome.status + ";log=" + outcome.log;
}

std::string uart_projection(const Outcome &outcome) {
  return parser_projection(outcome) +
         ";framing=" + std::to_string(outcome.framing_errors) +
         ";overruns=" + std::to_string(outcome.overruns);
}

std::string i2c_projection(const Outcome &outcome) {
  return workflow_projection(outcome) +
         ";transactions=" + std::to_string(outcome.i2c_transactions) +
         ";failures=" + std::to_string(outcome.i2c_failures);
}

std::string make_time(std::uint32_t &random) {
  const auto hour = next_random(random) % 24;
  const auto minute = next_random(random) % 60;
  const auto second = next_random(random) % 60;
  std::array<char, 12> result{};
  std::snprintf(result.data(), result.size(), "%02u%02u%02u.%02u", hour, minute,
                second, next_random(random) % 100);
  return result.data();
}

std::string make_date(std::uint32_t &random) {
  const auto month = next_random(random) % 12 + 1;
  constexpr std::array<unsigned int, 12> days{31, 29, 31, 30, 31, 30,
                                              31, 31, 30, 31, 30, 31};
  const auto day = next_random(random) % days[month - 1] + 1;
  std::array<char, 7> result{};
  std::snprintf(result.data(), result.size(), "%02u%02u24", day, month);
  return result.data();
}

Outcome run_nmea_chunks(const std::vector<std::string> &chunks) {
  auto runtime = make_runtime();
  setup();
  inject_chunks(*runtime, chunks);
  return observe(*runtime);
}

Outcome run_sessions(std::size_t sessions, bool insert_neutral_actions,
                     const host_sim::SdCardConfig *config = nullptr,
                     host_sim::SimTime i2c_stretch = host_sim::SimTime::zero(),
                     bool i2c_stuck_low = false) {
  auto runtime = make_runtime(config, i2c_stretch, i2c_stuck_low);
  setup();
  lock_time(*runtime);
  for (std::size_t session = 0; session < sessions; ++session) {
    runtime->set_button_pressed(true);
    loop();
    if (insert_neutral_actions) {
      runtime->set_button_pressed(true);
      loop();
      loop();
    }
    runtime->set_button_pressed(false);
    loop();
    if (insert_neutral_actions) {
      runtime->set_button_pressed(false);
      loop();
      loop();
    }
  }
  return observe(*runtime);
}

void poll_uart(host_sim::Runtime &runtime, host_sim::SimTime duration,
               host_sim::SimTime period) {
  for (host_sim::SimTime elapsed{}; elapsed < duration; elapsed += period) {
    runtime.advance_by(period);
    loop();
  }
}

Outcome run_physical_uart(const std::string &message,
                          host_sim::SimTime poll_period,
                          unsigned long baud = 9600) {
  auto runtime = make_runtime();
  setup();
  runtime->transmit_gps(message, baud);
  poll_uart(*runtime, 160ms, poll_period);
  return observe(*runtime);
}

class Campaign {
public:
  Campaign(const std::filesystem::path &output, std::uint32_t seed)
      : seed_(seed) {
    if (output.has_parent_path()) {
      std::filesystem::create_directories(output.parent_path());
    }
    matrix_.open(output);
    if (!matrix_) {
      throw std::runtime_error("failed to open metamorphic matrix");
    }
    matrix_ << "seed,pair_id,category,transformation,projection,baseline,"
               "transformed,"
               "differential,classification,admission_eligible,error\n";
  }

  void check(const std::string &id, const std::string &category,
             const std::string &transformation, const std::string &projection,
             const std::string &baseline, const std::string &transformed,
             const std::string &error = "") {
    ++pairs_;
    const bool differential = baseline != transformed || !error.empty();
    failures_ += differential;
    matrix_ << seed_ << ',' << csv_field(id) << ',' << csv_field(category)
            << ',' << csv_field(transformation) << ',' << csv_field(projection)
            << ',' << csv_field(baseline) << ',' << csv_field(transformed)
            << ',' << (differential ? 1 : 0) << ','
            << (differential ? "UNEXPLAINED" : "NONE") << ",0,"
            << csv_field(error) << '\n';
  }

  template <typename Function>
  void guarded_check(const std::string &id, const std::string &category,
                     const std::string &transformation,
                     const std::string &projection, Function run) {
    try {
      const auto [baseline, transformed] = run();
      check(id, category, transformation, projection, baseline, transformed);
    } catch (const std::exception &exception) {
      check(id, category, transformation, projection, "", "", exception.what());
    }
  }

  template <typename Function>
  void guarded_control(const std::string &id, const std::string &category,
                       const std::string &transformation,
                       const std::string &projection, Function run) {
    ++controls_;
    std::string baseline;
    std::string transformed;
    std::string error;
    try {
      std::tie(baseline, transformed) = run();
      if (baseline == transformed) {
        error = "detection control produced equal projections";
      }
    } catch (const std::exception &exception) {
      error = exception.what();
    }
    const bool detected = error.empty();
    failures_ += !detected;
    matrix_ << seed_ << ',' << csv_field(id) << ',' << csv_field(category)
            << ',' << csv_field(transformation) << ',' << csv_field(projection)
            << ',' << csv_field(baseline) << ',' << csv_field(transformed)
            << ',' << (detected ? 1 : 0) << ','
            << (detected ? "CONTROL_DETECTED" : "CONTROL_MISSED") << ",0,"
            << csv_field(error) << '\n';
  }

  std::size_t pairs() const { return pairs_; }
  std::size_t controls() const { return controls_; }
  std::size_t failures() const { return failures_; }

private:
  std::uint32_t seed_;
  std::ofstream matrix_;
  std::size_t pairs_ = 0;
  std::size_t controls_ = 0;
  std::size_t failures_ = 0;
};

void add_detection_controls(Campaign &campaign) {
  const auto first = valid_rmc("123519.00", "230394");
  const auto second = valid_rmc("225446.50", "311224");
  campaign.guarded_control(
      "control-fragment", "nmea_fragmentation_control",
      "drop_sentence_terminator", "lock_time_date", [first] {
        return std::pair{parser_projection(run_nmea_chunks({first})),
                         parser_projection(run_nmea_chunks(
                             {first.substr(0, first.size() - 2)}))};
      });
  campaign.guarded_control(
      "control-neutral-parser", "parser_neutral_insertion_control",
      "insert_state_changing_valid_rmc", "lock_time_date", [first, second] {
        return std::pair{parser_projection(run_nmea_chunks({first})),
                         parser_projection(run_nmea_chunks({first, second}))};
      });
  campaign.guarded_control(
      "control-button", "button_idle_insertion_control",
      "insert_additional_press_release", "sessions_status_log", [] {
        return std::pair{workflow_projection(run_sessions(1, false)),
                         workflow_projection(run_sessions(2, false))};
      });
  campaign.guarded_control(
      "control-sd", "sd_latency_control", "replace_latency_with_capacity_fault",
      "sessions_status_log", [] {
        host_sim::SdCardConfig constrained;
        constrained.capacity_bytes = 1;
        return std::pair{
            workflow_projection(run_sessions(1, false)),
            workflow_projection(run_sessions(1, false, &constrained))};
      });
  campaign.guarded_control(
      "control-i2c", "i2c_clock_stretch_control",
      "replace_stretch_with_stuck_low_fault", "workflow_i2c_trace", [] {
        return std::pair{
            i2c_projection(run_sessions(1, false)),
            i2c_projection(run_sessions(1, false, nullptr,
                                        host_sim::SimTime::zero(), true))};
      });
  campaign.guarded_control(
      "control-uart", "uart_poll_cadence_control",
      "replace_lossless_cadence_with_baud_mismatch", "lock_time_date_errors",
      [first] {
        return std::pair{
            uart_projection(run_physical_uart(first, 1ms)),
            uart_projection(run_physical_uart(first, 1ms, 115200))};
      });
}

void add_fragmentation_pairs(Campaign &campaign, std::uint32_t &random) {
  for (std::size_t index = 0; index < kCasesPerCategory; ++index) {
    const auto time = make_time(random);
    const auto day = make_date(random);
    const auto message = valid_rmc(time, day);
    const auto first = next_random(random) % (message.size() - 2) + 1;
    const auto second =
        first + next_random(random) % (message.size() - first - 1) + 1;
    campaign.guarded_check(
        "fragment-" + std::to_string(index), "nmea_fragmentation",
        "split_at=" + std::to_string(first) + ":" + std::to_string(second) +
            ";time=" + time + ";date=" + day,
        "lock_time_date", [message, first, second] {
          const auto baseline = run_nmea_chunks({message});
          const auto transformed = run_nmea_chunks(
              {message.substr(0, first), message.substr(first, second - first),
               message.substr(second)});
          return std::pair{parser_projection(baseline),
                           parser_projection(transformed)};
        });
  }
}

void add_neutral_parser_pairs(Campaign &campaign, std::uint32_t &random) {
  const auto non_rmc =
      nmea_sentence("GNGGA,123519.00,4807.03800,N,01131.00000,E,1,08,0.9");
  for (std::size_t index = 0; index < kCasesPerCategory; ++index) {
    const auto locked_time = make_time(random);
    const auto locked_date = make_date(random);
    const auto invalid_time = make_time(random);
    const auto invalid_date = make_date(random);
    const auto initial_message = valid_rmc(locked_time, locked_date);
    const auto invalid = checksum_invalid_rmc(invalid_time, invalid_date);
    campaign.guarded_check(
        "neutral-parser-" + std::to_string(index), "parser_neutral_insertion",
        "locked_time=" + locked_time + ";locked_date=" + locked_date +
            ";invalid_time=" + invalid_time + ";invalid_date=" + invalid_date,
        "preserved_lock_time_date", [initial_message, invalid, non_rmc] {
          const auto baseline = run_nmea_chunks({initial_message});
          const auto transformed =
              run_nmea_chunks({initial_message, non_rmc, invalid, "\r\n\n\r"});
          return std::pair{parser_projection(baseline),
                           parser_projection(transformed)};
        });
  }
}

void add_button_pairs(Campaign &campaign, std::uint32_t &random) {
  for (std::size_t index = 0; index < kCasesPerCategory; ++index) {
    const auto sessions = next_random(random) % 3 + 1;
    campaign.guarded_check(
        "button-" + std::to_string(index), "button_idle_insertion",
        "duplicate_levels_and_insert_idle_loops;sessions=" +
            std::to_string(sessions),
        "sessions_status_log", [sessions] {
          const auto baseline = run_sessions(sessions, false);
          const auto transformed = run_sessions(sessions, true);
          return std::pair{workflow_projection(baseline),
                           workflow_projection(transformed)};
        });
  }
}

void add_sd_latency_pairs(Campaign &campaign, std::uint32_t &random) {
  for (std::size_t index = 0; index < kCasesPerCategory; ++index) {
    const auto sessions = next_random(random) % 2 + 1;
    host_sim::SdCardConfig config;
    config.initialization_time =
        std::chrono::microseconds(next_random(random) % 2000 + 1);
    config.open_time =
        std::chrono::microseconds(next_random(random) % 1000 + 1);
    config.write_byte_time =
        std::chrono::microseconds(next_random(random) % 200 + 1);
    config.close_time =
        std::chrono::microseconds(next_random(random) % 1000 + 1);
    campaign.guarded_check(
        "sd-latency-" + std::to_string(index), "sd_latency",
        "sessions=" + std::to_string(sessions) +
            ";init_us=" + std::to_string(config.initialization_time.count()) +
            ";open_us=" + std::to_string(config.open_time.count()) +
            ";write_byte_us=" + std::to_string(config.write_byte_time.count()) +
            ";close_us=" + std::to_string(config.close_time.count()),
        "sessions_status_log", [sessions, config] {
          const auto baseline = run_sessions(sessions, false);
          const auto transformed = run_sessions(sessions, false, &config);
          if (transformed.elapsed_us <= baseline.elapsed_us) {
            throw std::runtime_error(
                "SD latency did not increase elapsed time");
          }
          return std::pair{workflow_projection(baseline),
                           workflow_projection(transformed)};
        });
  }
}

void add_i2c_stretch_pairs(Campaign &campaign, std::uint32_t &random) {
  for (std::size_t index = 0; index < kCasesPerCategory; ++index) {
    const auto stretch =
        std::chrono::microseconds(next_random(random) % 250 + 1);
    campaign.guarded_check(
        "i2c-stretch-" + std::to_string(index), "i2c_clock_stretch",
        "stretch_us=" + std::to_string(stretch.count()), "workflow_i2c_trace",
        [stretch] {
          const auto baseline = run_sessions(1, false);
          const auto transformed = run_sessions(1, false, nullptr, stretch);
          if (baseline.i2c_transactions == 0 ||
              transformed.i2c_transactions == 0) {
            throw std::runtime_error("clock-stretch relation ran no I2C");
          }
          if (transformed.elapsed_us <= baseline.elapsed_us) {
            throw std::runtime_error(
                "clock stretching did not increase elapsed time");
          }
          return std::pair{i2c_projection(baseline),
                           i2c_projection(transformed)};
        });
  }
}

void add_uart_poll_pairs(Campaign &campaign, std::uint32_t &random) {
  for (std::size_t index = 0; index < kCasesPerCategory; ++index) {
    const auto time = make_time(random);
    const auto day = make_date(random);
    const auto message = valid_rmc(time, day);
    const auto transformed_period =
        std::chrono::milliseconds(next_random(random) % 7 + 2);
    campaign.guarded_check(
        "uart-poll-" + std::to_string(index), "uart_poll_cadence",
        "poll_ms=" + std::to_string(transformed_period.count()) +
            ";time=" + time + ";date=" + day,
        "lock_time_date_errors", [message, transformed_period] {
          const auto baseline = run_physical_uart(message, 1ms);
          const auto transformed =
              run_physical_uart(message, transformed_period);
          return std::pair{uart_projection(baseline),
                           uart_projection(transformed)};
        });
  }
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr
        << "usage: blocker_peeling_metamorphic_campaign <matrix.csv> <seed>\n";
    return EXIT_FAILURE;
  }

  try {
    const auto parsed_seed = std::stoull(argv[2]);
    if (parsed_seed > std::numeric_limits<std::uint32_t>::max()) {
      throw std::out_of_range("seed does not fit in uint32_t");
    }
    const auto seed = static_cast<std::uint32_t>(parsed_seed);
    Campaign campaign(argv[1], seed);
    std::uint32_t random = seed;
    add_detection_controls(campaign);
    add_fragmentation_pairs(campaign, random);
    add_neutral_parser_pairs(campaign, random);
    add_button_pairs(campaign, random);
    add_sd_latency_pairs(campaign, random);
    add_i2c_stretch_pairs(campaign, random);
    add_uart_poll_pairs(campaign, random);
    std::cout << "seed=" << seed << " pairs=" << campaign.pairs()
              << " controls=" << campaign.controls()
              << " failures=" << campaign.failures() << '\n';
    if (campaign.pairs() != kExpectedPairCount) {
      std::cerr << "unexpected metamorphic pair count\n";
      return EXIT_FAILURE;
    }
    if (campaign.controls() != kExpectedControlCount) {
      std::cerr << "unexpected metamorphic control count\n";
      return EXIT_FAILURE;
    }
    return campaign.failures() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::exception &exception) {
    std::cerr << exception.what() << '\n';
    return EXIT_FAILURE;
  }
}
