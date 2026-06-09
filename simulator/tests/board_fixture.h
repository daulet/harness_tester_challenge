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

inline std::size_t component_offset(const std::string &source,
                                    const std::string &reference) {
  const auto component =
      source.find("(property \"Reference\" \"" + reference + "\"");
  if (component == std::string::npos) {
    throw std::runtime_error("component mutation did not find " + reference);
  }
  return component;
}

inline void relabel_component_pad(std::string &source,
                                  const std::string &reference,
                                  const std::string &pad,
                                  const std::string &from_net,
                                  const std::string &to_net) {
  const auto component = component_offset(source, reference);
  const auto pad_offset = source.find("(pad \"" + pad + "\"", component);
  if (pad_offset == std::string::npos) {
    throw std::runtime_error("component mutation did not find " + reference +
                             " pad " + pad);
  }
  replace_after(source, pad_offset, from_net, to_net);
}

inline void replace_component_value(std::string &source,
                                    const std::string &reference,
                                    const std::string &from_value,
                                    const std::string &to_value) {
  replace_after(source, component_offset(source, reference),
                "(property \"Value\" \"" + from_value + "\"",
                "(property \"Value\" \"" + to_value + "\"");
}

inline void remove_segment_by_uuid(std::string &source,
                                   const std::string &uuid) {
  const auto uuid_offset = source.find("(uuid \"" + uuid + "\")");
  if (uuid_offset == std::string::npos) {
    throw std::runtime_error("segment mutation did not find UUID " + uuid);
  }
  const auto segment = source.rfind("\n\t(segment", uuid_offset);
  const auto end = source.find("\n\t)", uuid_offset);
  if (segment == std::string::npos || end == std::string::npos) {
    throw std::runtime_error("segment mutation could not isolate UUID " + uuid);
  }
  source.erase(segment, end + 3 - segment);
}

inline std::string relabelled_resistor_pad1_pcb(
    const std::string &root, const std::string &reference,
  const std::string &from_net, const std::string &to_net,
  const std::string &filename) {
  auto source = read_file(root + "/kicad_files/hardware_challenge.kicad_pcb");
  relabel_component_pad(source, reference, "1", from_net, to_net);
  const auto path = root + "/build/" + filename;
  write_file(path, source);
  return path;
}

inline std::string component_value_pcb(const std::string &root,
                                       const std::string &reference,
                                       const std::string &from_value,
                                       const std::string &to_value,
                                       const std::string &filename) {
  auto source = read_file(root + "/kicad_files/hardware_challenge.kicad_pcb");
  replace_component_value(source, reference, from_value, to_value);
  const auto path = root + "/build/" + filename;
  write_file(path, source);
  return path;
}

} // namespace test_support
