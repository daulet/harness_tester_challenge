# PCB/Schematic Findings Tracker

Working ledger as of 2026-06-04. This file tracks candidate claims and their relationship to `../../../harness_tester_challenge/BUG_THEORY_LOG.md`; it is not a second approved-theory log.

## Current count

- 5 candidate claims were reviewed in this pass.
- 5/5 overlap existing theory-log entries.
- 0 novel candidates remain from this pass.
- 3/5 are already represented by approved entries: items 1 and 2 are absorbed into A26, and item 5 is a duplicate of A27.
- 2/5 are already rejected: item 3 is R19, and item 4 is R20.
- No exact simulator bug was found that changes any disposition. The simulator remains non-authoritative; actual KiCad source and the theory log controlled the review.

## Candidate ledger

| # | Candidate claim | Current disposition | Existing log relation | Why |
| --- | --- | --- | --- | --- |
| 1 | U5 RFIN has no required input DC-block or external match | Not novel; absorbed into A26 | A26 approved | A26 already says the RFIN matching/DC-block network is omitted from the input and misplaced on RFOUT. |
| 2 | C5/L1 are on U5 RFOUT instead of U5 RFIN | Not novel; absorbed into A26 | A26 approved | This is the same RF-front-end root-cause cluster as item 1, not a separate bubble. |
| 3 | C5 is 100n instead of the MAX2679 reference 1000pF / 1n value | Rejected | R19 rejected | `100n` is the series RF coupling/DC-block value; the `1000pF` reference capacitor is the VCC bypass part, not C5. |
| 4 | Antenna keepout removes the patch ground plane | Rejected / needs narrower wording | R20 rejected | The keepout is real, but the TE documentation supports recommended counterpoise practice rather than a hard requirement for copper directly beneath the exact footprint. |
| 5 | Antenna feed is not a controlled-impedance RF path through the antenna region | Not novel; duplicate of A27 | A27 approved | A27 already has the same keepout, local-reference, trace-width, and controlled-impedance thesis. |

## Review provenance

- `review-rf-rfin`: agent rejected it as a new candidate; coordinator Claude review returned duplicate/subset of A26.
- `review-rf-output-match`: agent accepted the underlying electrical defect but said it is not a separate bubble; coordinator Claude review returned duplicate/subset of A26.
- `review-rf-c5-value`: agent rejected it; coordinator Claude review returned duplicate of rejected R19.
- `review-antenna-ground`: agent returned needs narrowing; coordinator Claude review returned duplicate of rejected R20.
- `review-antenna-feed`: agent rejected it as a duplicate; coordinator Claude review returned duplicate of A27.
- Each spawned agent attempted a non-interactive Claude CLI review with the simulator caveat. Their local agent shells could not see the local Claude login, so the coordinator shell reran the required Claude reviews non-interactively with the same caveat and relayed the results back into the agent threads.

## Separate weak or unpromoted leads

- U3 LNA_EN is unconnected while U5 is an external LNA.
  - Status: weak.
  - Reason: u-blox documents ANT_ON/LNA_EN as optional external-LNA control, so this is a power-management omission rather than a clear bring-up blocker.

- C7 is 10u in a 0402 footprint on +12V.
  - Status: weak.
  - Reason: no exact MPN, voltage rating, or dielectric is specified, so the schematic alone does not prove an invalid capacitor.

- Teensy VIN/VUSB isolation is not addressed.
  - Status: weak.
  - Reason: this is a real integration risk if USB and external power are used together, but it depends on the intended power-use case.

## Source anchors

- A26, A27, R19, and R20 in `../../../harness_tester_challenge/BUG_THEORY_LOG.md`
- `kicad_files/hardware_challenge.kicad_pcb`
- `kicad_files/hardware_challenge.kicad_sch`
- Official MAX2679/MAX2679B, NEO-M8, and TE antenna documentation already cited in the theory log
