#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "host_simulator/runtime.h"

namespace host_sim {

enum class GpsConstellationMode {
  GpsOnly,
  Concurrent,
};

enum class GpsChecksumMode {
  Valid,
  Corrupt,
};

struct GpsReceiverConfig {
  SimTime acquisition_time{26'000'000};
  SimTime epoch_period{1'000'000};
  GpsConstellationMode constellation = GpsConstellationMode::Concurrent;
  GpsChecksumMode checksum = GpsChecksumMode::Valid;
};

class GpsReceiver {
public:
  explicit GpsReceiver(Runtime &runtime);
  ~GpsReceiver();

  GpsReceiver(const GpsReceiver &) = delete;
  GpsReceiver &operator=(const GpsReceiver &) = delete;

  void start(GpsReceiverConfig config = {});
  void stop();
  bool running() const;
  std::size_t epochs_emitted() const;
  const std::string &output() const;

private:
  struct State;
  std::shared_ptr<State> state_;
};

} // namespace host_sim
