#include "host_simulator/oracle.h"

#include <stdexcept>

namespace host_sim {

std::vector<std::string>
compare_reachability(const ReachabilityObservation &observation,
                     const ReachabilityExpectation &expectation) {
  std::vector<std::string> mismatches;
  if (observation.wire_begun != expectation.wire_begun) {
    mismatches.push_back("wire_begun");
  }
  if (observation.expander_accessed != expectation.expander_accessed) {
    mismatches.push_back("expander_accessed");
  }
  if (observation.time_locked != expectation.time_locked) {
    mismatches.push_back("time_locked");
  }
  if (observation.uart_routed != expectation.uart_routed) {
    mismatches.push_back("uart_routed");
  }
  return mismatches;
}

std::uint64_t HarnessOracle::observation(const Harness &harness,
                                         std::size_t driven_pin) {
  if (driven_pin >= kHarnessPins) {
    throw std::out_of_range("harness oracle driven pin");
  }

  std::array<bool, kHarnessPins> visited{};
  std::vector<std::size_t> pending{driven_pin};
  visited[driven_pin] = true;
  std::uint64_t result = 0;
  while (!pending.empty()) {
    const auto pin = pending.back();
    pending.pop_back();
    if (pin != driven_pin) {
      result |= 1ULL << pin;
    }
    for (const auto neighbor : harness.neighbors(pin)) {
      if (!visited[neighbor]) {
        visited[neighbor] = true;
        pending.push_back(neighbor);
      }
    }
  }
  return result;
}

std::array<std::uint64_t, kHarnessPins>
HarnessOracle::observations(const Harness &harness) {
  std::array<std::uint64_t, kHarnessPins> result{};
  for (std::size_t pin = 0; pin < kHarnessPins; ++pin) {
    result[pin] = observation(harness, pin);
  }
  return result;
}

bool HarnessOracle::all_rows_match(
    const Harness &harness,
    const std::array<std::uint64_t, kHarnessPins> &expected) {
  return observations(harness) == expected;
}

} // namespace host_sim
