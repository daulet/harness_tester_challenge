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

## Campaign C004 - Metamorphic state-sequence expansion, seed A

- **Status:** COMPLETE; NO NEW CANDIDATE.
- **Seed:** `1296389185`.
- **Coverage:** 384 paired transformations, with 64 pairs each for NMEA
  fragmentation, neutral parser-message insertion, button duplicate-level and
  idle-loop insertion, bounded successful SD latency, bounded I2C clock
  stretching, and lossless physical-UART polling cadence.
- **Oracle:** each relation declares a narrow state projection that must remain
  invariant. Any projection mismatch or execution error is unconditionally
  classified as unexplained; there is no allowlist. Six deliberately
  invariant-breaking controls must all be detected.
- **Result:** zero differential and zero error. Repeating the complete campaign
  generated byte-identical output. All six controls produced their expected
  differential.
- **Artifact:** `build/blocker_peeling/metamorphic_matrix_c004.csv`; generated
  descriptor SHA-256
  `a83d6bbc8d4a02c9a59b6976739bcc78a07e4c7e12ef9c76e6bc6f6966ae4678`.
- **Stopping-rule effect:** first consecutive expansion with no new candidate.

## Campaign C005 - Metamorphic state-sequence expansion, seed B

- **Status:** COMPLETE; NO NEW CANDIDATE.
- **Seed:** `1296389186`.
- **Coverage and oracle:** the same six independent metamorphic relations as
  C004, with a distinct generated input/event sequence for each of 384 pairs.
- **Result:** zero differential and zero error. Repeating the complete campaign
  generated byte-identical output, all six controls fired, and an explicit
  cross-seed check proved the generated descriptor vector differs from C004.
- **Artifact:** `build/blocker_peeling/metamorphic_matrix_c005.csv`; generated
  descriptor SHA-256
  `4328dc56d974199176e72d7f365fb8ef8b223def5860c5a8100432e6e34247c8`.
- **Stopping-rule effect:** second consecutive expansion with no new candidate;
  the `goal.md` stopping condition is met.
- **Explicit boundary:** the relations do not claim coverage of unspecified
  switch bounce, I2C fault-diagnostic policy, SD reinsertion policy, deferred
  close/sync failures, RF acquisition, or unmodeled target timing.

### C004/C005 initial direct-Claude adversarial review

- **Review outcome:** three fresh direct Opus/xhigh processes rejected the first
  staged form or required conditional verification.
- **Accepted review points:** matrix hashes were circular evidence because the
  seed itself was a CSV column; the I2C relation allowed equal elapsed time;
  neutral parser traffic was followed by a valid RMC that overwrote any
  intermediate state; and the campaign lacked explicit expected-differential
  controls.
- **Resolution:** transformation descriptors now record generated parameters;
  C005 compares seed-independent descriptor vectors; I2C requires a nonempty
  transaction trace and strictly increased microsecond time; neutral traffic is
  observed after an established lock without a final overwrite; and six
  detection controls are present in each matrix.
- **Rejected review points:** the claim that reseeding cannot count as a second
  expansion contradicts the literal `goal.md` rule permitting "new scenarios or
  deterministic seeds." The alleged `nmea_buf` overflow, wrong SD filename,
  mutable-global leakage, and UART drain contradiction were disproved against
  generated firmware and Runtime source: the buffer is 128 bytes, logging uses
  `results.txt`, all mutable firmware globals are reset, Runtime construction
  clears Serial1/Wire/SD, and `loop()` drains until a sentence terminator rather
  than one byte per call.
- **Coordinator:** AGREES with the four accepted hardening points and DISAGREES
  with the source-disproved and goal-text-disproved rejections.

### C004/C005 follow-up direct-Claude council

- **Generic commit reviewer:** PASS; no substantiated blocker. It identified SD
  latency-effect observability as a non-blocking asymmetry.
- **Metamorphic-oracle reviewer:** PASS after source preconditions; no
  demonstrated blocker. It independently recommended proving SD latency changed
  simulated time.
- **Stopping-rule reviewer:** REJECT because it claimed the project had decided
  status-`V` RMC should update UTC, making the neutral-insertion invariant
  invalid.
- **Coordinator:** DISAGREES with that rejection. `SIMULATOR_BUG_EVIDENCE.md`
  A04 says the opposite: accepting status `V` as a fix is the accepted defect,
  and `FW_RMC_VALIDATE_ATOMIC` explicitly requires `status == 'A'`. Even so, the
  relation now uses a checksum-invalid status-`A` sentence, removing navigation
  status policy from the oracle. The SD relation also requires strictly
  increased microsecond time, adopting the two non-blocking recommendations.

### C004/C005 final exact-diff gate

- **Generic commit reviewer:** PASS; no critical or blocking issue.
- **Metamorphic-oracle reviewer:** PASS; no blocker remains across invariants,
  controls, anti-vacuity checks, deterministic replay, or descriptor diversity.
- **Final council:** PASS; the executable evidence supports the two-expansion
  stopping conclusion under the literal deterministic-seed rule.
- **Coordinator:** AGREES. The only repeated source-dependent caveat was exact
  `nmea_buf` size parity. The original firmware correctly remains at 64 bytes
  for the accepted A03 witness, while cumulative variant `P9_RMC_VALIDATED`
  includes `FW_NMEA_BOUNDS` and defines `char nmea_buf[128]`, matching the
  campaign declaration. The exact reviewed patch is commit `03f25ad`.

## Campaign C006 - Intended-workflow expansion

Date: 2026-06-08.

This campaign expands beyond bounded topology and metamorphic audits into
production workflows implied by the README. All test and repair changes are
confined to the simulator worktree and generated variants. `firmware/` and
`kicad_files/` remain unchanged.

### C006-01 - Unchecked post-open SD write-call failure

- **Status:** ACCEPTED AS A NEW ROOT by final direct-Claude chair.
- **Thesis:** `log_result()` checks only `SD.open()`. It discards the return
  count from each subsequent `print()` and `println()`, then calls a public
  `void close()`. A result can therefore be reported and displayed as complete
  after later write calls return zero.
- **Corrected target model:** Teensy SD forwards SdFat's all-or-zero result for
  each individual write call. The witness no longer uses the rejected
  byte-granular partial-write model.
- **Minimal reproduction:** inject a persistent write-call fault after four
  successful calls. Those calls store `230394 - 123519: `; the result and
  line-ending calls return zero. Firmware emits no failure diagnostic. The
  model exercises the target API's all-or-zero failure boundary and does not
  claim FAT allocation geometry.
- **Target-stack evidence:** Teensy `SDFile::write()` returns the underlying
  byte count; `SDFile::close()` discards SdFat's boolean completion result.
  SdFat `FatFile::write()` returns the full requested count or zero.
- **Review record:** one reviewer rejected the adverse-media trigger; two
  accepted the mechanism but incorrectly merged it into a nonexistent accepted
  SD root. The chair was given the actual ledger and direct library evidence
  and returned ACCEPT AS NEW ROOT. The coordinator agrees that this is distinct
  from open failure and from the old, fidelity-invalid byte-prefix witness.

### C006-02 - SD open failure leaves final RGB status GOOD

- **Status:** REJECTED.
- **Thesis:** after a passing test, `SD.open()` can fail, serial reports the
  failure, no record is stored, and firmware still presents final GOOD.
- **Witness:** `production_sd_open_result_divergence` reproduces the complete
  passing scan, open failure, empty SD record, and green final status.
- **Council:** three direct-Claude reviewers accepted simulator causality but
  rejected admission. The RGB state is defined as the harness verdict, not a
  transactional storage-health indicator; USB serial diagnoses the failure;
  the trigger is an adverse external-media condition.
- **Coordinator:** agrees with REJECT. Preserve as distinct from C006-01; do
  not merge it into the accepted post-open write-call root.

### C006-03 - No runtime harness-profile selection

- **Status:** REJECTED.
- **Thesis:** one compiled `EXPECTED_CONNECTIONS` table cannot directly test
  the README's more than 40 harness types.
- **Controls:** `production_alternate_profile_fixed_oracle` rejects an alternate
  topology; `production_alternate_profile_retargeted_oracle` passes when the
  generated build is retargeted to that topology.
- **Council:** three direct-Claude reviewers rejected the product invariant.
  The README does not require runtime profile selection, and per-SKU fixture or
  firmware builds remain a reasonable implementation.
- **Coordinator:** agrees with REJECT. The control proves per-build operation,
  not a product defect.

### C006-04 - NMEA append and terminator overflows as separate roots

- **Status:** MERGED into the existing NMEA buffer-capacity root.
- **Independent witnesses:** 65 non-delimiter bytes trigger ASan at the append
  write in `loop()`. A 63-byte payload plus newline makes `len == 64`; all
  appends are in bounds and ASan triggers at `buf[len] = 0` in `process_nmea()`.
- **Council:** all three direct-Claude reviewers returned MERGE. They conceded
  distinct write sites, triggers, and sanitizer stacks, but found one canonical
  reserve-one-byte capacity fix at the append boundary removes both.
- **Coordinator:** agrees with MERGE for root-cause accounting. Keep both tests
  because they prove two unsafe writes and guard against partial fixes.

### C006-05 - Output-mask and debug-rendering narrow shifts

- **Status:** ACCEPTED AS TWO INDEPENDENT ROOTS by final direct-Claude chair.
- **Thesis:** `1 << i` corrupts electrical stimulus for probe rows 32-39.
  Independently, `values & (1 << j)` invokes undefined behavior and corrupts
  serial connectivity rendering for columns 32-39.
- **Counterfactual:** generated variant `P4_output_mask_only` changes only the
  stimulus expression to `1ULL << i`. UBSan then aborts at the untouched debug
  expression with `shift exponent 32`.
- **Review record:** two first-round reviewers returned ACCEPT_SPLIT and one
  returned MERGE. The merge vote incorrectly treated the two expressions as
  one loop and one local correction. The final chair received the exact source
  sites, generated single-site repair, UBSan trace, and README workflow, then
  returned ACCEPT_SPLIT at medium-high confidence.
- **Coordinator:** agrees. The stimulus expression controls electrical test
  execution and verdict input. The separate rendering expression remains
  defective after that site is repaired and corrupts the only per-connection
  diagnostic for columns 32-39. They require two local edits and neither is a
  downstream manifestation of the other.

### C006-06 - CY8C9560 POR state modeled incorrectly

- **Status:** ACCEPTED SIMULATOR FIDELITY DEFECT; not a product-bug count.
- **Gap:** Runtime reset direction was `0xFF` and output latches were zero. The
  official register table specifies POR direction `0x00`, output latch `0xFF`,
  and pull-up drive mode.
- **Correction:** Runtime now models those POR values. `runtime_defaults`
  checks output and direction registers after reset.
- **Manufacturer source:** Infineon,
  <https://www.infineon.com/assets/row/public/documents/30/57/infineon-cy8c9520a-cy8c9540a-cy8c9560a-20--40--and-60-bit-i-o-expander-with-eeprom-datasheet-additionaltechnicalinformation-en.pdf>.
- **A06 impact:** the old witness falsely claimed `set_output()` caused a
  POR-input to all-output transition. The corrected witness establishes the
  all-input state left by a prior probe, then proves `set_output()` writes every
  port back to output. A06 remains source-valid.
- **Verification:** corrected runtime, A06/A07, P4 counterfactual, and shift
  sanitizer tests all pass.

### C006-07 - RMC status and checksum omissions as separate roots

- **Status:** MERGED into the existing RMC validation root.
- **Thesis:** receiver-declared invalid status `V` and transport checksum
  corruption violate different NMEA invariants and require different checks.
- **Counterfactual A:** `P3_rmc_status_only` adds only status capture and
  `status == 'A'`. It rejects checksum-valid `V` but still accepts a corrupted
  status-`A` sentence.
- **Counterfactual B:** `P3_rmc_checksum_only` adds only checksum verification.
  It rejects the corrupted sentence but still accepts checksum-valid `V`.
- **Verification:** `experimental_rmc_status_only` and
  `experimental_rmc_checksum_only` both pass. Generated metadata identifies
  the base source and exact single-purpose repairs.
- **Council:** all three direct-Claude reviewers returned MERGE. They accepted
  that the two single-purpose repairs are behaviorally independent, but found
  one natural validate-before-commit parser root and one combined local
  correction for the same state transition.
- **Coordinator:** agrees with MERGE for root-cause accounting. Keep both
  counterfactual tests because they prevent a partial validation fix.

### C006-08 - Held active trigger repeats tests and statistical records

- **Status:** REJECTED from the strict core; retained as a boundary theory.
- **Thesis:** the level gate has no edge, latch, or wait-for-release state. The
  full scan and SD append repeat on each active loop.
- **Existing witness:** `bug_a23_held_gate_relogs`; generated P7 control runs
  once while held and rearms only after release.
- **Distinctness:** fixing polarity alone leaves held-press repeats; adding a
  one-shot alone does not correct active-low polarity.
- **Council:** all three direct-Claude reviewers returned REJECT. They accepted
  the source mechanism, but found no explicit one-record-per-press invariant
  and judged duplicate statistical records below the challenge's show-stopper
  threshold.
- **Coordinator:** agrees with the strict rejection while preserving the
  deterministic behavior and counterfactual as boundary evidence.

### C006-09 - C7 10 uF 0402 capacitor on the 12 V rail

- **Status:** REJECTED.
- **Thesis:** C7 is a 10 uF capacitor in an 0402 footprint directly across the
  12 V input rail, so an ordinarily procured part would have inadequate voltage
  rating or unusable DC-biased capacitance.
- **Source witness:** `scenario_c7_10u_0402_on_12v` proves the value, footprint,
  and rail assignment directly from the unchanged KiCad sources.
- **Council:** three direct Opus/xhigh reviewers and a chair rejected admission.
  The source does not select an MPN or voltage rating, so it does not prove that
  an out-of-rating component was specified. The footprint/value combination is
  a procurement-risk warning rather than a deterministic contradiction.
- **Coordinator:** agrees with REJECT. The topology test is valid, but the
  missing component-rating metadata prevents a source-backed product bug.

### C006-10 - MAX2679 RF input network and RF output inductor as two roots

- **Status:** MERGED into the existing MAX2679 matching-network root.
- **Thesis:** the absent RF-input coupling/matching network and the series
  inductor placed in the RF-output path are independent defects.
- **Source witnesses:** `scenario_max2679_rfin_missing_input_network` and
  `scenario_max2679_rfout_series_inductor` independently prove the two
  topologies from the unchanged board.
- **Council:** all initial reviewers accepted both physical observations. The
  final chair returned MERGE at 0.75 confidence because both are manifestations
  of one misplaced/incomplete RF matching network around U3, repaired by
  relocating and completing that network.
- **Coordinator:** agrees with MERGE for conservative root counting. Preserve
  both tests because either half can remain after an incomplete layout fix.

### C006-11 - Antenna-feed reference void and patch counterpoise absence

- **Status:** MERGED into the existing antenna keepout/ground-reference root.
- **Thesis:** the transmission-line reference-plane void and the absent patch
  counterpoise should count separately.
- **Source witnesses:** `scenario_antenna_feed_reference_void` and
  `scenario_antenna_patch_counterpoise_absent` independently identify the feed
  and antenna-region copper facts.
- **Council:** the direct reviewers and final chair returned MERGE. The same
  oversized copper keepout creates both manifestations, and restoring the
  required ground-reference geometry repairs both.
- **Coordinator:** agrees with MERGE. Keep the two checks as separate
  manufacturing-layout assertions, but not as separate root causes.

### C006-12 - Teensy VIN and VUSB are not isolated

- **Status:** REJECTED.
- **Thesis:** the carrier exposes both VIN and VUSB without cutting or otherwise
  isolating the stock Teensy 4.1 power link, allowing USB and external power to
  contend.
- **Source witness:** `scenario_teensy_vin_vusb_unisolated` proves that VIN is
  externally powered while the VUSB pad is left on its generated unconnected
  net in the carrier sources.
- **Council:** all direct reviewers rejected strict admission. The simulator
  does not model the Teensy module's internal VIN/VUSB link, USB insertion,
  simultaneous source voltages, or contention current, and the product
  workflow does not require concurrent USB and external-power operation.
- **Coordinator:** agrees with REJECT. Reopen only with a source-backed module
  power model and an intended concurrent-power scenario.

### C006-13 - NEO-M8 SAFEBOOT_N reserved pin is connected to Teensy GPIO

- **Status:** REJECTED.
- **Thesis:** U3 SAFEBOOT_N is connected to A2 and firmware configures A2 in a
  way that can force the GNSS into safe-boot mode during startup.
- **Source witness:** `scenario_neo_m8_safeboot_reserved_pin_connected` proves
  the physical connection.
- **Council:** one reviewer initially accepted using an incorrect weak-pulldown
  premise; two rejected. The final chair rejected because standard Arduino
  `digitalWrite(LOW)` while the pin remains INPUT disables the pull-up rather
  than enabling a pulldown, setup occurs after GNSS startup sampling, and the
  simulator has no Teensy drive-state, NEO internal-pull, or boot-timing model.
- **Coordinator:** agrees with REJECT and rejects the initial acceptance. The
  topology is real, but no causal unsafe level at the SAFEBOOT_N sampling
  instant has been demonstrated.

### C006-14 - Missing CY8C9560 I/O-access settle interval

- **Status:** REJECTED after reopened blocker-peeling review.
- **Thesis:** once the accepted mapping, mask, initialization, and direction
  blockers are repaired, raw Port 5 is configured and sampled before the
  CY8C9560 `TIOAccess` maximum of 2.485 ms.
- **Initial arithmetic:** counting only 243 SCL clocks from the Port-5
  drive-mode write through completion of its returned input byte gives
  2.430 ms at 100 kHz. The Teensy register constants produce about 100.84 kHz,
  apparently making the interval shorter.
- **Direct source/spec review:** REJECT. The lower-bound calculation omitted
  distinct START-hold and STOP-setup intervals controlled by Teensy
  `SETHOLD=40`. Using the same source-backed timing model consistently gives
  approximately 2.490 ms at 100.84 kHz, before software overhead, which clears
  the 2.485 ms maximum.
- **Direct simulator-fidelity review:** REJECT the proposed delayed-effect
  model. The datasheet extract does not establish whether `TIOAccess` gates
  input-register visibility, output validity, or an internal command path, and
  it does not establish whether an eight-byte input read snapshots at START or
  updates byte by byte. A latency knob would therefore prove its own assumption.
- **Direct root/duplicate review:** REJECT admission while conceding a distinct
  fix axis. It additionally required proof that clock stretching and idempotent
  configuration writes do not close or avoid the alleged window.
- **Coordinator:** agrees with REJECT. The source-backed START/STOP terms alone
  disprove the claimed deficit under the candidate's latest-byte sampling
  model, while the alternate snapshot model lacks a primary-source basis. No
  simulator edit was made and no product bug is counted.

### C006-15 - Blind firmware rediscovery after workflow expansion

- **Status:** COMPLETE; NO NEW CANDIDATE.
- **Method:** three fresh direct non-interactive Opus/xhigh passes received the
  firmware and driver surfaces while excluding all known roots and were asked
  to find a distinct source/spec contradiction.
- **Result:** all three returned no new independent firmware root. Suggested
  items were already accepted, merged, or lacked an intended-use invariant.
- **Coordinator:** agrees. This does not close hardware, manufacturing,
  peripheral-fidelity, or cross-layer campaigns.

## Campaign C007 - Leaderboard-targeted warning and lifecycle pass

Date: 2026-06-08.

### C007-01 - Dedicated post-lock partial-RMC mutation witness

- **Status:** MERGED into RMC validation.
- **Test:** `bug_post_lock_timestamp_tearing`.
- **Sequence:** establish a valid `123519/230394` lock, consume the residual
  CRLF separator, inject a partial status-V RMC, then call the unchanged
  `log_result(false)`.
- **Observation:** the live UTC field becomes `235959.00`, the date remains
  `230394`, and SD contains `230394 - 235959.00: Failed`.
- **Council:** mechanism accepted unanimously; standalone count rejected
  because parse-to-local plus atomic validated commit is the existing RMC
  repair.

### C007-02 - Six-candidate intended-workflow council

| Candidate | Council result | Coordinator result |
|---|---|---|
| GPS control pins remain input-mode weak pulls | warning / merge / warning | Retain as warning, not strict. |
| Held active trigger repeats records | warning / reject / warning | Retain as boundary warning. |
| Reverse input forward-biases D1 | unanimous reject | Agree; intrinsic TVS behavior. |
| Antenna feed/reference keepout | unanimous merge | Agree; one keepout root. |
| Teensy VIN/VUSB bridge | unanimous warning | Accept warning; conditional service mode. |
| Post-lock RMC tear | unanimous merge | Agree; broad RMC-validation root. |

### C007-03 - Backup firmware/driver council

| Candidate | Council result | Coordinator result |
|---|---|---|
| Append and terminator OOB writes count separately | unanimous merge | Agree; preserve both regressions. |
| Probe scan drops Serial1 bytes | unanimous reject strict | Dedicated repaired-P9 witness passes; retain as warning, not strict. |
| Ignored I2C transaction failures | unanimous reject strict | Disagree only at warning tier; exact source defect, fault-gated impact. |

### C007-04 - Fresh blind post-repair firmware audit

- Three independent direct non-interactive Opus/xhigh passes audited the README,
  firmware, and CY8C driver with all known roots excluded.
- All three returned no additional independent deterministic or
  intended-use firmware root.
- This is the second consecutive firmware expansion with no new candidate.

### C007-05 - Repaired-P9 scan-time UART loss

- **Status:** ACCEPTED AS WARNING; rejected from the strict core.
- **Test:** `production_scan_uart_loss`.
- **Control boundary:** runs the cumulative P9 variant, where initialization,
  I2C, UART routing, GNSS talker handling, NMEA capacity, probe direction,
  mapping, verdict, button, LED, and SD blockers are already repaired.
- **Observation:** after startup traffic is drained, the witness verifies zero
  prior overruns, advances 850 ms into a normal GPS stream, and starts a
  button-triggered scan. The scan causes `serial1_rx_overruns() > 0` and leaves
  the 63-byte receive ring full when it returns.
- **Causality:** no original NMEA-capacity behavior is involved; the sole cause
  is the 40-row blocking I2C workload not servicing `Serial1`.
- **Council caveat:** the harness verdict is unaffected and a later GPS epoch
  can recover, so this remains warning-tier.
