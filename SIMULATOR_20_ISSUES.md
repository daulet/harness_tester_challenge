# Twenty Simulator-Backed Issues

Review date: 2026-06-05

This report selects exactly 20 product issues from the current simulator evidence.
Every selected witness passes in the current build. Each issue was independently
critiqued by Claude Opus at `xhigh` effort and checked against the authoritative
firmware or KiCad source. The wording below deliberately separates direct proof
from electrical or hardware consequence inference.

## What the simulator can test now

- Firmware setup, loop control flow, GPIO modes, serial input, NMEA parsing,
  result logging, and status transitions.
- CY8C9560 I2C transactions, register state, drive direction, and logical-to-raw
  harness-channel mapping.
- Harness opens, shorts, intermittent contacts, leakage, contact resistance,
  and representative R/L/C loading.
- KiCad schematic/PCB facts: pad nets, pin functions, component values,
  schematic-to-PCB parity, routed copper continuity, vias, and filled-zone
  connectivity.
- Representative ngspice checks for rails, reverse polarity, transient clamps,
  LED current, UART/I2C levels, and harness electrical faults.

It does not prove RF performance, antenna behavior, EMI, exact silicon behavior,
optical brightness, manufacturing yield, or production readiness.

## Selected issues

### 1. A01 - Expander initialization is never called

- **Proof:** `firmware/firmware.ino` never calls `CY8C9560::begin()`. The runtime
  observes neither `Wire2.begin()` nor expander access in
  `bug_a01_missing_begin`.
- **Claude critique:** ACCEPT. The witness fails against the isolated variant
  that adds `cy.begin()`, so it is discriminating.
- **Boundary:** reset pulsing and ID-read omission follow from source; the test
  directly observes only bus initialization and access.

### 2. A02 - GPS SAFEBOOT and reset pins are not outputs

- **Proof:** setup writes pins 3 and 4 but never calls `pinMode(..., OUTPUT)`;
  `bug_a02_missing_output_modes` leaves both modeled pins in input mode.
- **Claude critique:** ACCEPT.
- **Boundary:** the test proves the modes. Exact Teensy pull behavior and GPS
  pin voltage are hardware inference.

### 3. A03 - NMEA staging reaches the buffer boundary unguarded

- **Proof:** `nmea_buf[64]` accepts 64 bytes and leaves `nmea_idx == 64` in
  `bug_a03_nmea_buffer_reaches_end_without_guard`; both the next byte write and
  `buf[len] = 0` can address index 64.
- **Claude critique:** ACCEPT with narrowed wording.
- **Boundary:** the runtime proves the overflow precondition, not an ASan-caught
  out-of-bounds write.

### 4. A04 - Invalid RMC status is accepted as a time fix

- **Proof:** the `%*c` conversion discards the RMC A/V status. An injected
  `$GPRMC` sentence with status `V` sets `time_fixed` in
  `bug_a04_invalid_status_accepted`.
- **Claude critique:** ACCEPT; the same behavior reproduces on the original and
  isolated firmware builds.

### 5. A05 - Forty-pin masks use narrow signed shifts

- **Proof:** the source uses `1 << i` and `1 << j` while iterating 40 channels;
  `scenario_a05_narrow_masks` locks the buggy expressions and the host simulator
  corroborates high-pin wrap behavior.
- **Claude critique:** ACCEPT.
- **Boundary:** shifts at and above the signed `int` width are undefined, so the
  portable proof is the C++ source rule, not a fixed runtime value.

### 6. A07 - Pull-down setup immediately undoes the selected output

- **Proof:** `set_pd_inputs()` writes direction `0xFF` to every port immediately
  after `set_output()`. `bug_a07_selected_output_undone` observes all ports back
  in input mode.
- **Claude critique:** ACCEPT; this test is discriminating against the modeled
  reset state.

### 7. A08 - One matching row passes a broken harness

- **Proof:** `passed` becomes true on any exact row and is never cleared on later
  mismatches. `bug_a08_one_matching_row_passes` supplies one matching row and
  many failures, yet logs a pass.
- **Claude critique:** ACCEPT. The isolation variant removes upstream blockers
  while preserving this logic verbatim.

### 8. A09 - FAILED status is overwritten on the next loop

- **Proof:** the loop sets GOOD before the button gate and before a new test.
  `bug_a09_failed_status_overwritten` observes FAILED followed by GOOD without a
  replacement test result.
- **Claude critique:** ACCEPT.
- **Boundary:** pin-state ordering is proven; visible LED behavior is additionally
  masked by A21 and is not the claim here.

### 9. A10 - UART transmit and receive endpoints are not crossed

- **Proof:** MCU TX1 shares `UBX-TXD` with GPS TXD, while MCU RX1 shares
  `UBX-RXD` with GPS RXD. `scenario_a10_uart_same_direction` verifies the pad,
  net, and pin-function pairing.
- **Claude critique:** ACCEPT.
- **Boundary:** driver contention and absent receive data are electrical
  consequences; the hard proof is the same-direction topology.

### 10. A11 - CY_SDA is pulled down to ground

- **Proof:** R3 is 4.7 kOhm between `CY_SDA` and GND, while the adjacent SCL
  resistor is correctly tied to +3.3 V. `scenario_a11_sda_pull_down` verifies
  the bad SDA topology.
- **Claude critique:** ACCEPT.
- **Boundary:** the scenario test proves topology; exact bus voltage is not
  derived from the board netlist by ngspice.

### 11. A12 - RGB LED channels have no series resistors

- **Proof:** every LED cathode net has exactly D3 and U2 as references, with no
  resistor splitting the net. `scenario_a12_led_no_series_resistors` verifies
  all three channels.
- **Claude critique:** ACCEPT.
- **Boundary:** the missing components are proven. The overcurrent fixture is
  representative, not an exact board-current calculation. A16 currently masks
  this defect by disconnecting the anode.

### 12. A16 - The RGB LED anode is on an isolated copper island

- **Proof:** `scenario_a16_d3_anode_isolated` positively connects the main +3.3 V
  component through U2/U4, then shows D3 pad 1 belongs to a separate physical
  component.
- **Claude critique:** ACCEPT. Claude independently repeated the In1.Cu
  point-in-polygon analysis and found the same two islands.

### 13. A17 - The test-button polarity is inverted

- **Proof:** the board is +3.3 V -> 10 kOhm pull-up -> BTN_TEST -> switch -> GND,
  so pressed is LOW. Firmware returns on LOW. `bug_a17_pressed_button_does_not_run`
  confirms a press does not test.
- **Claude critique:** ACCEPT; the runtime polarity matches the KiCad source.

### 14. A18 - MAX2679 Vcc is overvolted through GPS VCC_RF

- **Proof:** `scenario_a18_lna_vcc_rf` verifies U5 Vcc is fed by U3 VCC_RF while
  U3 VCC is +3.3 V. Published limits put MAX2679 operation at 1.08-1.98 V and
  absolute maximum at 2.2 V, while NEO-M8 VCC_RF is approximately VCC - 0.1 V.
- **Claude critique:** ACCEPT with the topology/specification boundary.
- **Boundary:** the simulator proves the supply topology, not damage or RF
  performance.

### 15. A19 - Default multi-GNSS `$GNRMC` is ignored

- **Proof:** parsing is hard-coded to literal `$GPRMC`; `bug_a19_gnrmc_ignored`
  injects `$GNRMC` and observes no time fix.
- **Claude critique:** ACCEPT.
- **Boundary:** the system-level impact depends on NEO-M8 configuration; factory
  multi-GNSS configuration triggers the issue, while GPS-only configuration can
  emit `$GPRMC`.

### 16. A20 - Logical harness numbering diverges after CBL_19

- **Proof:** firmware treats logical indices as contiguous raw expander bits,
  but CBL_20 is port 3 bit 0 (raw bit 24), not raw bit 20.
  `bug_a20_logical_index_gap` and `pcb_source_parity` jointly establish the
  behavior and source mapping.
- **Claude critique:** ACCEPT.

### 17. A21 - RGB LED GPIOs are never configured as outputs

- **Proof:** `set_status()` only uses `digitalWrite()`; setup never configures
  pins 5, 6, or 7 as outputs. `bug_a21_leds_never_become_outputs` confirms all
  three remain inputs.
- **Claude critique:** ACCEPT with the mode-only boundary.
- **Boundary:** active cathode drive is absent; exact leakage and brightness are
  not modeled.

### 18. A23 - A persistent gate repeatedly tests and appends logs

- **Proof:** the gate is level-sensitive and has no edge, latch, or debounce
  state. `bug_a23_held_gate_relogs` executes two loops and observes two appended
  result records.
- **Claude critique:** ACCEPT.
- **Boundary:** because A17 is inverted, the current witness holds the released
  HIGH gate. The duplicate-run defect remains after correcting polarity.

### 19. A25 - Reverse input forward-biases the TVS before Q1 isolation

- **Proof:** D1 is a unidirectional SMAJ16A with cathode on raw input and anode
  on GND, before Q1. A negative raw input forward-biases D1. The KiCad scenario
  and ngspice orientation agree.
- **Claude critique:** ACCEPT after checking D1 and Q1 polarity.
- **Boundary:** `analog_reverse_polarity` checks protected rails but does not
  assert TVS/source current or dissipation.

### 20. A29 - U4 has a 14 mm part on a 12 mm footprint

- **Proof:** the selected part is `CY8C9560A-24AXIT`; the assigned footprint is
  `TQFP-100_12x12mm_P0.4mm`. Infineon's ordering/package data identifies the
  part as the 14 x 14 mm 100-pin TQFP.
- **Claude critique:** ACCEPT with the source/specification boundary.
- **Boundary:** the scenario test proves the selected pairing; the official
  package drawing supplies the mismatch premise.

## Withheld twenty-first product defect

**A06 - `set_output()` writes all ports to output** is a real source defect, but
it is not counted in the 20 above because its current witness is invalid as a
causal test. The runtime initializes every expander direction register to
`0x00`, so `bug_a06_all_outputs` would pass even if `set_output()` never wrote a
direction register. The real CY8C9560 power-on direction is input/high-impedance.

This is both a simulator reset-model bug and a non-discriminating regression
test. Fix the modeled reset value to `0xFF`, assert the pre-call state, and then
retain A06 as a fully simulator-backed twenty-first issue.

## Other simulator defects exposed by review

- `scenario_a25_reverse_polarity_path` reverses the words anode and cathode in
  its diagnostics. Its net assertions and ngspice diode orientation are correct.
- A03 executes only the buffer-boundary precondition; add ASan/UBSan coverage or
  a guarded memory witness before describing the overflow as dynamically caught.
- A05 is a source assertion with host corroboration, not a portable runtime
  regression because the bad shifts invoke undefined behavior.
- A29's schematic value search is not bounded to the U4 symbol and should be
  made structurally scoped.

## Verification

```text
ctest --test-dir build -R '^(bug_|scenario_)' --output-on-failure
21/21 candidate witnesses passed

Full suite previously verified: 41/41 passed
```
