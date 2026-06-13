# Harness Tester Challenge: Reaudited Findings

Reaudit date: 2026-06-06

## Core Findings

1. **Firmware: the CY8C9560 is never initialized**
   - Evidence: `firmware/firmware.ino:63,97-116` constructs and later uses
     `cy`, but never calls `cy.begin()`. `firmware/CY8C9560.cpp:3-15` shows
     that `begin()` is the only path that pulses XRES, starts `Wire2`, selects
     the I2C clock, and checks the device ID.
   - Impact: the harness expander is used without starting its I2C controller
     or running its initialization sequence.

2. **Firmware: ordinary NMEA input overflows the 64-byte receive buffer**
   - Evidence: `firmware/firmware.ino:118-126` stores each received byte using
     unchecked `nmea_idx++`. `firmware/firmware.ino:73-74` then writes a NUL at
     `buf[len]`. A normal 75-byte RMC sentence exceeds the allocation.
   - Reproduction: `audit_work/council/deep_firmware_spec_20260606/evidence/overflow_asan.txt`.
   - Impact: normal receiver output can corrupt memory.

3. **Firmware: the 40-pin masks use 32-bit signed shifts**
   - Evidence: `firmware/firmware.ino:143-152` evaluates `1 << i` and
     `1 << j` before conversion to `uint64_t`, while both loops reach 39.
   - Reproduction: `audit_work/council/deep_firmware_spec_20260606/evidence/shift_ubsan.txt`.
   - Impact: bit 31 is sign-extended and shifts 32 through 39 are invalid or
     produce zero, so the upper harness channels cannot be represented.

4. **Firmware: `set_output()` makes every expander GPIO an output**
   - Evidence: `firmware/CY8C9560.cpp:77-83` writes
     `REG_PIN_DIRECTION = 0x00` for every port. The selected mask is applied
     only to the drive-mode register.
   - Reproduction: `audit_work/council/deep_firmware_spec_20260606/agents/expander/evidence.txt`.
   - Impact: nonselected connected pins can become outputs with opposite latch
     values, creating contention. This remains after independently correcting
     `set_pd_inputs()`.

5. **Firmware: `set_pd_inputs()` disables the selected probe output**
   - Evidence: `firmware/firmware.ino:145-148` calls `set_output()`, then
     `set_pd_inputs()`, then reads. `firmware/CY8C9560.cpp:61-66` writes
     `REG_PIN_DIRECTION = 0xFF` for every port, including the selected pin.
   - Impact: the intended stimulus is an input before the measurement occurs.

6. **Firmware: any one matching row passes the whole harness**
   - Evidence: `firmware/firmware.ino:142,155-161` initializes `passed` false
     and only ever sets it true. Later mismatches cannot clear it.
   - Impact: one correct row can pass a harness with 39 incorrect rows.

7. **Firmware: FAILED is overwritten by GOOD on the next loop**
   - Evidence: `firmware/firmware.ino:160-163` sets FAILED after a bad test,
     but `firmware/firmware.ino:133-139` unconditionally sets GOOD on the next
     post-lock loop before reading the button.
   - Impact: the operator does not receive a persistent failure indication.

8. **Schematic: the GPS UART is wired TX-to-TX and RX-to-RX**
   - Evidence: `kicad_files/hardware_challenge.kicad_pcb:7687-7705` maps
     Teensy RX1 to `UBX-RXD` and TX1 to `UBX-TXD`;
     `kicad_files/hardware_challenge.kicad_pcb:11072-11092` maps NEO RXD and
     TXD to the same-direction nets.
   - Impact: the Teensy cannot receive the GPS module's UART output.

9. **Schematic: CY8C9560 SDA is pulled down instead of up**
   - Evidence: R3 is 4.7 kohm from `CY_SDA` to GND at
     `kicad_files/hardware_challenge.kicad_pcb:6877-7072`. The corresponding
     SCL resistor is correctly tied to +3.3V.
   - Impact: the open-drain SDA line cannot idle high, preventing normal I2C
     communication.

10. **Schematic: the RGB LED channels have no current-limiting resistors**
    - Evidence: D3's three cathodes connect directly to `LED_R`, `LED_G`, and
      `LED_B` in `kicad_files/hardware_challenge.kicad_sch:13431-13505`; no
      series element exists in any channel.
    - Impact: after the LED power and GPIO-mode faults are corrected, channel
      current is not bounded as required by the selected LED and MCU outputs.

11. **PCB: D3's common anode is physically open**
    - Evidence: D3 pad 1 is assigned +3.3V at
      `kicad_files/hardware_challenge.kicad_pcb:915-922`. Its route reaches the
      via at `(157.226, 32.9184)` in
      `kicad_files/hardware_challenge.kicad_pcb:15754-15832`, but the via
      intersects an isolated In1.Cu island rather than the main +3.3V plane.
    - Fabrication check:
      `audit_work/council/pcb_blind/evidence/gerber_connectivity.json`.
    - Impact: the RGB LED anode is unpowered.

12. **Firmware: the test-button polarity is inverted**
    - Evidence: R4 pulls `BTN_TEST` to +3.3V and SW1 connects it to GND when
      pressed. `firmware/firmware.ino:137-139` returns when the input is LOW
      and runs the test when it is HIGH.
    - Impact: the tester runs while the button is released and stops when the
      button is pressed.

13. **Schematic: MAX2679 VCC is driven above absolute maximum**
    - Evidence: U3 VCC is +3.3V at
      `kicad_files/hardware_challenge.kicad_pcb:11102-11109`; U3 VCC_RF is net
      8 at `kicad_files/hardware_challenge.kicad_pcb:10962-10970`; U5 A1/VCC
      is directly on the same net at
      `kicad_files/hardware_challenge.kicad_pcb:6433-6441`.
    - Primary limits: NEO-M8 specifies VCC_RF approximately equal to
      `VCC - 0.1V`; MAX2679 specifies 1.08V to 1.98V operation and 2.2V
      absolute maximum.
    - Impact: U5 is continuously supplied near 3.2V, above absolute maximum.

14. **Firmware: the parser rejects the NEO-M8N default `$GNRMC` talker**
    - Evidence: `firmware/firmware.ino:73-83` accepts only literal `$GPRMC`.
      The selected NEO-M8N default concurrent-GNSS configuration uses the `GN`
      talker, and the firmware sends no command to force GPS-only `GP` output.
    - Impact: a fresh default module cannot set `time_fixed`.

15. **Firmware: logical harness indexes diverge from raw expander bits**
    - Evidence: `firmware/firmware.ino:143-152` directly uses logical index
      `i` as a packed register bit. U4's Port 2 has only four GPIOs, so CBL_20
      begins at Port 3 bit 0, raw bit 24, as shown by
      `kicad_files/hardware_challenge.kicad_pcb:9189-9418`.
    - Impact: CBL_20 through CBL_39 are driven and read through incorrect raw
      bits; the final physical channels are never selected.

16. **Firmware: RGB status GPIOs are never configured as outputs**
    - Evidence: `firmware/firmware.ino:67-71` writes pins 5, 6, and 7, while
      `firmware/firmware.ino:97-108` never calls `pinMode(..., OUTPUT)` for
      them.
    - Impact: the common-anode LED cathodes receive only input-pull behavior,
      not active status drive.

17. **Firmware: the expected harness oracle violates passive transitive closure**
    - Evidence: `firmware/firmware.ino:20-61` encodes sparse pairwise edges,
      but a passive harness probe observes the entire connected component.
      Mechanical analysis finds six components and 36 of 40 rows different
      from component-minus-self. For example, 0-13 and 13-26 exist while row 0
      omits 26.
    - Reproduction:
      `audit_work/council/matrix_closure/check_matrix.py` and
      `audit_work/council/deep_firmware_spec_20260606/evidence/matrix_closure.txt`.
    - Impact: a harness matching the encoded graph cannot satisfy the exact
      row comparisons.

18. **Schematic/PCB: the physical red and blue LED dies are swapped**
    - Evidence: the selected Broadcom ASMB-KTF0-0A306 defines pin 2 as red
      cathode and pin 4 as blue cathode. The PCB connects pin 2 to `LED_B` and
      pin 4 to `LED_R` at
      `kicad_files/hardware_challenge.kicad_pcb:915-950`.
    - Impact: firmware red drives the blue die and firmware blue drives red.

19. **PCB: MAX2679 RFIN lacks its required input network**
    - Evidence: AE1 connects directly to U5 B1/RFIN at
      `kicad_files/hardware_challenge.kicad_pcb:2446,6452,13603`. The only
      L1/C5 series network is between U5 A2/RFOUT and U3 RF_IN at
      `kicad_files/hardware_challenge.kicad_pcb:6443,10168,1951,10982`.
    - Primary requirement: MAX2679 RFIN requires a DC-blocking capacitor and
      external matching; RFOUT is internally matched.
    - Impact: the required input network is absent and placed on the wrong side
      of the LNA.

20. **Schematic: D2 is not a Q1 gate-source transient clamp**
    - Evidence: D2 is PMEG10020ELR, an ordinary Schottky diode, at
      `kicad_files/hardware_challenge.kicad_sch:14872-14924`; Q1 is SiSS27DN
      at `kicad_files/hardware_challenge.kicad_sch:15619-15683`. The diode is
      placed between the P-channel MOSFET source and gate where a VGS Zener
      clamp is required.
    - Impact: an allowed positive input transient can drive Q1 VGS beyond its
      rating because D2 does not clamp the negative source-above-gate voltage.

21. **Schematic/PCB: U4 uses an incompatible package footprint**
    - Evidence: U4 is `CY8C9560A-24AXIT` but uses
      `TQFP-100_12x12mm_P0.4mm` at
      `kicad_files/hardware_challenge.kicad_sch:13983-14017` and
      `kicad_files/hardware_challenge.kicad_pcb:8915-8955`.
    - Primary package: 14 x 14 x 1.0 mm TQFP-100, 0.50 mm lead pitch, 16.0 mm
      nominal lead span.
    - Impact: the selected component cannot register to the fabricated
      12 mm, 0.40 mm-pitch land pattern.

22. **AE1 all-layer keepout removes the feed reference and patch counterpoise**
    - Evidence: `kicad_files/hardware_challenge.kicad_pcb:20677-20705`.
    - Boundary: the selected antenna requires a ground plane and the RF feed
      lacks local reference copper, but the supplied artifacts do not quantify
      a guaranteed loss-of-lock threshold.

23. **C7 requires 10 uF in 0402 directly across nominal +12V**
    - Evidence: `kicad_files/hardware_challenge.kicad_sch:16159-16177` and
      `kicad_files/hardware_challenge.kicad_pcb:1143-1159`.
    - Boundary: the current manufacturer search found no 10 uF/0402 production
      part rated for at least 12V, but the design does not specify an MPN or
      explicit voltage rating and immediate failure is not guaranteed.

24. **Reverse input forward-biases raw-side D1**
    - Evidence: D1 cathode and Q1 drain share the raw input net at
      `kicad_files/hardware_challenge.kicad_pcb:1173-1439,1697-1733`.
    - Boundary: Q1 still isolates the protected rail; damage or source collapse
      depends on external source current, impedance, and duration.

25. **A held trigger repeats tests and log records**
    - Evidence: `firmware/firmware.ino:137-163` has only a level gate and no
      edge or one-shot state.
    - Boundary: the behavior is deterministic after correcting button
      polarity, but duplicate records do not prevent the core measurement.
