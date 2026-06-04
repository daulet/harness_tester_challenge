#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace host_sim {

constexpr std::size_t kHarnessPins = 40;
constexpr std::size_t kExpanderPins = 64;

enum class DriveMode : std::uint8_t {
  PullUp = 0x00,
  PullDown = 0x01,
  OpenDrainHigh = 0x02,
  OpenDrainLow = 0x03,
  Strong = 0x04,
  SlowStrong = 0x05,
  HighImpedance = 0x06,
};

struct HarnessChannel {
  std::size_t harness_index = 0;
  std::string net;
  std::size_t expander_port = 0;
  std::size_t expander_bit = 0;
};

struct ExpanderPinDrive {
  bool is_input = true;
  bool output_value = false;
  DriveMode drive_mode = DriveMode::HighImpedance;
};

class Harness {
public:
  void connect(std::size_t left, std::size_t right);
  bool connected(std::size_t left, std::size_t right) const;
  const std::vector<std::size_t>& neighbors(std::size_t pin) const;

private:
  std::array<std::vector<std::size_t>, kHarnessPins> neighbors_{};
};

class BoardModel {
public:
  static BoardModel load(const std::string& schematic_map_path,
                         const std::string& io_map_path,
                         const std::string& overlay_path = {});

  const HarnessChannel& channel(std::size_t harness_index) const;
  std::size_t arduino_pin(const std::string& name) const;
  const std::map<std::string, std::size_t>& io_pins() const;
  const std::vector<std::string>& applied_faults() const;

  std::uint64_t resolve_inputs(
      const Harness& harness,
      const std::array<ExpanderPinDrive, kExpanderPins>& drives) const;

private:
  void load_schematic_map(const std::string& path);
  void load_io_map(const std::string& path);
  void apply_overlay(const std::string& path);
  std::size_t channel_index_for_net(const std::string& net) const;

  std::array<HarnessChannel, kHarnessPins> channels_{};
  std::array<std::vector<std::size_t>, kHarnessPins> external_to_internal_{};
  std::map<std::string, std::size_t> io_pins_{};
  std::vector<std::string> applied_faults_{};
};

}  // namespace host_sim
