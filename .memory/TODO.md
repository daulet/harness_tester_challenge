# Active simulator realism board

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
- [ ] **Next:** model finite Teensy UART RX capacity, overrun accounting, and firmware
  polling under continuous default GPS traffic.
  - Prerequisite: timed UART and NEO-M8 sentence cadence.
  - Completion gate for Q2: deterministic tests prove byte availability,
    button/reset transitions, I2C failures, GPS startup/cadence, and SD latency
    at exact simulated timestamps; unchanged firmware runs without wall-clock
    sleeps.

## Q3 - KiCad-derived closed-loop electrical simulation

- [ ] Generate board electrical configuration from parsed KiCad component,
  value, pin, and net data instead of duplicating topology in fixtures.
  - Prerequisite: Q1 structural BoardModel accessors.
- [ ] Export actual MCU and expander pin modes/drives to the electrical solver and
  feed solved logic levels back into `digitalRead()` and expander inputs.
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
