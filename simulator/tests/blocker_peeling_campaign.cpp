#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "host_simulator/oracle.h"
#include "host_simulator/runtime.h"
#include "blocker_peeling_harness_fixture.h"

extern void setup();
extern void loop();
extern char utc_time[20];
extern char date[20];
extern bool time_fixed;
extern bool test_button_latched;
extern int nmea_idx;
extern std::uint64_t EXPECTED_CONNECTIONS[];

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

#ifndef BLOCKER_VARIANT_DIR
#error BLOCKER_VARIANT_DIR must be defined by the build.
#endif

constexpr const char *kValidGnrmc =
    "$GNRMC,123519,A,0,N,0,E,0,0,230394\r\n";
constexpr std::size_t kMixedTopologyCount = 256;
constexpr std::size_t kSyntheticTopologyCount = 128;
constexpr std::uint32_t kTopologySeed = 0xC0FFEE42U;

using Observations =
    std::array<std::uint64_t, host_sim::kHarnessPins>;

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

std::string hex_mask(std::uint64_t value) {
  std::ostringstream formatted;
  formatted << "0x" << std::hex << std::setw(10) << std::setfill('0')
            << value;
  return formatted.str();
}

bool contains(const std::string &value, const std::string &needle) {
  return value.find(needle) != std::string::npos;
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

void reset_firmware_globals(const Observations &expected) {
  std::fill(std::begin(utc_time), std::end(utc_time), '\0');
  std::fill(std::begin(date), std::end(date), '\0');
  time_fixed = false;
  test_button_latched = false;
  nmea_idx = 0;
  for (std::size_t row = 0; row < host_sim::kHarnessPins; ++row) {
    EXPECTED_CONNECTIONS[row] = expected[row];
  }
}

Observations parse_probe_rows(const std::string &output) {
  Observations rows{};
  for (std::size_t row = 0; row < host_sim::kHarnessPins; ++row) {
    const auto prefix = "Pin " + std::to_string(row) + ": ";
    const auto start = output.find(prefix);
    if (start == std::string::npos) {
      throw std::runtime_error("missing probe row " + std::to_string(row));
    }
    const auto bits = output.substr(start + prefix.size(),
                                    host_sim::kHarnessPins);
    if (bits.size() != host_sim::kHarnessPins) {
      throw std::runtime_error("truncated probe row " + std::to_string(row));
    }
    for (std::size_t bit = 0; bit < bits.size(); ++bit) {
      if (bits[bit] == '1') {
        rows[row] |= std::uint64_t{1} << bit;
      } else if (bits[bit] != '0') {
        throw std::runtime_error("non-binary probe row " +
                                 std::to_string(row));
      }
    }
  }
  return rows;
}

host_sim::Harness component_harness(const std::vector<std::size_t> &pins) {
  host_sim::Harness harness;
  for (std::size_t index = 1; index < pins.size(); ++index) {
    harness.connect(pins.front(), pins[index]);
  }
  return harness;
}

struct Scenario {
  std::string id;
  std::string category;
  std::string topology_id;
  std::string pair_class;
  std::string expected_source;
  std::string source_independence;
  host_sim::Harness harness;
  Observations expected{};
};

class Campaign {
public:
  Campaign(host_sim::BoardModel model, const std::filesystem::path &output)
      : model_(std::move(model)), output_(output) {
    if (output_.has_parent_path()) {
      std::filesystem::create_directories(output_.parent_path());
    }
    matrix_.open(output_);
    if (!matrix_) {
      throw std::runtime_error("failed to open exposure matrix: " +
                               output_.string());
    }
    matrix_
        << "scenario_id,category,topology_id,pair_class,expected_source,"
           "source_independence,expected_verdict,firmware_verdict,"
           "oracle_changed_rows,firmware_row_mismatches,"
           "first_mismatched_row,mismatched_bit_mask,log_match,"
           "initial_status_match,post_idle_status_match,"
           "post_idle_log_match,error\n";
  }

  void run(const Scenario &scenario) {
    ++scenarios_;
    try {
      reset_firmware_globals(scenario.expected);
      host_sim::Runtime runtime(model_);
      runtime.set_harness(scenario.harness);
      runtime.set_button_pressed(false);
      runtime.inject_serial1_rx_bypass_capacity(kValidGnrmc);
      setup();
      loop();
      if (!time_fixed) {
        throw std::runtime_error("time lock was not acquired");
      }

      runtime.set_button_pressed(true);
      loop();
      const auto output = runtime.serial_output();
      if (count_occurrences(output, "Pin ") != host_sim::kHarnessPins) {
        throw std::runtime_error("firmware did not emit exactly 40 probe rows");
      }

      const auto oracle =
          host_sim::HarnessOracle::observations(scenario.harness);
      const auto firmware = parse_probe_rows(output);
      std::size_t oracle_changed_rows = 0;
      std::size_t firmware_row_mismatches = 0;
      std::size_t first_mismatched_row = host_sim::kHarnessPins;
      std::uint64_t mismatched_bit_mask = 0;
      for (std::size_t row = 0; row < host_sim::kHarnessPins; ++row) {
        oracle_changed_rows += oracle[row] != scenario.expected[row];
        if (firmware[row] != oracle[row]) {
          ++firmware_row_mismatches;
          if (first_mismatched_row == host_sim::kHarnessPins) {
            first_mismatched_row = row;
            mismatched_bit_mask = firmware[row] ^ oracle[row];
          }
        }
      }

      const bool expected_pass = oracle == scenario.expected;
      const bool printed_pass = contains(output, "Harness passed!");
      const bool printed_fail = contains(output, "Harness failed!");
      const std::string firmware_verdict =
          printed_pass == printed_fail ? "invalid"
                                       : (printed_pass ? "pass" : "fail");
      const bool verdict_match =
          firmware_verdict == (expected_pass ? "pass" : "fail");
      const auto log = runtime.sd_content("results.txt");
      const bool log_match =
          contains(log, expected_pass ? "Passed" : "Failed");
      const auto initial_status = runtime.led_state();
      const bool initial_status_match =
          expected_pass
              ? initial_status.green && !initial_status.red &&
                    !initial_status.blue
              : initial_status.red && !initial_status.green &&
                    !initial_status.blue;

      loop();
      const auto post_idle_status = runtime.led_state();
      const bool post_idle_status_match =
          expected_pass
              ? post_idle_status.green && !post_idle_status.red &&
                    !post_idle_status.blue
              : post_idle_status.red && !post_idle_status.green &&
                    !post_idle_status.blue;
      const bool post_idle_log_match =
          runtime.sd_content("results.txt") == log;

      const bool differential =
          firmware_row_mismatches != 0 || !verdict_match || !log_match ||
          !initial_status_match || !post_idle_status_match ||
          !post_idle_log_match;
      failures_ += differential;

      matrix_ << csv_field(scenario.id) << ','
              << csv_field(scenario.category) << ','
              << csv_field(scenario.topology_id) << ','
              << csv_field(scenario.pair_class) << ','
              << csv_field(scenario.expected_source) << ','
              << csv_field(scenario.source_independence) << ','
              << (expected_pass ? "pass" : "fail") << ','
              << firmware_verdict << ',' << oracle_changed_rows << ','
              << firmware_row_mismatches << ',';
      if (first_mismatched_row != host_sim::kHarnessPins) {
        matrix_ << first_mismatched_row << ','
                << hex_mask(mismatched_bit_mask);
      } else {
        matrix_ << "-1,0x0000000000";
      }
      matrix_ << ',' << (log_match ? 1 : 0) << ','
              << (initial_status_match ? 1 : 0) << ','
              << (post_idle_status_match ? 1 : 0) << ','
              << (post_idle_log_match ? 1 : 0) << ",\n";
    } catch (const std::exception &error) {
      ++failures_;
      matrix_ << csv_field(scenario.id) << ','
              << csv_field(scenario.category) << ','
              << csv_field(scenario.topology_id) << ','
              << csv_field(scenario.pair_class) << ','
              << csv_field(scenario.expected_source) << ','
              << csv_field(scenario.source_independence)
              << ",invalid,invalid,0,0,-1,0x0000000000,0,0,0,0,"
              << csv_field(error.what()) << '\n';
    }
  }

  std::size_t scenarios() const { return scenarios_; }
  std::size_t failures() const { return failures_; }

private:
  host_sim::BoardModel model_;
  std::filesystem::path output_;
  std::ofstream matrix_;
  std::size_t scenarios_ = 0;
  std::size_t failures_ = 0;
};

std::uint32_t next_random(std::uint32_t &state) {
  state = state * 1664525U + 1013904223U;
  return state;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: blocker_peeling_campaign <exposure-matrix.csv>\n";
    return EXIT_FAILURE;
  }

  try {
    const auto baseline_harness = blocker_peeling_test::declared_harness();
    const auto baseline_expected =
        host_sim::HarnessOracle::observations(baseline_harness);
    for (std::size_t row = 0; row < host_sim::kHarnessPins; ++row) {
      if (EXPECTED_CONNECTIONS[row] != baseline_expected[row]) {
        throw std::runtime_error(
            "compiled P9 matrix is not the declared passive closure");
      }
    }

    auto model = host_sim::BoardModel::load(
        variant_path("kicad_files/hardware_challenge.kicad_pcb"),
        root_path("kicad_files/hardware_challenge.kicad_sch"),
        host_sim::HarnessRoutingMode::SchematicIdeal);
    Campaign campaign(std::move(model), argv[1]);

    campaign.run({"declared-baseline",
                  "baseline",
                  "declared-edge-closure",
                  "not_applicable",
                  "compiled_passive_closure",
                  "source_declared_topology",
                  baseline_harness,
                  baseline_expected});

    for (std::size_t open = 0; open < host_sim::kHarnessPins; ++open) {
      std::array<bool, host_sim::kHarnessPins> opens{};
      opens[open] = true;
      campaign.run({"open-" + std::to_string(open),
                    "single_open",
                    "open:" + std::to_string(open),
                    "not_applicable",
                    "compiled_passive_closure",
                    "source_declared_topology",
                    blocker_peeling_test::declared_harness(opens),
                    baseline_expected});
    }

    for (std::size_t first = 0; first < host_sim::kHarnessPins; ++first) {
      for (std::size_t second = first + 1; second < host_sim::kHarnessPins;
           ++second) {
        std::array<bool, host_sim::kHarnessPins> opens{};
        opens[first] = true;
        opens[second] = true;
        campaign.run({"open-" + std::to_string(first) + "-" +
                          std::to_string(second),
                      "two_open",
                      "open:" + std::to_string(first) + "+" +
                          std::to_string(second),
                      "not_applicable",
                      "compiled_passive_closure",
                      "source_declared_topology",
                      blocker_peeling_test::declared_harness(opens),
                      baseline_expected});
      }
    }

    for (std::size_t first = 0; first < host_sim::kHarnessPins; ++first) {
      for (std::size_t second = first + 1; second < host_sim::kHarnessPins;
           ++second) {
        auto harness = baseline_harness;
        harness.connect(first, second);
        const bool same_component =
            (baseline_expected[first] & (std::uint64_t{1} << second)) != 0;
        campaign.run({"short-" + std::to_string(first) + "-" +
                          std::to_string(second),
                      "pair_addition",
                      "short:" + std::to_string(first) + "-" +
                          std::to_string(second),
                      same_component ? "intra_component"
                                     : "inter_component",
                      "compiled_passive_closure",
                      "source_declared_topology",
                      std::move(harness),
                      baseline_expected});
      }
    }

    std::uint32_t random_state = kTopologySeed;
    for (std::size_t index = 0; index < kMixedTopologyCount; ++index) {
      const auto open = next_random(random_state) % host_sim::kHarnessPins;
      auto first = next_random(random_state) % host_sim::kHarnessPins;
      auto second = next_random(random_state) % host_sim::kHarnessPins;
      while (first == open) {
        first = next_random(random_state) % host_sim::kHarnessPins;
      }
      while (second == open || second == first) {
        second = next_random(random_state) % host_sim::kHarnessPins;
      }
      if (first > second) {
        std::swap(first, second);
      }
      std::array<bool, host_sim::kHarnessPins> opens{};
      opens[open] = true;
      auto harness = blocker_peeling_test::declared_harness(opens);
      harness.connect(first, second);
      campaign.run({"mixed-" + std::to_string(index),
                    "mixed_open_short",
                    "seed:" + std::to_string(kTopologySeed) + ":" +
                        std::to_string(index),
                    "mixed",
                    "compiled_passive_closure",
                    "source_declared_topology",
                    std::move(harness),
                    baseline_expected});
    }

    std::size_t generated = 0;
    for (std::size_t first = 0;
         first < host_sim::kHarnessPins && generated < kSyntheticTopologyCount;
         ++first) {
      for (std::size_t second = first + 1;
           second < host_sim::kHarnessPins &&
           generated < kSyntheticTopologyCount;
           ++second) {
        for (std::size_t third = second + 1;
             third < host_sim::kHarnessPins &&
             generated < kSyntheticTopologyCount;
             ++third) {
          auto harness = component_harness({first, second, third});
          const auto expected =
              host_sim::HarnessOracle::observations(harness);
          campaign.run({"synthetic-3-" + std::to_string(generated),
                        "synthetic_three_pin",
                        std::to_string(first) + "-" +
                            std::to_string(second) + "-" +
                            std::to_string(third),
                        "not_applicable",
                        "scenario_oracle_fixture",
                        "implementation_differential_only",
                        std::move(harness),
                        expected});
          ++generated;
        }
      }
    }

    generated = 0;
    for (std::size_t first = 0;
         first < host_sim::kHarnessPins && generated < kSyntheticTopologyCount;
         ++first) {
      for (std::size_t second = first + 1;
           second < host_sim::kHarnessPins &&
           generated < kSyntheticTopologyCount;
           ++second) {
        for (std::size_t third = second + 1;
             third < host_sim::kHarnessPins &&
             generated < kSyntheticTopologyCount;
             ++third) {
          for (std::size_t fourth = third + 1;
               fourth < host_sim::kHarnessPins &&
               generated < kSyntheticTopologyCount;
               ++fourth) {
            auto harness =
                component_harness({first, second, third, fourth});
            const auto expected =
                host_sim::HarnessOracle::observations(harness);
            campaign.run({"synthetic-4-" + std::to_string(generated),
                          "synthetic_four_pin",
                          std::to_string(first) + "-" +
                              std::to_string(second) + "-" +
                              std::to_string(third) + "-" +
                              std::to_string(fourth),
                          "not_applicable",
                          "scenario_oracle_fixture",
                          "implementation_differential_only",
                          std::move(harness),
                          expected});
            ++generated;
          }
        }
      }
    }

    std::cout << "blocker-peeling campaign: " << campaign.scenarios()
              << " scenarios, " << campaign.failures()
              << " differentials, seed=" << kTopologySeed << "\n";
    return campaign.failures() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n";
    return EXIT_FAILURE;
  }
}
