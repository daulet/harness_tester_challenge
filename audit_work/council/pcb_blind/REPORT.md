# Blind PCB Layout Audit Final Report

Date: 2026-06-06
Scratch root: `audit_work/council/pcb_blind/`
Protected source root: repository root, read-only for this audit

## Result

The blind freeze contained exactly two promoted candidates. Both survive local
adjudication and two separate noninteractive Claude disproof reviews. They are
distinct physical roots, but both are already present in the submitted
findings:

- accepted blind roots: 2;
- rejected promoted candidates: 0;
- merged into existing submitted roots: 2;
- omitted frozen roots: 0;
- submitted roots disproved on the frozen comparison surface: 0.

No new candidate was opened during finalization.

## Compact Candidate Council

| ID | Frozen theory | Local verdict | Claude A | Claude B | Disagreement and adjudication | Submitted mapping | Final status |
|---|---|---|---|---|---|---|---|
| C01 | U4's `CY8C9560A-24AXIT` package cannot register to the selected 12 mm, 0.40 mm-pitch TQFP-100 land pattern. | **ACCEPT**, 0.99 | **ACCEPT**, 0.96 | **ACCEPT**, 0.96 | Reviewers differed only on how many central leads might partially overlap. The verdict uses the decisive 0.50 mm package pitch versus 0.40 mm pad pitch and does not claim an exact contact count. | `FINAL_FINDINGS.md:105-107`, finding 26; `BUG_THEORY_LOG.md:176-181`, A29 | **MERGED-ACCEPTED**, 0.99 |
| C02 | D3 pad 1 reaches a through-via that terminates in an isolated In1.Cu `+3.3V` island, leaving the common anode physically open. | **ACCEPT**, 0.98 | **ACCEPT**, 0.97 | **ACCEPT**, 0.93 | Reviewers used different zero/one-based polygon ordinals. Adjudication identifies the island by coordinates and bounding box, not ordinal. | `FINAL_FINDINGS.md:53-55`, finding 13; `BUG_THEORY_LOG.md:93-97`, A16 | **MERGED-ACCEPTED**, 0.98 |

C01 and C02 are not merged with each other. "Merged-accepted" means each blind
root deduplicates into the corresponding existing submitted root.

## C01 Evidence

**Root:** package/pad geometry makes reliable U4 assembly impossible.

- Board source:
  `kicad_files/hardware_challenge.kicad_pcb:8915-8932` identifies U4 as
  `CY8C9560A-24AXIT` in
  `Package_QFP:TQFP-100_12x12mm_P0.4mm`.
- Pad source:
  `kicad_files/hardware_challenge.kicad_pcb:9177-9192` shows 0.20 mm pad width
  and 0.40 mm pad-to-pad pitch.
- Machine measurements:
  `audit_work/council/pcb_blind/evidence/board_analysis/components.csv` and
  `audit_work/council/pcb_blind/evidence/board_analysis/pads.csv`.
- Primary drawing:
  `audit_work/council/pcb_blind/primary_drawings/CY8C9560A_datasheet.pdf`,
  page 22, Figure 12, specifies the 100-pin package as 14 x 14 x 1.0 mm with
  0.50 mm lead pitch and 16.0 mm nominal lead span.
- Extracted primary text:
  `audit_work/council/pcb_blind/primary_drawings/CY8C9560A_datasheet.txt:1698`.
- Evidence summary and render:
  `audit_work/council/pcb_blind/evidence/C01_U4_PACKAGE.md` and
  `audit_work/council/pcb_blind/renders/CY8C9560A_package_page22.png`.
- Independent reviews:
  `audit_work/council/pcb_blind/reviews/C01_claude_A.md` and
  `audit_work/council/pcb_blind/reviews/C01_claude_B.md`.

The 25-lead side has 24 pitch intervals, so the 0.10 mm per-interval error
produces a 2.40 mm end-to-end row mismatch. Rotation cannot repair a scale and
pitch mismatch.

## C02 Evidence

**Root:** fabricated copper leaves D3's common anode disconnected from the
board's main `+3.3V` network.

- Pad metadata:
  `kicad_files/hardware_challenge.kicad_pcb:915-922` assigns D3:1 to net 5
  `+3.3V`.
- Source route:
  `kicad_files/hardware_challenge.kicad_pcb:15754-15760` routes from
  `(158.1000, 32.9000)` to `(157.2444, 32.9000)`;
  `kicad_files/hardware_challenge.kicad_pcb:15810-15832` continues to the
  plated through-via at `(157.2260, 32.9184)`.
- Fabrication coordinate:
  `audit_work/council/pcb_blind/extracted/gerbers/hardware_challenge-F_Cu.gbr:146-148`
  flashes D3:1 at `(158.1000, -32.9000)` under the verified transform
  `x_gbr=x_kicad`, `y_gbr=-y_kicad`.
- Machine connectivity:
  `audit_work/council/pcb_blind/evidence/gerber_connectivity.json` reports no
  physical shorts and exactly one split multi-terminal net: D3:1 alone versus
  the other eleven net-5 terminals.
- Geometry and method:
  `audit_work/council/pcb_blind/evidence/C02_D3_OPEN.md`,
  `audit_work/council/pcb_blind/evidence/board_analysis/board_geometry.json`,
  and `audit_work/council/pcb_blind/renders/D3_pad1_FCu_crop.png`.
- Independent reviews:
  `audit_work/council/pcb_blind/reviews/C02_claude_A.md` and
  `audit_work/council/pcb_blind/reviews/C02_claude_B.md`.

The through-via has no rescuing path on F.Cu, In2.Cu, or B.Cu. Its In1.Cu
island is separated from the main plane by about 0.3914 mm.

## Submitted-Finding Comparison

No frozen root is omitted. C01 maps to finding 26/A29; C02 maps to finding
13/A16. No submitted root on this two-candidate comparison surface is false.
Other submitted findings were not reopened after the blind freeze.

There is one false submitted evidence detail, but not a false root:

- `FINAL_FINDINGS.md:106` and `BUG_THEORY_LOG.md:178` call the CY8C9560A
  package `14 x 14 x 1.4 mm`.
- The manufacturer drawing at
  `audit_work/council/pcb_blind/primary_drawings/CY8C9560A_datasheet.pdf`,
  page 22, says `14 x 14 x 1.0 mm`.
- C01 remains accepted because the decisive mismatches are 0.50 mm versus
  0.40 mm pitch, 14 mm versus 12 mm body family, and 16.0 mm lead span versus
  the fabricated land pattern.

## Rejected Theory Ledger

All blind rejects remain preserved in
`audit_work/council/pcb_blind/REJECTS.md`. None was promoted after freeze.

| IDs | Rejected theory group | Exact evidence paths |
|---|---|---|
| R01, R02, R12 | Other physical shorts, other multi-terminal opens, or copper touching wrong-net pads | `audit_work/council/pcb_blind/evidence/board_analysis/connectivity.json`; `audit_work/council/pcb_blind/evidence/gerber_connectivity.json` |
| R03 | MAX2679 WLP mismatch | `kicad_files/hardware_challenge.kicad_pcb`; `audit_work/council/pcb_blind/evidence/board_analysis/pads.csv` |
| R04 | NEO-M8N package mismatch | `audit_work/council/pcb_blind/primary_drawings/NEO-M8_datasheet.pdf`; `audit_work/council/pcb_blind/primary_drawings/NEO-M8_datasheet.txt`; `audit_work/council/pcb_blind/evidence/board_analysis/pads.csv` |
| R05 | GNSS antenna body/feed geometry prevents assembly | `audit_work/council/pcb_blind/primary_drawings/ANT-GNSSCP-TH25L1_datasheet.pdf`; `audit_work/council/pcb_blind/renders/ANT_dimensions_page2.png`; `audit_work/council/pcb_blind/evidence/board_analysis/pads.csv` |
| R06 | J1 mounting holes and pads are incompatible | `audit_work/council/pcb_blind/extracted/project/libraries/FC681465S smd.stp`; `audit_work/council/pcb_blind/evidence/board_analysis/pads.csv` |
| R07 | J3 is reversed or cannot mate at the edge | `audit_work/council/pcb_blind/evidence/board_analysis/pads.csv`; `imgs/board.png`; `audit_work/council/pcb_blind/extracted/gerbers/hardware_challenge-Edge_Cuts.gbr` |
| R08 | Teensy carrier cannot fit or be socketed | `audit_work/council/pcb_blind/evidence/board_analysis/components.csv`; `audit_work/council/pcb_blind/extracted/project/libraries/teensy.pretty/Teensy_4.1_Assembly.STEP` |
| R09 | SiSS27DN PowerPAK footprint mismatch | `audit_work/council/pcb_blind/evidence/board_analysis/pads.csv`; `audit_work/council/pcb_blind/extracted/project/libraries/powerpak.STEP` |
| R10 | Copper stackup or layer-reference swap | `kicad_files/hardware_challenge.kicad_pcb`; `audit_work/council/pcb_blind/extracted/gerbers/hardware_challenge-job.gbrjob` |
| R11 | Gerber/drill shift or mirror | `audit_work/council/pcb_blind/evidence/board_analysis/board_geometry.json`; `audit_work/council/pcb_blind/evidence/board_analysis/pads.csv`; `audit_work/council/pcb_blind/extracted/gerbers/hardware_challenge-PTH.drl` |

## Integrity and Reproduction

- Freeze record:
  `audit_work/council/pcb_blind/CANDIDATE_FREEZE.md`.
- Council disagreement record:
  `audit_work/council/pcb_blind/council/DISAGREEMENTS.md`.
- Source/archive hashes and coordinate transform:
  `audit_work/council/pcb_blind/evidence/SOURCE_MANIFEST.md`.
- Final protected-source snapshot:
  `audit_work/council/pcb_blind/evidence/FINAL_INTEGRITY.md`.
- Analysis programs:
  `audit_work/council/pcb_blind/scripts/analyze_board.py` and
  `audit_work/council/pcb_blind/scripts/gerber_connectivity.py`.
- Final independent rerun:
  `audit_work/council/pcb_blind/verification/board_analysis/` and
  `audit_work/council/pcb_blind/verification/gerber_connectivity.json`.

Both analysis programs pass `python3 -m py_compile`. The regenerated Gerber
JSON and pad/component inventories are byte-identical to the frozen evidence.
Original artifacts, fabrication outputs, and shared logs were not edited; all
audit writes are confined to `audit_work/council/pcb_blind/`.
