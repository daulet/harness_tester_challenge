# Simulator Scenario Review

Working review artifact as of 2026-06-04.

This is a second-pass scenario review, not a replacement for
`SIMULATOR_BUG_EVIDENCE.md`. "Novel" here means absent from that 11-item
simulator ledger. It does not mean absent from the broader
`../../../harness_tester_challenge/BUG_THEORY_LOG.md`.

The host simulator exercises parser, control-flow, probe-loop, and board-map
behavior through the C++ runtime. It now also has a first-pass ngspice layer for
power rails, protection, LED current, UART/I2C levels, and representative
harness electrical faults. RF, antenna, distributed impedance/return-path, EMI,
and production physical validation remain out of scope. The findings in this
review still use source-level checks where that is the authoritative boundary.

This pass also corrected the simulator's CY8C reset model to follow the
datasheet: XRES is active high, despite the board net being named `CY_RST_N`.

## Scenario coverage

| Scenario | Current pass result | Main checks |
| --- | --- | --- |
| Startup and expander bring-up | Existing A01/A02 remain proven; this pass adds the A03 buffer-boundary witness. | `bug_a01_*`, `bug_a02_*`, `bug_a03_*` |
| GPS parser and time-lock gate | Existing A04/A19 remain proven; this pass confirms the unbounded NMEA staging path. | `bug_a03_*`, `bug_a04_*`, `bug_a19_*` |
| Harness probe loop | Existing A06/A07/A20 remain proven; this pass adds A05 and A08 as additional probe-loop defects. | `scenario_a05_*`, `bug_a06_*`, `bug_a07_*`, `bug_a08_*`, `bug_a20_*` |
| Button, status, and logging | Existing A09/A17/A21/A23 remain proven. | `bug_a09_*`, `bug_a17_*`, `bug_a21_*`, `bug_a23_*` |
| UART and I2C physical wiring | Source-level scenario checks add A10 and A11. | `scenario_a10_*`, `scenario_a11_*` |
| LED implementation | Source-level scenario checks add A12 and A16. | `scenario_a12_*`, `scenario_a16_*` |
| Power and protection | Source-level scenario checks add A18 and A25. | `scenario_a18_*`, `scenario_a25_*` |
| Package correctness | Source-level scenario check adds A29. | `scenario_a29_*` |

## Ten additional findings

| ID | Bug | Evidence boundary | Current witness |
| --- | --- | --- | --- |
| A03 | `nmea_buf[64]` can overflow before a newline arrives | Runtime plus source | `bug_a03_nmea_buffer_reaches_end_without_guard` leaves `nmea_idx == 64`; the next byte writes past the array, and `process_nmea()` also writes `buf[len]`. |
| A05 | `1 << i` and `1 << j` are too narrow for a 40-pin mask | Source plus current-host corroboration | `scenario_a05_narrow_masks` verifies the source expressions; on this host, `./build/harness_simulator_original | tail -20` shows pin 32 wrapping back to the low bits. |
| A08 | One matching probe row is enough to pass the whole harness | Runtime plus source | `bug_a08_one_matching_row_passes` uses a harness with one exact row 13 match and later mismatches, yet the firmware prints `Harness passed!`. |
| A10 | UART TX is wired to TX and RX to RX instead of crossed | Source-level scenario check | `scenario_a10_uart_same_direction` verifies that MCU `RX1` and GPS `RXD` share `UBX-RXD`, while MCU `TX1` and GPS `TXD` share `UBX-TXD`. |
| A11 | `CY_SDA` is pulled down instead of up | Source-level scenario check | `scenario_a11_sda_pull_down` verifies that R3 ties `CY_SDA` to GND. |
| A12 | RGB LED channels have no current-limiting resistors | Source-level scenario check | `scenario_a12_led_no_series_resistors` verifies that each LED net has only D3 and U2 as references. |
| A16 | D3's +3.3V anode lands on an isolated copper island | Source-level scenario check | `scenario_a16_d3_anode_isolated` verifies that the main +3.3V path reaches U4 but not D3 pad 1. |
| A18 | MAX2679 Vcc is overvolted through `VCC_RF` | Source-level scenario check | `scenario_a18_lna_vcc_rf` verifies that U5 Vcc is fed from U3 `VCC_RF`, while U3 VCC is fed from +3.3V. |
| A25 | Reverse-polarity input forward-biases D1 before Q1 can isolate it | Source-level scenario check | `scenario_a25_reverse_polarity_path` verifies that J1 raw input, D1's input-side pad, and Q1 drain share one net before the protected +12V rail. |
| A29 | U4 uses the wrong CY8C9560A package footprint | Source-level scenario check plus datasheet review | `scenario_a29_u4_wrong_footprint` verifies the selected `CY8C9560A-24AXIT` part and 12 mm TQFP footprint pairing; the Infineon package documentation identifies the part as 14 mm TQFP. |

## Newly added runtime witnesses

```sh
cmake --build build -j4
ctest --test-dir build -R '^(scenario_|bug_a03|bug_a08)' --output-on-failure
```

The new test cases are intentionally narrow:

- `bug_a03_nmea_buffer_reaches_end_without_guard` proves the parser can fill
  all 64 bytes without a boundary check.
- `bug_a08_one_matching_row_passes` proves the pass flag is sticky after a
  single exact row match. It uses the existing unmasked variant only to expose
  that preserved downstream behavior after the already-known init/probe
  blockers are removed.
- `scenario_a10_*` through `scenario_a29_*` prove the source topology or
  package premises for the board-level findings without claiming analog or RF
  behavior that the runtime does not model.

For A05, the source expression is the reliable proof. The current host's
wrapped output is useful corroboration, but it is not a portable regression
assertion because shifting an `int` by 32 or more is undefined behavior.

The complete current suite, including the ngspice matrix, passes 41/41 tests.

One earlier candidate, A24, was removed from this review after rechecking the
rotated D3 symbol. The actual schematic and PCB both map `LED_B` to D3 pin 2
and `LED_R` to D3 pin 4, so there is no supported red/blue swap claim here.
