# Harness Tester Challenge: 24 Submission Findings

Evidence paths are relative to the project root. This list is intentionally
limited to the strongest organizer-facing findings and does not include the
separate rejected/disputed theory ledger.

The simulator-backed warning findings include their trigger, observed result,
and negative control below. The simulator uses generated repair variants only
to make the affected workflow reachable; it does not modify the original
`firmware/` or `kicad_files/` sources.

## Critical Findings

1. **The CY8C9560 is never initialized**
   - Evidence: `firmware/firmware.ino:63,97-116` constructs and uses `cy`
     without calling `cy.begin()`. `firmware/CY8C9560.cpp:3-15` shows that
     `begin()` starts `Wire2`, resets the device, sets the bus clock, and checks
     the device ID.
   - Impact: the harness expander is accessed without its required startup.

2. **Ordinary NMEA input overflows the 64-byte receive buffer**
   - Evidence: `firmware/firmware.ino:118-126` appends bytes using unchecked
     `nmea_idx++`, and `firmware/firmware.ino:73-74` writes another byte at
     `buf[len]`.
   - Impact: a normal long RMC sentence can write outside `nmea_buf`.

3. **The 40-pin masks use 32-bit signed shifts**
   - Evidence: `firmware/firmware.ino:143-152` evaluates both `1 << i` and
     `1 << j` while the loops reach 39.
   - Impact: channels 32-39 cannot be stimulated or rendered reliably.

4. **The expander direction programming destroys the selected-output setup**
   - Evidence: `firmware/CY8C9560.cpp:77-83` makes every GPIO an output in
     `set_output()`. The immediately following
     `firmware/CY8C9560.cpp:61-66` call makes every GPIO an input, including
     the selected probe pin.
   - Impact: nonselected pins can contend, and the intended stimulus is removed
     before measurement.

5. **Any one matching row passes the whole harness**
   - Evidence: `firmware/firmware.ino:142,155-161` initializes `passed` false
     and only ever changes it to true.
   - Impact: one correct row can pass a harness with many incorrect rows.

6. **FAILED is overwritten by GOOD on the next loop**
   - Evidence: `firmware/firmware.ino:133-139` sets GOOD before checking for a
     new test, overwriting the FAILED state set at
     `firmware/firmware.ino:160-163`.
   - Impact: failure indication is not persistent.

7. **The GPS UART is wired TX-to-TX and RX-to-RX**
   - Evidence: `kicad_files/hardware_challenge.kicad_pcb:7687-7705` and
     `kicad_files/hardware_challenge.kicad_pcb:11072-11092` connect the Teensy
     and NEO-M8N through same-direction signal names.
   - Impact: the Teensy cannot receive the GPS module's UART output.

8. **CY8C9560 SDA is pulled down instead of up**
   - Evidence: R3 is 4.7 kohm from `CY_SDA` to GND at
     `kicad_files/hardware_challenge.kicad_pcb:6877-7072`.
   - Impact: the open-drain SDA line cannot idle high.

9. **The RGB LED channels have no current-limiting resistors**
   - Evidence: D3 connects directly to `LED_R`, `LED_G`, and `LED_B` at
     `kicad_files/hardware_challenge.kicad_sch:13431-13505`.
   - Impact: LED and MCU current is not safely bounded.

10. **D3's common anode is physically open**
    - Evidence: D3 pad 1 is assigned +3.3V at
      `kicad_files/hardware_challenge.kicad_pcb:915-922`, but its route and via
      terminate in an isolated In1.Cu island. KiCad's fabrication-parity DRC
      also records the missing +3.3V connection in
      `audit_work/kicad8_drc.json`.
    - Impact: the RGB LED is unpowered.

11. **The test-button polarity is inverted**
    - Evidence: R4 pulls `BTN_TEST` high and SW1 shorts it to GND when pressed,
      while `firmware/firmware.ino:137-139` returns on LOW.
    - Impact: testing runs while released and stops while pressed.

12. **The MAX2679 supply exceeds its absolute maximum**
    - Evidence: U5 VCC is tied to the NEO-M8N `VCC_RF` output at
      `kicad_files/hardware_challenge.kicad_pcb:6433-6441,10962-10970`, while
      the GPS module is powered from +3.3V.
    - Impact: the 1.8V-class MAX2679 is supplied near 3.2V.

13. **The parser rejects the NEO-M8N default `$GNRMC` talker**
    - Evidence: `firmware/firmware.ino:73-83` accepts only literal `$GPRMC`,
      and firmware never reconfigures the receiver to GPS-only talker output.
    - Impact: a default concurrent-GNSS module cannot establish `time_fixed`.

14. **Logical harness indexes diverge from raw expander bits**
    - Evidence: `firmware/firmware.ino:143-152` uses logical index `i` directly
      as a packed register bit, but U4 Port 2 contains only four GPIOs;
      `kicad_files/hardware_challenge.kicad_pcb:9189-9418` shows CBL_20 begins
      at raw bit 24.
    - Impact: CBL_20-CBL_39 are driven and read through wrong bit positions.

15. **RGB status GPIOs are never configured as outputs**
    - Evidence: `firmware/firmware.ino:67-71` writes pins 5, 6, and 7, but
      `firmware/firmware.ino:97-108` never calls `pinMode(..., OUTPUT)` for
      them.
    - Impact: the LED cathodes receive input-pull behavior instead of active
      status drive.

16. **The physical red and blue LED dies are swapped**
    - Evidence: the selected ASMB-KTF0-0A306 defines pad 2 as red cathode and
      pad 4 as blue cathode, while
      `kicad_files/hardware_challenge.kicad_pcb:915-950` connects pad 2 to
      `LED_B` and pad 4 to `LED_R`.
    - Impact: red and blue status indications are reversed.

17. **D2 is not a Q1 gate-source transient clamp**
    - Evidence: D2 is a PMEG10020ELR Schottky diode at
      `kicad_files/hardware_challenge.kicad_sch:14872-14924`, placed between
      the SiSS27DN source and gate where a VGS Zener clamp is required.
    - Impact: an allowed positive input transient can exceed Q1's VGS rating.

18. **U4 uses an incompatible package footprint**
    - Evidence: U4 is `CY8C9560A-24AXIT` but uses
      `TQFP-100_12x12mm_P0.4mm` at
      `kicad_files/hardware_challenge.kicad_sch:13983-14017` and
      `kicad_files/hardware_challenge.kicad_pcb:8915-8955`.
    - Impact: the selected 14 mm, 0.50 mm-pitch part cannot fit the land pattern.

## Warning Findings

19. **RMC integrity and validity are not checked before accepting time**
    - Evidence: `firmware/firmware.ino:73-82` discards the status field with
      `%*c`, performs no checksum validation, and parses directly into live
      timestamp globals.
    - Reproduction: feed one syntactically parseable `$GPRMC` sentence with
      status `V`, then repeat with an intentionally incorrect checksum. In both
      cases, call the normal firmware loop once.
    - Observed: both inputs set `time_fixed` and print `Time received.`.
    - Executable witness:
      `ctest --test-dir ../.worktree/harness-tester-challenge/simulator/build
      -R '^(bug_a04_invalid_status_accepted|bug_a04_checksum_ignored)$'
      --output-on-failure`.
    - Control: a corrected parser must validate sentence framing, checksum,
      status, and complete field conversion before atomically committing the
      timestamp.
    - Impact: invalid, corrupted, or partially parsed RMC data can unlock
      testing or corrupt logged timestamps.

20. **Post-open SD write failures are silently accepted**
    - Evidence: `firmware/firmware.ino:86-94` checks only whether `SD.open()`
      succeeds. It ignores every `print()`/`println()` byte count and cannot
      observe the result of `close()`.
    - Reproduction: use an SD backend where `open()` succeeds, the first four
      write calls succeed, and all later write calls return zero. Establish a
      valid `123519/230394` timestamp and trigger one harness result.
    - Observed: `results.txt` contains only `230394 - 123519: `; the result and
      newline are absent, but firmware emits no logging-failure diagnostic.
    - Executable witness:
      `ctest --test-dir ../.worktree/harness-tester-challenge/simulator/build
      -R '^bug_sd_partial_log_accepted$' --output-on-failure`.
    - Control: an `SD.open()` failure follows the existing error branch. This
      witness opens successfully and fails only after data has been written,
      proving a distinct unchecked boundary.
    - Impact: firmware can report a completed test while storing a truncated or
      missing result record.

21. **A stock Teensy can backfeed USB from the external +5V rail**
    - Evidence: the carrier powers Teensy VIN from +5V while leaving the VUSB
      carrier pad unconnected at
      `kicad_files/hardware_challenge.kicad_pcb:8193-8213`. A stock Teensy 4.1
      connects VIN and VUSB unless its underside link is cut. PJRC explicitly
      says not to apply VIN while USB is connected unless those pads are
      separated:
      `https://www.pjrc.com/store/teensy41.html`.
    - Verification: with all power removed, check continuity between VIN and
      VUSB on an unmodified Teensy 4.1. Separately verify U2 pad 48 is on +5V
      and pad 49 has no carrier connection.
    - Executable carrier-topology witness:
      `ctest --test-dir ../.worktree/harness-tester-challenge/simulator/build
      -R '^scenario_teensy_vin_vusb_unisolated$' --output-on-failure`.
    - Caveat: the hazardous condition requires simultaneous external power and
      a normal powered USB cable. It is a service/programming warning, not a
      standalone-test blocker.
    - Impact: connecting USB while barrel power is present can drive the host's
      VBUS from the board supply.

22. **A held test button repeats complete tests and log records**
    - Evidence: `firmware/firmware.ino:137-163` uses only a level check and has
      no edge detector, one-shot latch, or wait-for-release state.
    - Reproduction: after correcting the inverted button polarity and
      establishing a valid time, hold the active test level across two
      consecutive calls to `loop()` without releasing it.
    - Observed: two complete result lines are appended to `results.txt`.
    - Executable witness:
      `ctest --test-dir ../.worktree/harness-tester-challenge/simulator/build
      -R '^bug_a23_held_gate_relogs$' --output-on-failure`.
    - Control: releasing the button or adding a press-edge/one-shot latch
      limits the same action to one record.
    - Impact: after correcting button polarity, one sustained press repeatedly
      scans the harness and appends duplicate statistical records.

23. **The 40-row scan blocks UART servicing long enough to lose GPS data**
    - Evidence: `firmware/firmware.ino:121-130` services `Serial1` only outside
      the blocking scan at `firmware/firmware.ino:143-158`. The unchanged
      driver performs about 59,400 I2C clocks per scan, or 594 ms at 100 kHz,
      while roughly 570 UART bytes arrive at 9600 baud.
    - Reproduction: run the complete repaired workflow at 100 kHz I2C and 9600
      baud GPS output, drain startup traffic, verify zero receive overruns,
      advance 850 ms into the normal stream, and trigger one 40-row scan.
    - Observed: the overrun counter changes from zero to nonzero and the
      Teensy-compatible 63-byte Serial1 receive ring is full when the scan
      returns.
    - Executable witness:
      `ctest --test-dir ../.worktree/harness-tester-challenge/simulator/build
      -R '^production_scan_uart_loss$' --output-on-failure`.
    - Control: polling the same GPS stream every 20 ms for three seconds
      produces zero overruns
      (`firmware_uart_polling_control`). The differential is the blocking scan,
      not GPS throughput alone or the separately repaired NMEA buffer defect.
    - Impact: the approximately 64-byte receive ring overflows and drops most
      GPS traffic received during every test.

24. **A partial post-lock RMC can tear the logged date and time**
    - Evidence: `firmware/firmware.ino:73-82` parses directly into live
      `utc_time` and `date` globals before testing whether the full parse
      succeeded or whether time was already fixed.
    - Reproduction: establish a valid lock with time `123519` and date
      `230394`, then feed a partial later RMC containing time `235959.00` but
      no parseable date and log a result.
    - Observed: the live time becomes `235959.00`, the date remains `230394`,
      and the file contains `230394 - 235959.00: Failed`.
    - Executable witness:
      `ctest --test-dir ../.worktree/harness-tester-challenge/simulator/build
      -R '^bug_post_lock_timestamp_tearing$' --output-on-failure`.
    - Counting caveat: this is a concrete failure mode of finding 19's broader
      parse/validate/atomic-commit defect and may be grouped with it rather than
      counted as a separate root cause.
    - Impact: a later partial sentence can replace the UTC field while leaving
      the previous date, and `firmware/firmware.ino:86-90` logs the mixed pair.
