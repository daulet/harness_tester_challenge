# Council D Blind Source and Specification Audit

Date: 2026-06-06

Repository commit: `069f7243d9ca87b31333d873bb33b08cfb54042a`

## Result

**Genuinely new independent show-stopping roots: 0.**

The blind pass froze 15 candidates before opening `BUG_THEORY_LOG.md`.
Primary-source adjudication produced:

- 0 new accepts
- 2 merges into existing canonical RF roots
- 13 rejects

The two merges are:

- D07 strengthens canonical finding 24: the AE1 all-layer keepout removes both
  the feed reference and the patch counterpoise. It is the same physical
  copper omission and the same correction.
- D10 strengthens canonical finding 23: L1 is an unnecessary RFOUT-side series
  element only because the required RFIN-side network was placed on the wrong
  side. It is not a separate root.

No simulator output was used as proof.

## Procedure

The blind boundary and admission rules are recorded in `PROTOCOL.md`. The
candidate freeze is `FROZEN_CANDIDATES.md`. Before that freeze, the audit read
the firmware, KiCad schematic and routed PCB, generated netlist, selected-part
primary documentation, and the canonical-root list in
`audit_work/council/final_reconcile/member_a/REPORT.md`.

Only after freezing D01-D15 were `BUG_THEORY_LOG.md`,
`FIRMWARE_DISCOVERY_PASS2_LEDGER.md`, and prior candidate ledgers opened for
deduplication.

Mechanical PCB evidence under `pcb/evidence/` reconstructs pad-to-copper
connectivity instead of trusting pad net metadata alone. It found:

- all 40 `CBL_*` nets each connect the intended J3 pad to the intended U4 pad;
- no cross-net copper overlap;
- no orphan routed component;
- the AE1 all-layer keepout and RF geometry described below.

The current PCB therefore does not reproduce the earlier J3.2 short/unattached
later-contact thesis. In this commit J3.2 is at `(174.125,96.890)`, J3.3 is at
`(171.585,94.350)`, and every `CBL_0` through `CBL_39` is one mechanically
continuous copper component.

## Frozen Candidate Adjudication

| ID | Final disposition | Primary/source determination | Review state |
|---|---|---|---|
| D01 | **REJECT** | MAX2679 A2 `RFOUT/SHDNB` is intentionally floating at DC through the L1/C5 path. The ADI datasheet explicitly defines floating A2 as active mode; a low through 25 kohm selects shutdown. | Completed prior hostile Claude rejection in `audit_work/council/wave2_rf/coordinator_reviews/RFOUT_SHDNB_FLOAT_DISPROVE.output.txt`; prior independent council rejections also exist. No fresh D gate needed after factual disproof. |
| D02 | **REJECT** | U5 is continuously powered by `VCC_RF` and defaults active through floating A2. u-blox describes `LNA_EN` as optional control for an external LNA, not a mandatory input to this always-powered configuration. | Completed prior hostile Claude rejection in `audit_work/council/wave2_rf_followup/coordinator/claude/outputs/RF-F10.output.txt`. No fresh D gate. |
| D03 | **REJECT** | U4 A0 is directly strapped to GND. Under the CY8C9560 addressing rules this selects normal address `0x20`; A1-A6 remain GPIO. The CBL_13-CBL_19 multiplexed pins are not sampled as extended address bits in this state. | **No candidate-specific completed Claude gate.** Primary Infineon evidence and the previous expander source audit reject it. |
| D04 | **REJECT** | CY8C9560 XRES is active high with an internal pull-down. `HIGH`, 10 ms, then `LOW`, 100 ms is the correct assert/release sequence. The `_N` net name is misleading, not authoritative silicon polarity. | Completed hostile Claude rejection in `audit_work/council/deep_firmware_spec_20260606/claude/outputs/DF23.txt`. |
| D05 | **REJECT** | The 64-bit read is literal, but all unselected valid GPIOs are configured as pull-down inputs and unimplemented Port-2 bits read zero. Upper bits do not independently contaminate the 40-channel comparison. The real neighboring roots are direction handling and the Port-2 raw-bit gap. | Completed hostile Claude rejection in `audit_work/council/deep_firmware_spec_20260606/claude/outputs/DF52.txt`. |
| D06 | **REJECT** | Factory state is output latch high with resistive pull-up mode, not opposed strong drivers. Any harmful all-output transition during probing is already canonical finding 6; omitted initialization is canonical finding 1. | Completed hostile Claude rejection in `audit_work/council/deep_firmware_spec_20260606/claude/outputs/DF54.txt`. |
| D07 | **MERGE 24** | The TE patch requires a ground plane for proper operation, and the board removes copper on all layers under AE1. This is real evidence, but it is the exact keepout, mechanism region, and correction already owned by canonical finding 24. Manufacturer latitude on plane size/location also prevents a separate guaranteed-loss-of-lock claim. | Completed prior hostile Claude verdict `MERGE` in `audit_work/council/wave2_rf_followup/coordinator/claude/outputs/RF-F08.output.txt`. Two fresh D gates were started but **interrupted by user direction** before output; status files are under `claude/D07/`. |
| D08 | **REJECT** | The thesis confuses C4 with C6. C6 is `1 nF`, exactly `1000 pF`, connected from U5 A1/`VCC_RF` to GND about 1.1 mm from U5. It satisfies the explicit MAX2679 bypass requirement. | Completed prior hostile Claude rejection in `audit_work/council/wave2_rf_followup/coordinator/claude/outputs/RF-F03.output.txt`. Two fresh D gates were **interrupted by user direction** before output; status files are under `claude/D08/`. |
| D09 | **REJECT** | TE recommends a pi footprint but states that this antenna series does not inherently require matching. Absence of a populated pi network is not a mandatory violated requirement or deterministic failure. | No candidate-specific fresh D gate. A completed prior hostile RF bundle rejected this class in `audit_work/council/wave2_rf_followup/coordinator/claude/outputs/RF-F10.output.txt`. |
| D10 | **MERGE 23** | MAX2679 RFOUT is internally matched. The board's 12 nH L1 and C5 are part of the same wrong-side network already captured by canonical finding 23: RFIN lacks its required DC block/input match while those parts sit after RFOUT. Same components and correction mean one root. | Completed prior hostile Claude verdict `MERGE` in `audit_work/council/wave2_rf_followup/coordinator/claude/outputs/RF-F01.output.txt`. |
| D11 | **REJECT** | u-blox explicitly requires VDD_USB to be connected to GND when USB is unused. U3 pad 7 implements the recommended unused-USB connection. | **No candidate-specific completed Claude gate.** Rejected directly by the primary u-blox requirement. |
| D12 | **REJECT** | u-blox explicitly permits connecting V_BCKP to VCC when no backup source is available. U3 pads 22 and 23 sharing +3.3 V implement that documented configuration. | **No candidate-specific completed Claude gate.** Rejected directly by the primary u-blox requirement. |
| D13 | **REJECT** | CY8C9560 drive-mode selection uses last-written-one priority: writing a one in a new mode selects it and clears the prior active mode for that pin. Explicitly clearing every old mode register is not required. | Completed hostile Claude rejection in `audit_work/council/deep_firmware_spec_20260606/claude/outputs/DF22.txt`. |
| D14 | **REJECT** | J3 exposes only the 40 harness channels and no board-ground test contact. A pin short to an external/chassis ground is outside the stated goal of determining which J3 pins connect to which other J3 pins. Treating that unsupported fault class as mandatory would require a new test architecture, not repair a defective implementation of the stated workflow. | **Unreviewed by Claude.** Source/spec scope adjudication only. |
| D15 | **REJECT** | The halt is real only when `SD.begin()` fails. A functioning microSD card is an explicit required operating component; no source or board defect forces initialization failure. Retry/error-state behavior is conditional media-fault hardening, not a normal-operation show-stopper. | Completed hostile Claude rejection in `audit_work/council/deep_firmware_spec_20260606/claude/outputs/DF33.txt`. |

## Primary Evidence

### Analog Devices MAX2679/MAX2679B

Primary source:
<https://www.analog.com/media/en/technical-documentation/data-sheets/MAX2679-MAX2679B.pdf>

- A1/VCC requires a close 1000 pF bypass.
- B1/RFIN requires a DC-blocking capacitor and external matching.
- A2/RFOUT is internally matched and floating A2 selects active mode.

Board facts:

- C6 is `1 nF` from `VCC_RF` to GND:
  `kicad_files/hardware_challenge.kicad_pcb:505-701`.
- U5 A1 is on that same net:
  `kicad_files/hardware_challenge.kicad_pcb:6433-6441`.
- L1/C5 are on RFOUT, while RFIN is directly connected to AE1:
  `kicad_files/hardware_challenge.kicad_pcb:13603-13635`,
  `15874-15914`, and `19524-19555`.

### u-blox NEO-M8

Primary source:
<https://content.u-blox.com/sites/default/files/NEO-8Q-NEO-M8-FW3_HIM_UBX-15029985.pdf>

- Connect V_BCKP to VCC when no backup supply is available.
- Connect VDD_USB to GND when USB is unused.
- VCC_RF may power an external LNA; LNA_EN is optional control.

Board facts are visible at
`kicad_files/hardware_challenge.kicad_pcb:10942-11110`.

### Infineon CY8C9560A

Primary source:
<https://www.infineon.com/assets/row/public/documents/30/57/infineon-cy8c9520a-cy8c9540a-cy8c9560a-20--40--and-60-bit-i-o-expander-with-eeprom-datasheet-additionaltechnicalinformation-en.pdf>

- XRES is active high.
- A0 low selects the normal `0x20` address and leaves A1-A6 as GPIO.
- Direction `0` is output and `1` is input.
- Drive modes use last-written-one selection.
- POR high state is resistive pull-up, not strong-high on every pin.

Firmware behavior is at `firmware/CY8C9560.cpp:3-84`.

### TE ANT-GNSSCP-TH25L1

Primary source:
<https://www.te.com/commerce/DocumentDelivery/DDEController?Action=srchrtrv&DocFormat=pdf&DocLang=English&DocNm=ant-gnsscp-th25l1-ds&DocType=Data+Sheet&PartCntxt=ANT-GNSSCP-TH25L1>

- A ceramic patch requires a ground plane for proper operation.
- The reference plane dimensions and centered placement are recommendations;
  other sizes and locations are permitted.
- A pi footprint is recommended, but this antenna series does not inherently
  require matching.

The board's all-layer keepout is
`kicad_files/hardware_challenge.kicad_pcb:20677-20705`.

## Review Integrity

No frozen candidate was promoted as a new root, so the original requirement
for two fresh Claude reviews per promoted candidate was not triggered.

Four extra disproof runs were started for D07 and D08 before the user stopped
candidate expansion. They were terminated before returning output. Their empty
stdout/stderr, prompts, and explicit `INTERRUPTED` status files are preserved
under:

- `audit_work/council/council_d/claude/D07/`
- `audit_work/council/council_d/claude/D08/`

No interrupted run is represented as a completed review.

## Integrity

Council D wrote only under `audit_work/council/council_d/`.
Protected firmware, KiCad sources, README, and shared reports were not edited.
