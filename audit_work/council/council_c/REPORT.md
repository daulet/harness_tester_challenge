# Council C Final Reconciliation

Date: 2026-06-06

## Scope and stop condition

Council C reviewed the frozen 23-candidate set in
`audit_work/council/council_c/candidates.tsv`. Per the final instruction, all
remaining Claude processes were stopped and no additional reviews were
started. This report uses only source inspection and outputs already present
when the stop was issued.

No firmware, KiCad, README, `FINAL_FINDINGS.md`, or `BUG_THEORY_LOG.md` files
were edited.

## Review completion

- Planned Claude invocations: 46 (two for each of 23 candidates)
- Completed with exit status 0: 24
- Interrupted after starting: 1 (`F13_A`; empty stdout, no status file)
- Never started: 21
- Candidates with both reviews complete: 11
- Candidates with one completed review: 2
- Candidates with no completed review: 10
- Non-empty reviewer stderr files: 0

For every started invocation `<ID>_<reviewer>`, the preserved artifacts are:

- `audit_work/council/council_c/prompts/<ID>_<reviewer>.txt`
- `audit_work/council/council_c/stdout/<ID>_<reviewer>.txt`
- `audit_work/council/council_c/stderr/<ID>_<reviewer>.txt`
- `audit_work/council/council_c/status/<ID>_<reviewer>.status`, if completed

`F13_A` has prompt/stdout/stderr artifacts but no status file. Never-started
reviews have no artifacts and are reported as `NOT STARTED`.

## Counts

Disposition of the frozen 23 candidates:

- Accepted existing roots: 11
- Rejected: 2
- Merged: 1
- Disputed and unreviewed: 1
- Unadjudicated because both reviews were never started: 8
- Accepted new independent roots: 0

Review-strength split:

- Fully two-reviewed strict accepts: 9
- Partially reviewed accepts: 2 (`F13`, `F15`)
- Fully two-reviewed rejection: 1 (`F12`)
- Fully two-reviewed merge: 1 (`F06` into `F07`)
- Local source rejection without a Council C Claude review: 1 (`N01`)

Thus Council C can certify a strict core of **9 accepted roots through the
requested two-review gate**. It records **11 accepted roots in total** when the
two source-supported but partially reviewed existing findings are included.

## Per-finding adjudication

| ID | Local verdict | Review A | Review B | Final Council C verdict |
|---|---|---|---|---|
| F01 | ACCEPT_EXISTING | ACCEPT, complete | MERGE to canonical finding 1, complete | ACCEPT_EXISTING; reviewers agree on the defect, B only rejects recounting it as new |
| F03 | ACCEPT_EXISTING | ACCEPT, complete | ACCEPT, complete | ACCEPT_EXISTING |
| F05 | ACCEPT_EXISTING | ACCEPT, complete | ACCEPT, complete | ACCEPT_EXISTING |
| F06 | MERGE into F07 at strict count boundary | ACCEPT, complete | MERGE into F07, complete | MERGE into F07; preserve the transient-direction observation as supporting evidence, not an independent root |
| F07 | ACCEPT_EXISTING | ACCEPT, complete | ACCEPT, complete | ACCEPT_EXISTING |
| F08 | ACCEPT_EXISTING | ACCEPT, complete | ACCEPT, complete | ACCEPT_EXISTING |
| F09 | ACCEPT_EXISTING | ACCEPT, complete | ACCEPT, complete | ACCEPT_EXISTING |
| F10 | ACCEPT_EXISTING | ACCEPT, complete | ACCEPT, complete | ACCEPT_EXISTING |
| F11 | ACCEPT_EXISTING | ACCEPT, complete | ACCEPT, complete | ACCEPT_EXISTING |
| F12 | REJECT as deterministic as-built show-stopper | REJECT, complete | REJECT, complete | REJECT |
| F13 | ACCEPT_EXISTING | INTERRUPTED; no verdict | MERGE to canonical finding 13, complete | ACCEPT_EXISTING, partial review coverage; B confirms the copper fault but correctly prevents duplicate counting |
| F14 | ACCEPT_EXISTING | ACCEPT, complete | MERGE to canonical finding 14, complete | ACCEPT_EXISTING; reviewers agree on the defect, B only rejects recounting it as new |
| F15 | ACCEPT_EXISTING | ACCEPT, complete | NOT STARTED | ACCEPT_EXISTING, partial review coverage |
| F16 | PROVISIONAL_ACCEPT from local/source record | NOT STARTED | NOT STARTED | UNADJUDICATED_BY_COUNCIL_C |
| F17 | PROVISIONAL_ACCEPT from local/source record | NOT STARTED | NOT STARTED | UNADJUDICATED_BY_COUNCIL_C |
| F18 | PROVISIONAL_ACCEPT from local/source record | NOT STARTED | NOT STARTED | UNADJUDICATED_BY_COUNCIL_C |
| F19 | PROVISIONAL_ACCEPT from local/source record | NOT STARTED | NOT STARTED | UNADJUDICATED_BY_COUNCIL_C |
| F21 | PROVISIONAL_ACCEPT from local/source record | NOT STARTED | NOT STARTED | UNADJUDICATED_BY_COUNCIL_C |
| F23 | PROVISIONAL_ACCEPT from local/source record | NOT STARTED | NOT STARTED | UNADJUDICATED_BY_COUNCIL_C |
| F25 | PROVISIONAL_ACCEPT from local/source record | NOT STARTED | NOT STARTED | UNADJUDICATED_BY_COUNCIL_C |
| F26 | PROVISIONAL_ACCEPT from local/source record | NOT STARTED | NOT STARTED | UNADJUDICATED_BY_COUNCIL_C |
| F24 | DISPUTED | NOT STARTED | NOT STARTED | DISPUTED_UNREVIEWED; no deterministic independent root established |
| N01 | REJECT_LOCAL | NOT STARTED | NOT STARTED | REJECT_LOCAL; primary MAX2679 operating-mode evidence defeats the required-pull-up thesis, but this did not pass a Council C Claude gate |

## Evidence index

### F01 - CY8C9560 initialization omitted

- Primary source: `firmware/firmware.ino`,
  `firmware/CY8C9560.cpp`, `firmware/CY8C9560.h`
- Reviews: `audit_work/council/council_c/stdout/F01_A.txt`,
  `audit_work/council/council_c/stdout/F01_B.txt`
- Disagreement: B uses `MERGE` because the candidate is the canonical existing
  finding, not because the mechanism is false.

### F03 - NMEA receive buffer overflow

- Primary source: `firmware/firmware.ino`
- Reviews: `audit_work/council/council_c/stdout/F03_A.txt`,
  `audit_work/council/council_c/stdout/F03_B.txt`
- Result: both accept the same existing root.

### F05 - Narrow shifts in 40-pin masks

- Primary source: `firmware/firmware.ino`
- Reviews: `audit_work/council/council_c/stdout/F05_A.txt`,
  `audit_work/council/council_c/stdout/F05_B.txt`
- Result: both accept.

### F06 - set_output makes every expander pin an output

- Primary source: `firmware/CY8C9560.cpp:77`,
  `firmware/firmware.ino:143`
- Reviews: `audit_work/council/council_c/stdout/F06_A.txt`,
  `audit_work/council/council_c/stdout/F06_B.txt`
- Disagreement: A accepts a distinct transient all-output/contention root. B
  finds that the masked STRONG register gives only the selected pin strong
  drive and that corrected F07 restores the measurement end state.
- Resolution: under the strict independent-root standard, merge F06 into F07.
  The code observation remains valid evidence but is not counted separately.

### F07 - set_pd_inputs removes the selected output

- Primary source: `firmware/CY8C9560.cpp:61`,
  `firmware/firmware.ino:143`
- Reviews: `audit_work/council/council_c/stdout/F07_A.txt`,
  `audit_work/council/council_c/stdout/F07_B.txt`
- Result: both accept the read-time loss of the probe drive.

### F08 - Any one matching row passes

- Primary source: `firmware/firmware.ino`
- Reviews: `audit_work/council/council_c/stdout/F08_A.txt`,
  `audit_work/council/council_c/stdout/F08_B.txt`
- Result: both accept.

### F09 - FAILED status overwritten by GOOD

- Primary source: `firmware/firmware.ino`, `README.md`
- Reviews: `audit_work/council/council_c/stdout/F09_A.txt`,
  `audit_work/council/council_c/stdout/F09_B.txt`
- Result: both accept.

### F10 - GPS UART is wired TX-to-TX and RX-to-RX

- Primary source: `kicad_files/hardware_challenge.kicad_sch`,
  `kicad_files/hardware_challenge.kicad_pcb`
- Reviews: `audit_work/council/council_c/stdout/F10_A.txt`,
  `audit_work/council/council_c/stdout/F10_B.txt`
- Result: both accept from schematic/PCB and component pin functions.

### F11 - CY8C9560 SDA is pulled down

- Primary source: `kicad_files/hardware_challenge.kicad_sch:16289`,
  `kicad_files/hardware_challenge.kicad_sch:16564`
- Reviews: `audit_work/council/council_c/stdout/F11_A.txt`,
  `audit_work/council/council_c/stdout/F11_B.txt`
- Result: both accept; B independently traces R3 from `CY_SDA` to GND.

### F12 - RGB LED channels lack current limiting

- Primary source: `kicad_files/hardware_challenge.kicad_sch`,
  `kicad_files/hardware_challenge.kicad_pcb`
- Reviews: `audit_work/council/council_c/stdout/F12_A.txt`,
  `audit_work/council/council_c/stdout/F12_B.txt`
- Result: both reject the claimed deterministic as-built show-stopper. D3's
  anode is isolated by F13 and its cathode GPIOs are not configured as outputs
  by F18, so the no-resistor condition does not itself create channel current
  on the submitted board. After those repairs, actual current still depends on
  LED forward voltage and GPIO drive behavior. Keep it as a design defect or
  conditional hazard, not a strict independent failing root.

### F13 - D3 common anode is physically isolated

- Primary source: `kicad_files/hardware_challenge.kicad_pcb:915`,
  `kicad_files/hardware_challenge.kicad_pcb:15754`,
  `kicad_files/hardware_challenge.kicad_pcb:15826`,
  `audit_work/kicad8_drc.json`,
  `audit_work/pcb_continuity/all_net_summary.txt`
- Review A: `audit_work/council/council_c/prompts/F13_A.txt`,
  `audit_work/council/council_c/stdout/F13_A.txt`,
  `audit_work/council/council_c/stderr/F13_A.txt`; interrupted, empty output,
  no status file
- Review B: `audit_work/council/council_c/stdout/F13_B.txt`; confirms the
  physical isolation and merges it to the already canonical finding 13
- Result: accept the existing root, but record only one completed review.

### F14 - Test button polarity is inverted

- Primary source: `firmware/firmware.ino:104`,
  `firmware/firmware.ino:137`,
  `kicad_files/hardware_challenge.kicad_sch:14996`,
  `kicad_files/hardware_challenge.kicad_sch:16219`
- Reviews: `audit_work/council/council_c/stdout/F14_A.txt`,
  `audit_work/council/council_c/stdout/F14_B.txt`
- Disagreement: B uses `MERGE` only because this is identical to canonical
  finding 14. Both reviewers accept the electrical/control-flow mechanism.

### F15 - MAX2679 supply is overvolted

- Primary source: `kicad_files/hardware_challenge.kicad_sch`,
  `kicad_files/hardware_challenge.kicad_pcb`
- Review A: `audit_work/council/council_c/stdout/F15_A.txt`; ACCEPT
- Review B: not started; no artifacts
- Result: accept the existing root with explicit one-of-two review coverage.

### F16 - Firmware rejects default GNRMC

- Primary source: `firmware/firmware.ino`,
  `kicad_files/hardware_challenge.kicad_sch`
- Reviews A/B: not started; no artifacts
- Result: no Council C final adjudication.

### F17 - Logical harness indexes diverge from raw expander bits

- Primary source: `firmware/firmware.ino`, `firmware/CY8C9560.cpp`,
  `kicad_files/hardware_challenge.kicad_sch`,
  `kicad_files/hardware_challenge.kicad_pcb`
- Reviews A/B: not started; no artifacts
- Result: no Council C final adjudication.

### F18 - Status LED GPIOs never become outputs

- Primary source: `firmware/firmware.ino`,
  `kicad_files/hardware_challenge.kicad_sch`
- Reviews A/B: not started; no artifacts
- Result: no Council C final adjudication.

### F19 - Expected harness oracle violates passive transitive closure

- Primary source: `firmware/firmware.ino`,
  `audit_work/council/matrix_closure/check_matrix.py`,
  `audit_work/council/deep_firmware_spec_20260606/evidence/matrix_closure.txt`
- Reviews A/B: not started; no artifacts
- Result: no Council C final adjudication. Simulator-derived claims are not
  used as independent proof; the listed matrix/source analysis is the relevant
  evidence boundary.

### F21 - Physical red and blue LED dies are swapped

- Primary source: `kicad_files/hardware_challenge.kicad_sch`,
  `kicad_files/hardware_challenge.kicad_pcb`
- Reviews A/B: not started; no artifacts
- Result: no Council C final adjudication.

### F23 - MAX2679 RFIN lacks required input network

- Primary source: `kicad_files/hardware_challenge.kicad_sch`,
  `kicad_files/hardware_challenge.kicad_pcb`
- Reviews A/B: not started; no artifacts
- Result: no Council C final adjudication.

### F25 - D2 is not a Q1 gate-source transient clamp

- Primary source: `kicad_files/hardware_challenge.kicad_sch`,
  `kicad_files/hardware_challenge.kicad_pcb`
- Reviews A/B: not started; no artifacts
- Result: no Council C final adjudication.

### F26 - CY8C9560 package does not fit assigned footprint

- Primary source: `kicad_files/hardware_challenge.kicad_sch`,
  `kicad_files/hardware_challenge.kicad_pcb`
- Reviews A/B: not started; no artifacts
- Result: no Council C final adjudication.

### F24 - MAX2679 antenna feed lacks a local RF reference

- Primary source: `kicad_files/hardware_challenge.kicad_pcb`
- Reviews A/B: not started; no artifacts
- Result: remains disputed. No completed Council C review establishes a
  deterministic show-stopper independent of F23.

### N01 - MAX2679 RFOUT-SHDNB has no enable bias

- Primary source: `kicad_files/hardware_challenge.kicad_sch`,
  `kicad_files/hardware_challenge.kicad_pcb`
- Existing independent source work:
  `audit_work/council/schematic_blind/candidates/SB-08_COUNCIL_VERDICT.md`,
  `audit_work/council/schematic_blind/reviews/SB-08_priority/`,
  `audit_work/council/blind_reconstruction/p04_council/TOPOLOGY_AND_PRIMARY_SOURCE.md`
- Reviews A/B: not started; no Council C artifacts
- Result: reject locally because the MAX2679 primary operating-mode evidence
  identifies floating RFOUT/SHDNB as active mode, defeating the thesis that a
  separate logic-high pull-up is required. This is not represented as having
  passed Council C's requested Claude gate.

## Final disagreements

1. **F06:** A accepted a separate transient-contention root; B merged it into
   F07. Council C adopts the merge for strict counting because F07 owns the
   uncontested measurement-time failure and B directly challenges the claimed
   strong-versus-strong transient.
2. **F01, F13, and F14:** `MERGE` responses refer to duplication with their
   already existing canonical finding, not to a false mechanism or a merge
   with another independent root.
3. **F12:** both reviewers reject the strict as-built consequence. This is the
   clearest candidate that should not remain in a show-stopper-only accepted
   list.
4. **N01:** locally rejected from primary source and prior preserved review
   evidence, but honestly marked as having zero completed Council C reviews.

## Final conclusion

No new independent root passed this Council C run. The completed adversarial
outputs reduce the proposed counted set by rejecting F12 and merging F06 into
F07. Nine existing roots have a complete two-review Council C acceptance
record. F13 and F15 remain source-supported existing accepts with incomplete
Council C review coverage. The eight untouched proposed accepts and disputed
F24 must not be described as Council C gated.
