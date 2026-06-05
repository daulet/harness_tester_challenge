# Simulator inventory

## Source owners

- Firmware subject: `firmware/firmware.ino`, `firmware/CY8C9560.*`.
- Isolation variant: `simulator/variants/firmware_unmasked.ino`.
- Runtime/peripherals: `simulator/src/runtime.cpp` and Arduino/SD/Wire shims.
- Board source parser and connectivity: `simulator/src/board.cpp`.
- Electrical process boundary: `simulator/src/analog.cpp`.
- Electrical circuit/models/fixtures: `simulator/ngspice/`.
- Authoritative design inputs: `kicad_files/*.kicad_sch` and `*.kicad_pcb`.

## Build and test targets

- Library: `host_simulator`.
- Firmware executables: original and unmasked normal/bug-case binaries.
- Board/runtime/scenario/analog test binaries.
- Current CTest surface: board parity, runtime defaults, source scenarios,
  analog fixtures, firmware behavior, and explicit bug witnesses.

## Public simulator APIs

- `BoardModel`: KiCad loading, scoped schematic/PCB component metadata, pad/net
  lookup, physical connectivity, harness mapping, digital input resolution.
- `Runtime`: GPIO, serial, SD, I2C/expander state, elapsed time, and analog
  stimulus export.
- `NgSpiceSimulator`: strict fixture load, process execution, named measurement
  parsing.

## Current duplicated truth

- `board_harness.cir` hard-codes representative board topology.
- Fixture files duplicate pulls, LED resistance, rail loads, and one harness
  channel rather than deriving them from KiCad/component data.
- `analog_stimulus()` assumes released I2C and nominal UART voltage instead of
  exporting runtime line state.
- `resolve_inputs()` performs boolean high-dominant resolution without voltage,
  current, low-driver dominance, or contention.

## External boundaries

- ngspice is installed and exercised locally.
- clang-format is installed; clang-tidy is absent.
- `kicad-cli` is absent at bootstrap.
- No Teensy toolchain, vendor SPICE model cache, hardware trace corpus, or HIL
  fixture is currently wired into CMake/CTest.
