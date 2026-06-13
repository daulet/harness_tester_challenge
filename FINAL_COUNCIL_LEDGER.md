# Final Council Theory Ledger

Date: 2026-06-06

This ledger preserves accepted, merged, disputed, and rejected theories. It is
not a sendable all-accepted bug list.

## Review Record

- Deep firmware/spec council: 64 candidates, 64 successful noninteractive
  hostile reviews, 0 new roots, 13 existing accepts, 1 replacement, 6 merges,
  44 rejects.
- Blind reconstruction: 35 frozen theories, 13 accepts, 5 merges, 17 rejects,
  0 new roots after comparison.
- Blind PCB council: 2 promoted candidates, each accepted by two independent
  hostile reviewers; both merge into existing roots.
- Council D blind source/spec pass: 15 theories, 0 new accepts, 2 merges, 13
  rejects.
- Council C: 24 completed hostile reviews before stop; 9 existing roots
  completed its two-review gate, with no new accepted root.

Primary council reports:

- `audit_work/council/deep_firmware_spec_20260606/COUNCIL_REPORT.md`
- `audit_work/council/blind_reconstruction/REPORT.md`
- `audit_work/council/pcb_blind/REPORT.md`
- `audit_work/council/council_d/REPORT.md`
- `audit_work/council/council_c/REPORT.md`
- `audit_work/council/final_reconcile/member_a/REPORT.md`
- `audit_work/council/final_reconcile/member_b/REPORT.md`

## Adjudicated Theories

| # | Theory | Status | Council result and deciding evidence |
|---:|---|---|---|
| 1 | CY8C9560 initialization omitted | ACCEPT | Source and executable trace; repeated hostile acceptance. |
| 2 | NMEA receive buffer overflow | ACCEPT | Ordinary 75-byte RMC reaches ASan overflow. |
| 3 | 40-pin masks use narrow shifts | ACCEPT | UBSan and target integer-width proof. |
| 4 | `set_output()` makes every GPIO output | ACCEPT, one council dissented on separate count | Source writes all-port `0x00`; separate counterfactual contention path. |
| 5 | `set_pd_inputs()` removes selected output | ACCEPT | Source writes all-port `0xFF` before read. |
| 6 | Any one matching row passes | ACCEPT | Direct state-machine reproduction. |
| 7 | FAILED becomes GOOD next loop | ACCEPT | Direct state-machine reproduction. |
| 8 | GPS UART wired same-direction | ACCEPT | Schematic and PCB net-number trace. |
| 9 | SDA pulled to GND | ACCEPT | R3 net trace; no viable idle-high source. |
| 10 | RGB channels lack current limiting | ACCEPT, one council rejected strict as-built severity | Missing series components are source-proven; adjacent LED faults mask live current. |
| 11 | D3 common anode is physically open | ACCEPT | Two-review PCB council plus Gerber connectivity. |
| 12 | Button polarity inverted | ACCEPT | Pull-up/switch topology and firmware branch agree. |
| 13 | MAX2679 VCC overvolted by VCC_RF | ACCEPT | VCC_RF near 3.2V versus 2.2V absolute maximum. |
| 14 | Default `$GNRMC` rejected | ACCEPT | Selected-module default and absent reconfiguration. |
| 15 | Logical/raw expander gap after CBL_19 | ACCEPT | Port-2 four-bit geometry and register packing. |
| 16 | LED GPIOs never OUTPUT | ACCEPT | Teensy pin-mode trace. |
| 17 | Expected matrix violates passive closure | ACCEPT_REPLACEMENT | Six components; 36/40 rows differ. Replaces old comment-only wording. |
| 18 | Physical red/blue dies swapped | ACCEPT_REWRITE | Broadcom pin table plus PCB pad nets. |
| 19 | MAX2679 RFIN network absent/misplaced | ACCEPT_REWRITE | Mandatory input network is missing; output is internally matched. |
| 20 | D2 is not a VGS Zener clamp | ACCEPT, blind council rejected default-trigger severity | Device type and source-gate topology are exact; consequence depends on the stated transient environment. |
| 21 | U4 package/footprint mismatch | ACCEPT | Two-review PCB council; package pitch/body mismatch is decisive. |
| 22 | AE1 keepout removes RF reference/counterpoise | DISPUTED | Geometry and requirement are real; deterministic loss of lock is unmeasured. |
| 23 | C7 10 uF/0402 on +12V | DISPUTED | No rated production tuple found; no MPN/rating is specified and failure mode is not guaranteed. |
| 24 | D1 forward-conducts on reverse input | DISPUTED | Raw-side fault path is real; Q1 still isolates load and severity is source-dependent. |
| 25 | Held trigger repeats tests/logs | DISPUTED | Mechanically true; below strict show-stopper threshold. |
| 26 | RMC status/checksum/shape not validated | MERGE/DISPUTED | One validation root; status `V` alone does not prove invalid UTC. |
| 27 | Stock Teensy VIN/VUSB bridge backfeeds during powered USB service | DISPUTED | Physical path is real; simultaneous service-power mode is optional. |
| 28 | GPS SAFEBOOT/RESET pins require OUTPUT mode | REJECT | Input-mode writes create weak pulls; reset has internal pull-up and startup order provides no deterministic failure. |
| 29 | MAX2679 RFOUT/SHDNB needs a pull-up | REJECT | Official datasheet says floating RFOUT/SHDNB enables active mode. |
| 30 | J3.2 shorts CBL_1/CBL_2 and later contacts are open | REJECT | Correct coordinate transform and Gerber graph show all 40 separate matching routes. |
| 31 | CY_SCL lacks a pull-up | REJECT | R2 is 4.7 kohm from CY_SCL to +3.3V. |
| 32 | L7805 necessarily thermally shuts down | REJECT | Load lower bound and effective board thermal resistance are not established. |
| 33 | Digital harness traces run under the antenna body | REJECT | Rounded-body geometry gives 0.457-1.438 mm clearance. |
| 34 | MAX2679 WLP footprint is wrong | REJECT | 0.83 mm body, 0.4 mm pitch, and bump map match. |
| 35 | NEO-M8N package footprint is wrong | REJECT | Body, pitch, and all 24 transformed pads match. |
| 36 | Q1 P-channel MOSFET is backwards | REJECT | Drain/raw input, source/protected rail is the intended topology. |
| 37 | D1 package or polarity is wrong | REJECT | DO-214AC geometry and cathode-to-positive orientation match. |
| 38 | D2 package or polarity is wrong | REJECT | CFP3 dimensions and K/A mapping match. |
| 39 | C6 and U5 physically collide | REJECT | Positive body and copper clearances; only courtyards overlap. |
| 40 | U2 copper-edge spacing makes the board unbuildable | REJECT | 0.48 mm is a 0.02 mm project-rule miss, not a supplied fab limit. |
| 41 | Fabrication archive is stale | REJECT | PCB, Gerbers, drills, and project archive are byte-identical. |
| 42 | Gerbers omit copper, mask, paste, drills, or profile geometry | REJECT | Full source-to-fabrication parity pass found no divergence. |
| 43 | NEO-M8 V_BCKP tied to VCC is invalid | REJECT | u-blox explicitly permits this when no backup supply is used. |
| 44 | NEO-M8 VDD_USB tied to GND is invalid | REJECT | u-blox explicitly requires GND when USB is unused. |
| 45 | CY8C9560 reset polarity is inverted | REJECT | XRES is active high; HIGH then LOW is correct assert/release. |
| 46 | CY8C9560 device ID extraction is wrong | REJECT | Register 0x2E high nibble 6 identifies the 60-I/O part. |
| 47 | CY8C9560 address constant includes the R/W bit | REJECT | `0b0100000` is the correct seven-bit address 0x20. |
| 48 | `Wire2` uses the wrong Teensy pins | REJECT | Wire2 SDA25/SCL24 matches U2 pads 17/16. |
| 49 | Old CY drive modes must be explicitly cleared | REJECT | Last-written-one priority selects the new mode. |
| 50 | Missing explicit CY settle delay breaks current probes | REJECT | Current reachable ports exceed the 2.485 ms access interval before sampling. |
| 51 | I2C short reads/stale port selects are a standalone bug | REJECT | Real conditional error handling, but requires an injected transaction failure. |
| 52 | Missing SD card halt is a normal-operation show-stopper | REJECT | Requires absent or failed external media; fail-stop behavior is explicit. |
| 53 | Logged timestamp is invalid because the probe takes about 594 ms | REJECT | Bounded latency; no freshness requirement is specified. |
| 54 | MAX2679 lacks its required 1000 pF VCC bypass | REJECT | C6 is 1 nF from VCC_RF to GND close to U5. |
| 55 | GNSS patch requires a populated pi matching network | REJECT | Manufacturer recommends a footprint but does not require matching for this antenna series. |
| 56 | Open NEO-M8 LNA_EN prevents the MAX2679 from running | REJECT | U5 is continuously powered and floating RFOUT/SHDNB selects active mode. |
| 57 | Expected matrix is asymmetric | REJECT | Mechanical extraction found a symmetric relation; the real defect is missing transitive closure. |
| 58 | An all-open harness passes because an expected row is zero | REJECT | No expected row is zero. |
| 59 | Teensy `sscanf` lacks floating-point support | REJECT | Target-compatible reproduction parses the skipped float fields. |
| 60 | C1 has the same 0402 voltage impossibility as C7 | REJECT | Qualified 1 uF/35V/0402 production parts exist. |

## Admission Summary

- Strict accepted/rewrite roots: 21
- Organizer-boundary candidates: 6
- Merged concerns: retained with their parent roots
- Rejected theories: preserved above
- Honest accepted count after this audit: not 35

The largest source-backed submission is the 21-item core in
`FINAL_FINDINGS_REAUDITED.md`, optionally followed by boundary items 22-25.

## 2026-06-08 Workflow Expansion

| # | Theory | Status | Deciding evidence |
|---:|---|---|---|
| 61 | Post-open SD write-call failures are ignored | ACCEPT | Source ignores all write counts; target-faithful all-or-zero call failure truncates the record silently. |
| 62 | Stock Teensy VIN/VUSB bridge can backfeed powered USB | ACCEPT_WARNING | Carrier powers VIN and leaves VUSB unconnected; module bridge is documented, but simultaneous-power use is conditional. |
| 63 | Driver ignores all I2C transaction outcomes | REJECT_WARNING | Source fact is exact, but all three final council seats found the harmful path fault-gated and merged into initialization/SDA failures. |
| 64 | Post-lock partial RMC tears time/date state | MERGE | Dedicated regression passes; atomic parse/validate/commit is the existing RMC-validation repair. |
| 65 | GPS control writes remain weak pulls | DISPUTED_WARNING | Mode defect is exact; deterministic reset/safe-boot failure is not established. |
| 66 | Held trigger repeats complete records | DISPUTED_WARNING | Deterministic after polarity repair; below strict show-stopper severity. |
| 67 | Scan blocks Serial1 long enough to drop bytes | ACCEPT_WARNING | Dedicated repaired-P9 test proves independent transport loss; verdict remains unaffected and later epochs recover. |
| 68 | NMEA append and terminator writes are separate roots | MERGE | Separate sites and sanitizer stacks, one reserve-space capacity repair. |
| 69 | Reverse input makes D1 conduct | REJECT | Expected unidirectional-TVS behavior; protected load remains isolated by Q1. |
| 70 | Fresh post-repair firmware pass found another root | REJECT_NO_CANDIDATE | Three independent direct source audits found no new admissible firmware root. |

Current deliverables:

- Organizer-targeted: `FINAL_FINDINGS_24_SUBMISSION.md`
- Extended mechanisms: `FINAL_FINDINGS_30_SOURCE_BACKED.md`
