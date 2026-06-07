#pragma once

#include <array>
#include <cstddef>
#include <utility>

#include "host_simulator/board.h"

namespace blocker_peeling_test {

inline constexpr std::array<std::pair<std::size_t, std::size_t>, 38>
    kDeclaredHarnessEdges{{
        {0, 13},  {0, 39},  {1, 33},  {1, 38},  {2, 37},  {3, 7},
        {3, 27},  {3, 35},  {3, 36},  {4, 12},  {4, 29},  {4, 35},
        {5, 32},  {5, 34},  {6, 23},  {6, 33},  {7, 9},   {7, 32},
        {8, 31},  {9, 30},  {10, 29}, {10, 37}, {11, 19}, {11, 28},
        {12, 27}, {13, 26}, {14, 15}, {14, 25}, {15, 24}, {15, 27},
        {16, 23}, {17, 22}, {17, 34}, {18, 21}, {19, 20}, {25, 27},
        {27, 29}, {34, 35},
    }};

inline host_sim::Harness declared_harness(
    std::array<bool, host_sim::kHarnessPins> open_channels = {}) {
  host_sim::Harness harness;
  for (const auto &[left, right] : kDeclaredHarnessEdges) {
    if (!open_channels[left] && !open_channels[right]) {
      harness.connect(left, right);
    }
  }
  return harness;
}

inline std::array<std::uint64_t, host_sim::kHarnessPins>
declared_sparse_rows() {
  std::array<std::uint64_t, host_sim::kHarnessPins> rows{};
  for (const auto &[left, right] : kDeclaredHarnessEdges) {
    rows[left] |= std::uint64_t{1} << right;
    rows[right] |= std::uint64_t{1} << left;
  }
  return rows;
}

} // namespace blocker_peeling_test
