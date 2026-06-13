# Bug Theory Review Log

Generated review artifact only. This is not source code and does not replace the original schematic, PCB, or firmware files.

Snapshot: 2026-06-04.

Current result: 26 approved, 24 rejected, 2 pending.

Latest six-way admission pass: no new independent bubble survived. A03 was strengthened with an ordinary RMC overflow witness; P04 and P05 remain pending rather than promoted.

Latest simulator-20 admission pass: no new independent bubble survived. All 20 listed items were already-approved A-items. The report's A06 withholding exposed a real witness-quality issue, but its claim that the simulator reset model is wrong was rejected.

Legend:
- `Approved`: the thesis survived adversarial Claude review and local re-check.
- `Rejected`: the thesis was disproved or narrowed away.
- `Pending`: side-agent / Claude review has not finished yet.
- `Bubble`: whether the result should be carried into the final bug list.

## Approved

### A01 - firmware - `cy.begin()` is never called
- Thesis: the GPIO expander is used without initialization.
- Evidence: `CY8C9560 cy` is constructed in `firmware/firmware.ino:63`, but `setup()` only initializes serial, GPIO, and SD in `firmware/firmware.ino:97-116`. `CY8C9560::begin()` performs reset, starts I2C, sets the I2C clock, and checks the device ID in `firmware/CY8C9560.cpp:3-15`.
- Claude review: `ACCEPT` via side agent `019e9474-63f3-7092-b7e1-92e7bbb5616b`.
- Adjudication: Approved.

### A02 - firmware - GPS SAFEBOOT and reset pins are written without being configured as outputs
- Thesis: `PIN_UBX_SAFEBOOT` and `PIN_UBX_RST_N` are driven with `digitalWrite()` before `pinMode(..., OUTPUT)`.
- Evidence: `firmware/firmware.ino:104-107` configures only the button and TIMEPULSE pins, then writes SAFEBOOT and RST_N. No output mode is set for pins 3 or 4.
- Claude review: `ACCEPT` via side agent `019e9474-662e-7820-90ce-a5fb1c003d78`.
- Adjudication: Approved.

### A03 - firmware - NMEA input buffer can overflow
- Thesis: `nmea_buf[64]` is written without a bounds check before newline or carriage return arrives.
- Evidence: `nmea_buf` is 64 bytes in `firmware/firmware.ino:118`, but `nmea_idx` increments unbounded in `firmware/firmware.ino:122-126`. `process_nmea()` also writes a terminator at `buf[len]` in `firmware/firmware.ino:73-74`. The u-blox M8 RMC example is 75 bytes including CRLF, so this is not limited to malformed input.
- Claude review: `ACCEPT` via side agent `019e9476-2138-7812-9b73-1d3d3a171400`.
- Adjudication: Approved.

### A04 - firmware - GPRMC validity/status is ignored
- Thesis: the parser accepts a `$GPRMC` sentence even if its status field says the fix is invalid.
- Evidence: `firmware/firmware.ino:75-79` parses the time/date fields with `%*c` for the status byte, then immediately marks `time_fixed = true`; there is no check for `A` versus `V`.
- Claude review: `ACCEPT` via side agent `019e9476-204f-7c02-83c6-9f8e4ad96fc3`.
- Adjudication: Approved.

### A05 - firmware - 40-pin masks use a narrow integer literal
- Thesis: `1 << i` and `1 << j` are not safe for indices above the host integer width.
- Evidence: the design claims 40 harness pins in `firmware/firmware.ino:18`, while the loop uses `1 << i` and `1 << j` in `firmware/firmware.ino:143-152` instead of a 64-bit literal such as `1ULL`.
- Claude review: `ACCEPT` via side agent `019e9477-f071-7b03-af25-4ad4db4f295b`.
- Adjudication: Approved.

### A06 - firmware - `set_output()` makes every expander pin an output
- Thesis: the helper ignores the selected mask when setting direction.
- Evidence: `firmware/CY8C9560.cpp:77-83` writes the selected output values, then writes `REG_PIN_DIRECTION` as `0x00` for every port, making all pins outputs.
- Claude review: `ACCEPT` via side agent `019e9477-f140-7550-95a4-875a5cf529e2`.
- Adjudication: Approved.

### A07 - firmware - `set_pd_inputs()` immediately undoes the selected output
- Thesis: after selecting one output pin, the next helper makes all pins inputs again.
- Evidence: `firmware/firmware.ino:144-147` calls `cy.set_output(output_mask, output_mask)` and then `cy.set_pd_inputs(~output_mask)`. `set_pd_inputs()` writes `REG_PIN_DIRECTION = 0xFF` for every port in `firmware/CY8C9560.cpp:61-66`.
- Claude review: `ACCEPT` via side agent `019e9477-f1c9-71c1-805d-40c4c31dc067`.
- Adjudication: Approved.

### A08 - firmware - one matching row is enough to pass the entire harness
- Thesis: the loop sets `passed = true` if any single probe row matches and never clears it when another row fails.
- Evidence: `passed` starts false in `firmware/firmware.ino:142`, but `firmware/firmware.ino:155-157` only sets it true on a match.
- Claude review: `ACCEPT` via side agent `019e9478-0ac4-7972-8b57-9a529fa5735e`.
- Adjudication: Approved.

### A09 - firmware - FAILED LED status is immediately overwritten by GOOD
- Thesis: a failed test is shown briefly, then the next loop iteration forces GOOD.
- Evidence: `firmware/firmware.ino:133-135` sets GOOD whenever `time_fixed` is true, while the test result only sets FAILED or GOOD at `firmware/firmware.ino:160-163`.
- Claude review: `ACCEPT` via side agent `019e9478-0b6d-7510-ab30-70756f2de1c9`.
- Adjudication: Approved.

### A10 - schematic - UART lines are wired same-direction instead of crossed
- Thesis: MCU TX is connected to GPS TX and MCU RX is connected to GPS RX.
- Evidence: the Teensy pins are `0_RX1_CRX2_CS1` and `1_TX1_CTX2_MISO1` in `kicad_files/hardware_challenge.kicad_sch:7377-7405`; the GPS pins are `TXD/SPI_MISO` and `RXD/SPI_MOSI` in `kicad_files/hardware_challenge.kicad_sch:4630-4659`. The labels place `UBX-TXD` on TX and `UBX-RXD` on RX on both sides in `kicad_files/hardware_challenge.kicad_sch:10218-10228`, `10614-10624`, `10966-10976`, and `11802-11812`.
- Claude review: `ACCEPT` via side agent `019e947f-bee6-7a60-87e8-13daf52faec0`.
- Adjudication: Approved.

### A11 - schematic - I2C SDA is pulled down instead of up
- Thesis: the SDA bus is held low by R3.
- Evidence: `R3` is the 4k7 resistor in `kicad_files/hardware_challenge.kicad_sch:16289-16353`; the `CY_SDA` net is shown at `kicad_files/hardware_challenge.kicad_sch:11208-11218`, and the review established that R3 connects that net to GND rather than +3.3V.
- Claude review: `ACCEPT` via side agent `019e947f-bf92-7d70-a290-0c44da8ea641`.
- Adjudication: Approved.

### A12 - schematic - RGB LED has no current-limiting resistors
- Thesis: D3's color channels are driven directly from the MCU pins.
- Evidence: D3 is an RGB LED in `kicad_files/hardware_challenge.kicad_sch:13431-13505`; its channels are directly labeled `LED_R`, `LED_G`, and `LED_B` in the schematic, with no series resistor in those paths.
- Claude review: `ACCEPT` via side agent `019e947f-c03a-74f0-87c2-30076a27110f`.
- Adjudication: Approved.

### A16 - PCB - D3 +3.3V anode is on a separate copper island
- Thesis: D3 pad 1 is assigned to +3.3V but appears isolated from the main +3.3V network.
- Evidence: D3 pad 1 is on net 5 in `kicad_files/hardware_challenge.kicad_pcb:915-922`; its local route runs to the via at `157.226, 32.9184` in `kicad_files/hardware_challenge.kicad_pcb:15755-15832`, but local inspection found that via lands in a separate island rather than the main +3.3V plane.
- Claude review: `ACCEPT` via side agent `019e948d-cc59-7a42-b0cc-f357a3a270a2`.
- Adjudication: Approved. The side agent checked the possible hidden B.Cu rebuttal and found none; the via lands only in the small separate In1.Cu island.

### A17 - firmware - test button polarity is inverted
- Thesis: the schematic makes a pressed button read LOW, but the firmware returns on LOW and only runs the test when the button is not pressed.
- Evidence: firmware returns when `digitalRead(PIN_BTN_TEST) == LOW` in `firmware/firmware.ino:137-139`; the schematic has a pull-up resistor and switch-to-ground arrangement on BTN_TEST.
- Claude review: `ACCEPT` via side agent `019e9493-03e3-7181-8459-49c89142ab52`.
- Adjudication: Approved. The side agent checked the possible normally-closed interpretation and found the local SW1 metadata explicitly says normally open.

### A18 - schematic - MAX2679 Vcc is overvolted through VCC_RF
- Thesis: U5 Vcc is fed from the GPS module's VCC_RF output, which is derived from the module's +3.3V supply and exceeds the MAX2679 supply limit.
- Evidence: U5 is the MAX2679 in `kicad_files/hardware_challenge.kicad_sch:13508-13579`; the GPS symbol exposes `VCC_RF` in `kicad_files/hardware_challenge.kicad_sch:4828-4845`. This is the refined version of R03, not the rejected direct-+3.3V wording.
- Claude review: `ACCEPT` via side agent `019e9495-427b-70e2-a5a0-82be6a237fc0`.
- Adjudication: Approved. The side agent verified U5 A1 on `Net-(U3-VCC_RF)`, U3 VCC on +3.3V, MAX2679 operating VCC at 1.08 V to 1.98 V, absolute maximum VCC at 2.2 V, and NEO-M8 VCC_RF at VCC - 0.1 V.

### A19 - firmware - parser only accepts `$GPRMC`, not default multi-GNSS `$GNRMC`
- Thesis: the firmware waits for a GPS-only talker ID even though the selected NEO-M8N default multi-GNSS output uses `GN`.
- Evidence: `firmware/firmware.ino:75-76` only matches and parses `$GPRMC`; there is no `$GNRMC` branch or NEO-M8 reconfiguration path. U3 is a NEO-M8N, and the official u-blox default multi-GNSS talker ID is `GN`, so the default RMC sentence is `$GNRMC`.
- Claude review: `ACCEPT` via side agent `019e949e-b42f-7d02-871c-5bfb7f4cbfb8`.
- Adjudication: Approved. The side agent checked the wording caveat that the GNSS module itself can still acquire a fix; the real failure is that firmware never sets `time_fixed`.

### A20 - firmware - harness logical index diverges from expander register-bit index after CBL_19
- Thesis: firmware treats `CBL_i` as raw expander bit `i`, but the schematic leaves a four-bit register gap between CBL_19 and CBL_20.
- Evidence: `read_inputs()`, `set_output()`, and `set_pd_inputs()` pack the CY8C9560 registers byte-by-byte in `firmware/CY8C9560.cpp:25-84`, while the probe loop uses `1 << i` directly in `firmware/firmware.ino:143-152`. CBL_20..27 are on GPort3 bits 0..7, CBL_28..35 are on GPort4 bits 0..7, and CBL_36..39 are on GPort5 bits 0..3, so those logical harness pins are raw bits 24..43 rather than 20..39.
- Claude review: `ACCEPT` via side agent `019e94a3-9a4c-71d3-a7a7-de41dc0eaf61`.
- Adjudication: Approved. The side agent checked for a hidden compaction layer and found none.

### A21 - firmware - status LED pins are never configured as outputs
- Thesis: `set_status()` writes the RGB cathodes before any `pinMode(..., OUTPUT)` for pins 5, 6, and 7.
- Evidence: `PIN_LED_R/G/B` are 5, 6, and 7 in `firmware/firmware.ino:9-11`; `set_status()` only calls `digitalWrite()` in `firmware/firmware.ino:67-71`; `setup()` calls `set_status(BUSY)` before its only visible `pinMode()` calls, which are for pins 8 and 2 in `firmware/firmware.ino:103-105`. D3 is a common-anode RGB LED with cathodes on those nets.
- Claude review: `ACCEPT` via side agent `019e94a4-0b08-73e3-ae41-950bfefef80f`.
- Adjudication: Approved on rereview. The strongest rebuttal was weak input pull behavior, but that does not provide active cathode drive for the status LED.

### A22 - firmware - `EXPECTED_CONNECTIONS` is explicitly illustrative but used as the real oracle
- Thesis: the firmware decides pass/fail from a table the source itself labels as only an example.
- Evidence: `firmware/firmware.ino:19` says `// Example connection matrix, just for illustration`, but `firmware/firmware.ino:155-156` compares every observed probe result directly against `EXPECTED_CONNECTIONS[i]` and uses that comparison to set `passed`.
- Claude review: `ACCEPT` via side agent `019e94e3-1e48-72c0-ba0f-258342daa5ea` after a second non-interactive Claude pass.
- Adjudication: Approved. The only possible rebuttal is that the comment is stale, but there is no source evidence of another authoritative harness topology or any runtime replacement for this table.

### A23 - firmware - a held test trigger reruns and re-logs the harness test indefinitely
- Thesis: once the button gate is satisfied, every subsequent `loop()` reruns the full test and appends another result.
- Evidence: `firmware/firmware.ino:137-163` has only a level gate on `digitalRead(PIN_BTN_TEST)` and no edge-detect, latch, or one-shot state. `log_result()` opens `results.txt` with `FILE_WRITE` and writes a result line in `firmware/firmware.ino:86-91`.
- Claude review: `ACCEPT` via side agent `019e94e3-1e48-72c0-ba0f-258342daa5ea` after a second non-interactive Claude pass.
- Adjudication: Approved. Even if the known button-polarity bug is fixed, a held valid press still causes repeated test cycles and duplicate records.

### A24 - schematic - D3 red and blue channels are swapped
- Thesis: the status LED net names do not match the actual D3 die pins.
- Evidence: `Device:LED_ABGR` defines pin 2 as `BK`, pin 3 as `GK`, and pin 4 as `RK` in `kicad_files/hardware_challenge.kicad_sch:3727-3777`. The schematic wires `LED_R` to D3 pin 2 and `LED_B` to D3 pin 4 through the corresponding labels at `kicad_files/hardware_challenge.kicad_sch:10922-10942` and `10240-10252`. Therefore firmware's red command drives blue and firmware's blue command drives red.
- Claude review: `ACCEPT` via side agent `019e94e3-5f6c-7b53-92b2-a0f26a13c219` after a non-interactive Claude pass.
- Adjudication: Approved. This is a real UI defect because the README explicitly defines the status colors as red failed, blue busy, green good.
- Bubble: yes.

### A25 - schematic - reverse-polarity input forward-biases D1 before Q1 can isolate it
- Thesis: D1 is on the raw J1 input side of Q1, so a reverse-polarity input forward-biases the unidirectional TVS to ground before Q1 can isolate the protected rail.
- Evidence: J1 pad 1 is net 9, D1 pad 1 is the same net 9, and D1 pad 2 is GND in `kicad_files/hardware_challenge.kicad_pcb:11479-11509` and `kicad_files/hardware_challenge.kicad_pcb:1423-1441`. Q1 pad 5 is drain on net 9, while pads 1 through 3 are source on `+12V` in `kicad_files/hardware_challenge.kicad_pcb:1697-1738`. D1 is explicitly a unidirectional SMAJ16A in `kicad_files/hardware_challenge.kicad_sch:13362-13407`.
- Claude review: `ACCEPT` via side agent `019e94fc-0960-7b02-bb1d-33b221e6e3c8` after a non-interactive Claude pass focused on J1 numbering, D1 polarity, Q1 source/drain orientation, and net-9 continuity.
- Adjudication: Approved. With reverse polarity, raw net 9 goes negative relative to board GND, so D1 forward-biases from GND into the input before Q1 can protect the downstream `+12V` rail. This is a real failure of the stated reverse-polarity-protection goal, not merely a component-choice preference.
- Bubble: yes.

### A26 - PCB - MAX2679 RFIN matching/DC-block network is omitted from the input and misplaced on RFOUT
- Thesis: the LNA input is tied directly to the antenna while the only series `12n` / `100n` RF network is on the output side.
- Evidence: AE1 pad 1 and U5 B1/RFIN are both on `Net-(AE1-A)` in `kicad_files/hardware_challenge.kicad_pcb:2446-2455` and `kicad_files/hardware_challenge.kicad_pcb:6452-6459`, with the direct route shown in `kicad_files/hardware_challenge.kicad_pcb:13603-13635`. U5 A2/RFOUT is on `Net-(L1-Pad2)` in `kicad_files/hardware_challenge.kicad_pcb:6443-6450`; L1 and C5 then form the only series chain to U3 RF_IN in `kicad_files/hardware_challenge.kicad_pcb:10168-10186`, `kicad_files/hardware_challenge.kicad_pcb:1951-1967`, and `kicad_files/hardware_challenge.kicad_pcb:10982-10990`. The official MAX2679 datasheet says B1/RFIN requires a DC-blocking capacitor and external matching components, while A2/RFOUT is internally matched to 50 ohms.
- Claude review: `ACCEPT` via side agent `019e9584-5d8d-7ae1-ae35-fbf3d24e4e2a`; direct non-interactive Claude pass also returned `ACCEPT`.
- Adjudication: Approved. This is one RF-front-end root-cause cluster, not separate bugs for bare RFIN, misplaced L1/C5, and C5 value.
- Bubble: yes, as one cluster. It has not yet been inserted into the clean 22-item sendable file.

### A27 - PCB - antenna-side RF trace loses its local controlled-impedance reference through the all-layer keepout
- Thesis: the antenna-side net is routed through an all-layer copper-pour keepout without a continuous local GND reference, so the top-layer RF segment is not implemented as a controlled-impedance path.
- Evidence: net 1 runs as a `0.8128` mm F.Cu segment through the antenna region and then necks down to `0.127` mm near U5 in `kicad_files/hardware_challenge.kicad_pcb:13603-13635`. The keepout forbids copper pours on F.Cu, In1.Cu, In2.Cu, and B.Cu in `kicad_files/hardware_challenge.kicad_pcb:20677-20705`. The board stackup places In1.Cu only `0.1` mm below F.Cu in `kicad_files/hardware_challenge.kicad_pcb:48-97`, but In1.Cu is the `+3.3V` zone rather than GND in `kicad_files/hardware_challenge.kicad_pcb:26339-26359`; the declared GND zones are on F.Cu, In2.Cu, and B.Cu in `kicad_files/hardware_challenge.kicad_pcb:20707-20728`. The official MAX2679 datasheet requires controlled-impedance lines on high-frequency inputs and outputs.
- Claude review: `ACCEPT` via side agent `019e9589-54f6-7842-8957-f9dba7e14714`.
- Adjudication: Approved with narrow wording. This does not claim a numeric impedance or measured RF-performance loss; it claims only that the routed path lacks the local reference structure needed for the documented controlled-impedance requirement.
- Bubble: yes, as a separate layout defect. It has not yet been inserted into the clean 22-item sendable file.

### A28 - schematic - D2 is not a real Q1 gate-source transient clamp
- Thesis: D2 is an ordinary Schottky rather than a Zener, so a high positive transient clamped by D1 can exceed Q1's +/-20 V VGS rating.
- Evidence: D2 is PMEG10020ELR, a Schottky, from +12V/source to Q1 gate; Q1 is SiSS27DN with +/-20 V VGS and D1 is SMAJ16A.
- Claude review: direct non-interactive Claude pass returned `True.` when given the corrected orientation and transient path; another direct pass explained that D2 only forward-clamps gate above source and does not clamp the negative VGS created when source rises above gate.
- Adjudication: Approved. D2 is wired with anode on gate and cathode on source, which is the right orientation for a gate-source clamp only if it is a Zener. The populated PMEG10020ELR is just a Schottky, so it cannot limit the source-above-gate excursion that D1 still allows.
- Bubble: yes.

### A29 - schematic/PCB - U4 uses the wrong CY8C9560A package footprint
- Thesis: U4 is specified as the 14 mm CY8C9560A-24AXIT package but laid out with a 12 mm TQFP-100 footprint.
- Evidence: U4 is `CY8C9560A-24AXIT` with `Package_QFP:TQFP-100_12x12mm_P0.4mm` in `kicad_files/hardware_challenge.kicad_sch:13991-14017`; the PCB repeats `Package_QFP:TQFP-100_12x12mm_P0.4mm` and explicitly describes a `12x12x1 mm` body in `kicad_files/hardware_challenge.kicad_pcb:8915-8920`. The official Infineon CY8C9560A datasheet identifies the 100-pin TQFP package outline as `14 x 14 x 1.4 mm`, and its ordering table lists `CY8C9560A-24AXIT` as that 100-pin TQFP part.
- Claude review: direct non-interactive Claude pass returned `True.` to the footprint-mismatch thesis.
- Adjudication: Approved. The selected part cannot fit the assigned 12 mm body footprint.
- Bubble: yes.

## Pending

### P04 - firmware - `process_nmea()` accepts malformed or corrupted `$GPRMC` time/date fields as a time lock
- Thesis: the parser accepts any parseable `$GPRMC` prefix without validating checksum, numeric shape, or date/time range.
- Evidence: `process_nmea()` only checks `strncmp(buf, "$GPRMC", 6)` and `sscanf(...) == 2` in `firmware/firmware.ino:73-83`; it does not verify the NMEA checksum or validate field ranges before setting `time_fixed = true`.
- Claude review: an earlier direct non-interactive Claude pass said this is distinct from the A/V status check, but the latest coordinator Claude pass returned `REJECT` as not a distinct show-stopping bug versus A04.
- Adjudication: Pending because the review history is split. The latest independent firmware agent agreed with the rejection and called it parser hardening rather than a separate challenge bug, but the earlier direct review treated checksum/range validation as a distinct defect.
- Bubble: undecided.

### P05 - schematic/assembly - Teensy VUSB/VIN backfeed hazard during USB service
- Thesis: U2 is powered from the board's +5 V rail through VIN while VUSB is left unconnected, so attaching USB during powered service/programming can backfeed the host through the stock Teensy 4.1 VIN/VUSB link.
- Evidence: U2 pad 48/VIN is on `+5V` in `kicad_files/hardware_challenge.kicad_pcb:8193-8202`; U2 pad 49/VUSB is explicitly unconnected in `kicad_files/hardware_challenge.kicad_pcb:8204-8213`. PJRC documents that Teensy 4.1 connects VIN and VUSB by default and warns against powering VIN while USB is attached unless the underside link is cut.
- Claude review: the latest coordinator non-interactive Claude pass returned `DISPUTED`. It accepted the factual premises but said this is a conditional module-level assembly/service hazard rather than a deterministic board-intrinsic netlist defect.
- Adjudication: Pending. Side agent `019e95f1-da9e-7611-b6dd-f803ad3b7598` accepted it as a narrow service/programming-path defect and wanted it bubbled; coordinator Claude disagreed with that escalation and kept it conditional.
- Bubble: undecided.

## Rejected

### R01 - firmware - initial LED pinMode thesis, superseded on rereview
- Thesis: `set_status()` writes the LED pins without `pinMode(..., OUTPUT)`.
- Evidence: `set_status()` does write pins directly in `firmware/firmware.ino:67-71`.
- Claude review: `REJECT` via side agent `019e9474-64a7-7cb3-a006-1d0a91e3518c`.
- Adjudication: Superseded on rereview by A21. The first review was too cautious; the later review checked Teensy input-mode behavior and concluded that weak pull behavior does not rescue the missing active output drive.
- Bubble: no as this original wording; yes for the refined A21 thesis.

### R02 - schematic - MAX2679 RF path direction is reversed
- Thesis: the LNA input/output pins were believed to be swapped.
- Evidence: this depended on misreading the mirrored U5 symbol orientation.
- Claude review: `REJECT` via side agent `019e947f-c108-7301-9370-15b26009e333`.
- Adjudication: Rejected. After accounting for `(mirror y)`, the RF path direction is correct.
- Bubble: no.

### Focused fabrication/mechanical rerun - side agents `019e95f3-4379-7f61-8952-ed16f0731c5f`, `019e95f5-1340-72d1-a5c3-e04495f3ddd0`, `019e95f5-1336-7203-8768-a95335c84d11`, `019e95f5-12da-7fd1-9ede-b4b8162a408c`

The isolated subagents all preserved the read-only constraint, but their local Claude auth state was unavailable. The unique new candidates below were rerun through the authenticated root `claude -p` session before adjudication. Repeated source-vs-gerber parity claims were already logged in the earlier source-vs-gerber section and remained rejected.

- U2 / Teensy41 edge-fit or overhang.
  - Evidence: U2 is at `kicad_files/hardware_challenge.kicad_pcb:7087-7090`, `7153-7199`, and `7463-7500`; the board edge is at `11976-12047`. The visible module outline is tight to the carrier width, but no pad or actual body overhang is proven.
  - Claude review: `REJECT`.
  - Agent verdict: rejected.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- J1 barrel-jack front-face clearance.
  - Evidence: J1 is at `kicad_files/hardware_challenge.kicad_pcb:11134-11138` and `11344-11452`; the board edge is at `11943-12047`. The F.Fab body remains inboard, and the source does not prove unusable plug fit.
  - Claude review: `REJECT`.
  - Agent verdict: rejected.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- AE1 / U3 body clearance.
  - Evidence: AE1 is at `kicad_files/hardware_challenge.kicad_pcb:2279-2457`; U3 is at `10620-10869`. Under the verified KiCad rotation convention, the two F.Fab bodies retain about 1.14 mm horizontal gap.
  - Claude review: `REJECT`.
  - Agent verdict: rejected.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- Lower slot edge clearance.
  - Evidence: the lower slots are at `kicad_files/hardware_challenge.kicad_pcb:11954-11974`; the outer board edge is at `11986-12047`. The slots retain about 1.5 mm side margin and 2 mm bottom margin.
  - Claude review: `REJECT`.
  - Agent verdict: rejected.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- U2 underside collision / assembly stack.
  - Evidence: U2 is at `kicad_files/hardware_challenge.kicad_pcb:7087-7152`; nearby C4 is at `8413-8422`. The repo does not provide header-height, underside keepout, or Z-stack evidence sufficient to prove a collision.
  - Claude review: `REJECT`.
  - Agent verdict: rejected.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- U4 reference silk allegedly crosses pads 49/50.
  - Evidence: U4 pads 49/50 are at `kicad_files/hardware_challenge.kicad_pcb:9573-9583`; their mask flashes are at `gerbers.zip/hardware_challenge-F_Mask.gbr:319-320`; the nearby `U4` silk stroke is at `gerbers.zip/hardware_challenge-F_Silkscreen.gbr:6172-6174`. Once pad rotation is respected, the silk stops before the actual 0.2 mm pad width.
  - Claude review: `REJECT`.
  - Agent verdict: rejected.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- U5 pin-1 silk triangle allegedly intrudes into C6 pad 1 opening.
  - Evidence: C6 pad 1 is at `kicad_files/hardware_challenge.kicad_pcb:685-701`; its mask/paste openings are at `gerbers.zip/hardware_challenge-F_Mask.gbr:67-70` and `gerbers.zip/hardware_challenge-F_Paste.gbr:59-62`. The U5 triangle is at `kicad_files/hardware_challenge.kicad_pcb:6320-6330` and `gerbers.zip/hardware_challenge-F_Silkscreen.gbr:6881-6890`, with only about 0.05 mm edge intrusion into the 0402 opening.
  - Claude review: first root pass `ACCEPT`; stricter rereview `REJECT` as a minor silk-clearance warning rather than a challenge-level assembly defect.
  - Agent verdict: rejected.
  - Agent agreement with Claude: agrees with the stricter rejection, disagrees with the first broad acceptance.
  - Bubble: no.

- U1 exposed tab allegedly has no usable paste aperture.
  - Evidence: U1 defines one large F.Mask tab opening plus four separate F.Paste windows at `kicad_files/hardware_challenge.kicad_pcb:6805-6853`; the generated mask/paste output is at `gerbers.zip/hardware_challenge-F_Mask.gbr:171-176` and `gerbers.zip/hardware_challenge-F_Paste.gbr:115-123`.
  - Claude review: `REJECT`.
  - Agent verdict: rejected.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

### R03 - schematic - MAX2679 is directly tied to +3.3V
- Thesis: U5 Vcc was originally described as directly tied to +3.3V.
- Evidence: the actual connection is through the GPS module's `VCC_RF`, not directly to the +3.3V net.
- Claude review: `REJECT` via side agent `019e947f-c1e8-7da3-be9f-70556eceaf1f`.
- Adjudication: Rejected as worded. The narrower VCC_RF-overvoltage thesis was separately approved as A18.
- Bubble: no.

### R03b - PCB - Q1 source pads are disconnected from +12V copper
- Thesis: Q1 pads 1/2/3 were originally believed to be isolated from the incoming +12V route.
- Evidence: the first transform used the wrong handedness for KiCad's board coordinates. With the corrected transform, Q1 source pads resolve at y=`33.935`, directly on the +12V copper/zone around `kicad_files/hardware_challenge.kicad_pcb:20645-20675`.
- Claude review: `REJECT` on corrected rereview.
- Adjudication: Rejected. The apparent open was a coordinate-transform error.
- Bubble: no.

### R03c - PCB - D1 anode is disconnected from the input/Q1-drain route
- Thesis: D1 pad 1 was originally believed to be physically separated from net 9 copper.
- Evidence: with the corrected transform, D1 pad 1 resolves at `(138, 36)` and is reached by the net-9 route; the prior mismatch came from the same rotation-sign error.
- Claude review: `REJECT` on corrected rereview.
- Adjudication: Rejected. The apparent open was a coordinate-transform error.
- Bubble: no.

### R03d - PCB - antenna pad is disconnected from the LNA input path
- Thesis: AE1 pad 1 was originally believed to be separate from the net-1 route toward U5 B1.
- Evidence: with the corrected transform, AE1 pad 1 resolves at `(134.5, 77)` and joins the F.Cu RF arc toward U5 B1.
- Claude review: `REJECT` on corrected rereview.
- Adjudication: Rejected. The apparent open was a coordinate-transform error.
- Bubble: no.

### R04 - PCB - Q1 gate is disconnected
- Thesis: Q1 gate pad 4 was suspected to be isolated from D2/R1.
- Evidence: Q1 pad 4 is on net 10 in `kicad_files/hardware_challenge.kicad_pcb:1724-1731`, and the net-10 route is continuous through D2, Q1, and R1 in `kicad_files/hardware_challenge.kicad_pcb:16068-16099`.
- Claude review: `REJECT` via side agent `019e948d-cf84-7191-903f-850829c29c88`.
- Adjudication: Rejected.
- Bubble: no.

### R05 - schematic - D_SEL left floating
- Thesis: the GPS D_SEL pin was suspected to require an external strap.
- Evidence: local review found that D_SEL floating is a valid default configuration for this module.
- Claude review: `REJECT` via side agent `019e94b2-7485-77b0-af89-4f153750abe4`.
- Adjudication: Rejected. The adversarial review confirmed that open D_SEL is the valid UART/DDC default and grounding it is what selects SPI.
- Bubble: no.

### R06 - firmware - CY8C reset polarity is wrong
- Thesis: `CY8C9560::begin()` was suspected to leave reset asserted.
- Evidence: `firmware/CY8C9560.cpp:5-9` drives reset high, waits, then low; local datasheet review established active-high reset.
- Claude review: `REJECT` via side agent `019e94b2-74ef-7713-afc3-b6e14cceffe0`.
- Adjudication: Rejected. The adversarial review confirmed that U4 pad 62 is XRES and that the high-then-low pulse is the correct active-high reset sequence.
- Bubble: no.

### R07 - schematic - D1 TVS orientation is wrong
- Thesis: D1 was suspected to be reversed.
- Evidence: local review found the unidirectional TVS orientation itself is correct; the real PCB issue is the disconnected anode in A14.
- Claude review: `REJECT` via side agent `019e94b2-752e-7153-8bd6-0279c588cf39`.
- Adjudication: Rejected. The adversarial review confirmed cathode-high/anode-ground is the expected orientation for the SMAJ16A input clamp.
- Bubble: no.

### R08 - PCB - RF_IN was open
- Thesis: the GPS RF_IN path was initially suspected to be disconnected.
- Evidence: local review found the route is completed by arcs in `kicad_files/hardware_challenge.kicad_pcb:15890-15905`.
- Claude review: `REJECT` via side agent `019e94b2-7602-7332-892e-c53fce74c782`.
- Adjudication: Rejected. The adversarial review confirmed that the apparent gap is closed by the two net-6 arcs between C5 and U3 RF_IN.
- Bubble: no.

### R09 - PCB - C6/U5 courtyard overlap
- Thesis: C6 and U5 were suspected to overlap physically.
- Evidence: local review found courtyard overlap but no actual F.Fab body overlap.
- Claude review: `REJECT` via side agent `019e94b0-3f37-7c71-ae8a-9720e06702ba`.
- Adjudication: Rejected. The adversarial review measured a 0.557454 mm Y gap between the F.Fab bodies, so only the larger courtyards overlap.
- Bubble: no.

### R11 - PCB - missing GND zone
- Thesis: the board was initially suspected to lack a ground plane.
- Evidence: local review found the GND zone is present.
- Claude review: `REJECT` via side agent `019e94c7-25b4-7bb1-8291-f21f46b78673`.
- Adjudication: Rejected. The board has a filled GND zone on F.Cu, In2.Cu, and B.Cu; the large In2.Cu filled polygon is enough to disprove the missing-plane thesis.
- Bubble: no.

### R14 - PCB - BTN_TEST net is physically open
- Thesis: R4 pad 2 is on BTN_TEST but is not connected to the SW1/U2 BTN_TEST route.
- Evidence: R4/SW1/U2 are all on net 57, with SW1 pad 2 at `kicad_files/hardware_challenge.kicad_pcb:2249-2263` and route segments around `kicad_files/hardware_challenge.kicad_pcb:19790-19832`; local inspection found R4 pad 2 separate from that route.
- Claude review: `REJECT` via side agent `019e9492-e39e-7611-adef-e7281bf953a2`.
- Adjudication: Rejected. The original local transform was wrong. R4 pad 2 resolves to `(167.320101, 34.75484)`, exactly where the net-57 route terminates before the via, while the other nearby coordinate belongs to R4 pad 1 on +3.3V.
- Bubble: no.

### R12 - firmware - first GPS timestamp is reused forever
- Thesis: `utc_time` and `date` are only populated before `time_fixed` becomes true, so later tests do not log current time.
- Evidence: `firmware/firmware.ino:75-80` only writes the parsed strings under `&& !time_fixed`, while `log_result()` always uses those stored strings in `firmware/firmware.ino:86-95`.
- Claude review: `REJECT` via side agent `019e9492-f0ed-7801-81f3-fd12ec2576e4`.
- Adjudication: Rejected. `sscanf(...) == 2` is the left operand of `&& !time_fixed`, so later valid GPRMC lines still execute `sscanf` and refresh the buffers before the one-time body is skipped.
- Bubble: no.

### R13 - firmware - EXPECTED_CONNECTIONS is MSB-first while runtime probes LSB-first
- Thesis: the connection table's bit order does not match `1 << i` / `1 << j` runtime indexing.
- Evidence: the first table row in `firmware/firmware.ino:20-24` has its own bit at bit 39, while the probe uses bit `i` in `firmware/firmware.ino:143-152`.
- Claude review: `REJECT` via side agent `019e9493-15af-7d42-849c-0ca27ce6e457`.
- Adjudication: Rejected. The table is a symmetric adjacency matrix in ordinary numeric bit positions; row 0 sets bits 39 and 13, and rows 39 and 13 reciprocate bit 0. The printed text order is irrelevant because pass/fail uses integer equality.
- Bubble: no.

### R15 - firmware/schematic - CY reset and interrupt pin constants do not match the board
- Thesis: `CY_RST = 22` and `CY_IRQ_N = 23` were suspected to target the wrong Teensy pins.
- Evidence: firmware uses those constants in `firmware/CY8C9560.h:9-10`. The initial suspicion came from confusing Teensy footprint pad numbers with Arduino digital pin names.
- Claude review: `REJECT` via side agent `019e94a1-0a4e-7951-ab71-aa4deac64041`.
- Adjudication: Rejected. `CY_RST_N` lands on Teensy physical pad 44, whose pin name is `22_A8_CTX1`, and `CY_INT` lands on physical pad 45, whose pin name is `23_A9_CRX1_MCLK1`.
- Bubble: no.

### R16 - schematic - D2 is backwards as a P-channel gate clamp
- Thesis: D2 was suspected to be reversed because its cathode is on +12V/source and its anode is on the Q1 gate.
- Evidence: Q1 is the P-channel SiSS27DN, with source on +12V and gate on `Net-(D2-A)`; D2 is a PMEG10020ELR Schottky rectifier with K on +12V and A on the gate.
- Claude review: `REJECT` via side agent `019e94a1-f5ce-77e0-b4b5-a2c915536c66`.
- Adjudication: Rejected. The part is an ordinary Schottky, not a Zener clamp; reversing it would clamp the normal on-state gate drive instead. At nominal 12 V, the existing `VGS` is about -12 V, within the SiSS27DN ±20 V limit.
- Bubble: no.

### R16b - PCB - J3 is partially off-board, formerly A22/R10
- Thesis: J3 was originally believed to have pads 7 through 40 beyond the right board edge.
- Evidence: the first transform used the wrong handedness. Corrected J3 centers run from x=`174.125` down to x=`125.865`, with rows at y=`94.35` and `96.89`; the PTH drill file confirms those coordinates.
- Claude review: `REJECT` on corrected rereview.
- Adjudication: Rejected. J3 is on-board; the apparent off-board result was a coordinate-transform error.
- Bubble: no.

### R18 - cross-surface fresh hunt
- Thesis: no additional high-confidence cross-surface candidate survived the fresh hunt.
- Evidence: side agent `019e94e3-6053-7321-b3ce-55d5e3dd154c` reported no new candidate beyond the already-approved firmware, schematic, and D3-island issues.
- Claude review: no new candidate to review.
- Adjudication: Rejected as a search pass, not a concrete bug thesis.
- Bubble: no.

### R17 - PCB - J3 pad 2 allegedly shorts CBL_1 and CBL_2 and leaves later contacts unattached
- Thesis: J3 pad 2 was claimed to carry both `CBL_1` and `CBL_2`, with pads 3 through 40 left unattached.
- Evidence: J3 is at `kicad_files/hardware_challenge.kicad_pcb:2469-2473`; pad 2 is explicitly `CBL_1` at `kicad_files/hardware_challenge.kicad_pcb:5560-5569`; pad 3 is explicitly `CBL_2` at `kicad_files/hardware_challenge.kicad_pcb:5571-5580`. The simulator overlay at `../.worktree/harness-tester-challenge/simulator/simulator/data/pcb_fault_overlay.csv:1-8` is hand-authored: it clears all attachments, then reattaches only `CBL_0 -> pad 1`, `CBL_1 -> pad 2`, and `CBL_2 -> pad 2`. `../.worktree/harness-tester-challenge/simulator/simulator/src/board.cpp:165-199` blindly replays that overlay, and `../.worktree/harness-tester-challenge/simulator/simulator/tests/board_model_cases.cpp:58-79` only proves the overlay behaves as authored.
- Claude review: `REJECT` via side agent `019e94e8-684f-71d1-b2a9-25a796c69953`; a direct non-interactive Claude pass also returned `REJECT`; fresh side agent `019e94f5-38e3-7f83-9bab-4e6f441a3a3e` independently returned `REJECT`; side agent `019e958c-76a7-7f80-96e9-79cd7c2a27d8` reran the review with the simulator-gap requirement and returned `REJECT`.
- Adjudication: Rejected. The required simulator gap is explicit: the simulator does not parse KiCad at test time, and the J3 fault is asserted by a hand-authored overlay rather than derived from routed copper. Resolving the J3 `-90` rotation with KiCad's board-coordinate convention gives pad 2 center `(174.125, 96.89)` and pad 3 center `(171.585, 94.35)`, 3.59 mm apart. The PTH drill file independently confirms those coordinates; the user-supplied `(174.125, 91.81)` / `(176.665, 94.35)` pair is the opposite-handed transform. Net 48 starts at pad 2 in `kicad_files/hardware_challenge.kicad_pcb:19148-19154`; net 50 starts at pad 3 in `kicad_files/hardware_challenge.kicad_pcb:19284-19290`. The two route sets have no shared endpoint or overlapping copper. All 40 J3 pads have a same-net route endpoint, including pads 39 and 40 in `kicad_files/hardware_challenge.kicad_pcb:5967-5987` with corresponding route starts at `kicad_files/hardware_challenge.kicad_pcb:17253` and `17469`.
- Bubble: no.

### R19 - schematic/PCB - C5 is allegedly the wrong value because it is 100n rather than 1000pF
- Thesis: C5 was claimed to be an incorrect `100n` capacitor where the MAX2679 reference uses `1000pF` / `1nF`.
- Evidence: C5 is labeled `100n` in `kicad_files/hardware_challenge.kicad_sch:12454-12469`, and it sits in series between U3 RF_IN and L1 in `kicad_files/hardware_challenge.kicad_pcb:1951-1967` and `kicad_files/hardware_challenge.kicad_pcb:10168-10186`.
- Claude review: `REJECT` via side agent `019e9589-540a-7c61-8fd7-a2944f8eb328`.
- Adjudication: Rejected. `100n` is `0.1uF`, which matches the MAX2679 series RF coupling/matching capacitor value. The `1000pF` reference capacitor is the VCC bypass part, not this series C5 path. This thesis compares C5 against the wrong reference component.
- Bubble: no.

### R20 - PCB - antenna keepout allegedly proves the patch reference plane is broken
- Thesis: the all-layer keepout under AE1 was claimed to prove that the patch antenna lacks the required reference plane.
- Evidence: AE1 is centered at `(132, 77)` in `kicad_files/hardware_challenge.kicad_pcb:2279-2455`, inside the `25 mm x 25 mm` all-layer keepout in `kicad_files/hardware_challenge.kicad_pcb:20677-20705`.
- Claude review: `REJECT` via side agent `019e9589-54a7-73a2-be11-610fa1156afe`.
- Adjudication: Rejected. The TE datasheet gives a `70 mm x 70 mm` reference ground plane for its measured plots and recommends similar layout practice, but it does not state that copper must exist directly beneath this exact footprint or on every layer under it. This remains a design-review concern, not a document-backed concrete defect.
- Bubble: no.

### R21 - firmware/schematic - CY8C9560 bus is wired to `Wire` pins but firmware uses `Wire2`
- Thesis: the expander was claimed to be wired to Teensy 4.1 pins 18/SDA and 19/SCL while `CY8C9560.h` aliases `WIRE` to `Wire2`.
- Evidence: the original claim misread the Teensy symbol definition as instantiated wiring. The actual schematic labels `CY_SCL` and `CY_SDA` on U2 pads 16/17, which are `24_A10_TX6_SCL2` and `25_A11_RX6_SDA2` in `kicad_files/hardware_challenge.kicad_sch:6328-6346`; the PCB confirms U2 pad 16 is `CY_SCL` and pad 17 is `CY_SDA` in `kicad_files/hardware_challenge.kicad_pcb:7841-7861`. U2 pads 40/41, the 18/19 pins, are explicitly unconnected in `kicad_files/hardware_challenge.kicad_pcb:8105-8125`.
- Claude review: direct non-interactive Claude pass returned `False`, explaining that the CY8C9560 is actually wired to pins 24/25, which match `Wire2`.
- Adjudication: Rejected. Firmware and instantiated hardware agree on Wire2.
- Bubble: no.

## Parallel Deep Search Audit Notes

These notes preserve candidate theories from the five-way deep-search pass even when they were rejected, derivative, weak, or still awaiting a completed Claude review. They are not automatically included in the numbered approved/rejected counts above.

### Firmware pass - side agent `019e95b7-f99a-71a0-b2da-926cd090fa11`

- `EXPECTED_CONNECTIONS` is an edge list rather than a transitive connected-component observation matrix.
  - Evidence: rows 0, 13, 26, and 39 describe one connected component but are not transitively closed; the simulator's known-good harness intentionally builds connected components from the edge list.
  - Claude review: `ACCEPT-BUT-DERIVATIVE`.
  - Agent verdict: real observation, but derivative of A22 plus A08.
  - Existing-log agreement: agrees with A22/A08 treatment.
  - Bubble: no.

- `process_nmea()` accepts malformed or corrupted `$GPRMC` time/date fields as a time lock.
  - Evidence: it only checks `strncmp("$GPRMC", 6)` and `sscanf(...) == 2`; it does not validate checksum, numeric shape, or date/time ranges.
  - Claude review: not completed before interruption.
  - Agent verdict: plausible independent parser-validation issue.
  - Existing-log agreement: adjacent to A04, but not yet adjudicated as independent.
  - Bubble: pending.

- Later parseable `$GPRMC` lines overwrite `utc_time` and `date` after `time_fixed` is already true.
  - Evidence: `sscanf(...) == 2 && !time_fixed` evaluates `sscanf` first, so later sentences still refresh the buffers.
  - Claude review: not completed before interruption.
  - Agent verdict: derivative refinement of A04/R12 rather than a separate bubble.
  - Existing-log agreement: agrees with R12's rejection and A04's status-field issue.
  - Bubble: no.

- `time_fixed` is never cleared after later loss of GPS validity.
  - Evidence: once set, the flag permanently enables testing.
  - Claude review: not completed before interruption.
  - Agent verdict: probably not a standalone defect because the design may only require one acquired timestamp.
  - Existing-log agreement: no existing direct item.
  - Bubble: no.

### Schematic pass - side agent `019e95b7-fa0c-7ab1-bce1-1547424f9a49`

- D2 is not a real Q1 gate-source transient clamp.
  - Evidence: D2 is `PMEG10020ELR`, a 100 V Schottky rather than a Zener; D1 is `SMAJ16A`; Q1 is `SiSS27DN` with +/-20 V `VGS` max.
  - Claude review: not completed before interruption.
  - Agent verdict: strong provisional accept.
  - Existing-log agreement: agrees with pending P02.
  - Bubble: pending.

- U3 `LNA_EN` lacks a required 10 k pull-down.
  - Evidence: U3 pin 14 is explicitly no-connect.
  - Claude review: not run because local review rejected it first.
  - Agent verdict: not an independent defect as drawn.
  - Existing-log agreement: no prior direct item.
  - Bubble: no.

- U3 `VDD_USB` is wrongly tied high while USB is unused.
  - Evidence: local review found U3 pin 7 tied to GND, matching u-blox guidance for unused USB.
  - Claude review: not run because local review rejected it first.
  - Agent verdict: rejected.
  - Existing-log agreement: no prior direct item.
  - Bubble: no.

- U3 `SAFEBOOT_N` should not be routed to a Teensy GPIO.
  - Evidence: u-blox says SAFEBOOT_N may be left open in normal use but also says service/update access may require it.
  - Claude review: not run because local review found the claim ambiguous.
  - Agent verdict: not enough to call a defect.
  - Existing-log agreement: no prior direct item.
  - Bubble: no.

- CY8C9560 INT needs an external pull-up.
  - Evidence: local review found the datasheet describes INT as active-high strong-drive.
  - Claude review: not run because local review rejected it first.
  - Agent verdict: rejected.
  - Existing-log agreement: no prior direct item.
  - Bubble: no.

- CY8C9560 A0 is invalid because it is tied directly to GND.
  - Evidence: local review found direct GND is equivalent to the permitted strong pull-down strap.
  - Claude review: not run because local review rejected it first.
  - Agent verdict: rejected.
  - Existing-log agreement: no prior direct item.
  - Bubble: no.

- CY8C9560 bus is wired to Teensy `Wire` pins 18/19, but firmware uses `Wire2`.
  - Evidence: schematic uses `18_A4_SDA` and `19_A5_SCL` for `CY_SDA`/`CY_SCL`; `firmware/CY8C9560.h` hardcodes `#define WIRE Wire2`; PJRC documents Teensy 4.1 `Wire2` on pins 25/24.
  - Claude review: narrow follow-up still running.
  - Agent verdict: likely real cross-surface interface mismatch and likely independent from A01/A11.
  - Existing-log agreement: no prior direct item.
  - Bubble: pending.

- MAX2679 missing resistor/input-network detail.
  - Evidence: same RF matching omission already captured by A26.
  - Claude review: not separately needed.
  - Agent verdict: derivative only.
  - Existing-log agreement: agrees with A26 cluster treatment.
  - Bubble: no.

- D2 orientation is backwards.
  - Evidence: rechecked against the existing schematic.
  - Claude review: not rerun.
  - Agent verdict: rejected.
  - Existing-log agreement: agrees with R16.
  - Bubble: no.

- D_SEL floating is invalid.
  - Evidence: rechecked against u-blox defaults.
  - Claude review: not rerun.
  - Agent verdict: rejected.
  - Existing-log agreement: agrees with R05.
  - Bubble: no.

### PCB copper pass - side agent `019e95b7-fa5b-7452-bcca-50c2c6df8485`

- D3 pad 1 is physically disconnected from the main +3.3V rail.
  - Evidence: same D3 island evidence as A16.
  - Claude review: `ACCEPT`.
  - Agent verdict: accept.
  - Existing-log agreement: agrees with A16 exactly.
  - Bubble: already bubbled by A16.

- Possible +12V split/open between Q1 source and U1/C1/C7.
  - Evidence: rejected after tracing continuous net 3 copper from Q1 through D2 to U1.
  - Claude review: not run because local review rejected it first.
  - Agent verdict: rejected.
  - Existing-log agreement: agrees with R03b.
  - Bubble: no.

- Possible +5V open from U1 to Teensy U2.
  - Evidence: rejected after tracing continuous net 4 copper from U1 pad 3 to U2 pad 48.
  - Claude review: not run because local review rejected it first.
  - Agent verdict: rejected.
  - Existing-log agreement: no prior direct item.
  - Bubble: no.

- J3 / CBL pad-center attachment or header short/open.
  - Evidence: all 40 J3 pad centers have same-net route endpoints; no different-net shared endpoint survived the check.
  - Claude review: not rerun.
  - Agent verdict: rejected.
  - Existing-log agreement: agrees with R17.
  - Bubble: no.

### RF/power/protection pass - side agent `019e95b7-fabc-7b02-902c-933135f10371`

- D2 is not a valid Q1 gate-source transient clamp.
  - Evidence: same P02 topology: D2 is a Schottky, D1 can clamp near 26 V, and Q1 has +/-20 V `VGS` max.
  - Claude review: not completed before interruption.
  - Agent verdict: plausible and serious.
  - Existing-log agreement: agrees with P02.
  - Bubble: pending.

- U3 VCC/V_BCKP lack local decoupling.
  - Evidence: U3 VCC and V_BCKP are tied to +3.3 V without a clearly local explicit capacitor.
  - Claude review: not run.
  - Agent verdict: insufficient evidence.
  - Existing-log agreement: no prior direct item.
  - Bubble: no.

- C7 10 uF in generic 0402 footprint may be unsuitable on 12 V input.
  - Evidence: C7 is `10u` with `Capacitor_SMD:C_0402_1005Metric`.
  - Claude review: not run.
  - Agent verdict: weak without a BOM, MPN, voltage rating, or dielectric.
  - Existing-log agreement: no prior direct item.
  - Bubble: no.

- A25 reverse-polarity current path may need explicit current limiting.
  - Evidence: D1 is directly from raw input to GND before Q1.
  - Claude review: not run.
  - Agent verdict: derivative of A25.
  - Existing-log agreement: agrees with A25.
  - Bubble: no new bubble.

- LNA_EN is unconnected while VCC_RF powers external LNA path.
  - Evidence: U3 LNA_EN is open while VCC_RF feeds U5 Vcc.
  - Claude review: not run.
  - Agent verdict: not enough evidence; likely intentional fixed supply use.
  - Existing-log agreement: no prior direct item.
  - Bubble: no.

- V_BCKP tied directly to +3.3 V.
  - Evidence: schematic wiring.
  - Claude review: not run.
  - Agent verdict: probably allowed by u-blox.
  - Existing-log agreement: no prior direct item.
  - Bubble: no.

- L7805 capacitor arrangement.
  - Evidence: 10 uF input, 1 uF output, and 100 nF on +5 V.
  - Claude review: not run.
  - Agent verdict: no clear ST datasheet violation found.
  - Existing-log agreement: no prior direct item.
  - Bubble: no.

### Simulator audit pass - side agent `019e95b7-fb2b-7de2-877c-1263cb0b99e7`

- Simulator supports A01 narrowly.
  - Evidence: original firmware never calls `cy.begin()`, and runtime requires both Wire begun and reset released before expander access works.
  - Claude review: `ACCEPT`.
  - Agent verdict: accept.
  - Existing-log agreement: agrees with A01.
  - Bubble: already bubbled by A01.

- Simulator supports A20 well.
  - Evidence: `schematic_harness_map.csv` maps CBL_20 to raw bit 24, and board-model tests explicitly check bit 24 rather than bit 20.
  - Claude review: `ACCEPT`.
  - Agent verdict: accept.
  - Existing-log agreement: agrees with A20.
  - Bubble: already bubbled by A20.

- Any claim that simulator directly demonstrates A08 is overstated.
  - Evidence: patched schematic tests fail on the raw-bit gap before they can serve as a false-positive witness for `passed = true` on any row.
  - Claude review: incomplete before interruption.
  - Agent verdict: refinement only.
  - Existing-log agreement: agrees A08 is real in source but disagrees with stronger simulator-backed wording.
  - Bubble: no new bubble.

- Simulator reset polarity is opposite the real CY8C9560 conclusion.
  - Evidence: runtime treats `CY_RST_N = 0` as asserted and `1` as released, while R06 concluded the real device reset is active-high.
  - Claude review: not run.
  - Agent verdict: real simulator-model mismatch.
  - Existing-log agreement: agrees with R06 rejection and disagrees with simulator polarity model.
  - Bubble: simulator audit only, not hardware final list.

- R17 overlay is hand-authored, not derived from routed PCB.
  - Evidence: simulator reads checked-in CSV data and blindly replays `pcb_fault_overlay.csv`.
  - Claude review: not rerun.
  - Agent verdict: agrees with rejection.
  - Existing-log agreement: agrees with R17.
  - Bubble: no.

- Unmasked variant bypasses A01/A06/A07 rather than validating them.
  - Evidence: patched variant adds custom `configure_probe()`, manually releases reset, and manually starts Wire.
  - Claude review: not run.
  - Agent verdict: modeling caveat only.
  - Existing-log agreement: agrees A01/A06/A07 are source bugs but disagrees with any stronger simulator claim.
  - Bubble: no.

- Simulator flattens pin-mode/electrical behavior.
  - Evidence: runtime stores writes regardless of output mode, injects button level directly, and infers LED state from stored values.
  - Claude review: not run.
  - Agent verdict: model boundary only.
- Existing-log agreement: agrees A02/A17/A21 are source findings but says simulator is not evidence for their electrical semantics.
- Bubble: no.

## Parallel Rerun Audit Notes

These notes preserve the second parallel pass requested after the list was reopened. They include rejected and disputed theories rather than silently filtering them out.

### Firmware rerun - side agent `019e958c-76a7-7f80-96e9-79cd7c2a27d8`

- Malformed or corrupted `$GPRMC` can become a time lock because the parser checks only prefix plus `sscanf(...) == 2`.
  - Evidence: `firmware/firmware.ino:73-83` uses `strncmp(buf, "$GPRMC", 6)` and permissive scansets for time/date, with no checksum or range validation.
  - Claude review: `REJECT`.
  - Agent verdict: agree. Real defensive-hardening gap, but derivative of A04 and not a board-specific show-stopper.
  - Bubble: no; retained as P04 pending only because the local follow-up still wants a stricter challenge-scope judgment.

- `EXPECTED_CONNECTIONS` is an edge list but firmware compares it as full connected-component observations.
  - Evidence: rows such as 0, 13, and 26 are not transitively closed, while simulator fixture construction turns them into connected components.
  - Claude review: `REJECT`.
  - Agent verdict: agree. This is derivative of A22 plus A08.
  - Bubble: no.

- SAFEBOOT_N is active-low, but firmware writes LOW in setup and would force safe boot.
  - Evidence: `firmware/firmware.ino:106` writes LOW; schematic names the pin `~{SAFEBOOT}`.
  - Claude review: `REJECT`.
  - Agent verdict: agree. As shipped, A02 means the pin was never made an output, so this is only a latent consequence of A02.
  - Bubble: no.

- CY8C9560 ignores I2C transaction failures.
  - Evidence: `firmware/CY8C9560.cpp:17-50` discards `endTransmission()` and `requestFrom()` status.
  - Claude review: `REJECT`.
  - Agent verdict: agree. The concrete absent-expander failure is already A01; the rest is generic robustness.
  - Bubble: no.

- Later parseable RMC lines overwrite `utc_time` and `date` after `time_fixed` is true.
  - Evidence: `sscanf(...) == 2 && !time_fixed` evaluates `sscanf` first, so globals still refresh.
  - Claude review: `REJECT`.
  - Agent verdict: agree. This is desirable current-timestamp behavior and matches R12 rejection.
  - Bubble: no.

- No debounce on test button can produce multiple runs from one physical press.
  - Evidence: raw level gate at `firmware/firmware.ino:137`; append logging at `:86-91`.
  - Claude review: incomplete before interruption.
  - Agent verdict: likely derivative of A23.
  - Bubble: no.

### Schematic rerun - side agent `019e95b7-f99a-71a0-b2da-926cd090fa11`

- `Wire` versus `Wire2` mismatch.
  - Evidence: recheck found `CY_SCL`/`CY_SDA` on U2 pins 24/25, not 18/19.
  - Claude review: `REJECT`.
  - Agent verdict: agree. This became R21.
  - Bubble: no.

- D2 is not a real Q1 gate-source transient clamp.
  - Evidence: PMEG10020ELR is a Schottky, Q1 is +/-20 V VGS, and D1 can clamp above that level.
  - Claude review: not completed in the agent pass.
  - Agent verdict: plausible.
  - Bubble: promoted separately to A28 after local direct Claude review.

- U3 VCC/V_BCKP local decoupling is missing.
  - Evidence: no clearly dedicated local capacitor was found.
  - Claude review: incomplete.
  - Agent verdict: weak / not proven from primary source.
  - Bubble: no.

- U3 LNA_EN is unconnected.
  - Evidence: pin 14 is open while VCC_RF feeds U5.
  - Claude review: incomplete.
  - Agent verdict: likely not independent because fixed VCC_RF-powered operation may be intentional.
  - Bubble: no.

- VIN/VUSB simultaneous-power hazard on Teensy.
  - Evidence: Teensy VIN is tied to +5 V while VUSB remains available.
  - Claude review: incomplete.
  - Agent verdict: conditional integration hazard, not a clear intentional challenge bug.
  - Bubble: no.

- D_SEL floating, VDD_USB tied GND, V_BCKP tied VCC, and missing LNA_EN pulldown.
  - Evidence: rechecked against u-blox primary docs.
  - Claude review: not needed after local disproof.
  - Agent verdict: rejected.
  - Bubble: no.

### PCB rerun - side agent `019e95b7-fa0c-7ab1-bce1-1547424f9a49`

- D3 +3.3 V island.
  - Evidence: same isolated-pad result as A16.
  - Claude review: not rerun.
  - Agent verdict: agrees with A16.
  - Bubble: already bubbled.

- +12 V split/open between Q1 source and U1/C1/C7.
  - Evidence: corrected route/zone tracing shows continuity.
  - Claude review: launched but incomplete.
  - Agent verdict: rejected, agrees with R03b.
  - Bubble: no.

- +5 V open from U1 to U2.
  - Evidence: continuous route from U1 pad 3 to U2 pad 48.
  - Claude review: not launched.
  - Agent verdict: rejected.
  - Bubble: no.

- J3 pad short/open or unattached contacts.
  - Evidence: all 40 pad centers have same-net route endpoints.
  - Claude review: not rerun.
  - Agent verdict: rejected, agrees with R17.
  - Bubble: no.

- AE1 has no ground return because of the all-layer keepout.
  - Evidence: keepout alone does not prove a broken return path.
  - Claude review: not launched.
  - Agent verdict: rejected / derivative of R20.
  - Bubble: no.

- U2 pad 51 unconnected means +3.3 V is floating.
  - Evidence: U2 pads 15 and 46 are connected to +3.3 V; pad 51 is only a redundant unconnected 3V3 pad.
  - Claude review: not launched.
  - Agent verdict: rejected.
  - Bubble: no.

- U3 decoupling and D1 transient return/via density.
  - Evidence: not clearly provable from PCB alone.
  - Claude review: not launched.
  - Agent verdict: weak layout-quality concerns only.
  - Bubble: no.

### RF/power rerun - side agent `019e95b7-fa5b-7452-bcca-50c2c6df8485`

- D2 transient clamp.
  - Evidence: same as A28/P02.
  - Claude review: not run in this pass.
  - Agent verdict: plausible, already logged.
  - Bubble: promoted separately to A28.

- U5 RFOUT/SHDNB floats and disables the LNA.
  - Evidence: MAX2679 datasheet says floating is active.
  - Claude review: not run.
  - Agent verdict: rejected.
  - Bubble: no.

- U3 LNA_EN omission, VCC_RF filter omission, missing U5 0.1 uF, output-route reference concern, antenna keepout, C7 10 uF/0402, L7805 caps, missing SAW filter, and AE1 drill/land pattern.
  - Evidence: checked against primary docs and local source.
  - Claude review: not run after local disproof.
  - Agent verdict: rejected, derivative, or insufficient.
  - Bubble: no.

### Simulator rerun - side agent `019e95b7-fabc-7b02-902c-933135f10371`

- Simulator injects hard-coded `$GPRMC` directly into Serial1.
  - Evidence: `inject_gps()` bypasses real UART wiring and default `$GNRMC` behavior.
  - Claude review: `ACCEPT`.
  - Agent verdict: agree. Simulator-fidelity gap only, consistent with A10/A19.
  - Bubble: no hardware bubble.

- Simulator button polarity is inverted.
  - Evidence: `set_button_pressed(true)` makes BTN_TEST read HIGH, while real button press grounds BTN_TEST.
  - Claude review: `ACCEPT`.
  - Agent verdict: agree. Simulator-fidelity gap only, derivative of A17.
  - Bubble: no hardware bubble.

- Simulator reset polarity is inverted.
  - Evidence: runtime treats low as asserted and high as released, opposite real CY8C9560 behavior.
  - Claude review: `ACCEPT`.
  - Agent verdict: agree. Simulator-fidelity gap only.
  - Bubble: no hardware bubble.

- Simulator LED assertions are not physical LED evidence.
  - Evidence: `led_state()` reports stored pin values, ignoring missing pinMode, red/blue swap, and isolated anode.
  - Claude review: `ACCEPT`.
  - Agent verdict: agree. Simulator-fidelity gap only.
  - Bubble: no hardware bubble.

- Broad R3/I2C-gap thesis, known-good-harness circularity, narrow patched-variant R3 limitation, and board-route-continuity limitation.
  - Evidence: simulator has no electrical I2C model and explicitly does not model board-route continuity.
  - Claude review: broad R3 and circularity rejected; narrow R3 and route continuity incomplete.
  - Agent verdict: model-boundary notes only.
  - Bubble: no.

### Component/package rerun - side agent `019e95b7-fb2b-7de2-877c-1263cb0b99e7`

- U4 footprint mismatch.
  - Evidence: CY8C9560A-24AXIT is 14 mm TQFP while source uses 12 mm TQFP-100.
  - Claude review: agent pass incomplete.
  - Agent verdict: strong provisional new root cause.
  - Bubble: promoted separately to A29 after local direct Claude review and Infineon datasheet check.

- P03, U3 decoupling, C7 10 uF/0402, L7805 caps, V_BCKP, VDD_USB, LNA_EN, and antenna keepout.
  - Evidence: rechecked against local source and primary docs.
  - Claude review: not completed.
  - Agent verdict: rejected or under-proven.
  - Bubble: no.

## Fresh Parallel Expansion Pass

These are preserved from the newer five-way pass requested for broader search depth. They are not promoted to numbered findings unless they survive local rereview.

### Firmware-only pass - side agent `019e95e5-6b3b-7a12-95f7-fc7d53af31a2`

- Stale `DRIVE_MODE_STRONG` state may survive from one probe iteration into the next.
  - Evidence: `set_output()` writes `DRIVE_MODE_STRONG` in `firmware/CY8C9560.cpp:77-83`, while `set_pd_inputs()` writes only `DRIVE_MODE_PULL_DOWN` in `firmware/CY8C9560.cpp:61-66`.
  - Claude review: `DERIVATIVE`.
  - Agent verdict: rejected as independent. `set_pd_inputs()` also writes `REG_PIN_DIRECTION = 0xFF` for every port, so the old strong-drive mode is not an active-drive failure after the helper runs.
  - Agent agreement with Claude: agrees.
  - Bubble: no; derivative of A07.

- Later malformed `$GPRMC` input can partially overwrite an already locked timestamp.
  - Evidence: `sscanf(...)` is evaluated before `!time_fixed` in `firmware/firmware.ino:76`, and `log_result()` later uses the same `utc_time` and `date` buffers in `firmware/firmware.ino:86-95`.
  - Claude review: `REJECT`.
  - Agent verdict: rejected. This overlaps the malformed-input space already represented by P04, while later valid refreshes are already addressed by R12.
  - Agent agreement with Claude: agrees with outcome.
  - Bubble: no; derivative of P04 and R12.

- Full 64-bit input reads are compared directly against a 40-bit harness oracle.
  - Evidence: `NUM_HARNESS_PINS` is 40 in `firmware/firmware.ino:18`, but `read_inputs()` returns all eight expander ports in `firmware/CY8C9560.cpp:57-59`, and the full `values` word is compared in `firmware/firmware.ino:148-156`.
  - Claude review: `REJECT`.
  - Agent verdict: rejected. `set_pd_inputs()` pulls down all unselected bits, including unused upper bits, so they do not independently contaminate the comparison.
  - Agent agreement with Claude: agrees.
- Bubble: no.

### Simulator/evidence pass - side agent `019e95e5-6e39-7301-9cd3-ae4631eeba77`

- `pcb_source_parity` proves routed J3 continuity.
  - Evidence: `../.worktree/harness-tester-challenge/simulator/simulator/tests/board_model_cases.cpp:75-113` parses J3 pad declarations and net fields, while `:148-163` only compares those names to `CBL_0..CBL_39`. It does not inspect segments, vias, zones, drills, or route endpoints. `../.worktree/harness-tester-challenge/simulator/README.md:47-50` explicitly says the simulator does not emulate board-route continuity.
  - Claude review: `REJECT`.
  - Agent verdict: rejected. The test proves metadata parity, not physical copper continuity.
- Agent agreement with Claude: agrees.
- Bubble: no.

### PCB-only pass - side agent `019e95e5-6d3b-7cc0-b9ff-2eee6aa8a586`

- J3 horizontal-header body overlaps U4.
  - Evidence: J3 is the horizontal 2x20 header at `kicad_files/hardware_challenge.kicad_pcb:2469-2472`; U4 is centered at `kicad_files/hardware_challenge.kicad_pcb:8915-8919`. The first Claude pass used the opposite-handed transform and called an overlap. Under the already-verified KiCad transform, J3 local `+x` maps toward board `+y`, so the header body projects off the bottom edge, matching `imgs/board.png`.
  - Claude review: first pass `ACCEPT`, corrected rereview `REJECT`.
  - Agent verdict: rejected.
  - Agent agreement with Claude: disagrees with the first pass, agrees with corrected rereview.
  - Bubble: no; adjacent to prior J3 coordinate-transform rejects.

- U5-to-U3 RF output path has the same missing local GND-reference defect as A27.
  - Evidence: output-side RF nets are routed on F.Cu at `kicad_files/hardware_challenge.kicad_pcb:15858-15905` and `19525-19555`. Unlike A27's antenna-side path, these segments are outside the all-layer keepout at `kicad_files/hardware_challenge.kicad_pcb:20677-20705`.
  - Claude review: `REJECT`.
  - Agent verdict: rejected.
  - Agent agreement with Claude: agrees.
  - Bubble: no; not a distinct A27 extension.

- AE1 is invalid because it is flush to the left edge and the keepout removes its plane.
  - Evidence: AE1 is centered at `kicad_files/hardware_challenge.kicad_pcb:2279-2282`, with 25 mm body geometry at `2350-2427`; the board edge is at x=`119.5` in `11943-12047`, and the keepout exactly matches the 25x25 antenna footprint at `20677-20705`.
  - Claude review: `REJECT`.
  - Agent verdict: rejected.
  - Agent agreement with Claude: agrees.
  - Bubble: no; derivative of R20.

- U1 has a package mismatch because L7805 is in TO-252.
  - Evidence: U1 is explicitly `Package_TO_SOT_SMD:TO-252-2` at `kicad_files/hardware_challenge.kicad_pcb:6483-6488`. Its value is only generic `L7805`, and the PCB description lists TO-220, TO-263, and TO-252 as valid package families at `6500-6536`.
  - Claude review: `REJECT`.
  - Agent verdict: rejected.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- U3 lacks local VCC/V_BCKP decoupling.
  - Evidence: U3 V_BCKP and VCC are on +3.3V at `kicad_files/hardware_challenge.kicad_pcb:11092-11110`. C4 is at `11733-11801`, much closer to U4 than U3.
  - Claude review: `DERIVATIVE`.
  - Agent verdict: derivative, not a new bubble.
- Agent agreement with Claude: agrees.
- Bubble: no; same weak/rejected U3-decoupling concern already logged.

### Cross-surface/package pass - side agent `019e95e5-6f47-7552-8ca9-45c84d334898`

- `SAFEBOOT_N` is driven LOW and would force GPS safe boot.
  - Evidence: `firmware/firmware.ino:7` defines `PIN_UBX_SAFEBOOT = 3`; `firmware/firmware.ino:106` writes `digitalWrite(PIN_UBX_SAFEBOOT, LOW)`; U3 pin 2 is `~{SAFEBOOT}` in `kicad_files/hardware_challenge.kicad_sch:4417`, and the `UBX-SAFEBOOT` net appears at `10064` and `12132`.
  - Claude review: `DERIVATIVE`.
  - Agent verdict: not a separate bug. A02 already establishes that the pin is never configured as an output, so the LOW write is not actively driving the GPS as shipped.
  - Agent agreement with Claude: agrees.
  - Bubble: no; derivative of A02.

- U5 MAX2679 WLP pin map is wrong.
  - Evidence: U5 is `MAX2679` in `kicad_files/hardware_challenge.kicad_sch:4193`; the symbol defines A1/Vcc, A2/RFOUT, B1/RFIN, and B2/GND at `4424`, `4622`, `4730`, and `4813`; the PCB repeats those assignments at `kicad_files/hardware_challenge.kicad_pcb:6433-6468`.
  - Claude review: `REJECT`.
  - Agent verdict: rejected. Symbol, footprint, and datasheet pin map agree.
- Agent agreement with Claude: agrees.
- Bubble: no.

### Schematic-only pass - side agent `019e95e5-6c45-7190-8783-438348ded934`

- C1/C7 may be invalid +12V 0402 capacitors.
  - Evidence: C1 is `1u` in `Capacitor_SMD:C_0402_1005Metric` at `kicad_files/hardware_challenge.kicad_sch:14669`; C7 is `10u` in the same generic 0402 footprint at `kicad_files/hardware_challenge.kicad_sch:16159`; neither has an MPN or voltage rating.
  - Claude review: `REJECT`.
  - Agent verdict: rejected. The source does not prove a functional failure without a BOM or voltage-rating constraint.
- Agent agreement with Claude: agrees.
- Bubble: no.

### Source-vs-gerber parity pass - side agent `019e95ef-7304-7931-ad42-5490938b5716`

- Top-level `gerbers.zip` is stale relative to `kicad_project.zip`.
  - Evidence: `kicad_files/hardware_challenge.kicad_pcb` is byte-identical to `kicad_project.zip:hardware_challenge.kicad_pcb`. Every top-level `gerbers.zip` member is byte-identical to the matching `kicad_project.zip:gerbers/<member>` file, including copper, mask, edge, drill, and job files.
  - Claude review: first pass was internally inconsistent, saying `ACCEPT` while also saying there was no mismatch; rerun `REJECT`.
  - Agent verdict: rejected. The two archives carry the same fabrication artifact set.
  - Agent agreement with Claude: agrees with rerun, disagrees with malformed first verdict.
  - Bubble: no.

- Live copper layer is omitted from export.
  - Evidence: source declares exactly four signal layers at `kicad_files/hardware_challenge.kicad_pcb:15-19`: `F.Cu`, `In1.Cu`, `In2.Cu`, and `B.Cu`. `gerbers.zip:hardware_challenge-job.gbrjob:44-64` lists exactly those four copper outputs, and all four `.gbr` files exist.
  - Claude review: `REJECT`.
  - Agent verdict: rejected. No copper layer omission is shown.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- J3 drill pattern is missing or stale in Gerbers.
  - Evidence: J3 source origin is at `kicad_files/hardware_challenge.kicad_pcb:2469-2473`; pad declarations are at `:5549-5579`. Using KiCad's board-coordinate rotation, pads 1/2/3 land at about `(174.125,94.35)`, `(174.125,96.89)`, and `(171.585,94.35)` mm. `gerbers.zip:hardware_challenge-PTH.drl:327-330` and `:387-388` contain matching inch/y-negative drill coordinates.
  - Claude review: `REJECT`.
  - Agent verdict: rejected. The apparent mismatch was another coordinate-transform mistake.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- J1 NPTH export is missing or malformed.
  - Evidence: J1 NPTH pads are a `2 x 1.5 mm` oval slot and a `2 mm` round hole at `kicad_files/hardware_challenge.kicad_pcb:11465-11477`. `gerbers.zip:hardware_challenge-NPTH.drl:16-22` contains one `T2` round hit plus the `T1` slot sequence.
  - Claude review: `REJECT`.
  - Agent verdict: rejected. The slot is represented correctly as drill motion, not as a second center point.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- PTH drill counts are stale or incomplete.
  - Evidence: source has 258 vias at `0.3 mm` and 108 through-hole pads: 59 at `1.1 mm`, 40 at `1.0 mm`, 8 at `0.8 mm`, and 1 at `1.2 mm`. `gerbers.zip:hardware_challenge-PTH.drl` has 366 points with matching inch-rounded tool counts: 258 at `0.29972`, 59 at `1.09982`, 40 at `1.00076`, 8 at `0.80010`, and 1 at `1.19888` mm.
  - Claude review: `REJECT`.
  - Agent verdict: rejected. Counts and sizes match after export rounding.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- Edge_Cuts outline differs between source and Gerber.
  - Evidence: source outline and slots are at `kicad_files/hardware_challenge.kicad_pcb:11943-12047`. `gerbers.zip:hardware_challenge-Edge_Cuts.gbr:27-56` contains the same outer boundary and the same two slot rectangles.
  - Claude review: `REJECT`.
  - Agent verdict: rejected. No outline contradiction is present.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- J1 pin 3 tied to GND may misuse the barrel-jack switch contact.
  - Evidence: J1 is `Barrel_Jack_Switch` with Cliff FC681465S footprint at `kicad_files/hardware_challenge.kicad_sch:16503`; local net tracing shows pin 1 on raw input and pins 2/3 on GND.
  - Claude review: first terse pass `ACCEPT`; stricter adversarial rerun `REJECT`.
  - Agent verdict: rejected. The pin-3 behavior is not established by the schematic alone.
  - Agent agreement with Claude: agrees with stricter rejection.
  - Bubble: no.

- U2 redundant power pins are left unconnected.
  - Evidence: U2 is defined at `kicad_files/hardware_challenge.kicad_sch:12720`; local net tracing shows U2 pin 51 (`3V3`) and pin 52 (`GND`) unconnected while pins 15/46 are on +3.3V and pins 1/34/47 are on GND.
  - Claude review: `REJECT`.
  - Agent verdict: rejected. These are redundant module pins, not required separate rails.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- J1 barrel-jack symbol/footprint pin numbering is inconsistent.
  - Evidence: J1 is instantiated with pins 1/2/3 in `kicad_files/hardware_challenge.kicad_sch:16503-16553`; the PCB maps pad 1 to raw input, pad 2 to GND, and pad 3 to GND at `kicad_files/hardware_challenge.kicad_pcb:11479-11509`.
  - Claude review: `REJECT`.
  - Agent verdict: rejected. Pin 1 is the barrel center/input, pin 2 is sleeve/GND, and pin 3 is the switched contact.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- C7 10u in 0402 on +12V is an impossible package/value mismatch.
  - Evidence: C7 is `10u` in `C_0402_1005Metric` at `kicad_files/hardware_challenge.kicad_sch:16159-16177`; the PCB repeats that footprint at `kicad_files/hardware_challenge.kicad_pcb:963-1030` and puts it across +12V/GND at `1143-1159`.
  - Claude review: `REJECT`.
  - Agent verdict: rejected. 10u 0402 parts exist, and without an MPN or voltage-rating field this is not a provable package mismatch. The narrower voltage-rating concern remains under-proven.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- J3 metadata-only parity is a simulator evidence limitation.
  - Evidence: same `board_model_cases.cpp` evidence as above; `../.worktree/harness-tester-challenge/simulator/simulator/src/board.cpp:124-143,157-220` derives runtime connectivity from the schematic map rather than routed copper.
  - Claude review: `ACCEPT`.
  - Agent verdict: accepted as a simulator-only gap. This does not revive R17 because the actual log already has independent route-level disproof for that claimed J3 short/open.
  - Agent agreement with Claude: agrees.
  - Bubble: no hardware bubble.

- The unmasked variant is not evidence about original A05 shift behavior.
  - Evidence: original `firmware/firmware.ino:143-152` uses `1 << i` and `1 << j`, while `../.worktree/harness-tester-challenge/simulator/simulator/variants/firmware_unmasked.ino:159-166` uses `1ULL << i` and `1ULL << j`. `../.worktree/harness-tester-challenge/simulator/README.md:70-74` describes the variant as a separate patched path.
  - Claude review: `ACCEPT`.
  - Agent verdict: accepted as a simulator-only evidence limitation. The variant remains useful for exposing A20, but it is not a witness for original A05 because indices 32 through 39 execute different expressions.
  - Agent agreement with Claude: agrees.
  - Bubble: no hardware bubble.

- `schematic_io_map.csv` independently proves real GPIO/I2C mapping.
  - Evidence: `../.worktree/harness-tester-challenge/simulator/simulator/data/README.md:3-11` says runtime uses checked-in board data; `../.worktree/harness-tester-challenge/simulator/simulator/src/board.cpp:146-154` loads IO CSV rows directly; `board_model_cases.cpp:141-163` checks J3/harness mapping only, not IO-map entries; `runtime.cpp:316-330,386-390` relies on those IO names for reset, button, and LED behavior.
  - Claude review: `REJECT`, but its reasoning explicitly says the IO-map entries are never cross-checked.
  - Agent verdict: accepted as a simulator-only gap because the reasoning concedes the missing parity check.
  - Agent agreement with Claude: disagrees with the label, agrees with the reasoning.
  - Bubble: no hardware bubble.

- Original-good/original-broken simulator cases are usable as harness-topology witnesses.
  - Evidence: `../.worktree/harness-tester-challenge/simulator/simulator/tests/firmware_cases.cpp:147-151` expects `expect_wire=false` and has no probe-line assertion for both original cases. `../.worktree/harness-tester-challenge/simulator/simulator/src/runtime.cpp:351-360` returns empty I2C data when unavailable; `:254-259` returns `-1` on empty read; `firmware/CY8C9560.cpp:25-33` ORs those reads into the input word.
  - Claude review: `ACCEPT`.
- Agent verdict: accepted only as a derivative evidence note. Those cases witness A01 failure, not harness topology, because both fixtures fail before meaningful connectivity measurement.
- Agent agreement with Claude: agrees.
- Bubble: no.

## Latest Parallel Split

This section preserves the newest parallel pass requested after reopening the search. The newest agents were asked to run the same adversarial Claude review protocol, but several fresh local sessions could not see the host Claude login and therefore reported `unavailable` rather than fabricating a review result.

### Firmware parser/state pass - side agent `019e95fc-244b-7941-9386-8f5f2bebf1f8`

- `EXPECTED_CONNECTIONS` may be an edge list while runtime observes connected components.
  - Evidence: `firmware/firmware.ino:20-60` contains rows such as row 0 -> bits 13/39 and row 13 -> bits 0/26, while `firmware/firmware.ino:143-156` compares the whole observed row directly.
  - Claude review: unavailable in the agent sandbox.
  - Agent verdict: disputed, not bubbled. It is technically plausible if the table is intended as topology, but overlaps A22/A08 and is not independently source-proven as intended behavior.
  - Agent agreement with Claude: N/A.
  - Bubble: no.

- Previously probed pins may retain strong-drive mode.
  - Evidence: `firmware/CY8C9560.cpp:77-83` writes strong-drive mode, while `firmware/CY8C9560.cpp:61-66` writes pull-down mode.
  - Claude review: unavailable in the agent sandbox.
  - Agent verdict: rejected as independent because `set_pd_inputs()` also writes direction `0xFF`; A07 already captures the dominant failure.
  - Agent agreement with Claude: N/A.
  - Bubble: no.

- Later malformed RMC can corrupt an already locked timestamp.
  - Evidence: `sscanf(...) == 2 && !time_fixed` in `firmware/firmware.ino:76` evaluates the parse before the lock check, and `log_result()` later emits the same buffers in `firmware/firmware.ino:86-90`.
  - Claude review: unavailable in the agent sandbox.
  - Agent verdict: rejected as independent; this is a malformed-input variant of P04 plus R12.
  - Agent agreement with Claude: N/A.
  - Bubble: no.

- `time_fixed` is never cleared after later loss of GPS validity.
  - Evidence: `firmware/firmware.ino:75-83` only ever sets the boolean, and `firmware/firmware.ino:133-139` gates testing only on that boolean.
  - Claude review: unavailable in the agent sandbox.
  - Agent verdict: rejected; the design only requires an acquired timestamp, not continuously valid live navigation.
  - Agent agreement with Claude: N/A.
  - Bubble: no.

- Full 64-bit expander reads are compared against a 40-bit oracle.
  - Evidence: `firmware/CY8C9560.cpp:57-59` reads all eight ports, while `firmware/firmware.ino:18` and `155` define and compare only the 40-pin oracle.
  - Claude review: unavailable in the agent sandbox.
  - Agent verdict: rejected; `set_pd_inputs(~output_mask)` intentionally pulls down unselected upper bits, and A20 is the actual mapping defect.
  - Agent agreement with Claude: N/A.
  - Bubble: no.

- Malformed/corrupted `$GPRMC` acceptance without checksum/range validation.
  - Evidence: `firmware/firmware.ino:73-83`.
  - Claude review: unavailable in the agent sandbox; coordinator Claude later returned `REJECT`.
  - Agent verdict: rejected as new; this is already P04 and is parser hardening rather than a stronger distinct bug.
  - Agent agreement with Claude: agrees with coordinator rejection.
  - Bubble: no.

### Power-tree pass - side agent `019e95f1-da9e-7611-b6dd-f803ad3b7598`

- Teensy VUSB/VIN backfeed hazard during USB service.
  - Evidence: U2 VIN is on `+5V` at `kicad_files/hardware_challenge.kicad_pcb:8193-8202`; U2 VUSB is unconnected at `kicad_files/hardware_challenge.kicad_pcb:8204-8213`.
  - Claude review: `ACCEPT` in the side-agent pass; coordinator Claude later returned `DISPUTED`.
  - Agent verdict: accept as a narrow service/programming-path defect.
  - Agent agreement with Claude: agrees with the side-agent Claude accept, disagrees with the coordinator Claude dispute.
  - Bubble: reopened as P05, not yet approved.

- U3 VCC/V_BCKP local decoupling is missing.
  - Evidence: U3 power pins are on `+3.3V` at `kicad_files/hardware_challenge.kicad_pcb:11092`, while the obvious capacitors are C3/C4 elsewhere.
  - Claude review: `REJECT`.
  - Agent verdict: rejected as layout-quality concern, not primary-source-backed challenge defect.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

- C7 `10u` in 0402 on `+12V`.
  - Evidence: `kicad_files/hardware_challenge.kicad_sch:16159` and `kicad_files/hardware_challenge.kicad_pcb:1143`.
  - Claude review: `DERIVATIVE`.
  - Agent verdict: rejected as underconstrained without MPN, dielectric, or voltage-rating data.
  - Agent agreement with Claude: partial.
  - Bubble: no.

- U1 thermal dissipation, L7805 capacitor arrangement, U3 LNA_EN open, V_BCKP tied to VCC, C6 value wording, missing fuse/current limit before D1, +3.3V rail overload, and missing extra VCC_RF filtering.
  - Evidence: checked against the local schematic/PCB and primary part documentation.
  - Claude review: `REJECT` or `DERIVATIVE`.
  - Agent verdict: rejected or derivative of A18/A25.
  - Agent agreement with Claude: agrees.
  - Bubble: no.

### Fabrication/package pass - side agent `019e95fc-2416-7da0-bf91-8d89a4a8efd3`

- D3 selected-part pinout mismatch.
  - Evidence: `ASMB-KTF0-0A306` is selected at `kicad_files/hardware_challenge.kicad_sch:13441`; local pad mapping has pad 2 -> `LED_B`, pad 3 -> `LED_G`, pad 4 -> `LED_R` at `kicad_files/hardware_challenge.kicad_pcb:915-922`.
  - Claude review: unavailable in the agent sandbox.
  - Agent verdict: accepted only as a stronger A24 refinement, not a new bubble. The selected Broadcom part's actual pin order makes the red/blue swap more concrete.
  - Agent agreement with Claude: N/A.
  - Bubble: already represented by A24.

- C7 `10u`/0402, AE1 keepout as a broken plane, stale/incomplete Gerbers, and internal Edge.Cuts rectangles.
  - Evidence: checked against `kicad_files/hardware_challenge.kicad_sch`, `kicad_files/hardware_challenge.kicad_pcb`, `gerbers.zip`, and `kicad_project.zip`.
  - Claude review: unavailable in the agent sandbox.
  - Agent verdict: rejected. C7 remains under-proven, A27 is the narrower real keepout issue, Gerbers match the project export, and the internal rectangles are not proven accidental.
  - Agent agreement with Claude: N/A.
  - Bubble: no.

### Simulator/provenance pass - side agent `019e95fc-242e-7a32-b6e8-12cce1e791cb`

- `pcb_source_parity` proves routed J3 continuity.
  - Evidence: `simulator/simulator/tests/board_model_cases.cpp:75-163` checks pad metadata and names only; `simulator/README.md:47-50` says route continuity is not modeled.
  - Claude review: unavailable in the agent sandbox.
  - Agent verdict: rejected as physical proof; metadata parity only.
  - Agent agreement with Claude: N/A.
  - Bubble: no.

- `schematic_io_map.csv` independently proves GPIO/I2C wiring.
  - Evidence: `simulator/simulator/src/board.cpp:97-154` loads the CSV directly, while parity tests only check J3/harness mapping.
  - Claude review: unavailable in the agent sandbox.
  - Agent verdict: rejected as independent proof; simulator provenance gap only.
  - Agent agreement with Claude: N/A.
  - Bubble: no.

- Simulator reset polarity, hard-coded `$GPRMC` injection, original-good/original-broken topology evidence, deleted J3 overlay, and dirty simulator worktree provenance.
  - Evidence: `simulator/simulator/src/runtime.cpp:318-325`, `simulator/simulator/src/original_main.cpp:14`, `simulator/simulator/tests/firmware_cases.cpp:141-154`, and the existing R17 source evidence.
  - Claude review: unavailable in the agent sandbox.
  - Agent verdict: rejected as hardware proof. The simulator remains useful for A01/A20-style logic, but not for physical copper continuity, actual GPIO wiring, actual reset polarity, or real GPS transport.
  - Agent agreement with Claude: N/A.
  - Bubble: no.

## Simulator Evidence Admission Pass

Source under review: `../.worktree/harness-tester-challenge/simulator/SIMULATOR_BUG_EVIDENCE.md`.

Result: 11/11 simulator-listed items remain approved as existing findings; 0 new bubbles; 0 bug theses rejected. The simulator file is a narrower corroboration ledger, not a new source of distinct findings.

Reviewed by side agents `019e95e5-6c45-7190-8783-438348ded934`, `019e95ef-7304-7931-ad42-5490938b5716`, `019e95f1-d8ac-70e0-8bc6-8e14356ac017`, `019e95f1-da9e-7611-b6dd-f803ad3b7598`, and `019e95f1-dcb3-79f2-9bd6-790857392b29`, plus a usable coordinator Claude `opus/xhigh` pass. One separate stale Claude pass incorrectly claimed the source file was absent and was discarded.

- A01 - `cy.begin()` is never called.
  - Existing disposition: approved A01 / final finding 1.
  - Admission result: accepted existing.
  - Evidence: `setup()` never calls `cy.begin()` in `firmware/firmware.ino:97-116`; `begin()` performs reset, Wire2 setup, clock setup, and ID check in `firmware/CY8C9560.cpp:3-15`.
  - Simulator caveat: original-firmware test corroborates the missing initialization, but does not create the thesis.
  - Bubble: already bubbled.

- A02 - GPS SAFEBOOT and reset pins are written without output mode.
  - Existing disposition: approved A02 / final finding 2.
  - Admission result: accepted existing.
  - Evidence: `firmware/firmware.ino:103-107` writes pins 3 and 4 without any `pinMode(..., OUTPUT)`.
  - Simulator caveat: runtime observes default input mode, but the source omission is direct.
  - Bubble: already bubbled.

- A04 - `$GPRMC` validity/status is ignored.
  - Existing disposition: approved A04 / final finding 4.
  - Admission result: accepted existing.
  - Evidence: `firmware/firmware.ino:75-79` uses `%*c` for the RMC status byte and sets `time_fixed` after any successful time/date parse.
  - Simulator caveat: witness runs through `firmware_unmasked.ino`, but that variant does not alter `process_nmea()`.
  - Bubble: already bubbled.

- A06 - `set_output()` makes every expander pin an output.
  - Existing disposition: approved A06 / final finding 6.
  - Admission result: accepted existing.
  - Evidence: `firmware/CY8C9560.cpp:77-83` writes `REG_PIN_DIRECTION = 0x00` for all eight ports.
  - Simulator caveat: helper test manually initializes reset and Wire before isolating the helper.
  - Bubble: already bubbled.

- A07 - `set_pd_inputs()` immediately undoes the selected output.
  - Existing disposition: approved A07 / final finding 7.
  - Admission result: accepted existing.
  - Evidence: `firmware/firmware.ino:143-146` calls `set_output()` then `set_pd_inputs()`, and `firmware/CY8C9560.cpp:61-66` writes `REG_PIN_DIRECTION = 0xFF` for all ports.
  - Simulator caveat: helper test isolates the sequence, but source already proves the clobber.
  - Bubble: already bubbled.

- A09 - FAILED status is overwritten by GOOD on the next loop.
  - Existing disposition: approved A09 / final finding 9.
  - Admission result: accepted existing.
  - Evidence: `firmware/firmware.ino:133-135` unconditionally calls `set_status(GOOD)` before the button gate, while failure is only written at `:163`.
  - Simulator caveat: witness uses `firmware_unmasked.ino` to expose the later loop state; physical LED visibility still depends on A21.
  - Bubble: already bubbled.

- A17 - test button polarity is inverted.
  - Existing disposition: approved A17 / final finding 14.
  - Admission result: accepted existing.
  - Evidence: `firmware/firmware.ino:137-139` returns on LOW, while the schematic uses a pull-up plus switch-to-ground button path.
  - Simulator caveat: runtime hardcodes pressed = LOW, so the simulator is circular on polarity; the schematic is the real proof.
  - Bubble: already bubbled.

- A19 - firmware only accepts `$GPRMC`, not default `$GNRMC`.
  - Existing disposition: approved A19 / final finding 16.
  - Admission result: accepted existing.
  - Evidence: `firmware/firmware.ino:75-76` only matches literal `$GPRMC`; there is no reconfiguration path for the NEO-M8N default multi-GNSS talker output.
  - Simulator caveat: synthetic GPS injection corroborates parser behavior, but the default-talker's `GN` premise remains external-doc-backed.
  - Bubble: already bubbled.

- A20 - harness logical index diverges from expander register-bit index after CBL_19.
  - Existing disposition: approved A20 / final finding 17.
  - Admission result: accepted existing.
  - Evidence: firmware uses raw `1 << i` in `firmware/firmware.ino:143-152`, while the KiCad mapping places CBL_20 on GPort3 bit 0, raw bit 24.
  - Simulator caveat: `bug_a20` runs through `firmware_unmasked.ino`; the stronger evidence is the board-model parse of the real KiCad source, not the patched probe line alone.
  - Bubble: already bubbled.

- A21 - status LED pins are never configured as outputs.
  - Existing disposition: approved A21 / final finding 18.
  - Admission result: accepted existing.
  - Evidence: `firmware/firmware.ino:9-11`, `:67-71`, and `:97-105` contain writes to pins 5, 6, and 7 but no `pinMode(..., OUTPUT)` for them.
  - Simulator caveat: LED-state interpretation is model-defined, but the missing mode setup is direct source evidence.
  - Bubble: already bubbled.

- A23 - a held valid test gate reruns and re-logs indefinitely.
  - Existing disposition: approved A23 / final finding 20.
  - Admission result: accepted existing.
  - Evidence: `firmware/firmware.ino:137-163` has only a level gate and `log_result()` appends on each run in `:86-91`.
  - Simulator caveat: witness uses `firmware_unmasked.ino` and the current HIGH gate because the shipped polarity bug otherwise prevents a physical press from running the test.
  - Bubble: already bubbled.

## Six-Way Parallel Admission Pass

Scope: fresh independent passes over firmware/parser behavior, pin/package mapping, PCB continuity, power-tree behavior, RF/front-end behavior, and fabrication/assembly concerns.

Result: no new independent bubble survived. The only positive new result was a stronger witness for existing A03: an ordinary u-blox RMC example already exceeds the 64-byte firmware buffer. P04 and P05 remain pending because the latest pass did not justify promotion over the existing split review history.

Review protocol: side agents `019e96c6-2361-71c0-a5d0-497d07565ac8`, `019e96c6-3353-7790-afa9-1f8ba629d5ab`, `019e96c6-434a-7b82-a95e-4ccc829bd6af`, `019e96c6-54e8-7021-acb3-2c9efcfa00cf`, `019e96c6-653c-73a0-99bf-3da66a34d988`, and `019e96c6-7425-7510-be77-66ba72e2c609` each used non-interactive `claude -p --model opus --effort xhigh --no-session-persistence` disproof passes where available. Scratch-only helpers were kept under `audit_work/`; no source, simulator, or log files were edited by the agents.

### Firmware/parser pass - side agent `019e96c6-2361-71c0-a5d0-497d07565ac8`

- Ordinary vendor RMC overflows `nmea_buf[64]`.
  - Evidence: `firmware/firmware.ino:118-126` has an unbounded write until CR/LF, while the u-blox M8 RMC example is 75 bytes including CRLF.
  - Claude review: `ACCEPT`.
  - Agent verdict: stronger A03 witness, not a new sink or new bubble.
  - Bubble: already represented by A03.

- Later malformed RMC partially corrupts an already-locked timestamp.
  - Evidence: `sscanf(...) == 2 && !time_fixed` in `firmware/firmware.ino:76` evaluates `sscanf` first, so a later malformed line can refresh `utc_time` while leaving stale `date`.
  - Claude review: `DISPUTED`.
  - Agent verdict: real mechanism, but derivative of P04 plus rejected R12.
  - Bubble: no.

- Empty speed/course blocks an otherwise parseable RMC.
  - Evidence: `%*f,%*f` in `firmware/firmware.ino:76` rejects blank speed/course fields.
  - Claude review: `DISPUTED`.
  - Agent verdict: no target-output proof that this is a real default-stream failure.
  - Bubble: no.

- CRLF causes meaningful double-processing.
  - Evidence: `loop()` processes on either CR or LF in `firmware/firmware.ino:122-126`.
  - Claude review: `REJECT`.
  - Agent verdict: the second call sees an empty buffer, so it cannot match `$GPRMC`.
  - Bubble: no.

- Full 64-bit input read may contaminate the 40-bit compare.
  - Evidence: `read_inputs()` reads eight bytes in `firmware/CY8C9560.cpp:57-59`, while `firmware/firmware.ino:155` compares the whole `uint64_t`.
  - Claude review: `DISPUTED`.
  - Agent verdict: no source proof that unused upper bits read nonzero.
  - Bubble: no.

- Unchecked I2C short reads become `0xFF` measurements.
  - Evidence: `read_register()` and `read_registers()` ignore `requestFrom()` and `available()` state in `firmware/CY8C9560.cpp:17-34`.
  - Claude review: `REJECT`.
  - Agent verdict: this is generic I2C robustness and still tends to fail rather than false-pass; A01/A11 are the concrete board failures.
  - Bubble: no.

- `set_output()` clobbers all output latches, not only the selected probe bit.
  - Evidence: `write_registers(REG_OUTPUT_BASE, values, 8)` in `firmware/CY8C9560.cpp:77-78` writes all eight bytes from `output_mask`.
  - Claude review: `DISPUTED`.
  - Agent verdict: real but immediately dominated by A07 because `set_pd_inputs()` returns all pins to inputs.
  - Bubble: no.

- No explicit electrical settling delay before read.
  - Evidence: `firmware/firmware.ino:145-148` changes output/pulldown state and reads without `delay()`.
  - Claude review: `REJECT`.
  - Agent verdict: the helper sequence already performs many 100 kHz I2C transactions before the read; no source proves extra settling is needed.
  - Bubble: no.

- Probe loop can starve Serial1 and stale the timestamp.
  - Evidence: `loop()` reads only one sentence before entering the 40-pin probe loop in `firmware/firmware.ino:122-163`.
  - Claude review: `REJECT`.
  - Agent verdict: no GPS rate or buffer-depth proof, and `time_fixed` is already latched before testing.
  - Bubble: no.

- SAFEBOOT LOW is a separate present bug.
  - Evidence: `firmware/firmware.ino:106` writes LOW to SAFEBOOT.
  - Claude review: `REJECT`.
  - Agent verdict: derivative of A02 because the pin is never made OUTPUT.
  - Bubble: no.

- `time_fixed` never clears after later loss of GPS validity.
  - Evidence: `process_nmea()` only sets the flag in `firmware/firmware.ino:73-83`; `loop()` only checks that boolean in `:133-135`.
  - Claude review: `REJECT`.
  - Agent verdict: README requires time acquisition for logging, not continuous navigation validity.
  - Bubble: no.

- Timestamp refresh after lock is a logging race.
  - Evidence: later valid RMCs refresh globals before `log_result()` uses them in `firmware/firmware.ino:76-91`.
  - Claude review: `REJECT`.
  - Agent verdict: latest valid time is consistent with the stated logging goal.
  - Bubble: no.

- `SD.open()` failure silently drops a result.
  - Evidence: `log_result()` only prints debug text if `SD.open()` fails in `firmware/firmware.ino:86-94`.
  - Claude review: initial `ACCEPT`, strict rerun `REJECT`.
  - Agent verdict: conditional runtime error handling, not a deterministic board/source defect.
  - Bubble: no.

### Pin/package pass - side agent `019e96c6-3353-7790-afa9-1f8ba629d5ab`

- U3 SAFEBOOT is on the wrong pin.
  - Evidence: schematic and PCB show pin/pad 1 is SAFEBOOT and pin/pad 2 is D_SEL in `kicad_files/hardware_challenge.kicad_sch:4417` and `kicad_files/hardware_challenge.kicad_pcb:10882`.
  - Claude review: `REJECT`.
  - Agent verdict: board is correct; an older note had stale pin wording.
  - Bubble: no.

- U4 has a second pin-map defect beyond A29.
  - Evidence: U4 symbol and footprint pin assignments match the Infineon table.
  - Claude review: `REJECT`.
  - Agent verdict: only the body-size/pitch mismatch in A29 survives.
  - Bubble: no.

- U1 `L7805` in `TO-252-2` is a package mismatch.
  - Evidence: `kicad_files/hardware_challenge.kicad_sch:15082` and `kicad_files/hardware_challenge.kicad_pcb:6483` map IN/GND/OUT correctly.
  - Claude review: `REJECT`.
  - Agent verdict: generic `L7805` does not prove a package conflict.
  - Bubble: no.

- U5 MAX2679 WLP pad map/orientation is wrong.
  - Evidence: U5 maps A1=Vcc, A2=RFOUT, B1=RFIN, B2=GND in `kicad_files/hardware_challenge.kicad_sch:4260` and `kicad_files/hardware_challenge.kicad_pcb:6433`.
  - Claude review: `REJECT`.
  - Agent verdict: pin map is correct; existing U5 defects remain A18/A26.
  - Bubble: no.

- D2 K/A footprint polarity is reversed.
  - Evidence: pin/pad 1 is K and pin/pad 2 is A in `kicad_files/hardware_challenge.kicad_sch:3971` and `kicad_files/hardware_challenge.kicad_pcb:8750`.
  - Claude review: `REJECT`.
  - Agent verdict: polarity is correct; A28 is the Schottky-versus-Zener issue.
  - Bubble: no.

- D1 SMAJ16A symbol/footprint polarity is wrong.
  - Evidence: pin/pad 1 is cathode and pin/pad 2 is anode in `kicad_files/hardware_challenge.kicad_sch:4142` and `kicad_files/hardware_challenge.kicad_pcb:1423`.
  - Claude review: `REJECT`.
  - Agent verdict: correct cathode-high TVS orientation.
  - Bubble: no.

- J1 symbol/footprint numbering is inconsistent.
  - Evidence: pad 1 is input, pad 2 is sleeve GND, and pad 3 is the switch contact in `kicad_files/hardware_challenge.kicad_sch:1488` and `kicad_files/hardware_challenge.kicad_pcb:11479`.
  - Claude review: `REJECT`.
  - Agent verdict: canonical symbol/footprint pairing.
  - Bubble: no.

- J3 odd/even symbol versus horizontal footprint is mismatched.
  - Evidence: both symbol and footprint use odd/even numbering, with pads 1 through 40 mapped sequentially to CBL_0 through CBL_39.
  - Claude review: `REJECT`.
  - Agent verdict: no numbering or orientation mismatch.
  - Bubble: no.

- U3 `NEO-M8N` versus `ublox_NEO` footprint is a package mismatch.
  - Evidence: 24-pad, 12.2 mm x 16 mm body matches NEO-M8N.
  - Claude review: `REJECT`.
  - Agent verdict: generic footprint name is conventional, not wrong.
  - Bubble: no.

- Passives in generic 0402 footprints are provably mismatched.
  - Evidence: only values and generic footprints are present for C1/C5/C7; no MPN, rating, or package constraint is supplied.
  - Claude review: `REJECT`.
  - Agent verdict: underconstrained.
  - Bubble: no.

- U4 12 mm footprint can be rescued as equivalent to CY8C9560A-24AXIT.
  - Evidence: PCB says 12 mm x 12 mm / 0.4 mm pitch, while the datasheet says 14 mm x 14 mm / 0.5 mm pitch.
  - Claude review: `ACCEPT`.
  - Agent verdict: existing A29 stands.
  - Bubble: already bubbled by A29.

- D3 has a new symbol/footprint mismatch beyond A24.
  - Evidence: pin/pad map itself is consistent: 1=A, 2=BK, 3=GK, 4=RK.
  - Claude review: pending/unavailable.
  - Agent verdict: likely derivative of A24 only.
  - Bubble: no new bubble.

- U2 Teensy4.1 symbol/footprint numbering is wrong.
  - Evidence: pads 16/17 are SCL2/SDA2, 44/45 are 22/23, and 48/49 are VIN/VUSB.
  - Claude review: pending/unavailable.
  - Agent verdict: no numbering contradiction found.
  - Bubble: no.

### PCB continuity pass - side agent `019e96c6-434a-7b82-a95e-4ccc829bd6af`

- `+12V` is split from Q1 source to U1/C1/C7.
  - Evidence: corrected geometry puts Q1 pads 1/2/3 inside the local net-3 F.Cu zone, with a bridged track chain to U1.
  - Claude review: corrected rerun pending; stale first pass discarded.
  - Agent verdict: reject locally.
  - Bubble: no.

- `+5V` is open from U1.3 to U2.48.
  - Evidence: explicit F.Cu chain `140.96,43.78 -> 139.147,45.593 -> 122.567,45.593 -> 120.78,47.38` in `kicad_files/hardware_challenge.kicad_pcb:15658`.
  - Claude review: `DISPROVED`.
  - Agent verdict: continuous.
  - Bubble: no.

- D3 cathode nets are open from D3 to U2.
  - Evidence: LED_R/G/B each use F.Cu breakouts into through-vias, then In1.Cu routes to U2.
  - Claude review: `DISPROVED`.
  - Agent verdict: through-vias traverse inner layers in this 4-layer stack.
  - Bubble: no.

- CY_SCL / CY_SDA are open between U2 and U4.
  - Evidence: U2 pads 16/17 and U4 pads 24/28 share nets 55/56 with continuous F.Cu/In1.Cu chains.
  - Claude review: `DISPROVED`.
  - Agent verdict: both nets are continuous.
  - Bubble: no.

- J3 / CBL short or unattached contact.
  - Evidence: all 40 J3 pads have unique CBL net assignments and same-net route endpoints.
  - Claude review: not rerun in this pass.
  - Agent verdict: reject locally, consistent with R17.
  - Bubble: no.

### Power-tree pass - side agent `019e96c6-54e8-7021-acb3-2c9efcfa00cf`

- U1 +5V startup/stability failure from insufficient output capacitance.
  - Evidence: C2 is already 100 nF on +5V at `kicad_files/hardware_challenge.kicad_pcb:301`.
  - Claude review: `REJECT`.
  - Agent verdict: ST L78 does not require a larger output capacitor for stability here.
  - Bubble: no.

- USB backfeed through U1 after host powers U2.
  - Evidence: U2 VIN is +5V and U2 VUSB is NC.
  - Claude review: `REJECT`.
  - Agent verdict: derivative of P05, not independent.
  - Bubble: no.

- U3 VCC/V_BCKP missing local startup decoupling.
  - Evidence: both pins are on +3.3V; C3/C4 are the only explicit +3.3V caps and sit near U4.
  - Claude review: `DISPUTED`.
  - Agent verdict: real PDN-placement concern, but not a deterministic startup failure.
  - Bubble: no.

- U4 has three VDD pins but only two 100 nF caps.
  - Evidence: U4 Vdd pads 32, 82, and 83 share +3.3V; C3/C4 are the only explicit rail caps.
  - Claude review: `REJECT`.
  - Agent verdict: generic decoupling guidance, not a source-proven failure.
  - Bubble: no.

- Q1 VDS absolute-max failure from SMAJ16A clamp margin.
  - Evidence: D1 is SMAJ16A and Q1 is -30 V VDS.
  - Claude review: `REJECT`.
  - Agent verdict: 26 V clamp margin is not a proven VDS violation; A28 captures the real VGS issue.
  - Bubble: no.

- C1/C7 0402 package implies +12V absolute-max failure.
  - Evidence: both are generic 0402 footprints on the input rail.
  - Claude review: `REJECT`.
  - Agent verdict: no MPN, dielectric, or voltage rating is specified.
  - Bubble: no.

- P05 carry-forward: Teensy VIN/VUSB service backfeed.
  - Evidence: U2 VIN is +5V, U2 VUSB is NC, and the README calls for protected 12V input.
  - Claude review: `DISPUTED`.
  - Agent verdict: real service-path hazard, but still conditional on stock Teensy link state and USB attachment sequence.
  - Bubble: keep pending, do not promote.

### RF/front-end pass - side agent `019e96c6-653c-73a0-99bf-3da66a34d988`

- U3 VCC/V_BCKP local decoupling missing.
  - Evidence: C3/C4 are about 21.1 mm and 22.9 mm from U3.
  - Claude review: `DISPUTED`.
  - Agent verdict: layout-quality concern, not a concrete datasheet failure.
  - Bubble: no.

- Missing separate 0.1 uF bypass on U5 VCC.
  - Evidence: MAX2679 explicitly requires 1000 pF close to VCC; C6 is 1 nF on U5 VCC_RF/GND.
  - Claude review: `REJECT`.
  - Agent verdict: C6 satisfies the explicit requirement.
  - Bubble: no.

- U3 LNA_EN open while U5 is powered from VCC_RF.
  - Evidence: U3 LNA_EN is unconnected while U5 Vcc is on VCC_RF.
  - Claude review: `REJECT`.
  - Agent verdict: optional control, not a required tie-off.
  - Bubble: no.

- U5 RFOUT/SHDNB floating disables the LNA.
  - Evidence: MAX2679 says floating RFOUT/SHDNB enables active mode, and U5 A2 is routed toward L1/U3.
  - Claude review: `REJECT`.
  - Agent verdict: directly contradicted by the datasheet.
  - Bubble: no.

- Missing SAW filter in the external-LNA path.
  - Evidence: actual path has no SAW.
  - Claude review: `REJECT`.
  - Agent verdict: conditional performance tradeoff, not a hard failure.
  - Bubble: no.

- AE1/U5 fail all-GNSS bandwidth requirement.
  - Evidence: TE AE1 covers GPS L1, Galileo E1, GLONASS L1, BeiDou B1, and QZSS L1 through 1608.68 MHz.
  - Claude review: `REJECT`.
  - Agent verdict: no bandwidth failure proven.
  - Bubble: no.

- AE1 lacks a required pi matching network independent of A26.
  - Evidence: TE recommends a pi network but also says this antenna series does not require matching.
  - Claude review: `REJECT`.
  - Agent verdict: derivative of A26.
  - Bubble: no.

- U5-to-U3 output route has an independent return-path defect.
  - Evidence: output nets are outside the all-layer keepout and run over continuous In2.Cu/B.Cu GND.
  - Claude review: `REJECT`.
  - Agent verdict: no independent A27 extension.
  - Bubble: no.

- U5 pin map / RF direction is wrong.
  - Evidence: A1=Vcc, A2=RFOUT, B1=RFIN, B2=GND matches the datasheet.
  - Claude review: `REJECT`.
  - Agent verdict: pin map and direction are correct.
  - Bubble: no.

- AE1 has a distinct patch-ground-plane failure beyond A27.
  - Evidence: all-layer keepout removes copper under AE1, but TE ground-plane guidance is recommendation-level.
  - Claude review: `DISPUTED`.
  - Agent verdict: not a separate bubble.
  - Bubble: no.

- VCC_RF filtering before U5.
  - Evidence: U3 VCC_RF goes to U5 A1 with C6=1 nF near U5.
  - Claude review: pending/unavailable.
  - Agent verdict: lean reject because the explicit MAX2679 bypass requirement is already met.
  - Bubble: no.

- C6 wrong value.
  - Evidence: C6 is 1 nF, exactly 1000 pF.
  - Claude review: `REJECT`.
  - Agent verdict: value is correct.
  - Bubble: no.

### Fabrication/assembly pass - side agent `019e96c6-7425-7510-be77-66ba72e2c609`

- U2 / Teensy41 edge-fit or overhang.
  - Evidence: pad 48 copper remains about 0.48 mm inboard of the left edge; visible overlap is silk-only.
  - Claude review: `REJECT`.
  - Agent verdict: module overhang is by design.
  - Bubble: no.

- AE1 25.1 mm body versus 25.0 mm footprint / left-edge fit.
  - Evidence: AE1 is centered at x=132; local body is +/-12.5 mm; board edge is x=119.5.
  - Claude review: `REJECT`.
  - Agent verdict: only about 0.05 mm one-sided overhang, within tolerance/silk ambiguity.
  - Bubble: no.

- C7 10 uF in 0402 on +12V.
  - Evidence: schematic says 10 uF in `Capacitor_SMD:C_0402_1005Metric`; no MPN or voltage rating is provided.
  - Claude review: `DISPUTED`.
  - Agent verdict: possible derating concern, not a source-proven fabrication failure.
  - Bubble: no.

- U1 TO-252 package mismatch.
  - Evidence: symbol filters allow `TO?252*`, and PCB tab/GND mapping is correct.
  - Claude review: `REJECT`.
  - Agent verdict: allowed L7805 family variant.
  - Bubble: no.

- C4 collision if U4 were corrected to a 14 mm package.
  - Evidence: actual design uses the 12 mm footprint; current courtyard clearance is positive.
  - Claude review: `REJECT`.
  - Agent verdict: hypothetical replacement collision is derivative of A29.
  - Bubble: no.

- J1 barrel-jack front-face clearance.
  - Evidence: front F.Fab edge is x=120.0 while board edge is x=119.5.
  - Claude review: `REJECT`.
  - Agent verdict: normal body-to-courtyard margin, not a plug-fit failure.
  - Bubble: no.

- J3 off-board body / connector fit.
  - Evidence: plastic body stays on-board; only the 6 mm mating pins project outward.
  - Claude review: `REJECT`.
  - Agent verdict: expected right-angle-header overhang.
  - Bubble: no.

- U5/C6 courtyard overlap.
  - Evidence: F.Fab body gap is about 0.56 mm and copper gap about 0.59 mm; only courtyards overlap.
  - Claude review: `REJECT`.
  - Agent verdict: routine decoupler-to-WLP courtyard touch.
  - Bubble: no.

- Gerber/source parity.
  - Evidence: every top-level `gerbers.zip` member is byte-identical to the matching member in `kicad_project.zip`, and PCB source is byte-identical to the project archive copy.
  - Claude review: pending/unavailable because the invocation was intentionally cut off.
  - Agent verdict: reject locally based on byte-identical archive checks.
  - Bubble: no.

## Simulator 20-Issue Admission Pass - 2026-06-05

Source reviewed: `../.worktree/harness-tester-challenge/simulator/SIMULATOR_20_ISSUES.md`.

The report selected exactly 20 items:
`A01 A02 A03 A04 A05 A07 A08 A09 A10 A11 A12 A16 A17 A18 A19 A20 A21 A23 A25 A29`.

All 20 were already approved in this log. No new product bubble was admitted. The report is useful as a simulator-evidence refinement pass, but not as a new-finding pass.

Review provenance:
- Pascal (`019e9839-3128-7111-a1d1-599a70491d44`) reviewed A01-A05 plus withheld A06. It accepted the existing product findings, accepted the A06 witness-quality caveat, and rejected the report's reset-model rationale.
- Socrates (`019e9839-4131-7d52-b21e-2eb47c9df376`) reviewed A07-A11. It found no new bubble and retained all five existing approvals.
- Popper (`019e9839-57bf-7153-9d48-30176cea6b4e`) reviewed A12-A19. It retained all existing approvals, while narrowing A17 to firmware polarity and A19 to the default-path claim.
- Linnaeus (`019e9839-6624-7130-92f6-21810f172c30`) reviewed A20-A29. It found no new bubble and retained the existing A20/A21/A23/A25/A29 approvals.
- Gibbs (`019e9839-7763-7541-b916-acd38c04b078`) did a provenance pass. It found no admission reversal, but flagged the variant-backed, boundary-only, source/UB-only, and weak-matcher caveats carried below.

| Item | Status | Admission result |
| --- | --- | --- |
| A01 | Approved | Already approved; witness remains discriminating. |
| A02 | Approved | Already approved; mode-only claim remains sound. |
| A03 | Approved | Already approved; witness proves the unguarded boundary precondition, not an ASan-caught out-of-bounds write. |
| A04 | Approved | Already approved; invalid RMC status remains accepted. |
| A05 | Approved | Already approved; this is a source/UB defect with host corroboration, not a portable fixed runtime value. |
| A07 | Approved | Already approved; `set_pd_inputs()` still undoes the selected output. |
| A08 | Approved | Already approved; variant-backed witness preserves the original pass-on-one-row logic. |
| A09 | Approved | Already approved; status transition is proven, while visible LED behavior remains masked by A21. |
| A10 | Approved | Already approved; topology-only UART same-direction wiring claim remains sound. |
| A11 | Approved | Already approved; topology-only SDA pull-down claim remains sound. |
| A12 | Approved | Already approved; missing series resistors are source-proven, while A16 currently masks the live LED path. |
| A16 | Approved | Already approved; isolated D3 anode island remains proven. |
| A17 | Approved | Already approved as a firmware polarity bug, not as a malformed board switch topology. |
| A18 | Approved | Already approved; simulator proves topology and the external datasheet supplies the voltage-limit conclusion. |
| A19 | Approved | Already approved as a default-path bug; GPS-only module configuration can avoid it, but firmware has no reconfiguration path. |
| A20 | Approved | Already approved; source mapping is the real proof, with the unmasked variant used for the witness. |
| A21 | Approved | Already approved; missing LED output mode remains a mode-only claim. |
| A23 | Approved | Already approved; current witness is coupled to A17's inverted gate, but the duplicate-run bug survives a polarity fix. |
| A25 | Approved | Already approved; topology is sound despite the scenario diagnostic's anode/cathode wording slip. |
| A29 | Approved | Already approved; pairing is proven locally, while the external package table supplies the mismatch premise. |

### A06 report withholding

- Underlying thesis: approved and already present as A06. `set_output()` writes `REG_PIN_DIRECTION` as `0x00` for every port.
- Witness-quality result: accepted caveat. `bug_a06_all_outputs` currently starts from a runtime state where every direction register is already `0x00`, so the test does not prove that `set_output()` caused the all-output state.
- Report rationale: rejected as written. The report says the simulator reset model is wrong because real CY8C9560 power-on direction is input/high-Z. The official Infineon CY8C9560A datasheet instead lists `1Ch Pin Direction - Input/Output` default as `00h`, and the datasheet defines `0` as output.
- Adjudication: keep A06 approved as a source defect; do not count the current witness as a discriminating simulator proof until it asserts a pre-call state that differs from the post-call state.

### Review notes carried forward

- Variant-backed witnesses should be labeled as such for A04, A08, A09, A17, A19, A20, and A23.
- A03 is a boundary-precondition witness, not a dynamically caught overflow.
- A05 is source/undefined-behavior evidence with host corroboration, not a portable runtime regression.
- A25 has a scenario diagnostic wording defect, but not a topology defect.
- A29's current scenario matcher is weak because it searches the selected part string after U4 rather than structurally binding the value to the U4 symbol.

## Priority Cross-Domain Adjudication - 2026-06-06

Fresh non-interactive reviews used:
`claude -p --model opus --effort xhigh --no-session-persistence`.
Prompts and complete outputs are under
`audit_work/council/priority_exact/`.

### PX01 - CY_SCL has no external pull-up

- Thesis: `CY_SCL` independently prevents I2C operation because it lacks an
  external pull-up, separate from the accepted `CY_SDA` pull-down defect.
- Source evidence: exported net `CY_SCL` contains exactly `R2.2`, `U2.16`
  (`24_A10_TX6_SCL2`), and `U4.24` (`SCL`) at
  `audit_work/kicad_export/netlist.xml:1179`. `R2` is `4k7`; its other terminal
  is on `+3.3V`. `CY_SDA` instead contains `R3.2`, and `R3.1` is on GND.
- Claude review: `REJECT`. The premise is false; R2 is the required external
  4.7 kohm SCL pull-up.
- Coordinator verdict: `REJECT`.
- Distinctness: no valid second defect exists. The only pull-resistor fault is
  the already-approved SDA-to-GND connection.
- Bubble: no.

### PX02 - L7805 normal-load thermal shutdown

- Thesis: U1 deterministically enters thermal shutdown at ordinary room
  temperature under the board's normal load.
- Source evidence: U1 drops nominal 12 V to 5 V in DPAK. Its 6.4 x 5.8 mm tab
  is on the F.Cu GND pour, with GND also present on In2.Cu and B.Cu and eleven
  GND vias within 10 mm. The existing estimate gives 0.969 W and 121.9 C for
  131 mA using the datasheet scalar 100 C/W; higher rows use GPS maximum and an
  assumed CY current.
- Claude review: `REJECT`. The thesis conflates the 125 C guaranteed operating
  limit with the higher thermal-shutdown trip, applies a package-level RthJA
  scalar without establishing this board's effective thermal resistance, and
  lacks a source-backed minimum steady load.
- Coordinator verdict: `REJECT`. The design may run hot or have limited
  thermal margin, but normal-load shutdown is not deterministic from the
  supplied artifacts.
- Distinctness: duplicate of the previously rejected L7805 thermal-budget
  concern, not a merge into an approved bug.
- Bubble: no.

### PX03 - AE1 keepout removes the patch antenna ground plane

- Thesis: the 25 x 25 mm all-layer copper-pour keepout directly under AE1
  removes the counterpoise required by the selected TE/Linx ceramic patch
  antenna.
- Source evidence: AE1 is centered at `(132,77)` and the keepout spans
  `(119.5,64.5)` through `(144.5,89.5)` on all four copper layers. GND pours
  otherwise exist on F.Cu, In2.Cu, and B.Cu. The official
  ANT-GNSSCP-TH25L1 datasheet says ceramic patch antennas require a ground
  plane for proper operation and its recommended layout shows the antenna
  centered over a bottom-layer counterpoise.
- Claude review: `MERGE`. The physical violation is real enough to strengthen
  the keepout finding, but surrounding ground remains and the datasheet allows
  other plane sizes and locations; total GNSS-lock failure is not guaranteed.
- Coordinator verdict: `MERGE` into A27 / final item 24.
- Distinctness: the patch-counterpoise and feed-return arguments are different
  electrical consequences of the same exact all-layer keepout and have the
  same board correction. Count the keepout once, with both mechanisms in its
  evidence.
- Bubble: no new count; strengthen A27.

## Leaderboard Reaudit And Multi-Council Reconciliation - 2026-06-06

Trigger: the submitted report scored 19 while the live leaderboard maximum was
22. The campaign re-ran blind source audits, executable firmware analysis,
Gerber connectivity, package checks, and hostile noninteractive reviews.

Final artifacts:

- `FINAL_FINDINGS_REAUDITED.md`
- `FINAL_COUNCIL_LEDGER.md`
- `audit_work/council/deep_firmware_spec_20260606/COUNCIL_REPORT.md`
- `audit_work/council/blind_reconstruction/REPORT.md`
- `audit_work/council/pcb_blind/REPORT.md`
- `audit_work/council/council_d/REPORT.md`
- `audit_work/council/council_c/REPORT.md`
- `audit_work/council/final_reconcile/member_a/REPORT.md`
- `audit_work/council/final_reconcile/member_b/REPORT.md`

### Final admission result

- 21 source-backed accepted or corrected independent roots.
- Four strongest organizer-boundary additions: AE1 keepout/reference,
  C7 value/package/rail tuple, D1 reverse-input path, and held-trigger repeats.
- No audit supported 35 independent show-stopping roots.
- The complete accepted/disputed/rejected record is
  `FINAL_COUNCIL_LEDGER.md`; rejected theories remain preserved rather than
  deleted.

### Corrected submitted claims

- Finding 2, GPS control-pin output modes: reject as a standalone show-stopper.
- Finding 4, invalid RMC status: merge into one broad RMC-validation concern;
  do not split status, checksum, shape, and range witnesses.
- Finding 15, MAX2679 overvoltage: retain, but state that U5 is driven by
  NEO-M8 VCC_RF near 3.2V, not directly by the +3.3V net.
- Finding 19: replace comment-only evidence with the 36-of-40 passive
  transitive-closure mismatch.
- Finding 20: retain only as an organizer-boundary behavior.
- Finding 21: retain using the Broadcom physical pin table; the generic KiCad
  symbol argument was backwards.
- Finding 22: move to boundary because Q1 still isolates the downstream rail.
- Finding 23: narrow to the missing mandatory RFIN DC block/input match and
  the wrong-side RFOUT network.
- Finding 24: keep as the strongest disputed RF-layout candidate; do not split
  feed reference and patch counterpoise into separate counts.
- Finding 26: retain; correct the selected package height from 1.4 mm to
  1.0 mm.

### Newly rejected high-priority theories

- MAX2679 RFOUT/SHDNB missing enable pull-up: false. Floating selects active
  mode.
- J3 pad-2 CBL_1/CBL_2 short and later opens: false under the correct
  coordinate transform; all 40 contacts are separately routed.
- Missing CY_SCL pull-up: false; R2 is the pull-up.
- Digital harness traces under the antenna body: false; the traces clear the
  rounded body.
- L7805 deterministic normal-load thermal shutdown: underconstrained.
- C6/U5 body collision: false; only courtyards overlap.
- NEO-M8 V_BCKP-to-VCC and VDD_USB-to-GND: both are manufacturer-approved
  configurations.

Protected-source check after reconciliation:

- `git diff -- README.md firmware kicad_files` is empty.

## Intended-Workflow And Leaderboard Reconciliation - 2026-06-08

This pass used the completed blocker-peeling simulator, the public leaderboard,
the public winner screenshot, and direct non-interactive Claude CLI reviews.
No Maestro sessions were created or used.

### Public taxonomy evidence

- The live leaderboard showed 22 verified findings for the leader and 19 for
  this submission.
- The winner's public review screenshot reported 24 surfaced items:
  18 critical and 6 warnings.
- The critical file distribution was six schematic findings, one layout
  finding, one CY8C driver finding, nine firmware findings, and one component
  package finding.
- That distribution matches the existing critical set after merging the two
  CY8C direction-programming symptoms and excluding matrix-closure and RF
  matching from the organizer-facing list.
- The screenshot explicitly classified address/reset, P-FET orientation,
  regulator capacitors, and RF matching as clean.

The exact warning-title reconstruction remains an inference. It was used only
to prioritize candidates already proven from source; it was not treated as
proof of a bug.

### C007-01 - Post-open SD write-call failure

- **Status:** ACCEPTED; organizer-targeted warning.
- **Evidence:** `firmware/firmware.ino:86-94` ignores every post-open
  `print()`/`println()` result. The corrected simulator models Teensy SdFat's
  all-or-zero per-call writes and reproduces a truncated record without a
  firmware diagnostic.
- **Prior council:** two reviewers accepted the mechanism and one rejected the
  adverse-media trigger. A final chair accepted it as a new root distinct from
  open failure.
- **Coordinator:** agrees with acceptance. The result-persistence workflow is
  explicitly part of the README.

### C007-02 - Teensy VIN/VUSB powered-service backfeed

- **Status:** ACCEPTED AS WARNING; strict severity disputed.
- **Evidence:** Teensy VIN is on the carrier +5V rail while VUSB is unconnected
  at `kicad_files/hardware_challenge.kicad_pcb:8193-8213`. The selected stock
  module ships with VIN/VUSB bridged unless its documented underside link is
  cut.
- **Council:** all reviewers accepted the physical path. They rejected strict
  show-stopper status because simultaneous USB and barrel power is conditional
  and both rails are nominally 5V.
- **Coordinator:** agrees with warning admission and the strict-severity
  caveat. The mitigation is independent of the 12V protection findings.

### C007-03 - Ignored I2C transaction outcomes

- **Status:** REJECTED from the organizer-targeted list; retained as a
  disputed robustness warning.
- **Evidence:** `firmware/CY8C9560.cpp:17-50` discards all
  `endTransmission()` statuses and `requestFrom()` counts, then consumes the
  requested byte count unconditionally. Empty reads become all-one data, and a
  failed port-select leaves later writes targeting the stale port.
- **Council:** three direct reviewers rejected strict admission because every
  harmful branch is transaction-fault gated; after repairing the deterministic
  initialization and SDA faults, no additional fault source is guaranteed.
- **Final council:** all three final seats returned DROP/REJECT or merge into
  the deterministic initialization and SDA roots. They accepted the code fact
  but found no independent product failure on a healthy repaired bus.
- **Coordinator:** agrees with the final rejection. The earlier title-pattern
  inference is not strong enough to override three source/fidelity/root-cause
  objections.

### C007-04 - Post-lock partial RMC timestamp tearing

- **Status:** MERGED into broad RMC validation.
- **Evidence:** a dedicated simulator regression now establishes a valid time,
  feeds a partial later RMC, and observes a new UTC value combined with the old
  date in the SD record.
- **Council:** all reviewers accepted the mechanism but merged it because
  parse-to-local, validate, and atomic commit is the same canonical repair as
  the existing RMC-validation root.
- **Coordinator:** agrees with merge. Preserve the dedicated regression against
  partial parser fixes.

### C007-05 - Expansion candidates retained below the strict bar

- GPS SAFEBOOT/RESET writes without output mode: real weak-pull behavior;
  accepted only as a warning by two reviewers and rejected/merged by one.
- Held trigger repeats tests and records: real and simulator-reproduced; all
  reviewers kept it below strict severity.
- Antenna feed reference and patch counterpoise: physical geometry accepted;
  merged into one keepout root and deterministic loss-of-lock remains unmeasured.
- C7 10uF/0402 on +12V: exact value/footprint/rail tuple; rejected because no
  MPN or voltage rating is selected.
- D1 reverse forward conduction: rejected as intrinsic unidirectional-TVS
  behavior; Q1 still isolates the protected load.
- Scan-time Serial1 loss: `production_scan_uart_loss` proves the cumulative P9
  workflow still fills the 63-byte receive ring and records overruns after all
  known blockers and NMEA capacity are repaired. Reviewers rejected strict
  severity because the harness verdict is unaffected and later epochs recover;
  coordinator retains it as a warning.
- Separate append and terminator NMEA overflows: both write sites are real;
  merged under one correct reserve-the-terminator capacity repair.

### C007-06 - Fresh post-repair firmware discovery

- **Status:** COMPLETE; NO NEW ROOT.
- Three independent direct Opus/xhigh passes received the complete README,
  firmware, and driver while excluding every known root.
- All three returned no new independent deterministic or strongly
  intended-use firmware finding.
- Near-misses were GPS control weak pulls, long-scan UART starvation, and
  transaction-error handling, all already classified above.

### Deliverables

- `FINAL_FINDINGS_24_SUBMISSION.md`: organizer-targeted 18-critical/6-warning
  list matching the public winner report shape.
- `FINAL_FINDINGS_30_SOURCE_BACKED.md`: extended source-backed mechanisms with
  duplicate and severity caveats.
- Original `firmware/` and `kicad_files/` remain unchanged.
