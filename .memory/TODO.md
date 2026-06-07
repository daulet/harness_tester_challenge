# Active simulator realism board

## BP - Blocker-peeling discovery campaign

- [x] **Done:** implement the initial P0-P3 generated repair lattice.
  - End-state: declarative repair/dependency/scenario catalogs generate
    deterministic variants and manifests without modifying source inputs.
  - Required witnesses: P1 Wire startup with physical SDA failure, P2 expander
    reachability, P3 default modeled GPS lock, and leave-one-repair-out
    counterfactuals including an ASan no-bounds witness.
  - Owner: `simulator/blocker_peeling/`, oracle source, CMake, and tests.
  - Completion: deterministic generation and immutable-source manifests,
    independent oracle, P0-P3 variants, leave-one-out controls, positive and
    negative ASan witnesses, 15/15 targeted CTests, and direct Claude
    pass-end review PASS after two findings were corrected.
- [x] **Done:** implement P4-P7 and P9 generated repairs and counterfactuals.
  - Completion: generated driver repairs isolate the selected probe output;
    UBSan proves the 64-bit-mask dependency; source-derived logical/raw mapping
    reaches bits 24-43 through an explicitly labeled schematic-ideal fixture;
    verdict, button-session, and SD write-call failure controls all have
    leave-one-out witnesses.
  - Validation: 89/89 CTests, including full-record and one-byte-short CRLF
    controls, with original firmware, driver, schematic, and PCB hashes
    unchanged.
- [x] **Done:** complete P8 closed-loop LED electrical feedback.
  - Completion: Runtime output-mode-gated LED drive states feed a source-derived
    board electrical configuration and ngspice solve; selected-part pin mapping,
    common-anode continuity, and missing series resistors are modeled from the
    KiCad artifacts plus the official D3 pinout.
  - Validation: the cumulative P8 variant produces the intended physical
    red/green/blue currents, while leave-one-out firmware-mode, power/resistor,
    and color-mapping variants each expose their distinct failure. Full baseline
    is 93/93 CTests.
- [x] **Done:** add the bounded generated digital-harness campaign and exposure
  matrix.
  - Completion: the P9 batch runner executes 2,113 scenarios covering the
    declared baseline, all 40 single opens, all 780 double opens, all 780 pair
    additions, 256 seeded mixed open/short cases, and 128 each of synthetic
    three- and four-pin components.
  - Validation: firmware rows, verdict, result log, immediate status, and
    held-button idle status agree with the independent component oracle in
    every scenario; `build/blocker_peeling/exposure_matrix.csv` contains zero
    differentials.
- [x] **Done:** add peripheral and event-sequence campaigns plus minimized
  witnesses.
  - Completion: 32 scenarios per variant cover RMC semantics/state
    composition, a 384-case deterministic parser corpus, UART/I2C transaction
    faults, button timing/bounce observations, and SD
    removal/capacity/reinsertion.
  - Result: the atomic RMC variant has zero differentials; its exact P9
    counterfactual has nine, all mapped to A04/BP-M002.
- [x] Run three-role, direct-Claude disproof councils for every candidate and
  produce the accepted, rejected, disputed, exposure-matrix, and final-report
  artifacts from `goal.md`.
  - Completion: BP-C003 through BP-C006 each received source/spec,
    simulator-fidelity, and root-cause/duplicate reviews plus final council.
    All mechanisms and dispositions are preserved in
    `BLOCKER_PEELING_THEORY_LOG.md`; the final report is under `reports/`.
- [x] Run a persisted, sanitizer-backed NMEA mutation-corpus expansion.
  - Completion: 842 checked-in cases regenerate byte-for-byte and pass twice
    under baseline, ASan, and UBSan after BP-C007 exposed and pinned a missing
    date-field delimiter check in the experimental repair.
  - Admission result: BP-C007 received three direct-Claude disproof reviews and
    final council, then merged into A04. Because it was a new candidate, the
    no-new-candidate streak remains zero.
- [ ] Run an independent state-sequence/metamorphic expansion across button,
  UART, I2C, SD, and parser transitions. The goal stopping rule is met only if
  two consecutive expansions produce no new candidates.

## Q0 - Campaign bootstrap

- [x] **Done:** promote the first-pass analog notes to a Tier 3 protocol,
  inventory, parity ledger, lessons log, and live status.
  - Validation: source inventory and baseline status inspection.

## Q1 - Correctness and witness credibility

- [x] **Done:** repair the CY8C9560 power-on/reset model and make A06 a
  causal witness.
  - End-state: direction defaults match source documentation; test proves
    pre-state and `set_output()` transition.
  - Owner: `runtime.cpp`, runtime/bug tests.
- [x] **Done:** add ASan/UBSan build targets that execute the unchanged firmware path and
  dynamically expose the NMEA overflow and narrow-mask undefined behavior.
  - Prerequisite: none; implement in the same pass as the reset-model correction
    so runtime changes immediately receive sanitizer coverage.
- [x] **Done:** fix A25 terminal diagnostics and replace A29's unscoped source search with
  structural schematic data exposed by `BoardModel`.
- [x] **Done:** add mutation-style negative controls for high-value witnesses so removal
  of the target action makes the test fail.
- [x] **Done:** refresh `SIMULATOR_20_ISSUES.md` and `SIMULATOR_BUG_EVIDENCE.md` after Q1
  so A06 no longer appears withheld for a simulator defect that has been fixed.
  - Completion gate for Q1: A06 has a causal transition witness, sanitizer cases
    fail on the known original-firmware UB and pass on bounded controls, all
    source-scenario diagnostics are structurally scoped, and the normal 43-test
    baseline has no regression.

## Q2 - Event-driven runtime and peripheral fidelity

- [x] **Done:** add a deterministic event queue and advance it through `delay()` and
  explicit time stepping.
  - Prerequisite: Q1 complete.
- [x] **Done:** deliver UART bytes according to configured baud and model TX/RX direction,
  framing, contention, and receive availability over time.
  - Prerequisite: event queue.
- [x] **Done:** model button bounce/press duration and reset/power-on timing.
  - Prerequisite: event queue.
- [x] **Done:** replace atomic I2C success with line-aware transactions, NACK, stuck-low,
  clock stretching, and device reset/address timing.
  - Prerequisite: event queue and Q1 expander reset fidelity.
- [x] **Done:** add NEO-M8 startup/default sentence cadence/checksum behavior.
  - Prerequisite: event queue and timed UART.
- [x] **Done:** add SD/FAT latency, capacity, removal, FILE_WRITE append
  semantics, and partial-write failures.
  - Prerequisite: event queue.
- [x] **Done:** model finite Teensy UART RX capacity, overrun accounting, and firmware
  polling under continuous default GPS traffic.
  - Prerequisite: timed UART and NEO-M8 sentence cadence.
  - Completion gate for Q2: deterministic tests prove byte availability,
    button/reset transitions, I2C failures, GPS startup/cadence, and SD latency
    at exact simulated timestamps; unchanged firmware runs without wall-clock
    sleeps.

## Q3 - KiCad-derived closed-loop electrical simulation

- [x] **Done:** generate I2C pull-network configuration from parsed KiCad component,
  value, pin, and net data instead of duplicating topology in fixtures.
  - Prerequisite: Q1 structural BoardModel accessors.
- [x] **Done:** export actual LED MCU pin modes/drives to the electrical solver
  and solve the resulting physical die currents.
  - Completion: P8 uses the Runtime GPIO state and parsed board facts rather
    than a test-authored LED stimulus.
- [ ] **Next:** extend closed-loop drive/solve/readback beyond the LED path to
  MCU and expander harness/I2C pins, then feed solved logic levels into
  `digitalRead()` and expander inputs.
  - Prerequisite: generated board electrical configuration and Q2 event queue.
- [ ] Replace the representative harness channel with all 40 mapped channels,
  including per-channel opens, shorts, leakage, R/L/C, and contention.
  - Prerequisite: generated configuration and closed-loop drive/level API.
- [ ] Add closed-loop tests for actual SDA pull-down, same-direction UART,
  missing/isolated LED path, driven-pin shorts, and rail collapse.
  - Completion gate for Q3: those five failures arise from unchanged firmware
    plus parsed KiCad topology without hand-entering the defective net/value into
    the test fixture, and solved levels change firmware-visible reads.

## Q4 - Limits, corners, and generated campaigns

- [ ] Add voltage/current/power/energy observations for TVS, Q1 VGS, regulators,
  LEDs, GPIOs, and rails.
  - Prerequisite: Q3 closed-loop electrical configuration.
- [ ] Add source-backed min/typ/max, tolerance, temperature, startup, dropout,
  ESR, and load sweeps with deterministic seeds and bounded cases.
  - Prerequisite: component-limit observations.
- [ ] Add generated single- and multi-fault harness campaigns with the property
  that correct harnesses pass and injected faults fail with useful attribution.
  - Prerequisite: Q3 all-channel model.
- [ ] Add NMEA and peripheral-sequence fuzz targets with a persisted regression
  corpus.
  - Prerequisite: Q2 state machines and Q1 sanitizer targets.
  - Partial completion: blocker peeling now has 384 in-process seeded RMC cases
    plus an 842-case persisted corpus with byte-identical regeneration and clean
    baseline/ASan/UBSan replay. Broader peripheral state-sequence fuzzing remains
    open.
  - Completion gate for Q4: bounded deterministic sweeps cover documented
    operating limits, every single harness fault is detected or explicitly
    classified as unobservable, and the fuzz regression corpus is clean under
    sanitizers.

## Q5 - Authoritative and differential boundaries

- [ ] Integrate `kicad-cli` ERC/DRC and package/footprint metadata checks when the
  executable is available; absence must be explicit rather than silently pass.
  - Prerequisite: none; may proceed independently when tooling becomes available.
- [ ] Add Teensy-target cross-compilation to catch host/target width and core API
  differences.
  - Prerequisite: toolchain availability.
- [ ] Define trace formats and differential tests for GPIO, UART, I2C, reset,
  and SD behavior against captured hardware/reference traces.
  - Prerequisite: Q2 event model and a reference trace source.
- [ ] Calibrate compact models against vendor curves or measured traces and
  record remaining fidelity gaps in `.memory/parity.md`.
  - Prerequisite: Q3/Q4 observations and authoritative curve/trace inputs.
  - Completion gate for Q5: available authoritative tools run in CI/CTest,
    target compilation is reproducible, and each modeled boundary has either a
    passing differential trace/curve comparison or an explicit external blocker.
