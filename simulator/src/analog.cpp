#include "host_simulator/analog.h"
#include "host_simulator/board.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace host_sim {

namespace {

const std::vector<std::string> &required_parameters() {
  static const std::vector<std::string> names = {
      "input_voltage_v",
      "surge_voltage_v",
      "surge_start_s",
      "surge_end_s",
      "input_source_resistance_ohm",
      "reverse_path_resistance_ohm",
      "rail5_load_ohm",
      "rail33_load_ohm",
      "harness_drive_voltage_v",
      "harness_resistance_ohm",
      "harness_inductance_h",
      "harness_capacitance_f",
      "harness_leakage_ohm",
      "harness_contact_resistance_ohm",
      "harness_open_resistance_ohm",
      "harness_short_ground_ohm",
      "harness_short_rail_ohm",
      "intermittent_enabled",
      "intermittent_start_s",
      "intermittent_end_s",
      "i2c_sda_pullup_ohm",
      "i2c_sda_pulldown_ohm",
      "i2c_scl_pullup_ohm",
      "i2c_scl_pulldown_ohm",
      "led_anode_path_ohm",
      "led_red_series_ohm",
      "led_green_series_ohm",
      "led_blue_series_ohm",
      "led_red_on",
      "led_green_on",
      "led_blue_on",
      "uart_tx_voltage_v",
      "uart_peer_voltage_v",
      "uart_peer_resistance_ohm",
  };
  return names;
}

const std::vector<std::string> &required_measurements() {
  static const std::vector<std::string> names = {
      "input_nominal_v",          "input_source_peak_v", "input_clamped_peak_v",
      "protected_nominal_v",      "rail5_nominal_v",     "rail33_nominal_v",
      "harness_before_v",         "harness_during_v",    "harness_after_v",
      "harness_source_current_a", "i2c_sda_high_v",      "i2c_sda_low_v",
      "i2c_scl_high_v",           "i2c_scl_low_v",       "led_red_current_a",
      "led_green_current_a",      "led_blue_current_a",  "uart_level_v",
  };
  return names;
}

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

bool executable_exists(const std::string &executable) {
  if (executable.find('/') != std::string::npos) {
    return ::access(executable.c_str(), X_OK) == 0;
  }
  const char *path = std::getenv("PATH");
  if (!path) {
    return false;
  }
  std::stringstream entries(path);
  std::string entry;
  while (std::getline(entries, entry, ':')) {
    const auto candidate = std::filesystem::path(entry) / executable;
    if (::access(candidate.c_str(), X_OK) == 0) {
      return true;
    }
  }
  return false;
}

double bool_value(const std::optional<bool> &value, double fallback) {
  if (!value.has_value()) {
    return fallback;
  }
  return *value ? 1.0 : 0.0;
}

double number_value(const std::optional<double> &value, double fallback) {
  return value.has_value() ? *value : fallback;
}

double parse_number(const std::string &value, const std::string &context) {
  std::istringstream parser(value);
  parser.imbue(std::locale::classic());
  double parsed = 0.0;
  if (!(parser >> parsed) || !std::isfinite(parsed)) {
    throw std::runtime_error("invalid numeric value for " + context);
  }
  parser >> std::ws;
  if (parser.peek() != std::char_traits<char>::eof()) {
    throw std::runtime_error("invalid numeric value for " + context);
  }
  return parsed;
}

double parse_resistance(const std::string &value,
                        const std::string &reference) {
  const auto normalized = trim(value);
  const auto marker = normalized.find_first_of("RrKkMm");
  if (marker == std::string::npos) {
    const auto resistance = parse_number(normalized, reference);
    if (resistance <= 0.0) {
      throw std::runtime_error("resistor value must be positive: " + reference);
    }
    return resistance;
  }

  double scale = 1.0;
  switch (normalized[marker]) {
  case 'K':
  case 'k':
    scale = 1e3;
    break;
  case 'M':
    scale = 1e6;
    break;
  case 'm':
    scale = 1e-3;
    break;
  default:
    break;
  }

  auto digits = normalized.substr(0, marker);
  const auto remainder = normalized.substr(marker + 1);
  if (!remainder.empty()) {
    digits += '.';
    digits += remainder;
  }
  const auto resistance = parse_number(digits, reference) * scale;
  if (!std::isfinite(resistance) || resistance <= 0.0) {
    throw std::runtime_error("resistor value must be positive: " + reference);
  }
  return resistance;
}

void add_parallel(std::optional<double> &equivalent, double resistance) {
  if (!equivalent.has_value()) {
    equivalent = resistance;
    return;
  }
  equivalent = 1.0 / (1.0 / *equivalent + 1.0 / resistance);
}

bool is_i2c_line(const std::string &net) {
  return net == "CY_SDA" || net == "CY_SCL";
}

bool is_pull_rail(const std::string &net) {
  return net == "+3.3V" || net == "GND";
}

LedChannel led_channel_for_net(const std::string &net) {
  if (net == "LED_R") {
    return LedChannel::Red;
  }
  if (net == "LED_G") {
    return LedChannel::Green;
  }
  if (net == "LED_B") {
    return LedChannel::Blue;
  }
  throw std::runtime_error("unsupported LED cathode net: " + net);
}

std::optional<bool> led_value(const AnalogStimulus &stimulus,
                              LedChannel channel) {
  switch (channel) {
  case LedChannel::Red:
    return stimulus.led_red_on;
  case LedChannel::Green:
    return stimulus.led_green_on;
  case LedChannel::Blue:
    return stimulus.led_blue_on;
  }
  throw std::logic_error("unknown LED channel");
}

double led_series_resistance(const BoardModel &model,
                             const std::string &net) {
  const auto physical = model.physical_nets().find(net);
  if (physical == model.physical_nets().end()) {
    return 1e12;
  }
  for (const auto &reference : physical->second.references) {
    const auto component = model.components().find(reference);
    if (component == model.components().end() ||
        component->second.schematic_lib_id.rfind("Device:R", 0) != 0) {
      continue;
    }
    const auto &value = component->second.pcb_value.empty()
        ? component->second.schematic_value
        : component->second.pcb_value;
    return parse_resistance(value, reference);
  }
  // No current-limiting component exists between the MCU and LED cathode.
  return 1.0;
}

bool has_suffix(const std::string &value, const std::string &suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

bool is_boolean_parameter(const std::string &name) {
  return name == "intermittent_enabled" || name == "led_red_on" ||
         name == "led_green_on" || name == "led_blue_on";
}

void write_parameter(std::ofstream &netlist, const std::string &spice_name,
                     double value) {
  if (!std::isfinite(value)) {
    throw std::runtime_error("invalid analog parameter " + spice_name);
  }
  netlist << ".param " << spice_name << '=' << std::setprecision(17) << value
          << "\n";
}

class TemporaryDirectory {
public:
  TemporaryDirectory() {
    auto pattern =
        (std::filesystem::temp_directory_path() / "harness-ngspice-XXXXXX")
            .string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    const auto result = ::mkdtemp(buffer.data());
    if (!result) {
      throw std::runtime_error("failed to create ngspice temporary directory");
    }
    path_ = result;
  }

  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

std::string read_text(const std::filesystem::path &path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("failed to read ngspice output: " + path.string());
  }
  std::ostringstream content;
  content << input.rdbuf();
  return content.str();
}

} // namespace

AnalogFixture AnalogFixture::load(const std::string &path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("failed to open analog fixture: " + path);
  }

  AnalogFixture fixture;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    const auto comment = line.find('#');
    if (comment != std::string::npos) {
      line.erase(comment);
    }
    line = trim(std::move(line));
    if (line.empty()) {
      continue;
    }
    const auto separator = line.find('=');
    if (separator == std::string::npos) {
      throw std::runtime_error("invalid analog fixture line " +
                               std::to_string(line_number));
    }
    const auto key = trim(line.substr(0, separator));
    const auto value = trim(line.substr(separator + 1));
    if (key == "name") {
      fixture.name = value;
      continue;
    }
    if (fixture.parameters.count(key) != 0) {
      throw std::runtime_error("duplicate analog fixture parameter: " + key);
    }
    const auto parsed = parse_number(value, key);
    fixture.parameters.emplace(key, parsed);
  }

  if (fixture.name.empty()) {
    throw std::runtime_error("analog fixture has no name: " + path);
  }
  const std::set<std::string> known(required_parameters().begin(),
                                    required_parameters().end());
  for (const auto &[key, unused] : fixture.parameters) {
    (void)unused;
    if (known.count(key) == 0) {
      throw std::runtime_error("unknown analog fixture parameter: " + key);
    }
  }
  for (const auto &name : required_parameters()) {
    if (fixture.parameters.count(name) == 0) {
      throw std::runtime_error("missing analog fixture parameter: " + name);
    }
  }
  for (const auto &[name, value] : fixture.parameters) {
    const auto is_passive_value = name.find("_ohm") != std::string::npos ||
                                  has_suffix(name, "_h") ||
                                  has_suffix(name, "_f");
    if (is_passive_value && value <= 0.0) {
      throw std::runtime_error("analog passive parameter must be positive: " +
                               name);
    }
    if (is_boolean_parameter(name) && value != 0.0 && value != 1.0) {
      throw std::runtime_error("analog boolean parameter must be 0 or 1: " +
                               name);
    }
  }
  if (fixture.parameter("surge_end_s") <= fixture.parameter("surge_start_s")) {
    throw std::runtime_error("surge_end_s must be after surge_start_s");
  }
  if (fixture.parameter("intermittent_end_s") <=
      fixture.parameter("intermittent_start_s")) {
    throw std::runtime_error(
        "intermittent_end_s must be after intermittent_start_s");
  }
  return fixture;
}

double AnalogFixture::parameter(const std::string &name) const {
  const auto value = parameters.find(name);
  if (value == parameters.end()) {
    throw std::out_of_range("unknown analog fixture parameter: " + name);
  }
  return value->second;
}

BoardElectricalOverlay
BoardElectricalOverlay::load(const std::string &path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("failed to open board electrical overlay: " + path);
  }

  BoardElectricalOverlay overlay;
  std::set<std::string> seen;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    const auto comment = line.find('#');
    if (comment != std::string::npos) {
      line.erase(comment);
    }
    line = trim(std::move(line));
    if (line.empty()) {
      continue;
    }
    const auto separator = line.find('=');
    if (separator == std::string::npos) {
      throw std::runtime_error("invalid board electrical overlay line " +
                               std::to_string(line_number));
    }
    const auto key = trim(line.substr(0, separator));
    const auto value = parse_number(trim(line.substr(separator + 1)), key);
    if (!seen.insert(key).second) {
      throw std::runtime_error("duplicate board electrical overlay key: " + key);
    }
    if (key == "use_identity_led_mapping") {
      if (value != 0.0 && value != 1.0) {
        throw std::runtime_error(
            "use_identity_led_mapping must be zero or one");
      }
      overlay.use_identity_led_mapping = value == 1.0;
      continue;
    }
    if (value < 0.0) {
      throw std::runtime_error("board electrical override cannot be negative: " +
                               key);
    }
    const auto override = value == 0.0 ? std::optional<double>{}
                                       : std::optional<double>{value};
    if (key == "led_anode_path_ohm") {
      overlay.led_anode_path_ohm = override;
    } else if (key == "led_red_series_ohm") {
      overlay.led_red_series_ohm = override;
    } else if (key == "led_green_series_ohm") {
      overlay.led_green_series_ohm = override;
    } else if (key == "led_blue_series_ohm") {
      overlay.led_blue_series_ohm = override;
    } else {
      throw std::runtime_error("unknown board electrical overlay key: " + key);
    }
  }

  const std::set<std::string> required = {
      "led_anode_path_ohm", "led_red_series_ohm",
      "led_green_series_ohm", "led_blue_series_ohm",
      "use_identity_led_mapping"};
  if (seen != required) {
    throw std::runtime_error(
        "board electrical overlay does not define every required key");
  }
  return overlay;
}

BoardElectricalConfig BoardElectricalConfig::i2c_from_board(
    const BoardModel &model) {
  std::optional<double> sda_pullup;
  std::optional<double> sda_pulldown;
  std::optional<double> scl_pullup;
  std::optional<double> scl_pulldown;
  for (const auto &[reference, component] : model.components()) {
    if (component.schematic_lib_id.rfind("Device:R", 0) != 0) {
      continue;
    }
    const auto pads = model.pads_for_component(reference);
    if (pads.size() != 2) {
      continue;
    }
    const auto &first = pads[0];
    const auto &second = pads[1];
    const auto line_pad =
        is_i2c_line(first.net) && is_pull_rail(second.net) ? &first
        : is_i2c_line(second.net) && is_pull_rail(first.net) ? &second
                                                            : nullptr;
    const auto rail_pad =
        line_pad == &first ? &second : line_pad == &second ? &first : nullptr;
    const auto line = line_pad == nullptr ? std::string{} : line_pad->net;
    const auto rail = rail_pad == nullptr ? std::string{} : rail_pad->net;
    if (line.empty()) {
      continue;
    }

    const auto controller_pad = line == "CY_SDA" ? "17" : "16";
    const auto controller_rail_pad = rail == "+3.3V" ? "15" : "1";
    if (!model.pcb_connected(line, reference, line_pad->pad, "U2",
                             controller_pad) ||
        !model.pcb_connected(rail, reference, rail_pad->pad, "U2",
                             controller_rail_pad)) {
      continue;
    }

    const auto &value = component.pcb_value.empty()
        ? component.schematic_value
        : component.pcb_value;
    const auto resistance = parse_resistance(value, reference);
    auto *target = line == "CY_SDA"
        ? (rail == "+3.3V" ? &sda_pullup : &sda_pulldown)
        : (rail == "+3.3V" ? &scl_pullup : &scl_pulldown);
    add_parallel(*target, resistance);
  }

  BoardElectricalConfig config;
  config.i2c_sda_pullup_ohm = sda_pullup.value_or(config.i2c_sda_pullup_ohm);
  config.i2c_sda_pulldown_ohm =
      sda_pulldown.value_or(config.i2c_sda_pulldown_ohm);
  config.i2c_scl_pullup_ohm = scl_pullup.value_or(config.i2c_scl_pullup_ohm);
  config.i2c_scl_pulldown_ohm =
      scl_pulldown.value_or(config.i2c_scl_pulldown_ohm);
  return config;
}

BoardElectricalConfig BoardElectricalConfig::from_board(
    const BoardModel &model) {
  auto config = i2c_from_board(model);
  const auto &led = model.component("D3");
  const auto &d3_value =
      led.pcb_value.empty() ? led.schematic_value : led.pcb_value;
  if (d3_value != "ASMB-KTF0-0A306") {
    throw std::runtime_error("unsupported D3 pin map: " + d3_value);
  }
  if (model.pad("D3", "1").net != "+3.3V" ||
      model.pad("U4", "32").net != "+3.3V") {
    throw std::runtime_error("unsupported D3 anode rail endpoints");
  }
  config.led_anode_path_ohm =
      model.pcb_connected("+3.3V", "D3", "1", "U4", "32") ? 1.0 : 1e12;
  config.led_red_series_ohm =
      led_series_resistance(model, model.pad("D3", "2").net);
  config.led_green_series_ohm =
      led_series_resistance(model, model.pad("D3", "3").net);
  config.led_blue_series_ohm =
      led_series_resistance(model, model.pad("D3", "4").net);

  // Broadcom ASMB-KTF0-0A306-DS100 defines pin 2 as red cathode,
  // pin 3 as green cathode, and pin 4 as blue cathode.
  config.physical_red_driven_by =
      led_channel_for_net(model.pad("D3", "2").net);
  config.physical_green_driven_by =
      led_channel_for_net(model.pad("D3", "3").net);
  config.physical_blue_driven_by =
      led_channel_for_net(model.pad("D3", "4").net);
  return config;
}

void BoardElectricalConfig::apply_i2c_to(AnalogFixture &fixture) const {
  const auto validate = [](const char *name, double value) {
    if (!std::isfinite(value) || value <= 0.0) {
      throw std::runtime_error(std::string("invalid board electrical value: ") +
                               name);
    }
  };
  validate("i2c_sda_pullup_ohm", i2c_sda_pullup_ohm);
  validate("i2c_sda_pulldown_ohm", i2c_sda_pulldown_ohm);
  validate("i2c_scl_pullup_ohm", i2c_scl_pullup_ohm);
  validate("i2c_scl_pulldown_ohm", i2c_scl_pulldown_ohm);
  fixture.parameters["i2c_sda_pullup_ohm"] = i2c_sda_pullup_ohm;
  fixture.parameters["i2c_sda_pulldown_ohm"] = i2c_sda_pulldown_ohm;
  fixture.parameters["i2c_scl_pullup_ohm"] = i2c_scl_pullup_ohm;
  fixture.parameters["i2c_scl_pulldown_ohm"] = i2c_scl_pulldown_ohm;
}

void BoardElectricalConfig::apply_to(AnalogFixture &fixture) const {
  apply_i2c_to(fixture);
  const auto validate = [](const char *name, double value) {
    if (!std::isfinite(value) || value <= 0.0) {
      throw std::runtime_error(std::string("invalid board electrical value: ") +
                               name);
    }
  };
  validate("led_anode_path_ohm", led_anode_path_ohm);
  validate("led_red_series_ohm", led_red_series_ohm);
  validate("led_green_series_ohm", led_green_series_ohm);
  validate("led_blue_series_ohm", led_blue_series_ohm);
  fixture.parameters["led_anode_path_ohm"] = led_anode_path_ohm;
  fixture.parameters["led_red_series_ohm"] = led_red_series_ohm;
  fixture.parameters["led_green_series_ohm"] = led_green_series_ohm;
  fixture.parameters["led_blue_series_ohm"] = led_blue_series_ohm;
}

void BoardElectricalConfig::map_led_stimulus(
    AnalogStimulus &stimulus) const {
  const auto logical = stimulus;
  stimulus.led_red_on = led_value(logical, physical_red_driven_by);
  stimulus.led_green_on = led_value(logical, physical_green_driven_by);
  stimulus.led_blue_on = led_value(logical, physical_blue_driven_by);
}

void BoardElectricalOverlay::apply_to(
    BoardElectricalConfig &config) const {
  if (led_anode_path_ohm) {
    config.led_anode_path_ohm = *led_anode_path_ohm;
  }
  if (led_red_series_ohm) {
    config.led_red_series_ohm = *led_red_series_ohm;
  }
  if (led_green_series_ohm) {
    config.led_green_series_ohm = *led_green_series_ohm;
  }
  if (led_blue_series_ohm) {
    config.led_blue_series_ohm = *led_blue_series_ohm;
  }
  if (use_identity_led_mapping) {
    config.physical_red_driven_by = LedChannel::Red;
    config.physical_green_driven_by = LedChannel::Green;
    config.physical_blue_driven_by = LedChannel::Blue;
  }
}

double AnalogObservation::measurement(const std::string &name) const {
  const auto value = measurements.find(name);
  if (value == measurements.end()) {
    throw std::out_of_range("unknown analog measurement: " + name);
  }
  return value->second;
}

ElectricalLevel AnalogObservation::level(const std::string &name,
                                         double low_max, double high_min,
                                         double absolute_max) const {
  const auto voltage = measurement(name);
  if (voltage > absolute_max) {
    return ElectricalLevel::Overvoltage;
  }
  if (voltage <= low_max) {
    return ElectricalLevel::Low;
  }
  if (voltage >= high_min) {
    return ElectricalLevel::High;
  }
  return ElectricalLevel::Indeterminate;
}

NgSpiceSimulator::NgSpiceSimulator(std::string simulator_root,
                                   std::string executable)
    : simulator_root_(std::move(simulator_root)),
      executable_(std::move(executable)) {}

bool NgSpiceSimulator::available() const {
  return executable_exists(executable_);
}

AnalogObservation NgSpiceSimulator::run(const AnalogFixture &fixture,
                                        const AnalogStimulus &stimulus) const {
  if (!available()) {
    throw std::runtime_error("ngspice executable is not available: " +
                             executable_);
  }

  TemporaryDirectory temporary;
  const auto netlist_path = temporary.path() / "fixture.cir";
  const auto log_path = temporary.path() / "ngspice.log";
  const auto common_netlist = std::filesystem::absolute(
      std::filesystem::path(simulator_root_) / "ngspice/board_harness.cir");
  const auto model_library =
      std::filesystem::absolute(std::filesystem::path(simulator_root_) /
                                "ngspice/models/first_pass_models.lib");

  std::ofstream netlist(netlist_path);
  if (!netlist) {
    throw std::runtime_error("failed to create generated ngspice netlist");
  }
  netlist.imbue(std::locale::classic());
  netlist << "Deterministic harness fixture: " << fixture.name << "\n";

  write_parameter(netlist, "VIN_BASE",
                  number_value(stimulus.input_voltage,
                               fixture.parameter("input_voltage_v")));
  write_parameter(netlist, "SURGE_DELTA",
                  number_value(stimulus.surge_voltage,
                               fixture.parameter("surge_voltage_v")));
  write_parameter(netlist, "SURGE_START", fixture.parameter("surge_start_s"));
  write_parameter(netlist, "SURGE_END", fixture.parameter("surge_end_s"));
  write_parameter(netlist, "INPUT_SOURCE_R",
                  fixture.parameter("input_source_resistance_ohm"));
  write_parameter(netlist, "REVERSE_PATH_R",
                  fixture.parameter("reverse_path_resistance_ohm"));
  write_parameter(netlist, "RAIL5_LOAD_R", fixture.parameter("rail5_load_ohm"));
  write_parameter(netlist, "RAIL33_LOAD_R",
                  fixture.parameter("rail33_load_ohm"));
  write_parameter(netlist, "HARNESS_DRIVE_V",
                  number_value(stimulus.harness_drive_voltage,
                               fixture.parameter("harness_drive_voltage_v")));
  write_parameter(netlist, "HARNESS_R",
                  fixture.parameter("harness_resistance_ohm"));
  write_parameter(netlist, "HARNESS_L",
                  fixture.parameter("harness_inductance_h"));
  write_parameter(netlist, "HARNESS_C",
                  fixture.parameter("harness_capacitance_f"));
  write_parameter(netlist, "HARNESS_LEAK_R",
                  fixture.parameter("harness_leakage_ohm"));
  write_parameter(netlist, "CONTACT_R",
                  fixture.parameter("harness_contact_resistance_ohm"));
  write_parameter(netlist, "OPEN_R",
                  fixture.parameter("harness_open_resistance_ohm"));
  write_parameter(netlist, "SHORT_GND_R",
                  fixture.parameter("harness_short_ground_ohm"));
  write_parameter(netlist, "SHORT_RAIL_R",
                  fixture.parameter("harness_short_rail_ohm"));
  write_parameter(netlist, "INTERMITTENT_ENABLED",
                  fixture.parameter("intermittent_enabled"));
  write_parameter(netlist, "INTERMITTENT_START",
                  fixture.parameter("intermittent_start_s"));
  write_parameter(netlist, "INTERMITTENT_END",
                  fixture.parameter("intermittent_end_s"));
  write_parameter(netlist, "SDA_PULLUP_R",
                  fixture.parameter("i2c_sda_pullup_ohm"));
  write_parameter(netlist, "SDA_PULLDOWN_R",
                  fixture.parameter("i2c_sda_pulldown_ohm"));
  write_parameter(netlist, "SCL_PULLUP_R",
                  fixture.parameter("i2c_scl_pullup_ohm"));
  write_parameter(netlist, "SCL_PULLDOWN_R",
                  fixture.parameter("i2c_scl_pulldown_ohm"));
  write_parameter(netlist, "SDA_FORCE_LOW",
                  bool_value(stimulus.i2c_sda_low, 0.0));
  write_parameter(netlist, "SCL_FORCE_LOW",
                  bool_value(stimulus.i2c_scl_low, 0.0));
  write_parameter(netlist, "LED_ANODE_PATH_R",
                  fixture.parameter("led_anode_path_ohm"));
  write_parameter(netlist, "LED_RED_R",
                  fixture.parameter("led_red_series_ohm"));
  write_parameter(netlist, "LED_GREEN_R",
                  fixture.parameter("led_green_series_ohm"));
  write_parameter(netlist, "LED_BLUE_R",
                  fixture.parameter("led_blue_series_ohm"));
  write_parameter(
      netlist, "LED_RED_ON",
      bool_value(stimulus.led_red_on, fixture.parameter("led_red_on")));
  write_parameter(
      netlist, "LED_GREEN_ON",
      bool_value(stimulus.led_green_on, fixture.parameter("led_green_on")));
  write_parameter(
      netlist, "LED_BLUE_ON",
      bool_value(stimulus.led_blue_on, fixture.parameter("led_blue_on")));
  write_parameter(netlist, "UART_TX_V",
                  number_value(stimulus.uart_tx_voltage,
                               fixture.parameter("uart_tx_voltage_v")));
  write_parameter(netlist, "UART_PEER_V",
                  fixture.parameter("uart_peer_voltage_v"));
  write_parameter(netlist, "UART_PEER_R",
                  fixture.parameter("uart_peer_resistance_ohm"));
  netlist << ".include \"" << model_library.string() << "\"\n";
  netlist << ".include \"" << common_netlist.string() << "\"\n";
  netlist.close();

  const auto child = ::fork();
  if (child < 0) {
    throw std::runtime_error("failed to fork ngspice process");
  }
  if (child == 0) {
    ::setenv("LC_ALL", "C", 1);
    ::setenv("LANG", "C", 1);
    ::execlp(executable_.c_str(), executable_.c_str(), "-b", "-o",
             log_path.c_str(), netlist_path.c_str(),
             static_cast<char *>(nullptr));
    _exit(errno == ENOENT ? 127 : 126);
  }

  int status = 0;
  while (::waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) {
      throw std::runtime_error("failed waiting for ngspice process");
    }
  }
  const auto log = read_text(log_path);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    throw std::runtime_error("ngspice failed for fixture " + fixture.name +
                             ":\n" + log);
  }

  AnalogObservation observation;
  const std::regex measurement_pattern(
      R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([-+0-9.eE]+))");
  std::stringstream lines(log);
  std::string line;
  std::smatch match;
  while (std::getline(lines, line)) {
    if (std::regex_search(line, match, measurement_pattern)) {
      observation.measurements[match[1].str()] =
          parse_number(match[2].str(), match[1].str());
    }
  }
  for (const auto &name : required_measurements()) {
    if (observation.measurements.count(name) == 0) {
      throw std::runtime_error("ngspice did not report " + name +
                               " for fixture " + fixture.name + ":\n" + log);
    }
  }
  return observation;
}

NgSpiceElectricalFeedback::NgSpiceElectricalFeedback(
    const BoardModel &model, NgSpiceSimulator simulator, AnalogFixture fixture)
    : simulator_(std::move(simulator)), fixture_(std::move(fixture)) {
  if (!simulator_.available()) {
    throw std::invalid_argument(
        "cannot configure electrical feedback without ngspice");
  }
  BoardElectricalConfig::i2c_from_board(model).apply_i2c_to(fixture_);
}

ElectricalSnapshot
NgSpiceElectricalFeedback::solve(const AnalogStimulus &stimulus) const {
  const auto observation = simulator_.run(fixture_, stimulus);
  return {observation.level("i2c_sda_high_v"),
          observation.level("i2c_scl_high_v")};
}

} // namespace host_sim
