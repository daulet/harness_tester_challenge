#include "host_simulator/board.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace host_sim {

namespace {

std::string trim(const std::string& value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::vector<std::string> split_csv(const std::string& line) {
  std::vector<std::string> fields;
  std::stringstream stream(line);
  std::string field;
  while (std::getline(stream, field, ',')) {
    fields.push_back(trim(field));
  }
  return fields;
}

std::vector<std::vector<std::string>> read_csv(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("failed to open board data file: " + path);
  }

  std::vector<std::vector<std::string>> rows;
  std::string line;
  while (std::getline(file, line)) {
    line = trim(line);
    if (line.empty() || line.front() == '#') {
      continue;
    }
    rows.push_back(split_csv(line));
  }
  return rows;
}

bool drives_high(const ExpanderPinDrive& drive) {
  if (drive.is_input) {
    return drive.drive_mode == DriveMode::PullUp;
  }
  if (drive.drive_mode == DriveMode::Strong || drive.drive_mode == DriveMode::SlowStrong) {
    return drive.output_value;
  }
  if (drive.drive_mode == DriveMode::OpenDrainHigh) {
    return true;
  }
  return false;
}

}  // namespace

void Harness::connect(std::size_t left, std::size_t right) {
  if (left >= kHarnessPins || right >= kHarnessPins) {
    throw std::out_of_range("harness pin index");
  }
  if (left == right) {
    return;
  }
  auto& left_neighbors = neighbors_[left];
  auto& right_neighbors = neighbors_[right];
  if (std::find(left_neighbors.begin(), left_neighbors.end(), right) == left_neighbors.end()) {
    left_neighbors.push_back(right);
  }
  if (std::find(right_neighbors.begin(), right_neighbors.end(), left) == right_neighbors.end()) {
    right_neighbors.push_back(left);
  }
}

bool Harness::connected(std::size_t left, std::size_t right) const {
  if (left >= kHarnessPins || right >= kHarnessPins) {
    throw std::out_of_range("harness pin index");
  }
  const auto& left_neighbors = neighbors_[left];
  return std::find(left_neighbors.begin(), left_neighbors.end(), right) != left_neighbors.end();
}

const std::vector<std::size_t>& Harness::neighbors(std::size_t pin) const {
  if (pin >= kHarnessPins) {
    throw std::out_of_range("harness pin index");
  }
  return neighbors_[pin];
}

BoardModel BoardModel::load(const std::string& schematic_map_path,
                           const std::string& io_map_path,
                           const std::string& overlay_path) {
  BoardModel model;
  model.load_schematic_map(schematic_map_path);
  model.load_io_map(io_map_path);
  if (!overlay_path.empty()) {
    model.apply_overlay(overlay_path);
  }
  return model;
}

const HarnessChannel& BoardModel::channel(std::size_t harness_index) const {
  if (harness_index >= kHarnessPins) {
    throw std::out_of_range("harness index");
  }
  return channels_[harness_index];
}

std::size_t BoardModel::arduino_pin(const std::string& name) const {
  const auto iter = io_pins_.find(name);
  if (iter == io_pins_.end()) {
    throw std::runtime_error("unknown board IO name: " + name);
  }
  return iter->second;
}

const std::map<std::string, std::size_t>& BoardModel::io_pins() const {
  return io_pins_;
}

const std::vector<std::string>& BoardModel::applied_faults() const {
  return applied_faults_;
}

void BoardModel::load_schematic_map(const std::string& path) {
  const auto rows = read_csv(path);
  std::array<bool, kHarnessPins> seen{};
  for (const auto& row : rows) {
    if (row.size() != 4) {
      throw std::runtime_error("invalid schematic map row in " + path);
    }
    const auto index = static_cast<std::size_t>(std::stoul(row[0]));
    const auto port = static_cast<std::size_t>(std::stoul(row[2]));
    const auto bit = static_cast<std::size_t>(std::stoul(row[3]));
    if (index >= kHarnessPins || port >= 8 || bit >= 8 || seen[index]) {
      throw std::runtime_error("invalid or duplicate harness channel in " + path);
    }
    channels_[index] = {index, row[1], port, bit};
    external_to_internal_[index].push_back(index);
    seen[index] = true;
  }
  if (!std::all_of(seen.begin(), seen.end(), [](bool value) { return value; })) {
    throw std::runtime_error("schematic map does not contain all 40 harness channels");
  }
}

void BoardModel::load_io_map(const std::string& path) {
  const auto rows = read_csv(path);
  for (const auto& row : rows) {
    if (row.size() < 2) {
      throw std::runtime_error("invalid IO map row in " + path);
    }
    const auto pin = static_cast<std::size_t>(std::stoul(row[1]));
    io_pins_.emplace(row[0], pin);
  }
}

void BoardModel::apply_overlay(const std::string& path) {
  const auto rows = read_csv(path);
  for (const auto& row : rows) {
    if (row.empty()) {
      continue;
    }
    const auto& operation = row[0];
    if (operation == "clear_attachments") {
      for (auto& attachments : external_to_internal_) {
        attachments.clear();
      }
      applied_faults_.push_back("J3 header attachment replacement");
      continue;
    }
    if (operation == "attach") {
      if (row.size() < 3) {
        throw std::runtime_error("invalid attach overlay row in " + path);
      }
      const auto internal = channel_index_for_net(row[1]);
      const auto external = static_cast<std::size_t>(std::stoul(row[2]));
      if (external >= kHarnessPins) {
        throw std::runtime_error("invalid external harness pin in " + path);
      }
      auto& attachments = external_to_internal_[external];
      if (std::find(attachments.begin(), attachments.end(), internal) == attachments.end()) {
        attachments.push_back(internal);
      }
      if (row.size() >= 4 && !row[3].empty()) {
        applied_faults_.push_back(row[3]);
      }
      continue;
    }
    throw std::runtime_error("unknown overlay operation: " + operation);
  }
}

std::size_t BoardModel::channel_index_for_net(const std::string& net) const {
  for (const auto& channel : channels_) {
    if (channel.net == net) {
      return channel.harness_index;
    }
  }
  throw std::runtime_error("unknown harness net: " + net);
}

std::uint64_t BoardModel::resolve_inputs(
    const Harness& harness,
    const std::array<ExpanderPinDrive, kExpanderPins>& drives) const {
  constexpr std::size_t external_offset = kHarnessPins;
  constexpr std::size_t node_count = kHarnessPins * 2;
  std::array<std::vector<std::size_t>, node_count> edges;

  for (std::size_t external = 0; external < kHarnessPins; ++external) {
    for (const auto internal : external_to_internal_[external]) {
      edges[internal].push_back(external_offset + external);
      edges[external_offset + external].push_back(internal);
    }
    for (const auto peer : harness.neighbors(external)) {
      edges[external_offset + external].push_back(external_offset + peer);
    }
  }

  std::array<bool, node_count> visited{};
  std::array<bool, kExpanderPins> level{};
  for (std::size_t start = 0; start < kHarnessPins; ++start) {
    if (visited[start]) {
      continue;
    }

    std::vector<std::size_t> pending{start};
    std::vector<std::size_t> component;
    visited[start] = true;
    while (!pending.empty()) {
      const auto node = pending.back();
      pending.pop_back();
      component.push_back(node);
      for (const auto next : edges[node]) {
        if (!visited[next]) {
          visited[next] = true;
          pending.push_back(next);
        }
      }
    }

    bool high = false;
    for (const auto node : component) {
      if (node >= kHarnessPins) {
        continue;
      }
      const auto& channel = channels_[node];
      const auto& drive = drives[channel.expander_port * 8 + channel.expander_bit];
      high = high || drives_high(drive);
    }
    const bool resolved = high;
    for (const auto node : component) {
      if (node < kHarnessPins) {
        const auto& channel = channels_[node];
        level[channel.expander_port * 8 + channel.expander_bit] = resolved;
      }
    }
  }

  std::uint64_t result = 0;
  for (std::size_t index = 0; index < kExpanderPins; ++index) {
    if (level[index]) {
      result |= (std::uint64_t{1} << index);
    }
  }
  return result;
}

}  // namespace host_sim
