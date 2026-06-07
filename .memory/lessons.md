# Simulator realism lessons

## 2026-06-07 - Campaign baselines must peel oracle-data defects too

- Symptom: P9 repaired probe mechanics and all-row aggregation, but the
  source-declared harness still failed as a known-good baseline.
- Root cause: `EXPECTED_CONNECTIONS` stores sparse edges while the measurement
  observes passive connected-component closure; 36 of 40 rows differ.
- Source evidence: the P6 matrix leave-one-out witness and the six components
  derived from the source edge graph.
- Permanent rule: before broad differential campaigns, repair known defects in
  expected data as well as control flow, and preserve a leave-one-out witness
  proving why the repair is required.

## 2026-06-07 - Shared topology ancestry limits what a clean campaign proves

- Symptom: 2,113 harness scenarios produced no firmware/oracle differential.
- Root cause: this is expected after the repaired implementation agrees with
  the component oracle; it does not prove that the repository's declared
  external harness graph matches an unavailable product specification.
- Source evidence: `build/blocker_peeling/exposure_matrix.csv` and the explicit
  `source_independence` column.
- Permanent rule: report implementation differentials separately from
  physical-specification truth, and never promote a clean shared-ancestry
  campaign into evidence that the underlying product topology is correct.

## 2026-06-07 - Net agreement does not validate selected-part semantics

- Symptom: the schematic and PCB consistently connected `LED_B` to D3 pad 2
  and `LED_R` to pad 4, so an earlier review rejected the color swap.
- Root cause: both artifacts reused the same footprint/net assumption; the
  selected ASMB-KTF0-0A306 datasheet independently defines pad 2 as red cathode
  and pad 4 as blue cathode.
- Source evidence: official Broadcom selected-part pinout, D3 PCB pad nets, and
  the P8 leave-one-color-mapping-out current witness.
- Permanent rule: validate symbol and footprint pin semantics against the exact
  selected component datasheet; schematic/PCB agreement proves consistency,
  not physical correctness.

## 2026-06-07 - Storage witnesses must exercise the deciding byte

- Symptom: a partial-write test failed during the timestamp body, so it could
  not validate the later line-ending byte-count check.
- Root cause: the repair assumed the host shim's one-byte `println()` behavior,
  while the target Print implementation uses CRLF.
- Source evidence: the P9 direct-Claude staged-diff review and the 24-byte
  one-byte-short CRLF counterexample.
- Permanent rule: emit protocol bytes explicitly when their count matters, and
  include both the exact boundary failure and a complete-success control.

## 2026-06-07 - Reviewer input must include untracked implementation files

- Symptom: a diff-only reviewer could assess CMake wiring while missing newly
  created generator, catalog, oracle, and test files.
- Root cause: plain `git diff` excludes untracked files.
- Source evidence: blocker-peeling pass-end review saw references to the new
  files but not their contents.
- Permanent rule: pass-end reviewer input includes tracked diffs plus explicit
  `/dev/null` diffs for every untracked pass file.

## 2026-06-05 - A passing witness can be causally empty

- Symptom: `bug_a06_all_outputs` passed even if `set_output()` did nothing.
- Root cause: the simulator reset direction was already `0x00`, equal to the
  asserted post-state and opposite the real input/high-impedance default.
- Source evidence: `Runtime::reset_expander_state()` and the A06 review in
  `SIMULATOR_20_ISSUES.md`.
- Permanent rule: every state-transition witness asserts a different pre-state
  and, where practical, a negative control or mutation that removes the action.

## 2026-06-05 - Parallel model layers hide emergent failures

- Symptom: firmware tests, board topology tests, and ngspice fixtures each prove
  isolated facts but cannot expose their combined behavior.
- Root cause: `Runtime::analog_stimulus()` exports only LED booleans, released
  I2C lines, and a nominal UART voltage; solved electrical levels never feed back
  into firmware reads.
- Source evidence: `simulator/src/runtime.cpp` and
  `simulator/include/host_simulator/analog.h`.
- Permanent rule: board facts are generated from KiCad, firmware produces pin
  drives, the electrical solver resolves nets, and resolved levels return to the
  firmware runtime.

## 2026-06-05 - Host execution is not target execution

- Symptom: host behavior can corroborate invalid shifts but cannot define their
  target result.
- Root cause: undefined behavior and host/target integer or core differences.
- Source evidence: A05 and the host-only CMake targets.
- Permanent rule: use sanitizers for language UB and a Teensy-target compile or
  trace for target-specific claims.

## 2026-06-05 - A formatter without repo policy creates unrelated churn

- Symptom: running default `clang-format -i` rewrote entire existing test files
  for a three-line change.
- Root cause: the repository has no `.clang-format`, so tool defaults do not
  represent an established local style.
- Source evidence: staged diff audit during Q1.
- Permanent rule: do not bulk-format existing files until a reviewed repository
  style exists; preserve surrounding style and format new files consistently.

## 2026-06-05 - Expected-failure tests must identify the failure

- Symptom: a nonzero-exit test could pass for an unrelated crash, while UBSan
  may recover and exit zero by default.
- Root cause: exit status alone is not evidence of a specific defect.
- Source evidence: A03 ASan reports `global-buffer-overflow` after `nmea_buf`;
  A05 UBSan reports an invalid shift in `firmware.ino`.
- Permanent rule: expected-failure wrappers require nonzero termination and
  diagnostic tokens that identify both the bug class and target behavior.

## 2026-06-05 - Do not retain subobject references from temporary models

- Symptom: structural A29 metadata appeared empty despite the parser parity test
  passing.
- Root cause: the scenario bound a reference returned by
  `board_model().component("U4")`; the temporary `BoardModel` was destroyed at
  the end of the statement.
- Source evidence: targeted scenario failure and local lifetime inspection.
- Permanent rule: keep source models alive for at least as long as references to
  parsed pads, components, nets, or graph objects.

## 2026-06-05 - Resolve reviewer uncertainty with the exact witness

- Symptom: pass-end review questioned whether UBSan stopped at shift exponent
  31 before reaching the documented exponent 32 failure.
- Root cause: the reviewer did not receive the direct sanitizer output and
  inferred generic signed-shift behavior.
- Source evidence: `firmware_ubsan_cases narrow-shift` exits 134 at
  `firmware.ino:152` with `shift exponent 32 is too large for 32-bit type int`.
- Permanent rule: when reviewer reasoning conflicts with a reproducible claim,
  run the smallest deciding witness and preserve the exact diagnostic.

## 2026-06-05 - Time advancement must not be reentrant

- Symptom: a callback that recursively advances time can move the inner clock
  past the outer target, after which the outer call writes an earlier time.
- Root cause: event callbacks execute inside the scheduler's advancement loop.
- Source evidence: Q2 event-queue design self-review.
- Permanent rule: callbacks may enqueue same-time or future work, but advancing
  or clearing the runtime from inside a callback is rejected.

## 2026-06-05 - Idle outputs are still active drivers

- Symptom: a UART contention model that only tracked transmitted bytes missed
  the board's TX-to-TX conflict when firmware sent no payload.
- Root cause: an enabled UART TX pin actively drives the idle-high state; every
  peer start bit drives the shared net low.
- Source evidence: U2 TX1 and U3 TXD share `UBX-TXD`, and firmware calls
  `Serial1.begin(9600)` during setup.
- Permanent rule: line models include enabled idle drive states, not only
  explicit write intervals.

## 2026-06-05 - Net names do not define silicon polarity

- Symptom: the `CY_RST_N` net and KiCad `RESET_N` pin name suggest active-low
  reset, while the selected CY8C9560A pin is active-high XRES.
- Root cause: schematic naming conflicts with the component datasheet.
- Source evidence: Infineon datasheet 38-12036 Rev. M identifies pin 62 as
  active-high XRES with an internal pull-down and high-Z held-reset pins.
- Permanent rule: peripheral polarity and reset behavior come from the selected
  silicon reference; board names remain evidence of design intent or error.

## 2026-06-05 - Physical faults and downstream witnesses need separate modes

- Symptom: making the actual SDA pull-down effective would prevent every
  expander transaction and mask firmware defects beyond I2C initialization.
- Root cause: a single always-ideal or always-physical bus cannot serve both
  board-level causality and isolated downstream firmware evidence.
- Source evidence: R3 connects `CY_SDA` to GND, while A06-A09 require successful
  expander transactions to reach their target behavior.
- Permanent rule: end-to-end tests default to parsed physical topology; tests
  that intentionally isolate a downstream layer must opt into ideal mode and
  state that boundary explicitly.

## 2026-06-05 - A reproduced mechanism is not automatically a product defect

- Symptom: CR and LF each cause a parser break, producing one empty LF-only
  firmware pass per NMEA sentence.
- Root cause: the loop treats either terminator independently while real NMEA
  output uses CRLF.
- Source evidence: `firmware_nmea_crlf_empty_pass` proves the extra pass, but
  the host RX queue is unbounded and shows no loss or misparse.
- Permanent rule: retain discriminating behavior tests, but do not promote a
  mechanism to a defect until its harmful consequence is source-backed and
  observable at the modeled boundary.

## 2026-06-05 - Open success does not prove durable logging

- Symptom: a full modeled card accepts the file open, truncates the timestamp
  after 12 bytes, and firmware emits no storage diagnostic.
- Root cause: `log_result()` checks only `File` truthiness and discards every
  `print`/`println` byte count.
- Source evidence: `bug_sd_partial_log_accepted` on unchanged firmware plus the
  capacity and removal controls in `sd_timing_capacity_and_removal`.
- Permanent rule: storage witnesses must inspect operation results and the
  resulting artifact; successful initialization/open is not proof of a complete
  or durable record.

## 2026-06-05 - Timed links also need finite consumers

- Symptom: source-correct GPS cadence crossed the UART intact in simulation even
  while firmware performed a long blocking harness test.
- Root cause: frame delivery had timing and topology but appended to an unbounded
  host string instead of the Teensy Serial1 software ring.
- Source evidence: PJRC allocates 64 Serial1 receive entries and keeps one ring
  slot empty; `bug_a03_uart_overrun_path` observes drop-new loss and a 63-byte
  unterminated parser fragment, while `firmware_uart_polling_control` does not.
- Permanent rule: every timed producer/consumer boundary needs capacity, loss
  policy, and accounting before throughput conclusions are credible.

## 2026-06-05 - Authored fixtures must yield to parsed board facts

- Symptom: the nominal analog fixture gave both I2C lines ideal pull-ups even
  though the PCB places R3 from `CY_SDA` to GND.
- Root cause: fixture parameters duplicated resistor values and topology instead
  of consuming the existing KiCad component and pad records.
- Source evidence: `analog_source_derived_i2c` derives R2/R3 from PCB values and
  nets, reproduces SDA low/SCL high, and flips both config and voltage when R3's
  value or rail net is mutated.
- Permanent rule: authored electrical defaults may fill unmodeled gaps, but a
  parsed board fact overrides the corresponding fixture parameter.
