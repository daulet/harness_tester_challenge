#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
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

struct BoardPad {
  std::string reference;
  std::string pad;
  std::string net;
  std::string pinfunction;
};

struct ExpanderPinDrive {
  bool is_input = true;
  bool output_value = false;
  DriveMode drive_mode = DriveMode::HighImpedance;
};

struct PhysicalNet {
  std::string name;
  std::size_t pad_count = 0;
  std::size_t track_count = 0;
  std::size_t via_count = 0;
  std::size_t zone_count = 0;
  std::size_t keepout_count = 0;
  std::set<std::string> references;
  std::set<std::string> pads;
};

struct ConnectivityMismatch {
  std::string reference;
  std::string pad;
  std::string schematic_net;
  std::string pcb_net;
};

struct PhysicalConnectivityStats {
  std::size_t pads = 0;
  std::size_t tracks = 0;
  std::size_t vias = 0;
  std::size_t zones = 0;
  std::size_t keepouts = 0;
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
  static BoardModel load(const std::string& pcb_path,
                         const std::string& schematic_path);

  const HarnessChannel& channel(std::size_t harness_index) const;
  const BoardPad& pad(const std::string& reference, const std::string& pad) const;
  std::size_t arduino_pin(const std::string& name) const;
  const std::map<std::string, std::size_t>& io_pins() const;
  const std::map<std::string, PhysicalNet>& physical_nets() const;
  const std::vector<ConnectivityMismatch>& connectivity_mismatches() const;
  const PhysicalConnectivityStats& physical_stats() const;
  const std::vector<ConnectivityMismatch>& schematic_mismatches() const;
  bool pcb_connected(const std::string& net,
                     const std::string& left_reference,
                     const std::string& left_pad,
                     const std::string& right_reference,
                     const std::string& right_pad) const;
  bool has_copper_at(const std::string& net,
                     const std::string& layer,
                     double x,
                     double y) const;

  std::uint64_t resolve_inputs(
      const Harness& harness,
      const std::array<ExpanderPinDrive, kExpanderPins>& drives) const;

private:
  std::array<HarnessChannel, kHarnessPins> channels_{};
  std::map<std::pair<std::string, std::string>, BoardPad> pads_;
  std::array<std::vector<std::size_t>, kHarnessPins> external_to_internal_{};
  std::map<std::string, std::size_t> io_pins_{};
  std::map<std::string, PhysicalNet> physical_nets_{};
  std::vector<ConnectivityMismatch> connectivity_mismatches_{};
  PhysicalConnectivityStats physical_stats_{};
  std::shared_ptr<void> physical_graph_;
};

}  // namespace host_sim
