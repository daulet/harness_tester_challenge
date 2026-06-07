#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "host_simulator/board.h"

namespace host_sim {

struct ReachabilityObservation {
  bool wire_begun = false;
  bool expander_accessed = false;
  bool time_locked = false;
  bool uart_routed = false;
};

struct ReachabilityExpectation {
  bool wire_begun = false;
  bool expander_accessed = false;
  bool time_locked = false;
  bool uart_routed = false;
};

std::vector<std::string>
compare_reachability(const ReachabilityObservation &observation,
                     const ReachabilityExpectation &expectation);

class HarnessOracle {
public:
  static std::uint64_t observation(const Harness &harness,
                                   std::size_t driven_pin);
  static std::array<std::uint64_t, kHarnessPins>
  observations(const Harness &harness);
  static bool
  all_rows_match(const Harness &harness,
                 const std::array<std::uint64_t, kHarnessPins> &expected);
};

} // namespace host_sim
