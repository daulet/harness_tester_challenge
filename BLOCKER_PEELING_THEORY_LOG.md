# Blocker-Peeling Theory Log

This log preserves every candidate raised by the blocker-peeling campaign,
including adversarial review outcomes and coordinator adjudication. Submission
wording belongs in `BLOCKER_PEELING_ACCEPTED.md`.

## BP-T001 - D3 red and blue cathodes are swapped

- **Status:** ACCEPTED
- **Root cause:** the selected LED's manufacturer pinout disagrees with the
  schematic/PCB footprint color labels.
- **Thesis:** Broadcom `ASMB-KTF0-0A306` uses pin 2 for the red cathode and pin
  4 for the blue cathode, but the design connects D3.2 to `LED_B` and D3.4 to
  `LED_R`. Once the masked LED power, resistor, and GPIO-mode defects are
  repaired, BUSY lights physical red and FAILED lights physical blue.
- **Primary specification evidence:** Broadcom
  `ASMB-KTF0-0A306-DS100`, package pin configuration: pin 1 anode, pin 2 red
  cathode, pin 3 green cathode, pin 4 blue cathode.
  <https://docs.broadcom.com/doc/ASMB-KTF0-0A306-DS100>
- **Source evidence:** `kicad_files/hardware_challenge.kicad_pcb` connects
  D3.2 to `LED_B` at lines 924-931, D3.3 to `LED_G` at lines 933-940, and D3.4
  to `LED_R` at lines 942-949. Firmware `set_status()` sinks `LED_B` for BUSY
  and `LED_R` for FAILED at `firmware/firmware.ino:67-70`.
- **Simulator evidence:** `blocker_peeling_p8_without_mapping` uses the
  source-derived selected-part mapping after repairing only LED power,
  resistance, and GPIO modes. It observes BUSY current in the physical red
  model, GOOD current in green, and FAILED current in blue. The P8 repaired
  mapping control observes blue, green, and red respectively.
- **Negative control:** `P8_without_BOARD_LED_COLOR_MAPPING` differs from P8
  only by retaining the source-derived D3 pin mapping.
- **Duplicate analysis:** independent of A12 (missing current limiting), A16
  (isolated common anode), and A21 (LED pins left as inputs). Correcting all
  three leaves the red/blue assignment unchanged.
- **Prior rejection:** `SIMULATOR_SCENARIO_REVIEW.md` rejected the swap because
  schematic and PCB agreed. That review did not compare either artifact with
  the selected-part manufacturer pinout and is superseded.
- **Source/spec reviewer:** ACCEPT. Green remaining on pin 3 rules out a simple
  package rotation; no contradictory datasheet was identified.
- **Simulator-fidelity reviewer:** REJECT. It argued that physical color names
  in the circuit model are circular and repeated the prior unverified-pinout
  concern. Coordinator disagrees: the official manufacturer pinout is the
  independent color oracle, the model now refuses any D3 value other than
  `ASMB-KTF0-0A306`, and ngspice is used only to prove downstream current after
  blockers are peeled. The rejection also incorrectly assumed BUSY might be
  intended red despite the firmware explicitly assigning BUSY to `LED_B`.
- **Root-cause/duplicate reviewer:** ACCEPT, conditional on official pinout
  verification. The condition is satisfied by the Broadcom source above.
- **Final council:** ACCEPT. The earlier rejection is overturned.
- **Residual caveat:** a deliberately non-standard footprint pad-numbering
  convention could overturn the physical-pad inference, but no source evidence
  supports such a convention and the footprint uses ordinary numbered pads.

## BP-D001 - Expected harness rows omit passive transitive closure

- **Status:** DUPLICATE OF EXISTING ACCEPTED ROOT; not a new submission count.
- **Existing root:** `FINAL_FINDINGS_REAUDITED.md` finding 17.
- **Thesis:** the symmetric sparse edge graph in `EXPECTED_CONNECTIONS` forms
  six passive components, but 36 of 40 rows omit members reachable through
  another declared edge.
- **New blocker-peeling evidence:** `blocker_peeling_p6_matrix_closure` proves
  that the harness built from the declared edges emits the component closure
  and passes only after `FW_EXPECTED_MATRIX_CLOSURE`.
  `blocker_peeling_p6_without_matrix_closure` retains the sparse rows and fails
  on the same physical harness.
- **Review outcome:** the direct adversarial campaign-design review accepted
  the closure mechanism and required the source-independence limitation below.
- **Limitation:** the repository provides no authoritative external-harness
  specification. The repair preserves the topology implied by the firmware's
  own edge graph; it proves internal electrical inconsistency, not that the
  declared graph is the intended product harness.

## Campaign C001 - Bounded digital harness topology sweep

- **Status:** COMPLETE; NO NEW CANDIDATE.
- **Coverage:** 2,113 scenarios: one declared baseline, 40 single opens, 780
  double opens, 780 pair additions, 256 seeded mixed open/short cases, 128
  synthetic three-pin components, and 128 synthetic four-pin components.
- **Result:** zero firmware-row, verdict, log, immediate-status, or
  post-idle-status differentials.
- **Artifact:** `build/blocker_peeling/exposure_matrix.csv`.
- **Interpretation:** the repaired digital implementation is exhausted within
  these explicit bounds. The result does not validate the source-declared
  harness topology against an external product specification.

## BP-M002 - Post-lock partial RMC mutates one timestamp field

- **Status:** MERGED INTO EXISTING RMC VALIDATION ROOT; not a new count.
- **Thesis:** `sscanf()` writes directly into global `utc_time` and `date`
  before `!time_fixed` is evaluated. After a valid lock, a partial void RMC can
  replace `utc_time`, leave the old `date`, and produce a mixed result-log
  timestamp.
- **Causal evidence:** the P04 reproduction in
  `audit_work/council/deep_firmware_spec_20260606/agents/parser/report.md`
  establishes a two-frame sequence where the second parse returns one stored
  assignment and logging reads the mixed globals.
- **Source/spec reviewer:** ACCEPT mechanism, REJECT separate count. C
  conversion assignments are committed before a later conversion fails, and
  the input is a valid-size void RMC, but the accepted validation repair
  already requires parse-to-temporaries followed by atomic commit.
- **Simulator-fidelity reviewer:** ACCEPT mechanism, MERGE count. Direct serial
  injection reaches the real firmware parser and SD path without relying on a
  GPS-model artifact; the observable impact is date-rollover-gated.
- **Root-cause/duplicate reviewer:** MERGE. The code site, canonical
  validate-then-commit fix, and existing pass-2 disposition are shared with
  finding 4/A04.
- **Coordinator:** AGREES WITH MERGE. Preserve as a stronger witness showing
  why a narrow status-only patch is insufficient, but do not count it
  independently.

## BP-R002 - SD write failures are silently accepted

- **Status:** REJECTED after simulator-fidelity correction.
- **Thesis:** `log_result()` checks only `SD.open()` and discards all
  `print()`/`println()` counts, so storage failure can leave a truncated record
  without a diagnostic.
- **Initial simulator evidence:** the old capacity model returned a
  byte-granular prefix and P9 diagnosed the short count while
  `P9_without_FW_SD_WRITE_CHECKS` remained silent.
- **Source/spec reviewer:** REJECT. Teensy 1.59 SdFat `FatFile::write()` and
  `ExFatFile::write()` return the full requested length or zero, never a
  byte-granular prefix. Small writes can fail later at `close()`/`sync()`, whose
  result the public `File.close()` API discards.
- **Simulator-fidelity reviewer:** ACCEPTED the broad ignored-error mechanism
  but independently noted the byte-granular mismatch and that the production
  fix would need a different completion signal.
- **Root-cause/duplicate reviewer:** REJECT. It classified the remaining
  behavior as conditional external-media handling and related it to prior
  rejected SD error-handling candidates.
- **Coordinator:** AGREES WITH REJECTION. The concrete fidelity objection is
  decisive for the prior witness and P9 repair. The simulator now returns
  all-or-zero per write call and explicitly leaves deferred close/sync failure
  unresolved. The conditional source observation remains logged but is not
  submission-grade.

## Campaign C002 - Peripheral and event-sequence sweep

- **Status:** COMPLETE; NO NEW ADMISSION-BEARING CANDIDATE.
- **Variants:** `P9_RMC_VALIDATED` and the exact
  `P9_RMC_VALIDATED_without_FW_RMC_VALIDATE_ATOMIC` counterfactual.
- **Coverage per variant:** 32 deterministic scenarios covering eleven individual
  RMC messages, four RMC state sequences, 384 seeded valid/invalid RMC cases,
  four button sequences, three UART sequences, three I2C sequences, and five
  SD sequences.
- **Result:** the atomic RMC variant had zero differential rows. The exact P9
  counterfactual had nine differential rows: seven classified as the existing
  A04 semantic-validation root and two as BP-M002 atomic-state composition.
  The 256-case invalid corpus was especially discriminating: the repaired
  variant accepted and mutated zero cases, while the counterfactual accepted
  222 and mutated persistent parser state in all 256.
- **Determinism and integrity:** both executables run twice; their CSV output
  must be byte-identical. The verifier hashes original firmware and KiCad
  inputs before and after execution.
- **Artifact:**
  `build/blocker_peeling/peripheral_exposure_matrix.csv`.
- **Interpretation:** C001 and C002 are consecutive bounded campaign expansions
  with no new accepted root. Observation-only I2C, button-bounce, and SD
  reinsertion rows were still reviewed below rather than silently discarded.

## BP-C003 - Release bounce can start a duplicate session

- **Status:** REJECTED.
- **Thesis:** after the active-low/latch repair, a release sampled at time zero,
  a rebound-low sample at 2 ms, and final release at 4 ms can rearm and start a
  second test. The campaign observes two result records.
- **Mechanism:** REAL in the event model. Press bounce that completes during the
  blocking scan produces one session; release bounce sampled between loops can
  produce two.
- **Source/spec reviewer:** REJECT. SW1 has a generic PTS645 footprint but no
  selected MPN or bounce-time requirement. The repository does not state a
  one-record-per-physical-actuation invariant, and the same observable is the
  intended release/repress behavior.
- **Simulator-fidelity reviewer:** REJECT. The runtime replays authored boolean
  edges; it has no contact, RC, pull-network, or stochastic switch model and
  cannot distinguish rebound from a deliberate fast second press.
- **Root-cause/duplicate reviewer:** REJECT. The witness depends on
  `FW_BUTTON_SESSION_EDGE`, shares the start-gate site and duplicate-log impact
  with the previously rejected held-trigger theory, and is below the
  show-stopping bar.
- **Final council:** REJECT. The mechanism remains a useful observation but not
  an admission oracle.
- **Coordinator:** AGREES. No rejection relied on denying the reproduced
  firmware behavior; the missing physical/product invariant is decisive.

## BP-C004 - Generated setup discards expander initialization failure

- **Status:** REJECTED AS A REPAIR ARTIFACT; not a new source finding.
- **Thesis:** the generated `(void) cy.begin();` continues after a one-shot
  startup address NACK.
- **Mechanism:** REAL in the generated variant. The NACKed register-pointer
  write leaves the runtime pointer at register 0, the following read does not
  return device ID `0x06`, and `begin()` returns false.
- **Source/spec reviewer:** MERGE INTO A01. The call exists only in the
  `FW_EXPANDER_BEGIN` repair; original firmware never calls `begin()`.
- **Simulator-fidelity reviewer:** REJECT, but incorrectly claimed the recovered
  read still selected register `0x2E` and made `begin()` return true.
- **Root-cause/duplicate reviewer:** REJECT independent count and identified the
  correct pointer-state chain above; any product-level residue belongs to A01.
- **Coordinator scrutiny:** DISAGREES with the fidelity review's causal detail.
  `Runtime::reset_expander_state()` zeroes `register_pointer`, and
  `perform_i2c_write()` does not call `i2c_write()` after an address NACK.
- **Final council:** REJECT rather than merge. A01 is the original-source
  omission; the ignored return exists only in its generated repair and cannot
  add a second source finding.

## BP-C005 - I2C faults are persisted as harness failures

- **Status:** REJECTED AS AN INDEPENDENT ROOT; realized board-fault impact
  merges into A11.
- **Thesis:** the driver discards `endTransmission()` and `requestFrom()`
  results. With SDA stuck low during a declared-good scan, firmware consumes
  empty reads, prints `Harness failed!`, and logs a failed harness.
- **Mechanism:** REAL at source level. The campaign records 2,040
  `BusStuckLow` transactions and a failed verdict.
- **Source/spec reviewer:** REJECT/MERGE A11. No product invariant requires a
  separate tester-fault diagnostic, and the injected persistent fault
  reinstates the same SDA-low condition as R3.
- **Simulator-fidelity reviewer:** REJECT. The runtime returns every stuck-bus
  transaction at zero simulated time, while Teensy 4 Wire waits and times out;
  therefore the clean 2,040-transaction completion trace is not target-faithful.
- **Root-cause/duplicate reviewer:** REJECT independent count. The source issue
  is diagnostic propagation, gated behind A01 and observable only under an
  A11-class fault in this campaign.
- **Final council:** REJECT. Preserve as an A11 impact witness and simulator
  timing limitation, not a new submission item.
- **Coordinator:** AGREES. The ignored return values are real, but the current
  witness does not establish an independent intentional defect.

## BP-C006 - SD reinsertion does not recover without reboot

- **Status:** REJECTED.
- **Thesis:** after removal, firmware never reruns `SD.begin()`, so reinsertion
  leaves later `SD.open()` calls unavailable until setup runs again.
- **Mechanism:** REAL and directionally target-faithful. The campaign observes
  zero stored sessions and two visible open-failure diagnostics.
- **Source/spec reviewer:** REJECT. No hot-swap or reinsertion-recovery
  requirement exists; logging is described as statistical.
- **Simulator-fidelity reviewer:** REJECT admission while accepting the
  mechanism. Card-detect timing and remount details are idealized, but a real
  reinserted card also requires initialization.
- **Root-cause/duplicate reviewer:** REJECT. This belongs to the previously
  rejected conditional external-media/open-failure family, is visibly
  diagnosed, and is reboot-recoverable.
- **Final council:** REJECT. It is distinct from the accepted/repaired
  write-call-completion path but still lacks an admission-grade product
  invariant.
- **Coordinator:** AGREES. Preserve the observation and explicit boundary; do
  not promote it.

## Campaign C003 - Persisted sanitizer-backed RMC corpus

- **Status:** COMPLETE; ONE NEW CANDIDATE, MERGED INTO A04.
- **Corpus:** 842 checked-in deterministic cases generated from seed
  `1129467955`: 128 valid RMCs, 576 invalid mutations, 128 post-lock mutation
  replays, and 10 loop/framing streams.
- **Execution:** the corpus is regenerated byte-for-byte before every run and
  replayed twice by baseline, AddressSanitizer, and UndefinedBehaviorSanitizer
  builds of `P9_RMC_VALIDATED`.
- **Initial result:** 39 differentials all came from the same minimized
  `date_suffix` mutation. Thirty-two unlocked cases acquired a lock from a
  seven-character date field, and seven post-lock cases replaced the prior
  timestamp.
- **Repair result:** `FW_RMC_VALIDATE_ATOMIC` now records the end of the date
  conversion with `%n` and requires the next byte to be a comma or checksum
  separator. All 842 cases pass under all three runners with no sanitizer
  diagnostic.
- **Artifacts:** checked-in corpus at
  `simulator/blocker_peeling/corpus/nmea_peripheral_regression.json`; matrices
  under `build/blocker_peeling/persisted_corpus*_matrix.csv`.
- **Stopping-rule effect:** the campaign produced BP-C007, so the consecutive
  no-new-candidate count resets to zero even though the candidate merged.

## BP-C007 - Width-limited RMC date parsing accepts a trailing character

- **Status:** MERGED INTO A04; NOT AN INDEPENDENT COUNT.
- **Thesis:** `%6[^,]` stores the first six date characters and succeeds without
  proving that the next byte is a field delimiter. A checksum-valid date field
  such as `280773X` therefore acquires a lock as `280773`.
- **Source mechanism:** REAL. The original and initial experimental parser both
  ended their `sscanf` format at the width-limited date conversion and checked
  only the assignment count.
- **Minimal witness:** 32 unlocked persisted cases accepted the malformed field;
  seven post-lock cases replaced the previous time/date. Adding only the
  conversion-end delimiter check removes all 39.
- **Source/spec reviewer:** MERGE. `ddmmyy` is a fixed six-digit, delimited RMC
  field, but the same parse site, incomplete-validation root, and remediation
  are already owned by A04. The synthetic input and low impact further defeat
  independent admission.
- **Simulator-fidelity reviewer:** REJECT independent admission because the
  default NEO-M8 model emits conformant dates and the minimized witness calls
  `process_nmea` directly. Its fallback disposition was MERGE into A04.
- **Coordinator:** DISAGREES with the fidelity rejection's causal rationale.
  Direct parser-negative testing proves the unchanged source behavior without a
  simulator model in the causal path; default GPS conformance limits operational
  reachability and impact, not the existence of the parser property. AGREES
  with the fallback MERGE disposition.
- **Root-cause/duplicate reviewer:** MERGE. The candidate is another
  incomplete-field-validation slice at the same `process_nmea` call and is
  fixed by the same validate-then-commit repair as A04.
- **Final council:** MERGE into A04. It found the fidelity rejection technically
  unsound as a disproof, upheld the coordinator disagreement on causality, and
  rejected a new count because the site, root, and minimal fix are not
  independent.
