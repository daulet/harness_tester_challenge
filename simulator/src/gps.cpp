#include "host_simulator/gps.h"

#include <array>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace host_sim {

namespace {

constexpr unsigned long kGpsBaud = 9600;

std::string nmea_sentence(const std::string &body, GpsChecksumMode mode) {
  std::uint8_t checksum = 0;
  for (const auto value : body) {
    checksum ^= static_cast<std::uint8_t>(value);
  }
  if (mode == GpsChecksumMode::Corrupt) {
    checksum ^= 0xFF;
  }

  std::ostringstream sentence;
  sentence << '$' << body << '*' << std::uppercase << std::hex
           << std::setfill('0') << std::setw(2)
           << static_cast<unsigned int>(checksum) << "\r\n";
  return sentence.str();
}

std::string utc_time(std::size_t epoch) {
  constexpr std::size_t kSecondsPerDay = 24 * 60 * 60;
  constexpr std::size_t kInitialSeconds = 12 * 60 * 60 + 35 * 60 + 19;
  const auto seconds =
      (kInitialSeconds + (epoch - 1) % kSecondsPerDay) % kSecondsPerDay;
  const auto hours = seconds / 3600;
  const auto minutes = (seconds / 60) % 60;
  const auto remainder = seconds % 60;
  std::array<char, 10> value{};
  std::snprintf(value.data(), value.size(), "%02zu%02zu%02zu.00", hours,
                minutes, remainder);
  return value.data();
}

std::string main_talker(GpsConstellationMode mode) {
  return mode == GpsConstellationMode::Concurrent ? "GN" : "GP";
}

std::vector<std::string> navigation_bodies(const GpsReceiverConfig &config,
                                           std::size_t epoch, bool valid) {
  const auto talker = main_talker(config.constellation);
  const auto time = utc_time(epoch);
  std::vector<std::string> bodies;
  if (valid) {
    bodies.push_back(talker + "GGA," + time +
                     ",4807.03800,N,01131.00000,E,1,08,0.9,545.4,M,46.9,M,,");
    bodies.push_back(talker + "GLL,4807.03800,N,01131.00000,E," + time +
                     ",A,A");
    bodies.push_back(talker +
                     "GSA,A,3,04,05,09,12,24,25,29,31,,,,,1.8,1.0,1.5,1");
  } else {
    bodies.push_back(talker + "GGA," + time + ",,,,,0,00,99.99,,,,,,");
    bodies.push_back(talker + "GLL,,,,," + time + ",V,N");
    bodies.push_back(talker + "GSA,A,1,,,,,,,,,,,,,99.99,99.99,99.99,1");
  }

  bodies.push_back(
      "GPGSV,1,1,04,04,77,045,42,05,55,120,40,09,35,220,38,12,20,300,36,1");
  if (config.constellation == GpsConstellationMode::Concurrent) {
    bodies.push_back(
        "GLGSV,1,1,04,65,70,030,41,66,50,110,39,72,30,210,37,78,15,290,35,1");
  }

  if (valid) {
    bodies.push_back(talker + "RMC," + time +
                     ",A,4807.03800,N,01131.00000,E,0.004,77.52,230394,,,A,V");
    bodies.push_back(talker + "VTG,77.52,T,,M,0.004,N,0.007,K,A");
  } else {
    bodies.push_back(talker + "RMC," + time + ",V,,,,,,,230394,,,N,V");
    bodies.push_back(talker + "VTG,,,,,,,,,N");
  }
  return bodies;
}

} // namespace

struct GpsReceiver::State : std::enable_shared_from_this<State> {
  explicit State(Runtime &runtime_value) : runtime(&runtime_value) {}

  void emit_startup() {
    const std::string body = "GPTXT,01,01,02,u-blox ag - www.u-blox.com";
    const auto sentence = nmea_sentence(body, config.checksum);
    output += sentence;
    runtime->transmit_gps(sentence, kGpsBaud);
  }

  void schedule_epoch(std::uint64_t expected_generation) {
    std::weak_ptr<State> weak = shared_from_this();
    runtime->schedule_after(config.epoch_period, [weak, expected_generation] {
      const auto state = weak.lock();
      if (!state || !state->running ||
          state->generation != expected_generation) {
        return;
      }
      state->emit_epoch(expected_generation);
    });
  }

  void emit_epoch(std::uint64_t expected_generation) {
    if (epochs_emitted == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("GPS epoch counter exhausted");
    }
    ++epochs_emitted;
    const bool valid = runtime->now() - started_at >= config.acquisition_time;
    std::string bytes;
    for (const auto &body : navigation_bodies(config, epochs_emitted, valid)) {
      bytes += nmea_sentence(body, config.checksum);
    }
    output += bytes;
    runtime->transmit_gps(bytes, kGpsBaud);
    schedule_epoch(expected_generation);
  }

  Runtime *runtime;
  GpsReceiverConfig config;
  SimTime started_at{};
  std::uint64_t generation = 0;
  std::size_t epochs_emitted = 0;
  bool running = false;
  std::string output;
};

GpsReceiver::GpsReceiver(Runtime &runtime)
    : state_(std::make_shared<State>(runtime)) {}

GpsReceiver::~GpsReceiver() { stop(); }

void GpsReceiver::start(GpsReceiverConfig config) {
  if (config.acquisition_time < SimTime::zero()) {
    throw std::invalid_argument("GPS acquisition time must not be negative");
  }
  if (config.epoch_period <= SimTime::zero()) {
    throw std::invalid_argument("GPS epoch period must be positive");
  }
  if (state_->generation == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error("GPS receiver generation exhausted");
  }

  ++state_->generation;
  state_->config = config;
  state_->started_at = state_->runtime->now();
  state_->epochs_emitted = 0;
  state_->output.clear();
  state_->running = true;
  state_->emit_startup();
  state_->schedule_epoch(state_->generation);
}

void GpsReceiver::stop() {
  if (!state_ || !state_->running) {
    return;
  }
  state_->running = false;
  if (state_->generation != std::numeric_limits<std::uint64_t>::max()) {
    ++state_->generation;
  }
}

bool GpsReceiver::running() const { return state_->running; }

std::size_t GpsReceiver::epochs_emitted() const {
  return state_->epochs_emitted;
}

const std::string &GpsReceiver::output() const { return state_->output; }

} // namespace host_sim
