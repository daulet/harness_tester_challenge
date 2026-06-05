#include "host_simulator/board.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace host_sim {

namespace {

struct SExpr {
  bool is_atom = false;
  std::string atom;
  std::vector<SExpr> children;
};

class Parser {
public:
  explicit Parser(std::string input) : input_(std::move(input)) {}

  SExpr parse() {
    skip_space();
    if (offset_ >= input_.size() || input_[offset_] != '(') {
      throw std::runtime_error("expected KiCad S-expression");
    }
    return parse_list();
  }

private:
  void skip_space() {
    while (offset_ < input_.size()) {
      if (std::isspace(static_cast<unsigned char>(input_[offset_]))) {
        ++offset_;
        continue;
      }
      if (input_[offset_] == ';') {
        while (offset_ < input_.size() && input_[offset_] != '\n') {
          ++offset_;
        }
        continue;
      }
      break;
    }
  }

  SExpr parse_list() {
    SExpr expr;
    ++offset_;
    skip_space();
    if (offset_ >= input_.size()) {
      throw std::runtime_error("unterminated KiCad list");
    }
    if (input_[offset_] == ')') {
      ++offset_;
      return expr;
    }
    SExpr head = parse_item();
    if (!head.is_atom) {
      throw std::runtime_error("KiCad list head must be an atom");
    }
    expr.atom = std::move(head.atom);
    while (true) {
      skip_space();
      if (offset_ >= input_.size()) {
        throw std::runtime_error("unterminated KiCad list");
      }
      if (input_[offset_] == ')') {
        ++offset_;
        return expr;
      }
      expr.children.push_back(parse_item());
    }
  }

  SExpr parse_item() {
    skip_space();
    if (offset_ >= input_.size()) {
      throw std::runtime_error("unexpected end of KiCad input");
    }
    if (input_[offset_] == '(') {
      return parse_list();
    }
    SExpr atom;
    atom.is_atom = true;
    if (input_[offset_] == '"') {
      ++offset_;
      while (offset_ < input_.size()) {
        const char ch = input_[offset_++];
        if (ch == '\\' && offset_ < input_.size()) {
          atom.atom += input_[offset_++];
          continue;
        }
        if (ch == '"') {
          return atom;
        }
        atom.atom += ch;
      }
      throw std::runtime_error("unterminated KiCad string");
    }
    while (offset_ < input_.size()) {
      const char ch = input_[offset_];
      if (std::isspace(static_cast<unsigned char>(ch)) || ch == '(' || ch == ')') {
        break;
      }
      atom.atom += ch;
      ++offset_;
    }
    return atom;
  }

  std::string input_;
  std::size_t offset_ = 0;
};

std::string read_file(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("failed to open KiCad source file: " + path);
  }
  std::ostringstream content;
  content << file.rdbuf();
  return content.str();
}

const SExpr* child_named(const SExpr& expr, const std::string& name) {
  for (const auto& child : expr.children) {
    if (!child.is_atom && child.atom == name) {
      return &child;
    }
  }
  return nullptr;
}

std::vector<const SExpr*> children_named(const SExpr& expr, const std::string& name) {
  std::vector<const SExpr*> result;
  for (const auto& child : expr.children) {
    if (!child.is_atom && child.atom == name) {
      result.push_back(&child);
    }
  }
  return result;
}

std::optional<std::string> atom_at(const SExpr& expr, std::size_t index) {
  if (index >= expr.children.size() || !expr.children[index].is_atom) {
    return std::nullopt;
  }
  return expr.children[index].atom;
}

std::string required_atom(const SExpr& expr, std::size_t index, const std::string& context) {
  const auto value = atom_at(expr, index);
  if (!value) {
    throw std::runtime_error("missing atom in " + context);
  }
  return *value;
}

long long scaled_coordinate(const std::string& value) {
  return static_cast<long long>(std::llround(std::stod(value) * 1000000.0));
}

struct Point {
  long long x = 0;
  long long y = 0;

  bool operator<(const Point& other) const {
    return x < other.x || (x == other.x && y < other.y);
  }

  bool operator==(const Point& other) const {
    return x == other.x && y == other.y;
  }
};

std::string point_key(const Point& point) {
  return std::to_string(point.x) + ":" + std::to_string(point.y);
}

Point parse_point(const SExpr& expr) {
  if (expr.atom != "at" && expr.atom != "xy" && expr.atom != "start" && expr.atom != "end") {
    throw std::runtime_error("expected KiCad point");
  }
  return {scaled_coordinate(required_atom(expr, 0, expr.atom)),
          scaled_coordinate(required_atom(expr, 1, expr.atom))};
}

struct UnionFind {
  std::unordered_map<std::string, std::string> parent;

  void add(const std::string& key) {
    parent.emplace(key, key);
  }

  std::string find(const std::string& key) {
    auto iter = parent.find(key);
    if (iter == parent.end()) {
      parent.emplace(key, key);
      return key;
    }
    if (iter->second == key) {
      return key;
    }
    iter->second = find(iter->second);
    return iter->second;
  }

  void unite(const std::string& left, const std::string& right) {
    const auto left_root = find(left);
    const auto right_root = find(right);
    if (left_root != right_root) {
      parent[right_root] = left_root;
    }
  }
};

struct PadRecord {
  std::string reference;
  std::string pad;
  std::string net;
  std::string pinfunction;
  Point point;
  long long radius = 0;
  std::set<std::string> layers;
};

struct SegmentRecord {
  std::string net;
  Point start;
  Point end;
  std::string layer;
  long long width = 0;
};

struct ViaRecord {
  std::string net;
  Point point;
  long long radius = 0;
  std::set<std::string> layers;
};

struct PolygonRecord {
  std::vector<Point> points;
};

struct ZoneRecord {
  std::string net;
  std::set<std::string> layers;
  std::vector<PolygonRecord> polygons;
  std::vector<PolygonRecord> filled_polygons;
  bool keepout = false;
};

struct PcbData {
  std::map<int, std::string> net_names;
  std::map<std::string, BoardComponent> components;
  std::vector<PadRecord> pads;
  std::vector<SegmentRecord> segments;
  std::vector<ViaRecord> vias;
  std::vector<ZoneRecord> zones;
  std::map<std::string, PhysicalNet> physical_nets;
};

struct PhysicalFeature {
  enum class Kind {
    Pad,
    Track,
    Via,
    Zone,
  };

  Kind kind = Kind::Pad;
  std::string net;
  std::string reference;
  std::string pad;
  Point start;
  Point end;
  long long radius = 0;
  std::set<std::string> layers;
  std::vector<PolygonRecord> polygons;
};

struct RuntimePhysicalGraph {
  std::vector<PhysicalFeature> features;
  std::vector<std::vector<std::size_t>> edges;
  std::vector<std::size_t> components;
  std::vector<ZoneRecord> keepouts;
};

std::string net_name(const std::map<int, std::string>& net_names, int net_id) {
  const auto iter = net_names.find(net_id);
  if (iter == net_names.end()) {
    return {};
  }
  return iter->second;
}

int parse_net_id(const SExpr& expr) {
  return std::stoi(required_atom(expr, 0, expr.atom));
}

std::set<std::string> parse_layers(const SExpr& expr) {
  std::set<std::string> layers;
  for (const auto& child : expr.children) {
    if (child.is_atom) {
      layers.insert(child.atom);
    }
  }
  return layers;
}

std::vector<PolygonRecord> parse_polygons(const SExpr& expr) {
  std::vector<PolygonRecord> polygons;
  for (const auto* polygon : children_named(expr, "polygon")) {
    PolygonRecord record;
    const auto* pts = child_named(*polygon, "pts");
    if (!pts) {
      continue;
    }
    for (const auto& point_expr : pts->children) {
      if (!point_expr.is_atom && point_expr.atom == "xy") {
        record.points.push_back(parse_point(point_expr));
      }
    }
    if (!record.points.empty()) {
      polygons.push_back(std::move(record));
    }
  }
  return polygons;
}

std::optional<PolygonRecord> parse_direct_polygon(const SExpr& expr) {
  PolygonRecord record;
  const auto* pts = child_named(expr, "pts");
  if (!pts) {
    return std::nullopt;
  }
  for (const auto& point_expr : pts->children) {
    if (!point_expr.is_atom && point_expr.atom == "xy") {
      record.points.push_back(parse_point(point_expr));
    }
  }
  if (record.points.empty()) {
    return std::nullopt;
  }
  return record;
}

Point rotate_board_point(Point point, int rotation) {
  const int normalized = (((-rotation) % 360) + 360) % 360;
  if (normalized == 0) {
    return point;
  }
  if (normalized == 90) {
    return {-point.y, point.x};
  }
  if (normalized == 180) {
    return {-point.x, -point.y};
  }
  if (normalized == 270) {
    return {point.y, -point.x};
  }
  throw std::runtime_error("unsupported KiCad PCB rotation");
}

Point translate_board_point(Point point, Point offset) {
  point.x += offset.x;
  point.y += offset.y;
  return point;
}

bool same_layer(const std::set<std::string>& left, const std::set<std::string>& right) {
  if (left.count("*.Cu") || right.count("*.Cu")) {
    return true;
  }
  const bool left_through_via = left.count("F.Cu") && left.count("B.Cu");
  const bool right_through_via = right.count("F.Cu") && right.count("B.Cu");
  if (left_through_via || right_through_via) {
    for (const auto& layer : left) {
      if (layer.find(".Cu") != std::string::npos) {
        return true;
      }
    }
    for (const auto& layer : right) {
      if (layer.find(".Cu") != std::string::npos) {
        return true;
      }
    }
  }
  for (const auto& layer : left) {
    if (right.count(layer)) {
      return true;
    }
  }
  return false;
}

long long cross(Point left, Point right, Point point) {
  return (right.x - left.x) * (point.y - left.y) -
      (right.y - left.y) * (point.x - left.x);
}

bool on_segment(Point left, Point right, Point point) {
  return cross(left, right, point) == 0 &&
      point.x >= std::min(left.x, right.x) &&
      point.x <= std::max(left.x, right.x) &&
      point.y >= std::min(left.y, right.y) &&
      point.y <= std::max(left.y, right.y);
}

bool segments_intersect(Point a_start, Point a_end, Point b_start, Point b_end) {
  const auto a1 = cross(a_start, a_end, b_start);
  const auto a2 = cross(a_start, a_end, b_end);
  const auto b1 = cross(b_start, b_end, a_start);
  const auto b2 = cross(b_start, b_end, a_end);
  if (((a1 > 0 && a2 < 0) || (a1 < 0 && a2 > 0)) &&
      ((b1 > 0 && b2 < 0) || (b1 < 0 && b2 > 0))) {
    return true;
  }
  return on_segment(a_start, a_end, b_start) || on_segment(a_start, a_end, b_end) ||
      on_segment(b_start, b_end, a_start) || on_segment(b_start, b_end, a_end);
}

bool point_in_polygon(Point point, const PolygonRecord& polygon) {
  if (polygon.points.size() < 3) {
    return false;
  }
  for (std::size_t index = 0; index < polygon.points.size(); ++index) {
    const auto next = (index + 1) % polygon.points.size();
    if (on_segment(polygon.points[index], polygon.points[next], point)) {
      return true;
    }
  }
  bool inside = false;
  for (std::size_t index = 0, previous = polygon.points.size() - 1;
       index < polygon.points.size(); previous = index++) {
    const auto& current = polygon.points[index];
    const auto& prior = polygon.points[previous];
    if (on_segment(prior, current, point)) {
      return true;
    }
    const bool crosses = ((current.y > point.y) != (prior.y > point.y)) &&
        (point.x < (prior.x - current.x) * (point.y - current.y) /
            static_cast<double>(prior.y - current.y) + current.x);
    if (crosses) {
      inside = !inside;
    }
  }
  return inside;
}

long double squared_distance(Point left, Point right) {
  const auto dx = left.x - right.x;
  const auto dy = left.y - right.y;
  return static_cast<long double>(dx) * dx + static_cast<long double>(dy) * dy;
}

long double point_to_segment_squared_distance(Point point, Point start, Point end) {
  const auto dx = static_cast<long double>(end.x - start.x);
  const auto dy = static_cast<long double>(end.y - start.y);
  if (dx == 0.0L && dy == 0.0L) {
    return squared_distance(point, start);
  }
  const auto projection = std::clamp(
      ((point.x - start.x) * dx + (point.y - start.y) * dy) / (dx * dx + dy * dy),
      0.0L, 1.0L);
  const Point nearest{
      static_cast<long long>(std::llround(start.x + projection * dx)),
      static_cast<long long>(std::llround(start.y + projection * dy)),
  };
  return squared_distance(point, nearest);
}

PcbData parse_pcb(const std::string& path) {
  const auto root = Parser(read_file(path)).parse();
  if (root.atom != "kicad_pcb") {
    throw std::runtime_error("not a KiCad PCB file: " + path);
  }

  PcbData pcb;
  for (const auto& child : root.children) {
    if (child.is_atom) {
      continue;
    }
    if (child.atom == "net") {
      const auto id = std::stoi(required_atom(child, 0, "net"));
      pcb.net_names[id] = required_atom(child, 1, "net");
      continue;
    }
    if (child.atom == "footprint") {
      BoardComponent component;
      std::string reference;
      Point footprint_point;
      int footprint_rotation = 0;
      if (const auto footprint = atom_at(child, 0)) {
        component.pcb_footprint = *footprint;
      }
      for (const auto* property : children_named(child, "property")) {
        if (atom_at(*property, 0) && *atom_at(*property, 0) == "Reference" &&
            atom_at(*property, 1)) {
          reference = *atom_at(*property, 1);
        } else if (atom_at(*property, 0) && *atom_at(*property, 0) == "Value" &&
                   atom_at(*property, 1)) {
          component.pcb_value = *atom_at(*property, 1);
        }
      }
      if (!reference.empty()) {
        component.reference = reference;
        pcb.components[reference] = component;
      }
      if (const auto* at = child_named(child, "at")) {
        footprint_point = parse_point(*at);
        if (at->children.size() >= 3 && at->children[2].is_atom) {
          footprint_rotation = std::stoi(at->children[2].atom);
        }
      }
      for (const auto* pad : children_named(child, "pad")) {
        PadRecord record;
        record.reference = reference;
        record.pad = required_atom(*pad, 0, "pad");
        if (const auto* net = child_named(*pad, "net")) {
          record.net = net_name(pcb.net_names, parse_net_id(*net));
        }
        if (const auto* pinfunction = child_named(*pad, "pinfunction")) {
          if (atom_at(*pinfunction, 0)) {
            record.pinfunction = *atom_at(*pinfunction, 0);
          }
        }
        if (const auto* at = child_named(*pad, "at")) {
          record.point = translate_board_point(
              rotate_board_point(parse_point(*at), footprint_rotation), footprint_point);
        }
        if (const auto* size = child_named(*pad, "size")) {
          const auto x = std::llabs(scaled_coordinate(required_atom(*size, 0, "size")));
          const auto y = std::llabs(scaled_coordinate(required_atom(*size, 1, "size")));
          record.radius = std::max(x, y) / 2;
        }
        if (const auto* layers = child_named(*pad, "layers")) {
          record.layers = parse_layers(*layers);
        }
        pcb.pads.push_back(std::move(record));
      }
      continue;
    }
    if (child.atom == "segment" || child.atom == "arc") {
      SegmentRecord record;
      if (const auto* net = child_named(child, "net")) {
        record.net = net_name(pcb.net_names, parse_net_id(*net));
      }
      if (const auto* start = child_named(child, "start")) {
        record.start = parse_point(*start);
      }
      if (const auto* end = child_named(child, "end")) {
        record.end = parse_point(*end);
      }
      if (const auto* layer = child_named(child, "layer")) {
        if (atom_at(*layer, 0)) {
          record.layer = *atom_at(*layer, 0);
        }
      }
      if (const auto* width = child_named(child, "width")) {
        record.width = std::llabs(scaled_coordinate(required_atom(*width, 0, "width")));
      }
      pcb.segments.push_back(std::move(record));
      continue;
    }
    if (child.atom == "via") {
      ViaRecord record;
      if (const auto* net = child_named(child, "net")) {
        record.net = net_name(pcb.net_names, parse_net_id(*net));
      }
      if (const auto* at = child_named(child, "at")) {
        record.point = parse_point(*at);
      }
      if (const auto* size = child_named(child, "size")) {
        record.radius = std::llabs(scaled_coordinate(required_atom(*size, 0, "size"))) / 2;
      }
      if (const auto* layers = child_named(child, "layers")) {
        record.layers = parse_layers(*layers);
      }
      pcb.vias.push_back(std::move(record));
      continue;
    }
    if (child.atom == "zone") {
      ZoneRecord record;
      if (const auto* net = child_named(child, "net")) {
        record.net = net_name(pcb.net_names, parse_net_id(*net));
      }
      if (const auto* layer = child_named(child, "layer")) {
        if (atom_at(*layer, 0)) {
          record.layers.insert(*atom_at(*layer, 0));
        }
      }
      if (const auto* layers = child_named(child, "layers")) {
        record.layers = parse_layers(*layers);
      }
      record.polygons = parse_polygons(child);
      for (const auto* filled : children_named(child, "filled_polygon")) {
        if (const auto polygon = parse_direct_polygon(*filled)) {
          record.filled_polygons.push_back(std::move(*polygon));
        }
      }
      record.keepout = child_named(child, "keepout") != nullptr;
      pcb.zones.push_back(std::move(record));
      continue;
    }
  }

  auto ensure_physical_net = [&pcb](const std::string& name) -> PhysicalNet& {
    auto [iter, inserted] = pcb.physical_nets.emplace(name, PhysicalNet{});
    if (inserted) {
      iter->second.name = name;
    }
    return iter->second;
  };
  for (const auto& pad : pcb.pads) {
    if (pad.net.empty()) {
      continue;
    }
    auto& net = ensure_physical_net(pad.net);
    ++net.pad_count;
    if (!pad.reference.empty()) {
      net.references.insert(pad.reference);
      net.pads.insert(pad.reference + ":" + pad.pad);
    }
  }
  for (const auto& segment : pcb.segments) {
    if (segment.net.empty()) {
      continue;
    }
    ++ensure_physical_net(segment.net).track_count;
  }
  for (const auto& via : pcb.vias) {
    if (via.net.empty()) {
      continue;
    }
    ++ensure_physical_net(via.net).via_count;
  }
  for (const auto& zone : pcb.zones) {
    if (zone.net.empty()) {
      continue;
    }
    auto& net = ensure_physical_net(zone.net);
    if (zone.keepout) {
      ++net.keepout_count;
    } else {
      ++net.zone_count;
    }
  }
  return pcb;
}

struct PinDefinition {
  std::string number;
  std::string name;
  Point point;
};

struct SymbolDefinition {
  std::map<std::string, PinDefinition> pins;
};

struct SchematicData {
  std::map<std::string, SymbolDefinition> library;
  std::map<std::string, BoardComponent> components;
  std::map<std::string, std::map<std::string, std::set<std::string>>> pin_nets;
  std::map<std::string, std::map<std::string, std::string>> pin_names;
};

struct WireRecord {
  Point start;
  Point end;
};

bool point_on_segment(const Point& point, const WireRecord& segment) {
  const auto cross = (point.y - segment.start.y) * (segment.end.x - segment.start.x) -
      (point.x - segment.start.x) * (segment.end.y - segment.start.y);
  if (cross != 0) {
    return false;
  }
  const auto min_x = std::min(segment.start.x, segment.end.x);
  const auto max_x = std::max(segment.start.x, segment.end.x);
  const auto min_y = std::min(segment.start.y, segment.end.y);
  const auto max_y = std::max(segment.start.y, segment.end.y);
  return point.x >= min_x && point.x <= max_x && point.y >= min_y && point.y <= max_y;
}

void attach_point_to_wires(const Point& point, const std::vector<WireRecord>& wires,
                           UnionFind& connections) {
  const auto key = point_key(point);
  connections.add(key);
  for (const auto& wire : wires) {
    if (point_on_segment(point, wire)) {
      connections.unite(key, point_key(wire.start));
      connections.unite(key, point_key(wire.end));
    }
  }
}

void collect_pin_definitions(const SExpr& expr, SymbolDefinition& definition) {
  for (const auto& child : expr.children) {
    if (child.is_atom) {
      continue;
    }
    if (child.atom == "pin") {
      PinDefinition pin;
      if (const auto* number = child_named(child, "number")) {
        if (atom_at(*number, 0)) {
          pin.number = *atom_at(*number, 0);
        }
      }
      if (const auto* name = child_named(child, "name")) {
        if (atom_at(*name, 0)) {
          pin.name = *atom_at(*name, 0);
        }
      }
      if (const auto* at = child_named(child, "at")) {
        pin.point = parse_point(*at);
      }
      if (!pin.number.empty()) {
        definition.pins[pin.number] = std::move(pin);
      }
    }
    collect_pin_definitions(child, definition);
  }
}

Point rotate_point(const Point& point, int rotation) {
  const int normalized = ((rotation % 360) + 360) % 360;
  if (normalized == 0) {
    return point;
  }
  if (normalized == 90) {
    return {-point.y, point.x};
  }
  if (normalized == 180) {
    return {-point.x, -point.y};
  }
  if (normalized == 270) {
    return {point.y, -point.x};
  }
  throw std::runtime_error("unsupported KiCad symbol rotation");
}

Point transform_pin_point(Point point, const Point& origin, int rotation,
                          const std::optional<std::string>& mirror) {
  // KiCad library symbols use an upward local Y axis while sheet coordinates
  // grow downward.
  point.y = -point.y;
  if (mirror && *mirror == "x") {
    point.y = -point.y;
  } else if (mirror && *mirror == "y") {
    point.x = -point.x;
  }
  // KiCad symbol rotation is clockwise in sheet coordinates after the local
  // upward Y axis has been converted into the sheet's downward Y axis.
  point = rotate_point(point, -rotation);
  point.x += origin.x;
  point.y += origin.y;
  return point;
}

std::string normalize_identifier(const std::string& value) {
  std::string result;
  bool last_was_underscore = false;
  for (char ch : value) {
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9')) {
      result += ch;
      last_was_underscore = false;
    } else {
      if (!result.empty() && !last_was_underscore) {
        result += '_';
        last_was_underscore = true;
      }
    }
  }
  while (!result.empty() && result.back() == '_') {
    result.pop_back();
  }
  return result;
}

std::optional<std::size_t> teensy_pin_number(const std::string& name) {
  std::size_t offset = 0;
  while (offset < name.size() && std::isdigit(static_cast<unsigned char>(name[offset]))) {
    ++offset;
  }
  if (offset == 0) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(std::stoul(name.substr(0, offset)));
}

SchematicData parse_schematic(const std::string& path) {
  const auto root = Parser(read_file(path)).parse();
  if (root.atom != "kicad_sch") {
    throw std::runtime_error("not a KiCad schematic file: " + path);
  }

  SchematicData schematic;
  if (const auto* libraries = child_named(root, "lib_symbols")) {
    for (const auto* symbol : children_named(*libraries, "symbol")) {
      const auto name = atom_at(*symbol, 0);
      if (!name) {
        continue;
      }
      SymbolDefinition definition;
      collect_pin_definitions(*symbol, definition);
      schematic.library[*name] = std::move(definition);
    }
  }

  UnionFind connections;
  std::vector<WireRecord> wires;
  std::map<std::string, std::vector<std::string>> label_points;
  std::map<std::string, std::set<std::string>> component_labels;
  for (const auto& child : root.children) {
    if (child.is_atom) {
      continue;
    }
    if (child.atom == "wire") {
      const auto* pts = child_named(child, "pts");
      if (!pts || pts->children.size() < 2) {
        continue;
      }
      Point previous = parse_point(pts->children.front());
      connections.add(point_key(previous));
      for (std::size_t index = 1; index < pts->children.size(); ++index) {
        const auto next = parse_point(pts->children[index]);
        wires.push_back({previous, next});
        const auto previous_key = point_key(previous);
        const auto next_key = point_key(next);
        connections.add(next_key);
        connections.unite(previous_key, next_key);
        previous = next;
      }
      continue;
    }
    if (child.atom == "junction") {
      if (const auto* at = child_named(child, "at")) {
        attach_point_to_wires(parse_point(*at), wires, connections);
      }
      continue;
    }
    if (child.atom == "label" || child.atom == "global_label" ||
        child.atom == "hierarchical_label") {
      const auto name = atom_at(child, 0);
      const auto* at = child_named(child, "at");
      if (!name || !at) {
        continue;
      }
      const auto point = parse_point(*at);
      attach_point_to_wires(point, wires, connections);
      const auto key = point_key(point);
      label_points[*name].push_back(key);
      continue;
    }
  }
  for (const auto& [name, points] : label_points) {
    if (points.empty()) {
      continue;
    }
    for (std::size_t index = 1; index < points.size(); ++index) {
      connections.unite(points.front(), points[index]);
    }
  }
  for (const auto& [name, points] : label_points) {
    for (const auto& point : points) {
      component_labels[connections.find(point)].insert(name);
    }
  }

  for (const auto& child : root.children) {
    if (child.is_atom || child.atom != "symbol") {
      continue;
    }
    const auto* lib_id_expr = child_named(child, "lib_id");
    const auto lib_id = lib_id_expr ? atom_at(*lib_id_expr, 0) : std::nullopt;
    const auto* at = child_named(child, "at");
    if (!lib_id || !at) {
      continue;
    }
    std::string reference;
    BoardComponent component;
    component.schematic_lib_id = *lib_id;
    for (const auto* property : children_named(child, "property")) {
      if (atom_at(*property, 0) && *atom_at(*property, 0) == "Reference" &&
          atom_at(*property, 1)) {
        reference = *atom_at(*property, 1);
      } else if (atom_at(*property, 0) && *atom_at(*property, 0) == "Value" &&
                 atom_at(*property, 1)) {
        component.schematic_value = *atom_at(*property, 1);
      } else if (atom_at(*property, 0) && *atom_at(*property, 0) == "Footprint" &&
                 atom_at(*property, 1)) {
        component.schematic_footprint = *atom_at(*property, 1);
      }
    }
    if (reference.empty()) {
      continue;
    }
    component.reference = reference;
    schematic.components[reference] = component;
    const auto library_iter = schematic.library.find(*lib_id);
    if (library_iter == schematic.library.end()) {
      continue;
    }
    Point origin = parse_point(*at);
    int rotation = 0;
    if (at->children.size() >= 3 && at->children[2].is_atom) {
      rotation = std::stoi(at->children[2].atom);
    }
    std::optional<std::string> mirror;
    if (const auto* mirror_expr = child_named(child, "mirror")) {
      if (atom_at(*mirror_expr, 0)) {
        mirror = *atom_at(*mirror_expr, 0);
      }
    }
    for (const auto* pin_expr : children_named(child, "pin")) {
      const auto number = atom_at(*pin_expr, 0);
      if (!number) {
        continue;
      }
      const auto pin_iter = library_iter->second.pins.find(*number);
      if (pin_iter == library_iter->second.pins.end()) {
        continue;
      }
      const auto point = transform_pin_point(pin_iter->second.point, origin, rotation, mirror);
      attach_point_to_wires(point, wires, connections);
      const auto key = point_key(point);
      const auto component = connections.find(key);
      const auto labels = component_labels.find(component);
      if (labels != component_labels.end()) {
        schematic.pin_nets[reference][*number] = labels->second;
      }
      schematic.pin_names[reference][*number] = pin_iter->second.name;
    }
  }

  return schematic;
}

bool drives_high(const ExpanderPinDrive& drive) {
  if (drive.is_input) {
    return drive.drive_mode == DriveMode::PullUp;
  }
  if (drive.drive_mode == DriveMode::Strong || drive.drive_mode == DriveMode::SlowStrong) {
    return drive.output_value;
  }
  if (drive.drive_mode == DriveMode::OpenDrainHigh) {
    return true;
  }
  return false;
}

std::optional<std::pair<std::size_t, std::size_t>> parse_gport(const std::string& pinfunction) {
  const auto marker = pinfunction.find("GPort");
  if (marker == std::string::npos) {
    return std::nullopt;
  }
  const auto port_start = marker + std::string("GPort").size();
  const auto bit_marker = pinfunction.find("_Bit", port_start);
  if (bit_marker == std::string::npos) {
    return std::nullopt;
  }
  const auto port = static_cast<std::size_t>(
      std::stoul(pinfunction.substr(port_start, bit_marker - port_start)));
  const auto bit_start = bit_marker + std::string("_Bit").size();
  std::size_t bit_end = bit_start;
  while (bit_end < pinfunction.size() &&
         std::isdigit(static_cast<unsigned char>(pinfunction[bit_end]))) {
    ++bit_end;
  }
  if (bit_end == bit_start) {
    return std::nullopt;
  }
  const auto bit = static_cast<std::size_t>(
      std::stoul(pinfunction.substr(bit_start, bit_end - bit_start)));
  return std::make_pair(port, bit);
}

RuntimePhysicalGraph build_physical_graph(const PcbData& pcb) {
  RuntimePhysicalGraph graph;
  for (const auto& zone : pcb.zones) {
    if (zone.keepout) {
      graph.keepouts.push_back(zone);
    }
  }
  for (const auto& pad : pcb.pads) {
    if (pad.net.empty()) {
      continue;
    }
    PhysicalFeature feature;
    feature.kind = PhysicalFeature::Kind::Pad;
    feature.net = pad.net;
    feature.reference = pad.reference;
    feature.pad = pad.pad;
    feature.start = pad.point;
    feature.end = pad.point;
    feature.radius = pad.radius;
    feature.layers = pad.layers;
    graph.features.push_back(std::move(feature));
  }
  for (const auto& segment : pcb.segments) {
    if (segment.net.empty()) {
      continue;
    }
    PhysicalFeature feature;
    feature.kind = PhysicalFeature::Kind::Track;
    feature.net = segment.net;
    feature.start = segment.start;
    feature.end = segment.end;
    feature.radius = segment.width / 2;
    feature.layers.insert(segment.layer);
    graph.features.push_back(std::move(feature));
  }
  for (const auto& via : pcb.vias) {
    if (via.net.empty()) {
      continue;
    }
    PhysicalFeature feature;
    feature.kind = PhysicalFeature::Kind::Via;
    feature.net = via.net;
    feature.start = via.point;
    feature.end = via.point;
    feature.radius = via.radius;
    feature.layers = via.layers;
    graph.features.push_back(std::move(feature));
  }
  for (const auto& zone : pcb.zones) {
    if (zone.net.empty() || zone.keepout) {
      continue;
    }
    const auto& source_polygons = zone.filled_polygons.empty() ? zone.polygons : zone.filled_polygons;
    for (const auto& polygon : source_polygons) {
      PhysicalFeature feature;
      feature.kind = PhysicalFeature::Kind::Zone;
      feature.net = zone.net;
      feature.layers = zone.layers;
      feature.polygons.push_back(polygon);
      graph.features.push_back(std::move(feature));
    }
  }

  auto connected = [](const PhysicalFeature& left, const PhysicalFeature& right) {
    if (left.net != right.net || !same_layer(left.layers, right.layers)) {
      return false;
    }
    const bool left_zone = left.kind == PhysicalFeature::Kind::Zone;
    const bool right_zone = right.kind == PhysicalFeature::Kind::Zone;
    if (left_zone || right_zone) {
      if (left_zone && right_zone) {
        for (const auto& left_polygon : left.polygons) {
          for (const auto& right_polygon : right.polygons) {
            if (!left_polygon.points.empty() &&
                point_in_polygon(left_polygon.points.front(), right_polygon)) {
              return true;
            }
            if (!right_polygon.points.empty() &&
                point_in_polygon(right_polygon.points.front(), left_polygon)) {
              return true;
            }
            for (std::size_t left_index = 0; left_index < left_polygon.points.size(); ++left_index) {
              const auto left_next = (left_index + 1) % left_polygon.points.size();
              for (std::size_t right_index = 0; right_index < right_polygon.points.size(); ++right_index) {
                const auto right_next = (right_index + 1) % right_polygon.points.size();
                if (segments_intersect(left_polygon.points[left_index], left_polygon.points[left_next],
                                       right_polygon.points[right_index], right_polygon.points[right_next])) {
                  return true;
                }
              }
            }
          }
        }
        return false;
      }
      const auto& zone = left_zone ? left : right;
      const auto& feature = left_zone ? right : left;
      for (const auto& polygon : zone.polygons) {
        if (point_in_polygon(feature.start, polygon) || point_in_polygon(feature.end, polygon)) {
          return true;
        }
        for (std::size_t index = 0; index < polygon.points.size(); ++index) {
          const auto next = (index + 1) % polygon.points.size();
          if (segments_intersect(feature.start, feature.end,
                                 polygon.points[index], polygon.points[next])) {
            return true;
          }
        }
      }
      return false;
    }

    const auto threshold = left.radius + right.radius + 100;
    if (segments_intersect(left.start, left.end, right.start, right.end)) {
      return true;
    }
    const auto threshold_squared = static_cast<long double>(threshold) * threshold;
    return point_to_segment_squared_distance(left.start, right.start, right.end) <= threshold_squared ||
        point_to_segment_squared_distance(left.end, right.start, right.end) <= threshold_squared ||
        point_to_segment_squared_distance(right.start, left.start, left.end) <= threshold_squared ||
        point_to_segment_squared_distance(right.end, left.start, left.end) <= threshold_squared;
  };

  graph.edges.resize(graph.features.size());
  for (std::size_t left = 0; left < graph.features.size(); ++left) {
    for (std::size_t right = left + 1; right < graph.features.size(); ++right) {
      if (connected(graph.features[left], graph.features[right])) {
        graph.edges[left].push_back(right);
        graph.edges[right].push_back(left);
      }
    }
  }

  graph.components.resize(graph.features.size(), static_cast<std::size_t>(-1));
  std::size_t next_component = 0;
  for (std::size_t start = 0; start < graph.features.size(); ++start) {
    if (graph.components[start] != static_cast<std::size_t>(-1)) {
      continue;
    }
    std::vector<std::size_t> pending{start};
    graph.components[start] = next_component;
    while (!pending.empty()) {
      const auto node = pending.back();
      pending.pop_back();
      for (const auto next : graph.edges[node]) {
        if (graph.components[next] == static_cast<std::size_t>(-1)) {
          graph.components[next] = next_component;
          pending.push_back(next);
        }
      }
    }
    ++next_component;
  }
  return graph;
}

std::optional<std::size_t> feature_index(const RuntimePhysicalGraph& graph,
                                         const std::string& reference,
                                         const std::string& pad) {
  for (std::size_t index = 0; index < graph.features.size(); ++index) {
    const auto& feature = graph.features[index];
    if (feature.kind == PhysicalFeature::Kind::Pad &&
        feature.reference == reference && feature.pad == pad) {
      return index;
    }
  }
  return std::nullopt;
}

const PadRecord* find_pcb_pad(const std::vector<PadRecord>& pads,
                              const std::string& reference,
                              const std::string& pad) {
  const auto iter = std::find_if(
      pads.begin(), pads.end(),
      [&](const PadRecord& candidate) {
        return candidate.reference == reference && candidate.pad == pad;
      });
  return iter == pads.end() ? nullptr : &*iter;
}

std::optional<std::string> schematic_net_for_pin(const SchematicData& schematic,
                                                 const std::string& reference,
                                                 const std::string& pad) {
  const auto reference_iter = schematic.pin_nets.find(reference);
  if (reference_iter == schematic.pin_nets.end()) {
    return std::nullopt;
  }
  const auto pad_iter = reference_iter->second.find(pad);
  if (pad_iter == reference_iter->second.end() || pad_iter->second.empty()) {
    return std::nullopt;
  }
  return *pad_iter->second.begin();
}

void add_mismatch(std::vector<ConnectivityMismatch>& mismatches,
                  const std::string& reference,
                  const std::string& pad,
                  const std::string& schematic_net,
                  const std::string& pcb_net) {
  const auto existing = std::find_if(
      mismatches.begin(), mismatches.end(),
      [&](const ConnectivityMismatch& mismatch) {
        return mismatch.reference == reference &&
            mismatch.pad == pad &&
            mismatch.schematic_net == schematic_net &&
            mismatch.pcb_net == pcb_net;
      });
  if (existing == mismatches.end()) {
    mismatches.push_back({reference, pad, schematic_net, pcb_net});
  }
}

}  // namespace

void Harness::connect(std::size_t left, std::size_t right) {
  if (left >= kHarnessPins || right >= kHarnessPins) {
    throw std::out_of_range("harness pin index");
  }
  if (left == right) {
    return;
  }
  auto& left_neighbors = neighbors_[left];
  auto& right_neighbors = neighbors_[right];
  if (std::find(left_neighbors.begin(), left_neighbors.end(), right) == left_neighbors.end()) {
    left_neighbors.push_back(right);
  }
  if (std::find(right_neighbors.begin(), right_neighbors.end(), left) == right_neighbors.end()) {
    right_neighbors.push_back(left);
  }
}

bool Harness::connected(std::size_t left, std::size_t right) const {
  if (left >= kHarnessPins || right >= kHarnessPins) {
    throw std::out_of_range("harness pin index");
  }
  const auto& left_neighbors = neighbors_[left];
  return std::find(left_neighbors.begin(), left_neighbors.end(), right) != left_neighbors.end();
}

const std::vector<std::size_t>& Harness::neighbors(std::size_t pin) const {
  if (pin >= kHarnessPins) {
    throw std::out_of_range("harness pin index");
  }
  return neighbors_[pin];
}

BoardModel BoardModel::load(const std::string& pcb_path,
                           const std::string& schematic_path) {
  const auto pcb = parse_pcb(pcb_path);
  const auto schematic = parse_schematic(schematic_path);

  BoardModel model;
  model.components_ = schematic.components;
  for (const auto& [reference, component] : pcb.components) {
    auto& merged = model.components_[reference];
    merged.reference = reference;
    merged.pcb_value = component.pcb_value;
    merged.pcb_footprint = component.pcb_footprint;
  }
  model.physical_nets_ = pcb.physical_nets;
  model.physical_graph_ = std::make_shared<RuntimePhysicalGraph>(build_physical_graph(pcb));
  for (const auto& pad : pcb.pads) {
    model.pads_.emplace(std::make_pair(pad.reference, pad.pad),
                        BoardPad{pad.reference, pad.pad, pad.net, pad.pinfunction});
  }
  model.physical_stats_.pads = pcb.pads.size();
  model.physical_stats_.tracks = pcb.segments.size();
  model.physical_stats_.vias = pcb.vias.size();
  for (const auto& zone : pcb.zones) {
    if (zone.keepout) {
      ++model.physical_stats_.keepouts;
    } else if (!zone.net.empty()) {
      ++model.physical_stats_.zones;
    }
  }

  std::map<std::string, PadRecord> j3_pads;
  std::vector<PadRecord> u4_pads;
  for (const auto& pad : pcb.pads) {
    if (pad.reference == "J3") {
      j3_pads[pad.pad] = pad;
    } else if (pad.reference == "U4") {
      u4_pads.push_back(pad);
    }
  }

  for (std::size_t index = 0; index < kHarnessPins; ++index) {
    const auto pad_name = std::to_string(index + 1);
    const auto pad_iter = j3_pads.find(pad_name);
    const auto intended_net = schematic_net_for_pin(schematic, "J3", pad_name);
    const std::string physical_net =
        pad_iter == j3_pads.end() ? std::string{} : pad_iter->second.net;

    if (intended_net && physical_net != *intended_net) {
      add_mismatch(model.connectivity_mismatches_, "J3", pad_name, *intended_net, physical_net);
    }

    std::optional<std::pair<std::size_t, std::size_t>> gport;
    const PadRecord* connected_pad = nullptr;
    if (!physical_net.empty()) {
      for (const auto& pad : u4_pads) {
        if (pad.net != physical_net) {
          continue;
        }
        const auto candidate_gport = parse_gport(pad.pinfunction);
        if (!candidate_gport) {
          continue;
        }
        if (!gport) {
          gport = candidate_gport;
        }
        if (pad_iter != j3_pads.end() &&
            model.pcb_connected(physical_net, "J3", pad_iter->second.pad, "U4", pad.pad)) {
          connected_pad = &pad;
          gport = candidate_gport;
          break;
        }
      }
    }

    if ((!gport || gport->first >= 8 || gport->second >= 8) && intended_net) {
      const auto u4_iter = schematic.pin_nets.find("U4");
      if (u4_iter != schematic.pin_nets.end()) {
        for (const auto& [u4_pad, nets] : u4_iter->second) {
          if (!nets.count(*intended_net)) {
            continue;
          }
          const auto* physical_u4_pad = find_pcb_pad(pcb.pads, "U4", u4_pad);
          if (!physical_u4_pad) {
            continue;
          }
          const auto candidate_gport = parse_gport(physical_u4_pad->pinfunction);
          if (candidate_gport && candidate_gport->first < 8 && candidate_gport->second < 8) {
            gport = candidate_gport;
            break;
          }
        }
      }
    }

    if (!gport || gport->first >= 8 || gport->second >= 8) {
      // A malformed or disconnected source should still leave the rest of the
      // board simulatable. The fallback bit is deliberately isolated unless a
      // physical path was found above.
      gport = std::make_pair(index / 8, index % 8);
      if (intended_net) {
        add_mismatch(
            model.connectivity_mismatches_, "J3", pad_name, *intended_net, physical_net);
      }
    }
    model.channels_[index] = {index, physical_net, gport->first, gport->second};
    if (connected_pad) {
      model.external_to_internal_[index].push_back(index);
    }
  }

  for (const auto& pad : pcb.pads) {
    if (pad.reference != "U2" || pad.net.empty()) {
      continue;
    }
    const auto pin = teensy_pin_number(pad.pinfunction);
    if (!pin || pad.net.rfind("unconnected-", 0) == 0) {
      continue;
    }
    const auto identifier = normalize_identifier(pad.net);
    if (!identifier.empty()) {
      model.io_pins_.emplace(identifier, *pin);
    }
  }

  // The simulator's runtime contract is the digital harness / MCU / expander
  // path plus the directly driven status LED. Keep mismatch evidence focused on
  // that path instead of treating passive-part orientation differences
  // elsewhere on the board as simulator defects.
  for (const auto& [reference, pin_map] : schematic.pin_nets) {
    if (reference != "J3" && reference != "U2" && reference != "U4" && reference != "D3") {
      continue;
    }
    for (const auto& [pad, nets] : pin_map) {
      if (nets.empty()) {
        continue;
      }
      std::string schematic_net = *nets.begin();
      if (nets.size() > 1) {
        for (auto iter = std::next(nets.begin()); iter != nets.end(); ++iter) {
          add_mismatch(model.connectivity_mismatches_,
                       reference,
                       pad,
                       schematic_net + " / " + *iter,
                       {});
        }
      }
      const auto* pcb_pad = find_pcb_pad(pcb.pads, reference, pad);
      const std::string pcb_net = pcb_pad ? pcb_pad->net : std::string{};
      if (pcb_net != schematic_net) {
        add_mismatch(model.connectivity_mismatches_, reference, pad, schematic_net, pcb_net);
      }
    }
  }

  auto report_missing_physical_pair = [&](const std::string& left_reference,
                                          const std::string& left_pad,
                                          const std::string& right_reference,
                                          const std::string& right_pad,
                                          const std::string& expected_net) {
    const auto* left_pcb_pad = find_pcb_pad(pcb.pads, left_reference, left_pad);
    const auto* right_pcb_pad = find_pcb_pad(pcb.pads, right_reference, right_pad);
    const bool physically_connected =
        left_pcb_pad &&
        right_pcb_pad &&
        left_pcb_pad->net == expected_net &&
        right_pcb_pad->net == expected_net &&
        model.pcb_connected(
            expected_net, left_reference, left_pad, right_reference, right_pad);
    if (physically_connected) {
      return;
    }
    add_mismatch(model.connectivity_mismatches_,
                 left_reference,
                 left_pad,
                 expected_net,
                 left_pcb_pad ? left_pcb_pad->net : std::string{});
    add_mismatch(model.connectivity_mismatches_,
                 right_reference,
                 right_pad,
                 expected_net,
                 right_pcb_pad ? right_pcb_pad->net : std::string{});
  };

  std::map<std::string, std::vector<std::pair<std::string, std::string>>> expected_pads;
  for (const auto& [reference, pin_map] : schematic.pin_nets) {
    if (reference != "J3" && reference != "U2" && reference != "U4" && reference != "D3") {
      continue;
    }
    for (const auto& [pad, nets] : pin_map) {
      if (!find_pcb_pad(pcb.pads, reference, pad)) {
        continue;
      }
      for (const auto& expected_net : nets) {
        expected_pads[expected_net].push_back({reference, pad});
      }
    }
  }
  for (const auto& [expected_net, pads] : expected_pads) {
    for (std::size_t left = 0; left < pads.size(); ++left) {
      for (std::size_t right = left + 1; right < pads.size(); ++right) {
        report_missing_physical_pair(
            pads[left].first,
            pads[left].second,
            pads[right].first,
            pads[right].second,
            expected_net);
      }
    }
  }

  auto report_runtime_pair_candidates = [&](const std::string& reference) {
    const auto reference_iter = schematic.pin_nets.find(reference);
    if (reference_iter == schematic.pin_nets.end()) {
      return;
    }
    for (const auto& [pad, nets] : reference_iter->second) {
      for (const auto& expected_net : nets) {
        for (const auto& u4_pad : u4_pads) {
          if (u4_pad.net == expected_net) {
            report_missing_physical_pair(reference, pad, "U4", u4_pad.pad, expected_net);
          }
        }
      }
    }
  };
  report_runtime_pair_candidates("J3");
  report_runtime_pair_candidates("U2");

  return model;
}

const HarnessChannel& BoardModel::channel(std::size_t harness_index) const {
  if (harness_index >= kHarnessPins) {
    throw std::out_of_range("harness index");
  }
  return channels_[harness_index];
}

const BoardPad& BoardModel::pad(const std::string& reference, const std::string& pad) const {
  const auto iter = pads_.find(std::make_pair(reference, pad));
  if (iter == pads_.end()) {
    throw std::out_of_range("board pad");
  }
  return iter->second;
}

const BoardComponent& BoardModel::component(const std::string& reference) const {
  const auto iter = components_.find(reference);
  if (iter == components_.end()) {
    throw std::out_of_range("board component");
  }
  return iter->second;
}

std::size_t BoardModel::arduino_pin(const std::string& name) const {
  const auto iter = io_pins_.find(name);
  if (iter == io_pins_.end()) {
    throw std::runtime_error("unknown board IO name: " + name);
  }
  return iter->second;
}

const std::map<std::string, std::size_t>& BoardModel::io_pins() const {
  return io_pins_;
}

const std::map<std::string, PhysicalNet>& BoardModel::physical_nets() const {
  return physical_nets_;
}

const std::vector<ConnectivityMismatch>& BoardModel::connectivity_mismatches() const {
  return connectivity_mismatches_;
}

const PhysicalConnectivityStats& BoardModel::physical_stats() const {
  return physical_stats_;
}

const std::vector<ConnectivityMismatch>& BoardModel::schematic_mismatches() const {
  return connectivity_mismatches_;
}

bool BoardModel::pcb_connected(const std::string& net,
                               const std::string& left_reference,
                               const std::string& left_pad,
                               const std::string& right_reference,
                               const std::string& right_pad) const {
  const auto graph = std::static_pointer_cast<RuntimePhysicalGraph>(physical_graph_);
  if (!graph) {
    return false;
  }
  const auto left = feature_index(*graph, left_reference, left_pad);
  const auto right = feature_index(*graph, right_reference, right_pad);
  if (!left || !right) {
    return false;
  }
  if (graph->features[*left].net != net || graph->features[*right].net != net) {
    return false;
  }
  return graph->components[*left] == graph->components[*right];
}

bool BoardModel::has_copper_at(const std::string& net,
                               const std::string& layer,
                               double x,
                               double y) const {
  const auto graph = std::static_pointer_cast<RuntimePhysicalGraph>(physical_graph_);
  if (!graph) {
    return false;
  }
  const Point point{scaled_coordinate(std::to_string(x)), scaled_coordinate(std::to_string(y))};
  const std::set<std::string> requested_layers{layer};
  // Keepouts in KiCad can prohibit a copper pour while still allowing an
  // explicit track, via, or pad through the same region. Check those routed
  // features first, then use the keepout only when deciding whether a zone
  // contributes copper at this point.
  for (const auto& feature : graph->features) {
    if (feature.kind == PhysicalFeature::Kind::Zone ||
        feature.net != net || !same_layer(feature.layers, requested_layers)) {
      continue;
    }
    const auto threshold = static_cast<long double>(feature.radius) * feature.radius;
    if (point_to_segment_squared_distance(point, feature.start, feature.end) <= threshold) {
      return true;
    }
  }
  for (const auto& keepout : graph->keepouts) {
    if (!same_layer(keepout.layers, requested_layers)) {
      continue;
    }
    const auto& polygons = keepout.filled_polygons.empty() ?
        keepout.polygons : keepout.filled_polygons;
    for (const auto& polygon : polygons) {
      if (point_in_polygon(point, polygon)) {
        return false;
      }
    }
  }
  for (const auto& feature : graph->features) {
    if (feature.kind != PhysicalFeature::Kind::Zone ||
        feature.net != net || !same_layer(feature.layers, requested_layers)) {
      continue;
    }
    for (const auto& polygon : feature.polygons) {
      if (point_in_polygon(point, polygon)) {
        return true;
      }
    }
  }
  return false;
}

std::uint64_t BoardModel::resolve_inputs(
    const Harness& harness,
    const std::array<ExpanderPinDrive, kExpanderPins>& drives) const {
  constexpr std::size_t external_offset = kHarnessPins;
  constexpr std::size_t node_count = kHarnessPins * 2;
  std::array<std::vector<std::size_t>, node_count> edges;

  for (std::size_t external = 0; external < kHarnessPins; ++external) {
    for (const auto internal : external_to_internal_[external]) {
      edges[internal].push_back(external_offset + external);
      edges[external_offset + external].push_back(internal);
    }
    for (const auto peer : harness.neighbors(external)) {
      edges[external_offset + external].push_back(external_offset + peer);
    }
  }

  std::array<bool, node_count> visited{};
  std::array<bool, kExpanderPins> level{};
  for (std::size_t start = 0; start < kHarnessPins; ++start) {
    if (visited[start]) {
      continue;
    }

    std::vector<std::size_t> pending{start};
    std::vector<std::size_t> component;
    visited[start] = true;
    while (!pending.empty()) {
      const auto node = pending.back();
      pending.pop_back();
      component.push_back(node);
      for (const auto next : edges[node]) {
        if (!visited[next]) {
          visited[next] = true;
          pending.push_back(next);
        }
      }
    }

    bool high = false;
    for (const auto node : component) {
      if (node >= kHarnessPins) {
        continue;
      }
      const auto& channel = channels_[node];
      const auto& drive = drives[channel.expander_port * 8 + channel.expander_bit];
      high = high || drives_high(drive);
    }
    const bool resolved = high;
    for (const auto node : component) {
      if (node < kHarnessPins) {
        const auto& channel = channels_[node];
        level[channel.expander_port * 8 + channel.expander_bit] = resolved;
      }
    }
  }

  std::uint64_t result = 0;
  for (std::size_t index = 0; index < kExpanderPins; ++index) {
    if (level[index]) {
      result |= (std::uint64_t{1} << index);
    }
  }
  return result;
}

}  // namespace host_sim
