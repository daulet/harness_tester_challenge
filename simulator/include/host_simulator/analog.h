#pragma once

#include <map>
#include <optional>
#include <string>

namespace host_sim {

struct AnalogFixture {
  std::string name;
  std::map<std::string, double> parameters;

  static AnalogFixture load(const std::string &path);
  double parameter(const std::string &name) const;
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

} // namespace host_sim
