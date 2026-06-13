# Blind Reconstruction Report

## Result

The blind ledger was frozen at 35 theories before reading `FINAL_FINDINGS.md`
or `BUG_THEORY_LOG.md`. Freeze proof:

- Frozen ledger: `FROZEN_LEDGER.md`
- Freeze record: `FREEZE_STATUS.txt`
- Frozen SHA-256:
  `11e59d08fc8009436e5bcf80adc67f1c433303627839b0d042a0d1b304d2acb7`

Final strict counts:

| Verdict | Count |
|---|---:|
| ACCEPT | 13 |
| MERGE | 5 |
| REJECT | 17 |
| DISPUTED | 0 |
| Total | 35 |

`ACCEPT` is reserved for an independent, source-proven show-stopper in the
as-written/as-drawn design. Generic hardening, optional configurations,
conditional transients, and speculative RF/thermal performance are excluded.

## Accepted Independent Roots

| Root | Source proof | Strongest disproof and adjudication | Confidence |
|---|---|---|---|
| P01: SDA pull resistor goes to GND | `kicad_files/hardware_challenge.kicad_pcb:6877-7072` | No alternative pull-up exists; open-drain SDA cannot idle high. Claude: `SURVIVES`. | High |
| P02: GPS UART is RX-RX/TX-TX | PCB `:7687-7705`, `:11072-11092` | Net-number trace confirms two receivers on one net and two transmitters on the other. Claude: survives. | High |
| P03: MAX2679 supply exceeds its limits | PCB `:6433-6468`, `:10967`; ADI MAX2679 and u-blox NEO-M8 electrical tables | Hostile review objected that the net is named `VCC_RF`, not `+3.3V`. That does not rescue it: NEO-M8 `VCC_RF` is approximately module VCC, while MAX2679 permits only 1.08-1.98 V operation and 2.2 V absolute maximum. | High |
| P05: reverse input forward-biases D1 across the source | PCB `:1173-1439`, `:1697-1733`; README goal at `README.md:24-26` | Q1 still isolates the downstream rail, but D1 creates a direct reverse-input fault path before Q1. That independently violates the stated reverse-polarity-protection goal. | Medium-high |
| P07: harness index no longer equals packed CY8 bit after CBL_19 | PCB `:9189-9418`; `firmware/CY8C9560.cpp:25-34`; `firmware/firmware.ino:143-155` | Physical port gaps make CBL_20 packed bit 24 and CBL_36 packed bit 40. No mapping layer exists. | High |
| P08: `CY8C9560::begin()` is never called | `firmware/firmware.ino:63,97-116`; `firmware/CY8C9560.cpp:3-15` | Reset may be optional, but `Wire2.begin()` is not called anywhere else. Claude: `SURVIVES`. | High |
| P09: `set_pd_inputs()` disables the selected stimulus | `firmware/firmware.ino:144-148`; `firmware/CY8C9560.cpp:61-66` | Direction `0xFF` is written to every port immediately before measurement. | High |
| P10: 40-pin masks use 32-bit `int` shifts | `firmware/firmware.ino:143-152` | Conversion to `uint64_t` occurs after `1 << i`; shifts reaching 31-39 are already invalid/wrong. | High |
| P11: any one matching row makes the whole harness pass | `firmware/firmware.ino:142,155-161` | `passed` is never cleared on mismatch, implementing OR-of-rows. | High |
| P12: button polarity is inverted | PCB `:8425-8619`; `firmware/firmware.ino:137-139` | Pull-up plus switch-to-GND means pressed is LOW, but LOW returns without testing. | High |
| P13: RGB LED pins remain inputs | `firmware/firmware.ino:67-71,102-108` | No `pinMode(..., OUTPUT)` exists for pins 5-7; writes only affect input-mode state. | High |
| P14: parser accepts only `$GPRMC`, not default `$GNRMC` | `firmware/firmware.ino:73-83`; NEO-M8 default concurrent-GNSS behavior | A non-default GPS-only configuration could emit GP, but firmware contains no reconfiguration path; default design never latches time. | High |
| P15: ordinary NMEA input can exceed the 64-byte buffer | `firmware/firmware.ino:73-76,118-126` | Input is written before any bound check and `buf[len] = 0` adds the terminal boundary write. | High |

## Frozen 35-Theory Verdict Ledger

| # | ID | Final | Source location | Strongest disproof / dedupe |
|---:|---|---|---|---|
| 1 | T01/P01 | ACCEPT | PCB `:6877-7072` | No viable SDA-high source. |
| 2 | T02/P02 | ACCEPT | PCB `:7687-7705,11072-11092` | Net-number trace confirms same-direction UART. |
| 3 | T03 | REJECT | PCB U3 pads 22-23 | `V_BCKP` is on `+3.3V`, not GND. |
| 4 | T04/P03 | ACCEPT | PCB `:6433-6468,10967` | `VCC_RF` naming does not change its over-limit voltage. |
| 5 | T05/P04 | REJECT | PCB `:6433-6468,10168-10186,1951-1967` | ADI page 7 explicitly says floating `RFOUT/SHDNB` enables active mode. |
| 6 | T06 | REJECT | PCB `:2446,6452,13603` | Real datasheet departure, but RF degradation is not a deterministic source-only show-stopper under this audit rule. |
| 7 | T07/P05 | ACCEPT | PCB `:1173-1439,1697-1733` | Q1 protects the load, but D1 still faults the reversed source before Q1. |
| 8 | T08 | REJECT | PCB Q1 pads `:1697-1733` | P-channel drain/source orientation is intentional. |
| 9 | T09 | REJECT | Schematic D2/Q1 | Requires an unspecified positive transient; not a default deterministic failure. |
| 10 | T10 | REJECT | Schematic U1 capacitors | Generic regulator-stability concern without a mandatory failure witness. |
| 11 | T11/P06 | MERGE | PCB `:20677-20705` | Same keepout and correction as submitted RF-feed-reference item 24; total lock failure is not guaranteed. |
| 12 | T12 | MERGE | PCB antenna feed/keepout | Same physical keepout root as T11. |
| 13 | T13 | REJECT | PCB J3/U4 route graph | All 40 J3-to-U4 nets are structurally routed. |
| 14 | T14 | REJECT | PCB segment scan | No different-net same-layer centerline crossings found. |
| 15 | T15 | REJECT | PCB J3/board edge | No source-proven mating obstruction. |
| 16 | T16/P07 | ACCEPT | PCB `:9189-9418`; firmware `:143-155` | Port gaps prove index divergence. |
| 17 | T17/P08 | ACCEPT | firmware `:63,97-116`; driver `:3-15` | No other `cy.begin()` or `Wire2.begin()`. |
| 18 | T18 | REJECT | driver `:3-9` | CY8 XRES is active high; high-then-low releases reset. |
| 19 | T19 | REJECT | driver `:53-55` | Device-family ID high nibble is `0x6`. |
| 20 | T20 | REJECT | header `:7`; PCB U2 pads 16-17 | `Wire2` matches Teensy pins 24/25. |
| 21 | T21/P09 | ACCEPT | firmware `:144-148`; driver `:61-66` | Selected output is changed back to input. |
| 22 | T22 | MERGE | driver `:77-83` | Same selected-pin/direction-mask implementation family as T21; submitted items 6 and 7 should be counted as one root cluster. |
| 23 | T23/P10 | ACCEPT | firmware `:143-152` | Narrow shift occurs before widening. |
| 24 | T24/P11 | ACCEPT | firmware `:142,155-161` | Any match permanently sets pass. |
| 25 | T25/P12 | ACCEPT | PCB `:8425-8619`; firmware `:137-139` | Pressed LOW exits; released HIGH tests. |
| 26 | T26 | MERGE | firmware `:137-163` | Held-trigger repetition is secondary to button handling and is not a show-stopper. |
| 27 | T27/P13 | ACCEPT | firmware `:67-71,102-108` | LED pins never become outputs. |
| 28 | T28 | REJECT | D3 common-anode topology | Inverted GPIO values are correct for common anode. |
| 29 | T29/P14 | ACCEPT | firmware `:73-83` | Default multi-GNSS talker is GN; no firmware reconfiguration exists. |
| 30 | T30/P15 | ACCEPT | firmware `:118-126` | Ordinary RMC length can cross the unguarded boundary. |
| 31 | T31 | MERGE | firmware `:73-76,118-126` | Terminal write is the same bounds root as T30. |
| 32 | T32 | REJECT | firmware `:75-80` | Validity omission is real, but not a default show-stopper and is masked by T29/T30. |
| 33 | T33 | REJECT | firmware `:16,109-113` | `BUILTIN_SDCARD` is the Teensy 4.1 onboard selector. |
| 34 | T34 | REJECT | firmware `:20-61` | Mechanical extraction found a symmetric matrix. |
| 35 | T35 | REJECT | firmware `:103-107`; NEO-M8 pin behavior | RESET_N has an internal pull-up and SAFEBOOT_N may remain open for normal startup. |

## P04 Dedicated Council

Final verdict: **REJECT**.

Exact topology is preserved in
`p04_council/TOPOLOGY_AND_PRIMARY_SOURCE.md`. U5 A2 connects through L1 and
series C5 to U3 RF_IN, so it is floating at DC and has no explicit 25 kohm
bias resistor.

The decisive primary-source contradiction is the official ADI datasheet:

https://www.analog.com/media/en/technical-documentation/data-sheets/MAX2679-MAX2679B.pdf

Page 6 describes high and low control through 25 kohm. Page 7's detailed
shutdown section explicitly states that active mode is enabled by **floating
the RFOUT bump** or by applying VIH through 25 kohm. The board implements the
floating alternative, so the claimed disabled state is false.

This theory would have been electrically independent from A1/VCC overvoltage
and B1/RFIN matching, but independence cannot rescue a false premise.

Dedicated review artifacts:

- `p04_council/review_A.prompt.md`
- `p04_council/review_A.output.txt`
- `p04_council/review_A.status.txt`
- `p04_council/review_B.prompt.md`
- `p04_council/review_B.output.txt`
- `p04_council/review_B.status.txt`

Both reviews were instructed to disprove P04 and reconcile the page-6/page-7
language. Review A returned `REJECT`. Review B ran for approximately 13 minutes
and returned only Claude's literal `Execution error`; its prompt, output, and
status are preserved, but it is not counted as a substantive vote.

## Comparison With Submitted Findings

### Genuinely New Independent Roots

**None.** After rejecting P04 and merging P06 into submitted item 24, every
surviving blind root already appears in `FINAL_FINDINGS.md`.

### Blind Omissions That Are Source-Backed Show-Stoppers

The blind 35 missed three strong roots already present in the submitted file:

| Submitted item | Why it survives |
|---|---|
| 12: no RGB current-limiting resistors | Direct LED/MCU overcurrent path; schematic-proven and independent. |
| 13: D3 anode on isolated +3.3 V island | Copper connectivity leaves the common anode unpowered; independent PCB root. |
| 26: wrong CY8C9560A footprint | Selected 14 mm / 0.5 mm-pitch part cannot fit the 12 mm / 0.4 mm footprint. |

### Likely False, Duplicate, Or Below The Show-Stopper Bar

| Submitted item | Strict disposition |
|---|---|
| 2: GPS SAFEBOOT/RESET not outputs | REJECT as show-stopper: normal startup is supported by RESET_N pull-up and open SAFEBOOT_N. |
| 6 plus 7: both direction helpers overwrite all ports | MERGE into one direction-mask root cluster; do not double count. |
| 9: FAILED immediately overwritten | Source-true UI state bug, but masked by the dead LED path and not an independent tester show-stopper. |
| 19: illustrative matrix used as oracle | REJECT: a stale comment does not prove the compiled matrix is wrong without an authoritative harness specification. |
| 20: held trigger repeats | REJECT at this threshold: duplicate logging/edge handling, not a show-stopper. |
| 23: RFIN matching/DC block | REJECT under the requested rule: source-proven datasheet departure, but the submitted impact is RF-performance based rather than deterministic failure. |
| 24: RF reference through antenna keepout | MERGE with T11/P06 as one keepout root; do not count feed return and patch counterpoise separately. |
| 25: D2 is Schottky rather than VGS Zener | REJECT at this threshold: depends on an unspecified positive transient waveform and source impedance. |

Items 4 and 21 are source-true secondary correctness/UI defects, but they are
not needed in the strongest show-stopper set.

## Hostile Claude Artifacts

Per-candidate prompts, outputs, and statuses are under `reviews/`. Each status
records the exact command:

`claude -p --model opus --effort xhigh --no-session-persistence`

Completed substantive frozen-candidate reviews are preserved as:

- `reviews/P01.{prompt.md,output.txt,status.txt}`
- `reviews/P02.{prompt.md,output.txt,status.txt}`
- `reviews/P03.{prompt.md,output.txt,status.txt}`
- `reviews/P05.{prompt.md,output.txt,status.txt}`
- `reviews/P06.{prompt.md,output.txt,status.txt}`
- `reviews/P08.{prompt.md,output.txt,status.txt}`

The final user instruction stopped expansion after the then-active calls.
P07 and P09 were interrupted while stopping the queue and have empty outputs
plus explicit interruption statuses. Queued P10-P15 reviews were not launched.
Existing empty interrupted P04 batch files remain preserved; the authoritative
P04 council is under `p04_council/`.

Primary documentation used for P03/P04:

- ADI MAX2679:
  https://www.analog.com/media/en/technical-documentation/data-sheets/MAX2679-MAX2679B.pdf
- u-blox NEO-M8:
  https://content.u-blox.com/sites/default/files/NEO-M8_DataSheet_%28UBX-13003366%29.pdf

## Boundary Verification

Original source hashes still match the frozen snapshot. No original source,
`FINAL_FINDINGS.md`, `BUG_THEORY_LOG.md`, or other shared log was edited.
All work from this reconstruction is under
`audit_work/council/blind_reconstruction/`.
