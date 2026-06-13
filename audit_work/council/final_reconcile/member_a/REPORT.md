# Organizer-Style Reconciliation

Date: 2026-06-06

Repository commit: `069f7243d9ca87b31333d873bb33b08cfb54042a`

Scope: all 26 entries in `FINAL_FINDINGS.md`, plus the strongest new and
replacement candidates. Original firmware, KiCad sources, README, submitted
findings, and shared theory log were read-only.

## Bottom Line

The leaderboard score of 19 does not reveal which entries received credit.
The following is the best-fit reconstruction after source review, exact
reproduction, and hostile Claude review.

**Best-fit organizer-credited 19:**

`1, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 23, 25, 26`

**Definite prior-report problems:**

- Remove `2`: true source observation, but no deterministic GPS failure.
- Rewrite/merge `4`: status `V` alone does not prove invalid UTC. If retained,
  make it one broad RMC-validation root, not separate status/checksum/shape
  findings.
- Rewrite `19`: the comment-only thesis is weak. Add the exact transitive
  closure proof; do not count it separately from the oracle-table defect.
- Remove `20`: real level-trigger behavior, but duplicate records from a held
  button are below the strict show-stopper bar.
- Rewrite `21`: the conclusion is true, but the submitted generic-symbol
  evidence is backwards. Use the selected Broadcom part's physical pinout.
- Remove `22`: D1 forward-conducts under reverse input, but Q1 still isolates
  the protected rail. Damage depends on external source current/impedance.
- Rewrite `23`: lead with the missing mandatory RFIN DC block/input network;
  avoid claiming every RFOUT-side part is independently wrong.
- Treat `24` as disputed: the all-layer reference void is real, but a
  deterministic loss-of-lock consequence is not measured or manufacturer-bound.

**Strict technical result:**

- 21 source-backed show-stoppers after rewriting `19`, `21`, and `23`.
- `24` is the best 22nd candidate, but is not in the same confidence tier.
- No new independent candidate beyond those rewrites survived the strict gate.
- A 35-item super-confident list is not supported by the artifacts.

## Original 26

| # | Finding | Organizer-likely credit | Strict show-stopper | Final action |
|---:|---|---|---|---|
| 1 | `cy.begin()` omitted | Credit | Accept | Keep. Expander reset, Wire2 initialization, clock setup, and ID check are all skipped. |
| 2 | GPS control pins lack OUTPUT mode | No credit | Reject | Remove. Pins receive weak pulls; source does not prove a required active drive or cold-start failure. |
| 3 | NMEA buffer overflow | Credit | Accept | Keep. A normal 75-byte u-blox RMC exceeds the 64-byte buffer, and `buf[len]=0` adds a second boundary write. |
| 4 | Status-`V` RMC accepted | No credit as written | Reject standalone / merge | Rewrite as one RMC-validation cluster only. Status `V` can coexist with valid UTC, so the submitted impact is overstated. |
| 5 | Narrow `1 << i/j` shifts | Credit | Accept | Keep. Signed-int shifts fail for the 40-bit mask domain. |
| 6 | `set_output()` makes all pins outputs | Credit | Accept | Keep. The all-output interval creates an independent high-versus-low contention path and survives fixing item 7. |
| 7 | `set_pd_inputs()` removes selected output | Credit | Accept | Keep. The intended driven pin is an input before `read_inputs()`. |
| 8 | Any matching row passes | Credit | Accept | Keep. One correct row can pass 39 incorrect rows. |
| 9 | FAILED immediately becomes GOOD | Credit | Accept | Keep. After masking LED defects are fixed, a failed harness still settles to GOOD in the next loop. |
| 10 | UART TX-to-TX and RX-to-RX | Credit | Accept | Keep. Deterministic schematic wiring failure. |
| 11 | SDA pulled down | Credit | Accept | Keep. R3 ties SDA to GND; R2 correctly pulls SCL up. |
| 12 | RGB LED lacks current resistors | Credit | Accept | Keep. Selected part requires bounded channel current. |
| 13 | D3 anode copper island | Credit | Accept | Keep. Routed pad/via terminates in an isolated In1.Cu island. |
| 14 | Button polarity inverted | Credit | Accept | Keep. Active-low switch is treated as active-high. |
| 15 | MAX2679 VCC overvoltage | Credit | Accept | Keep. NEO-M8 VCC_RF exceeds MAX2679's supply range. |
| 16 | `$GNRMC` rejected | Credit | Accept | Keep. Selected NEO-M8N factory multi-GNSS path emits `GN`; firmware sends no configuration and only accepts `GP`. |
| 17 | Logical/raw expander gap after CBL_19 | Credit | Accept | Keep. Port 2's four-bit width makes CBL_20 map to raw bit 24, not 20. |
| 18 | LED GPIOs never OUTPUT | Credit | Accept | Keep. Weak input pulls cannot actively drive the cathodes. |
| 19 | Illustrative table used as oracle | No credit as written | Accept after rewrite | Merge in the closure proof below. Same table, impact, and repair: one corrected oracle finding, not two. |
| 20 | Held trigger repeatedly logs | No credit | Reject | Remove from strict list. Mechanically true but requires sustained hold and produces recoverable duplicate records. |
| 21 | Red/blue LED swap | No credit as written | Accept after rewrite | Resubmit with selected-part pinout; submitted generic-symbol evidence is reversed. |
| 22 | Raw-side D1 defeats reverse protection | No credit | Reject | Remove. D1 can short the external source, but Q1 still isolates the protected rail and severity depends on source current. |
| 23 | MAX2679 input network misplaced | Credit | Accept after narrowing | Keep one RF-front-end finding centered on bare RFIN and its missing mandatory DC block/input match. |
| 24 | RF feed lacks local reference | Uncertain | Disputed | Best 22nd candidate. Keep narrow; do not claim measured lock failure. |
| 25 | D2 is not a VGS clamp | Credit | Accept | Keep. A Schottky does not clamp source-above-gate transient VGS. |
| 26 | U4 package/footprint mismatch | Credit | Accept | Keep. Selected 14 mm/0.5 mm TQFP cannot fit 12 mm/0.4 mm land pattern. |

## Corrected Wording

### Finding 4 - one validation cluster

`process_nmea()` commits global time/date and latches readiness after a partial
`sscanf` match without validating the complete RMC sentence. It discards the
A/V field, never checks `*HH`, and accepts malformed field shapes. These are
one parser-validation root and one repair region, not separate counts.

Admission note: the mechanical defect is real, but status `V` alone does not
prove invalid UTC and checksum failure requires corruption. Organizer credit
is unlikely unless the challenge counts broad parser validation.

### Finding 19 - oracle is not electrically realizable

`EXPECTED_CONNECTIONS` is used as an exact per-pin continuity oracle, but its
40 rows encode sparse direct edges rather than the transitive electrical
observation of a passive harness. Mechanical parsing finds six connected
components and 36/40 rows that differ from component-minus-self. For example,
the table contains 0-13 and 13-26 while row 0 omits 26; probing pin 0 must
observe pins 13, 26, and 39. Fold this proof into the existing
"illustrative oracle" finding rather than adding a second count.

Reproduction:
`audit_work/council/matrix_closure/check_matrix.py`

Output:
`audit_work/council/deep_firmware_spec_20260606/evidence/matrix_closure.txt`

### Finding 21 - physical red/blue swap

D3 is the Broadcom `ASMB-KTF0-0A306`. Its manufacturer pin table defines pad 2
as red cathode, pad 3 as green cathode, and pad 4 as blue cathode. The PCB
connects `LED_B` to pad 2 and `LED_R` to pad 4, so firmware red drives the
physical blue die and firmware blue drives the physical red die.

Do not use the generic KiCad `LED_ABGR` symbol naming as physical evidence.

Primary source:
<https://docs.broadcom.com/doc/ASMB-KTF0-0A306-DS100>

### Finding 23 - bare MAX2679 RFIN

AE1 is connected directly to MAX2679 B1/RFIN with no series DC-blocking
capacitor or required external input matching network. The only L1/C5 series
network is between A2/RFOUT and the NEO-M8 RF input, even though RFOUT is
internally matched. Count this as one RF-front-end placement/omission defect.
Do not split DC block, input match, and output placement into separate bugs.

### Finding 24 - narrow disputed form

The AE1-to-RFIN feed crosses an all-layer copper-pour keepout. Approximately
13 mm of the 0.8128 mm top-layer route has no local copper plane beneath it,
then necks to 0.127 mm. This is a distinct geometry defect from finding 23.

Do not claim a measured impedance, guaranteed loss of lock, or that In1.Cu
provides a reference inside the keepout. Strict admission remains disputed
because no antenna requirement, field solution, or receiver measurement
quantifies the consequence.

## Ranked Additions To The Credited 19

| Rank | Candidate | Technical confidence | Organizer-credit likelihood | Result |
|---:|---|---|---|---|
| 1 | Corrected finding 21, selected-part red/blue swap | Very high | High | Best resubmission. Original evidence was wrong; corrected evidence is binary and manufacturer-backed. |
| 2 | Strengthened finding 19, transitive-closure oracle mismatch | Very high | Medium-high | Exact mechanical proof. Submit as stronger evidence for one oracle finding, not a new extra count. |
| 3 | Narrow finding 24, all-layer reference void and width discontinuity | Medium | Medium | Best candidate for score 22, but below the first two in strict confidence. |
| 4 | Broad finding 4, complete RMC validation omitted | High source truth, low severity | Low-medium | One merged parser root only; likely treated as hardening or non-show-stopping. |
| 5 | C7 10 uF/0402 on +12 V | Medium component-spec concern | Low | Reject from final list. Rating is absent rather than explicitly wrong, and the shunt part is not a deterministic show-stopper. |
| 6 | Teensy VIN/VUSB service backfeed | High topology confidence | Low | Reject. Optional simultaneous USB/barrel-service condition with non-deterministic host-side consequence. |

## Strongest New Candidates

| Candidate | Technical result | New count? | Final disposition |
|---|---|---:|---|
| Matrix transitive closure | Proven | No | Merge into rewritten finding 19. |
| SAFEBOOT active-low command after OUTPUT correction | Disproved by startup chronology | No | Reject. U3 samples before line 106; no later reset exists. |
| NMEA checksum omitted | Proven | No | Merge into rewritten finding 4; corruption-gated. |
| C7 rating/package tuple | Catalog concern proven | No | Reject under show-stopper bar. |
| Teensy VIN/VUSB bridge | Topology proven | No | Reject under operating-scenario/severity bar. |
| Timestamp frozen after first RMC | Mechanically false | No | Reject. `sscanf` updates globals before `&& !time_fixed` is evaluated. |
| Digital CBL traces under AE1 | Geometric premise false | No | Reject. Traces clip the square keepout corner but clear the rounded ceramic by 0.457-1.438 mm. |
| Missing SCL pull-up | Factually false | No | Reject. R2 is 4.7 kOhm from CY_SCL to +3.3 V. |
| L7805 deterministic thermal shutdown | Underconstrained | No | Reject. Load lower bound and effective board thermal resistance are not established. |
| Patch ground-plane consequence | Plausible | No | Merge with disputed finding 24; same keepout and board correction. |

## Claude Reconciliation

Fresh prompts, stdout, stderr, and status are in:

`audit_work/council/final_reconcile/member_a/claude/`

| Candidate | Claude verdict | Council response |
|---|---|---|
| Submitted 2 | `UPHOLD_REJECT` | Agree. Source fact is true; show-stopping consequence is not. |
| Submitted 4 | `MERGE_VALIDATION` | Agree. Reject status-only framing; retain one validation cluster if desired. |
| Submitted 6 | `ACCEPT` | Agree. Separate contention consequence and separate repair from item 7. |
| Submitted 9 | `ACCEPT` | Agree. Counterfactually fixing LED hardware still leaves failed harnesses displayed as GOOD. |
| Submitted 16 | `ACCEPT` | Agree. Default NEO-M8N path and absent firmware configuration make this deterministic on a fresh module. |
| Submitted 19 | `MERGE_OLD_19` | Agree after scrutiny. Closure proof is exact, but same oracle/table repair means one strengthened finding. |
| Submitted 20 | `REJECT` | Agree. Real behavior, insufficient severity. |
| Submitted 21 | `ACCEPT_REWRITE` | Agree. Official Broadcom pin table resolves the prior evidence error. |
| Submitted 22 | `REJECT` | Agree on no credit. Disagree with calling D1 harmless: it can short the external source, but source-current dependence defeats determinism while Q1 still protects the load. |
| Submitted 23 | `ACCEPT_REWRITE` | Agree. Narrow to the missing mandatory RFIN network. |
| Submitted 24 | `OVERTURN_ACCEPT` | Partial disagreement. Claude correctly disproved the claimed nearby-plane rebuttal and established distinctness from 23; strict severity remains disputed without a quantified RF requirement or measurement. |
| SAFEBOOT level | `UPHOLD_REJECT` | Agree. Counterfactual OUTPUT mode does not change startup ordering. |
| NMEA checksum | `MERGE_4` | Agree. Real but corruption-gated and not a separate count. |
| C7 rating | `REJECT` | Agree. Do not override the challenge's show-stopper bar with a procurement-only bar. |
| Teensy VIN/VUSB | `UPHOLD_REJECT` | Agree. Real service hazard, not a deterministic tester failure. |
| Timestamp frozen | `UPHOLD_REJECT` | Agree. C++ evaluation order falsifies the thesis. |

Additional completed hostile rejections:

- SCL pull-up theory:
  `audit_work/council/priority_exact/outputs/scl.txt` - agree.
- L7805 thermal shutdown:
  `audit_work/council/priority_exact/outputs/thermal.txt` - agree.
- Patch ground-plane as a separate count:
  `audit_work/council/priority_exact/outputs/antenna.txt` - agree with merge,
  not separate count.
- Digital traces under the antenna:
  `audit_work/council/antenna_digital/member_ramanujan/ADJUDICATION.md`,
  `audit_work/council/antenna_digital/member_schrodinger/stdout.txt` - agree.
- Teensy VIN/VUSB second independent council:
  `audit_work/council/teensy_vusb/member_two/ADJUDICATION.md` - agree.

## Confidence Summary

**Organizer reconstruction:** medium confidence. The score gives only a count,
not item-level decisions. The seven non-credited/rework entries that best fit
the observed 19 are `2, 4, 19, 20, 21-as-written, 22, 24`.

**Technical classification:** high confidence for 21 accepted/rewrite findings,
high confidence for rejecting `2`, `20`, and `22`, and medium confidence for
leaving `24` disputed.

The most defensible next submission is 21 high-confidence findings plus the
narrow item 24 as a clearly labeled RF-layout candidate. That produces a
credible 22-item list. There is no evidence-backed path to 35 without lowering
the bar or double-counting roots.

## Integrity

Protected originals were not edited. Final verification after writing this
report showed:

- `git diff -- README.md firmware kicad_files` was empty.
- `FINAL_FINDINGS.md` SHA-256:
  `cf9bf38a6def8e5eb4245f5d805e006dcb99ed00b95702aa0cdc919571a54f83`
- `BUG_THEORY_LOG.md` SHA-256:
  `1536a00e395acb03d99b9b2196453fc484e943460bb850817f1f1640da546f6b`

All council prompts, reviews, reproductions, and this report are scratch
artifacts under `audit_work/council/`.
