# Harness Tester Challenge Findings

Note: this is my second submission with additional findings beyond 22 requested, in case some of them are rejected.

1. **Firmware: CY8C9560 is never initialized**
   - Evidence: `CY8C9560 cy` is constructed in `firmware/firmware.ino:63`, but `setup()` never calls `cy.begin()` in `firmware/firmware.ino:97`. `CY8C9560::begin()` performs reset, starts Wire2, sets the I2C clock, and checks the device ID in `firmware/CY8C9560.cpp:3`.
   - Impact: The harness expander is used before its reset and I2C setup have occurred.

2. **Firmware: GPS SAFEBOOT and RESET pins are written before being configured as outputs**
   - Evidence: `setup()` configures only pins 8 and 2 with `pinMode()`, then writes `PIN_UBX_SAFEBOOT` and `PIN_UBX_RST_N` in `firmware/firmware.ino:103`.
   - Impact: The GPS control pins are not actively driven as intended.

3. **Firmware: NMEA input buffer can overflow**
   - Evidence: `nmea_buf` is only 64 bytes in `firmware/firmware.ino:118`, but `nmea_idx` increments without a bounds check before newline or carriage return in `firmware/firmware.ino:122`. The u-blox M8 RMC example is 75 bytes including CRLF, so this is not limited to malformed input.
   - Impact: An ordinary RMC sentence can corrupt memory.

4. **Firmware: Invalid GPRMC fixes are accepted**
   - Evidence: The parser skips the RMC status field with `%*c` and sets `time_fixed = true` whenever time/date parse succeeds in `firmware/firmware.ino:75`.
   - Impact: A `$GPRMC` sentence with invalid fix status can unlock testing and produce a bogus timestamp.

5. **Firmware: 40-pin masks use narrow integer shifts**
   - Evidence: The 40-pin probe loop uses `1 << i` and `1 << j` in `firmware/firmware.ino:143`, while the result type is `uint64_t`.
   - Impact: Bits above the native integer width are computed incorrectly or invoke undefined behavior.

6. **Firmware: `set_output()` makes every expander pin an output**
   - Evidence: `set_output()` writes `REG_PIN_DIRECTION` as `0x00` for every port in `firmware/CY8C9560.cpp:77`.
   - Impact: The selected output mask is ignored and all expander pins become outputs.

7. **Firmware: `set_pd_inputs()` immediately turns every expander pin back into an input**
   - Evidence: The probe loop calls `cy.set_output(...)` followed by `cy.set_pd_inputs(...)` in `firmware/firmware.ino:144`, and `set_pd_inputs()` writes `REG_PIN_DIRECTION` as `0xFF` for every port in `firmware/CY8C9560.cpp:61`.
   - Impact: The intended driven probe pin is converted back into an input before measurement.

8. **Firmware: One matching probe row is enough to pass the entire harness**
   - Evidence: `passed` starts false, but the loop only ever sets it true when one row matches in `firmware/firmware.ino:142`.
   - Impact: A harness with one correct connection row and many bad rows can still pass.

9. **Firmware: FAILED status is overwritten by GOOD on the next loop**
   - Evidence: Once `time_fixed` is true, `set_status(GOOD)` executes before button handling in `firmware/firmware.ino:133`.
   - Impact: A failed result is only momentary and the LED returns to GOOD immediately.

10. **Schematic: UART TX and RX are wired same-direction instead of crossed**
    - Evidence: Teensy TX1 and RX1 are labeled `UBX-TXD` and `UBX-RXD` in `kicad_files/hardware_challenge.kicad_sch:10218`, while the GPS TXD and RXD pins use the same labels in `kicad_files/hardware_challenge.kicad_sch:10966`.
    - Impact: The Teensy and GPS serial interfaces cannot communicate.

11. **Schematic: I2C SDA is pulled down instead of up**
    - Evidence: R3 is a 4k7 resistor from `CY_SDA` to GND in `kicad_files/hardware_challenge.kicad_sch:16289`.
    - Impact: The SDA bus is held low instead of idling high.

12. **Schematic: RGB LED channels have no current-limiting resistors**
    - Evidence: D3 is wired directly to `LED_R`, `LED_G`, and `LED_B` in `kicad_files/hardware_challenge.kicad_sch:13431`, with no series resistors in those paths.
    - Impact: The LED and Teensy outputs can be overdriven.

13. **PCB: D3 +3.3V anode lands on an isolated copper island**
    - Evidence: D3 pad 1 is on `+3.3V` in `kicad_files/hardware_challenge.kicad_pcb:915`. Its route reaches the via at `(157.226, 32.9184)` in `kicad_files/hardware_challenge.kicad_pcb:15755`, but that via lands only in a separate `In1.Cu` island rather than the main +3.3V plane in `kicad_files/hardware_challenge.kicad_pcb:26339`.
    - Impact: The RGB LED anode is not actually powered from the board supply.

14. **Firmware: Test button polarity is inverted**
    - Evidence: The schematic uses a pull-up and switch-to-ground arrangement for `BTN_TEST`, while firmware returns when `digitalRead(PIN_BTN_TEST) == LOW` in `firmware/firmware.ino:137`.
    - Impact: The test runs when the button is not pressed instead of when it is pressed.

15. **Schematic: MAX2679 Vcc is overvolted through GPS VCC_RF**
    - Evidence: U5 is a MAX2679 in `kicad_files/hardware_challenge.kicad_sch:13508`, and its Vcc pin is tied to `Net-(U3-VCC_RF)`. U3 VCC_RF is fed from the GPS module supply path in `kicad_files/hardware_challenge.kicad_sch:4828`.
    - Impact: The LNA supply is driven from a voltage above its allowed operating range.

16. **Firmware: Parser accepts only `$GPRMC`, not default `$GNRMC`**
    - Evidence: The parser only matches `"$GPRMC"` in `firmware/firmware.ino:75`. The selected NEO-M8N is configured as a multi-GNSS receiver, whose default RMC talker ID is `GN`.
    - Impact: Default GPS output does not set `time_fixed`.

17. **Firmware: Harness logical index diverges from expander register-bit index after CBL_19**
    - Evidence: Firmware directly uses logical index `i` as the raw register bit in `firmware/firmware.ino:143`, but CBL_20 starts at GPort3 bit 0 rather than raw bit 20 in `kicad_files/hardware_challenge.kicad_sch:13124`.
    - Impact: CBL_20 through CBL_39 are probed against the wrong expander bits.

18. **Firmware: RGB status pins are never configured as outputs**
    - Evidence: `set_status()` writes pins 5, 6, and 7 in `firmware/firmware.ino:67`, but `setup()` only calls `pinMode()` for pins 8 and 2 in `firmware/firmware.ino:103`.
    - Impact: The status LED cathodes are not actively driven.

19. **Firmware: The expected connection table is explicitly illustrative but used as the real oracle**
    - Evidence: The source says `// Example connection matrix, just for illustration` in `firmware/firmware.ino:19`, then compares measured rows directly against `EXPECTED_CONNECTIONS[i]` in `firmware/firmware.ino:155`.
    - Impact: Pass/fail depends on an example matrix rather than a defined harness specification.

20. **Firmware: Holding the test trigger reruns and re-logs the test indefinitely**
    - Evidence: The loop has only a level check on `PIN_BTN_TEST` and no edge detect, latch, or one-shot state in `firmware/firmware.ino:137`.
    - Impact: A held valid trigger creates repeated test cycles and duplicate log entries.

21. **Schematic: D3 red and blue channels are swapped**
    - Evidence: `Device:LED_ABGR` defines pin 2 as blue cathode and pin 4 as red cathode in `kicad_files/hardware_challenge.kicad_sch:3727`. The schematic wires `LED_R` to D3 pin 2 and `LED_B` to D3 pin 4 in `kicad_files/hardware_challenge.kicad_sch:10240`.
    - Impact: Red and blue status indications are reversed.

22. **Schematic: Reverse-polarity input forward-biases D1 before Q1 can isolate it**
    - Evidence: J1 pad 1, D1 pad 1, and Q1 drain pad 5 all share net 9 in `kicad_files/hardware_challenge.kicad_pcb:11479`, `kicad_files/hardware_challenge.kicad_pcb:1423`, and `kicad_files/hardware_challenge.kicad_pcb:1733`. D1 is a unidirectional SMAJ16A from that raw-input node to GND in `kicad_files/hardware_challenge.kicad_sch:13362`.
    - Impact: Reverse polarity creates a forward-diode fault path through D1 instead of being isolated by Q1.

23. **PCB: MAX2679 RFIN matching/DC-block network is omitted from the input and misplaced on RFOUT**
    - Evidence: AE1 pad 1 and U5 B1/RFIN are both on `Net-(AE1-A)` in `kicad_files/hardware_challenge.kicad_pcb:2446`, `kicad_files/hardware_challenge.kicad_pcb:6452`, and `kicad_files/hardware_challenge.kicad_pcb:13603`, with no series capacitor or inductor between them. U5 A2/RFOUT is instead on `Net-(L1-Pad2)` in `kicad_files/hardware_challenge.kicad_pcb:6443`; L1 and C5 then form the only series RF chain to U3 RF_IN in `kicad_files/hardware_challenge.kicad_pcb:10168`, `kicad_files/hardware_challenge.kicad_pcb:1951`, and `kicad_files/hardware_challenge.kicad_pcb:10982`. The MAX2679 datasheet requires a DC-blocking capacitor and external matching components on RFIN, while RFOUT is already internally matched to 50 ohms.
    - Impact: The LNA input is left bare while the matching network is placed on the wrong side of the amplifier.

24. **PCB: Antenna-side RF trace lacks a local controlled-impedance reference through the keepout**
    - Evidence: Net 1 runs as a `0.8128` mm F.Cu segment through the antenna region and then narrows to `0.127` mm near U5 in `kicad_files/hardware_challenge.kicad_pcb:13603`. The keepout forbids copper pours on F.Cu, In1.Cu, In2.Cu, and B.Cu in `kicad_files/hardware_challenge.kicad_pcb:20677`. In1.Cu is only `0.1` mm below F.Cu in `kicad_files/hardware_challenge.kicad_pcb:48`, but it is a `+3.3V` zone rather than GND in `kicad_files/hardware_challenge.kicad_pcb:26339`; the GND zones are on F.Cu, In2.Cu, and B.Cu in `kicad_files/hardware_challenge.kicad_pcb:20707`.
    - Impact: The antenna-side high-frequency route is not implemented with the local reference structure required for controlled impedance.

25. **Schematic: D2 is not a real Q1 gate-source transient clamp**
    - Evidence: D2 is populated as a PMEG10020ELR Schottky diode in `kicad_files/hardware_challenge.kicad_sch:14872`, while Q1 is a SiSS27DN in `kicad_files/hardware_challenge.kicad_sch:15619` and D1 is a SMAJ16A in `kicad_files/hardware_challenge.kicad_sch:13371`. The PCB shows Q1 gate on `Net-(D2-A)` and source on `+12V` in `kicad_files/hardware_challenge.kicad_pcb:1697`, `kicad_files/hardware_challenge.kicad_pcb:1724`, and `kicad_files/hardware_challenge.kicad_pcb:1733`, so D2 is in the position where a Zener clamp would be needed.
    - Impact: A positive input transient that D1 allows above 20 V can over-stress Q1's gate-source rating because the populated Schottky does not clamp the negative source-above-gate excursion.

26. **Schematic/PCB: U4 uses the wrong CY8C9560A package footprint**
    - Evidence: U4 is specified as `CY8C9560A-24AXIT` with `Package_QFP:TQFP-100_12x12mm_P0.4mm` in `kicad_files/hardware_challenge.kicad_sch:13991`. The PCB repeats that 12 mm footprint and describes it as a `12x12x1 mm` body in `kicad_files/hardware_challenge.kicad_pcb:8915`. Infineon's CY8C9560A datasheet identifies the 100-pin TQFP package outline for this part as `14 x 14 x 1.4 mm`.
    - Impact: The selected expander package does not fit the board footprint.
