#pragma once

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace test_support {

inline std::string read_file(const std::string &path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("failed to open source file: " + path);
  }
  std::ostringstream content;
  content << file.rdbuf();
  return content.str();
}

inline void write_file(const std::string &path, const std::string &content) {
  std::ofstream file(path);
  if (!file) {
    throw std::runtime_error("failed to write source file: " + path);
  }
  file << content;
}

inline void replace_after(std::string &source, std::size_t offset,
                          const std::string &from, const std::string &to) {
  const auto marker = source.find(from, offset);
  if (marker == std::string::npos) {
    throw std::runtime_error("mutation marker not found: " + from);
  }
  source.replace(marker, from.size(), to);
}

inline std::string corrected_uart_pcb(const std::string &root,
                                      const std::string &filename) {
  auto source = read_file(root + "/kicad_files/hardware_challenge.kicad_pcb");
  const auto u2 = source.find("(property \"Reference\" \"U2\"");
  if (u2 == std::string::npos) {
    throw std::runtime_error("UART mutation did not find U2");
  }
  const auto rx_pad = source.find("(pad \"2\"", u2);
  const auto tx_pad = source.find("(pad \"3\"", rx_pad);
  if (rx_pad == std::string::npos || tx_pad == std::string::npos) {
    throw std::runtime_error("UART mutation did not find U2 serial pads");
  }
  replace_after(source, rx_pad, "(net 64 \"UBX-RXD\")", "(net 74 \"UBX-TXD\")");
  replace_after(source, tx_pad, "(net 74 \"UBX-TXD\")", "(net 64 \"UBX-RXD\")");
  const auto path = root + "/build/" + filename;
  write_file(path, source);
  return path;
}

} // namespace test_support
