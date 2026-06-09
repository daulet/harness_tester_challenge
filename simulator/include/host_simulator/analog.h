#pragma once

#include <map>
#include <optional>
#include <string>

namespace host_sim {

class BoardModel;
struct AnalogStimulus;

enum class LedChannel {
  Red,
  Green,
  Blue,
};

struct AnalogFixture {
  std::string name;
  std::map<std::string, double> parameters;

  static AnalogFixture load(const std::string &path);
  double parameter(const std::string &name) const;
};

struct BoardElectricalConfig {
  // Board-derived values override the corresponding authored fixture values.
  double i2c_sda_pullup_ohm = 1e12;
  double i2c_sda_pulldown_ohm = 1e12;
  double i2c_scl_pullup_ohm = 1e12;
  double i2c_scl_pulldown_ohm = 1e12;
  double led_anode_path_ohm = 1e12;
  double led_red_series_ohm = 1e12;
  double led_green_series_ohm = 1e12;
  double led_blue_series_ohm = 1e12;
  LedChannel physical_red_driven_by = LedChannel::Red;
  LedChannel physical_green_driven_by = LedChannel::Green;
  LedChannel physical_blue_driven_by = LedChannel::Blue;

  static BoardElectricalConfig i2c_from_board(const BoardModel &model);
  static BoardElectricalConfig from_board(const BoardModel &model);
  void apply_i2c_to(AnalogFixture &fixture) const;
  void apply_to(AnalogFixture &fixture) const;
  void map_led_stimulus(AnalogStimulus &stimulus) const;
};

struct BoardElectricalOverlay {
  std::optional<double> led_anode_path_ohm;
  std::optional<double> led_red_series_ohm;
  std::optional<double> led_green_series_ohm;
  std::optional<double> led_blue_series_ohm;
  bool use_identity_led_mapping = false;

  static BoardElectricalOverlay load(const std::string &path);
  void apply_to(BoardElectricalConfig &config) const;
};

struct AnalogStimulus {
  std::optional<double> input_voltage;
  std::optional<double> surge_voltage;
  std::optional<double> harness_drive_voltage;
  std::optional<double> uart_tx_voltage;
  std::optional<bool> led_red_on;
  std::optional<bool> led_green_on;
  std::optional<bool> led_blue_on;
  std::optional<bool> i2c_sda_low;
  std::optional<bool> i2c_scl_low;
};

enum class ElectricalLevel {
  Low,
  High,
  Indeterminate,
  Overvoltage,
};

struct AnalogObservation {
  std::map<std::string, double> measurements;

  double measurement(const std::string &name) const;
  ElectricalLevel level(const std::string &name, double low_max = 0.8,
                        double high_min = 2.0, double absolute_max = 3.6) const;
};

class NgSpiceSimulator {
public:
  NgSpiceSimulator(std::string simulator_root,
                   std::string executable = "ngspice");

  bool available() const;
  AnalogObservation run(const AnalogFixture &fixture,
                        const AnalogStimulus &stimulus = {}) const;

private:
  std::string simulator_root_;
  std::string executable_;
};

struct ElectricalSnapshot {
  ElectricalLevel i2c_sda = ElectricalLevel::Indeterminate;
  ElectricalLevel i2c_scl = ElectricalLevel::Indeterminate;
};

class ElectricalFeedback {
public:
  virtual ~ElectricalFeedback() = default;
  virtual ElectricalSnapshot solve(const AnalogStimulus &stimulus) const = 0;
};

class NgSpiceElectricalFeedback final : public ElectricalFeedback {
public:
  NgSpiceElectricalFeedback(const BoardModel &model,
                            NgSpiceSimulator simulator,
                            AnalogFixture fixture);

  ElectricalSnapshot solve(const AnalogStimulus &stimulus) const override;

private:
  NgSpiceSimulator simulator_;
  AnalogFixture fixture_;
};

} // namespace host_sim
