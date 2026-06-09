#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "SD.h"
#include "host_simulator/runtime.h"

namespace {

#ifndef HOST_SIM_ROOT
#error HOST_SIM_ROOT must be defined by the build.
#endif

std::string data_path(const std::string &relative) {
  return std::string(HOST_SIM_ROOT) + "/" + relative;
}

host_sim::BoardModel board_model() {
  return host_sim::BoardModel::load(
      data_path("kicad_files/hardware_challenge.kicad_pcb"),
      data_path("kicad_files/hardware_challenge.kicad_sch"));
}

bool require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << message << "\n";
  }
  return condition;
}

} // namespace

int main() {
  using namespace std::chrono_literals;

  bool ok = true;
  {
    host_sim::Runtime runtime(board_model());
    host_sim::SdCardConfig config;
    config.capacity_bytes = 100;
    config.initialization_time = 5ms;
    config.open_time = 2ms;
    config.write_byte_time = 100us;
    config.close_time = 1ms;
    runtime.configure_sd(config);

    ok &= require(host_sim::SD.begin(BUILTIN_SDCARD) && runtime.now() == 5ms,
                  "SD initialization did not consume configured time");
    auto first = host_sim::SD.open("results.txt", FILE_WRITE);
    ok &= require(first && runtime.now() == 7ms,
                  "SD append open did not consume configured time");
    ok &= require(first.print("abc") == 3 && runtime.now() == 7300us,
                  "SD write did not consume per-byte time");
    first.close();
    ok &= require(runtime.now() == 8300us,
                  "SD close did not consume configured time");

    auto second = host_sim::SD.open("results.txt", FILE_WRITE);
    ok &= require(second.print("d") == 1,
                  "FILE_WRITE did not reopen an existing file for append");
    second.close();
    ok &=
        require(runtime.sd_content("results.txt") == "abcd" &&
                    runtime.sd_used_bytes() == 4 && runtime.now() == 11400us,
                "SD append content or aggregate capacity accounting was wrong");
    ok &= require(!host_sim::SD.open("results.txt", 0),
                  "unsupported SD open mode unexpectedly succeeded");
  }

  {
    host_sim::Runtime runtime(board_model());
    host_sim::SdCardConfig config;
    config.capacity_bytes = 3;
    runtime.configure_sd(config);
    ok &= require(host_sim::SD.begin(BUILTIN_SDCARD),
                  "bounded SD card did not initialize");
    auto file = host_sim::SD.open("capacity.txt", FILE_WRITE);
    ok &= require(file.print("hello") == 0 &&
                      runtime.sd_content("capacity.txt").empty() &&
                      runtime.sd_used_bytes() == 0,
                  "SD capacity failure was not all-or-zero");
  }

  {
    host_sim::Runtime runtime(board_model());
    host_sim::SdCardConfig config;
    config.successful_write_calls_before_failure = 2;
    runtime.configure_sd(config);
    ok &= require(host_sim::SD.begin(BUILTIN_SDCARD),
                  "write-fault SD card did not initialize");
    auto file = host_sim::SD.open("failure.txt", FILE_WRITE);
    ok &= require(file.print("ab") == 2 && file.print("cd") == 2 &&
                      file.print("ef") == 0 && file.print("gh") == 0 &&
                      runtime.sd_content("failure.txt") == "abcd",
                  "persistent SD write-call failure did not occur at boundary");
  }

  {
    host_sim::Runtime runtime(board_model());
    host_sim::SdCardConfig config;
    config.write_byte_time = 100us;
    runtime.configure_sd(config);
    ok &= require(host_sim::SD.begin(BUILTIN_SDCARD),
                  "removal SD card did not initialize");
    auto file = host_sim::SD.open("removal.txt", FILE_WRITE);
    runtime.schedule_sd_available(false, 250us);
    ok &= require(file.print("hello") == 0 && runtime.now() == 250us && file &&
                      runtime.sd_content("removal.txt").empty() &&
                      file.print("x") == 0 &&
                      !host_sim::SD.open("other.txt", FILE_WRITE),
                  "SD removal did not fail the in-flight append");

    runtime.set_sd_available(true);
    ok &= require(!host_sim::SD.open("other.txt", FILE_WRITE),
                  "reinserted SD card opened without reinitialization");
    ok &= require(host_sim::SD.begin(BUILTIN_SDCARD) &&
                      host_sim::SD.open("removal.txt", FILE_WRITE),
                  "reinserted SD card did not recover after begin");
  }

  {
    host_sim::Runtime runtime(board_model());
    host_sim::SdCardConfig config;
    config.initialization_time = 1ms;
    runtime.configure_sd(config);
    runtime.schedule_sd_available(false, 500us);
    ok &= require(!host_sim::SD.begin(BUILTIN_SDCARD) && runtime.now() == 1ms,
                  "SD removal during initialization did not fail begin");
  }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
