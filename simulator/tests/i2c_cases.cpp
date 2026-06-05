#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

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
    host_sim::Wire2.begin();
    host_sim::Wire2.setClock(100000);
    host_sim::Wire2.beginTransmission(0x20);
    host_sim::Wire2.write(0x2E);

    ok &= require(host_sim::Wire2.endTransmission(false) == 4,
                  "physical board did not report its SDA stuck-low fault");
    const auto &trace = runtime.i2c_trace();
    ok &= require(
        trace.size() == 1 &&
            trace[0].status == host_sim::I2cStatus::BusStuckLow &&
            trace[0].sda_stuck_low && !trace[0].scl_stuck_low &&
            trace[0].start == 0us && trace[0].end == 0us,
        "physical SDA fault was not captured at the transaction boundary");
  }

  {
    host_sim::Runtime runtime(board_model());
    runtime.set_i2c_bus_mode(host_sim::I2cBusMode::Ideal);
    host_sim::Wire2.begin();
    host_sim::Wire2.setClock(100000);
    host_sim::Wire2.beginTransmission(0x20);
    host_sim::Wire2.write(0x2E);

    ok &= require(host_sim::Wire2.endTransmission(false) == 0 &&
                      runtime.now() == 190us,
                  "one-byte pointer write did not consume 19 I2C bit periods");
    ok &= require(
        host_sim::Wire2.requestFrom(0x20, 1) == 1 &&
            host_sim::Wire2.read() == 0x60 && runtime.now() == 390us,
        "one-byte repeated-start read did not consume 20 I2C bit periods");
    const auto &trace = runtime.i2c_trace();
    ok &= require(trace.size() == 2 &&
                      trace[0].operation == host_sim::I2cOperation::Write &&
                      !trace[0].send_stop && trace[0].transferred_bytes == 1 &&
                      trace[1].operation == host_sim::I2cOperation::Read &&
                      trace[1].transferred_bytes == 1,
                  "successful I2C transactions were not traced precisely");
  }

  {
    host_sim::Runtime runtime(board_model());
    runtime.set_i2c_bus_mode(host_sim::I2cBusMode::Ideal);
    host_sim::Wire2.begin();
    host_sim::Wire2.beginTransmission(0x21);
    host_sim::Wire2.write(0x2E);

    ok &= require(host_sim::Wire2.endTransmission() == 2 &&
                      runtime.now() == 110us,
                  "wrong-address transfer did not stop after address NACK");
  }

  {
    host_sim::Runtime runtime(board_model());
    runtime.set_i2c_bus_mode(host_sim::I2cBusMode::Ideal);
    host_sim::Wire2.begin();
    runtime.digital_write(
        static_cast<std::uint8_t>(runtime.model().arduino_pin("CY_RST_N")), 1);
    host_sim::Wire2.beginTransmission(0x20);
    host_sim::Wire2.write(0x2E);

    ok &= require(host_sim::Wire2.endTransmission() == 2,
                  "expander acknowledged its address while XRES was asserted");
  }

  {
    host_sim::Runtime runtime(board_model());
    runtime.set_i2c_bus_mode(host_sim::I2cBusMode::Ideal);
    host_sim::Wire2.begin();
    const auto reset =
        static_cast<std::uint8_t>(runtime.model().arduino_pin("CY_RST_N"));
    runtime.schedule_after(
        100us, [&runtime, reset] { runtime.digital_write(reset, 1); });
    host_sim::Wire2.beginTransmission(0x20);
    host_sim::Wire2.write(0x2E);

    ok &= require(
        host_sim::Wire2.endTransmission() == 3 && runtime.now() == 200us &&
            runtime.i2c_trace().back().status == host_sim::I2cStatus::DataNack,
        "mid-write reset did not abort the payload phase");
  }

  {
    host_sim::Runtime runtime(board_model());
    runtime.set_i2c_bus_mode(host_sim::I2cBusMode::Ideal);
    host_sim::Wire2.begin();
    host_sim::Wire2.beginTransmission(0x20);
    host_sim::Wire2.write(0x2E);
    ok &= require(host_sim::Wire2.endTransmission(false) == 0,
                  "read-reset setup did not select the device ID register");

    const auto reset =
        static_cast<std::uint8_t>(runtime.model().arduino_pin("CY_RST_N"));
    runtime.schedule_after(
        100us, [&runtime, reset] { runtime.digital_write(reset, 1); });
    ok &= require(
        host_sim::Wire2.requestFrom(0x20, 1) == 0 && runtime.now() == 390us &&
            runtime.i2c_trace().back().status == host_sim::I2cStatus::DataNack,
        "mid-read reset did not abort the data phase");
  }

  {
    host_sim::Runtime runtime(board_model());
    runtime.set_i2c_bus_mode(host_sim::I2cBusMode::Ideal);
    host_sim::Wire2.begin();
    runtime.set_i2c_next_nack(host_sim::I2cNack::Data);
    host_sim::Wire2.beginTransmission(0x20);
    host_sim::Wire2.write(0x2E);

    ok &= require(host_sim::Wire2.endTransmission() == 3 &&
                      runtime.now() == 200us &&
                      runtime.i2c_trace().back().transferred_bytes == 0,
                  "injected data NACK did not fail the first payload byte");
  }

  {
    host_sim::Runtime runtime(board_model());
    runtime.set_i2c_bus_mode(host_sim::I2cBusMode::Ideal);
    runtime.set_i2c_clock_stretch(250us);
    host_sim::Wire2.begin();
    host_sim::Wire2.beginTransmission(0x20);
    host_sim::Wire2.write(0x2E);

    ok &= require(host_sim::Wire2.endTransmission(false) == 0 &&
                      runtime.now() == 440us,
                  "clock stretching was not added to transaction time");
  }

  {
    host_sim::Runtime runtime(board_model());
    runtime.set_i2c_bus_mode(host_sim::I2cBusMode::Ideal);
    runtime.set_i2c_line_faults(false, true);
    host_sim::Wire2.begin();
    host_sim::Wire2.beginTransmission(0x20);
    host_sim::Wire2.write(0x2E);

    ok &= require(host_sim::Wire2.endTransmission() == 4 &&
                      runtime.now() == 0us &&
                      runtime.i2c_trace().back().scl_stuck_low,
                  "injected SCL stuck-low fault did not block START");
  }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
