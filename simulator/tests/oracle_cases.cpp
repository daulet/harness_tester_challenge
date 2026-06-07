#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

#include "host_simulator/oracle.h"

namespace {

bool require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

} // namespace

int main() {
  bool ok = true;
  host_sim::Harness harness;
  harness.connect(0, 1);
  harness.connect(1, 2);
  harness.connect(5, 6);

  ok &= require(host_sim::HarnessOracle::observation(harness, 0) ==
                    ((1ULL << 1) | (1ULL << 2)),
                "oracle did not compute the transitive component");
  ok &= require(host_sim::HarnessOracle::observation(harness, 1) ==
                    ((1ULL << 0) | (1ULL << 2)),
                "oracle did not remove only the driven pin");
  ok &= require(host_sim::HarnessOracle::observation(harness, 4) == 0,
                "oracle reported a connection for an isolated pin");

  auto expected = host_sim::HarnessOracle::observations(harness);
  ok &= require(host_sim::HarnessOracle::all_rows_match(harness, expected),
                "oracle rejected its own complete observation matrix");
  expected[39] = 1;
  ok &= require(!host_sim::HarnessOracle::all_rows_match(harness, expected),
                "oracle accepted a matrix with one incorrect row");

  const host_sim::ReachabilityObservation observation{true, false, true, false};
  const host_sim::ReachabilityExpectation expectation{true, true, true, true};
  const auto mismatches =
      host_sim::compare_reachability(observation, expectation);
  ok &= require(mismatches == std::vector<std::string>(
                                  {"expander_accessed", "uart_routed"}),
                "reachability oracle did not identify exact mismatches");
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
