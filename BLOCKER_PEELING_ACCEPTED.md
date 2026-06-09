# Blocker-Peeling Accepted Findings

## 1. RGB LED red and blue cathodes are swapped

The selected `ASMB-KTF0-0A306` datasheet defines pin 2 as the red cathode, pin
3 as the green cathode, and pin 4 as the blue cathode. The PCB instead connects
D3.2 to `LED_B`, D3.3 to `LED_G`, and D3.4 to `LED_R` at
`kicad_files/hardware_challenge.kicad_pcb:924-949`.

Firmware sinks `LED_B` for BUSY and `LED_R` for FAILED at
`firmware/firmware.ino:67-70`. Therefore, after the independently masked LED
power, resistor, and GPIO-mode faults are corrected, BUSY illuminates the
physical red die and FAILED illuminates the physical blue die.

The generated `P8_without_BOARD_LED_COLOR_MAPPING` counterfactual repairs the
LED power path, adds current limiting, and enables GPIO output modes while
retaining the as-designed cathode assignment. The closed-loop ngspice witness
observes BUSY current in red, GOOD current in green, and FAILED current in blue.
The corrected P8 control observes blue, green, and red respectively.

Manufacturer source:
<https://docs.broadcom.com/doc/ASMB-KTF0-0A306-DS100>

## 2. Post-open SD write-call failures are silently accepted

`firmware/firmware.ino:86-94` checks only whether `SD.open()` succeeds. It
ignores the result of each later `print()` and `println()` call and cannot
observe the completion result discarded by the public `close()` wrapper.

The target-faithful simulator uses all-or-zero failure for each SdFat write
call. `bug_sd_partial_log_accepted` injects a persistent media write fault after
the date, time, and separator calls complete. The result and newline calls then
return zero, the file contains only `230394 - 123519: `, and firmware emits no
failure diagnostic. This is an explicit call-boundary fault model, not a claim
about FAT allocation geometry.

This remains distinct from an `SD.open()` failure: the file opens successfully,
some calls complete, and the missing result is caused by unchecked later write
outcomes.

## 3. Electrical stimulus and serial rendering have separate narrow shifts

The electrical stimulus uses `1 << i` at
`firmware/firmware.ino:143-145`. The serial connection rendering separately
uses `1 << j` at `firmware/firmware.ino:150-153`.

The generated `P4_output_mask_only` counterfactual changes only the stimulus
expression to `1ULL << i`. UBSan then reaches and aborts at the untouched debug
expression when `j == 32`. The two sites control different product surfaces
and require separate local corrections, although an organizer may still group
them as one shift-width category.

## 4. Teensy VIN/VUSB powered-service backfeed is a valid warning

The carrier connects U2 VIN to +5V and leaves U2 VUSB on an unconnected carrier
net at `kicad_files/hardware_challenge.kicad_pcb:8193-8213`. A stock Teensy
4.1 retains its on-module VIN/VUSB bridge unless the documented underside link
is cut.

`scenario_teensy_vin_vusb_unisolated` proves the carrier-side topology. The
remaining module bridge and USB insertion are source-backed external boundary
facts, not simulator inventions. The finding is warning-tier because it
requires simultaneous external and USB power and does not block standalone
harness testing.

## 5. A complete repaired scan overruns the GPS UART receive ring

`firmware/firmware.ino:121-130` services `Serial1` only before the blocking
40-row probe loop at `firmware/firmware.ino:143-158`. The driver workload is
about 59,400 I2C clocks, or 594 ms at the configured 100 kHz bus rate.

`production_scan_uart_loss` runs the cumulative P9 variant, with all known
reachability blockers and the NMEA-capacity defect repaired. It drains startup
traffic, verifies zero prior overruns, then starts the blocking scan 850 ms into
the normal stream. The scan records receive overruns and leaves the
Teensy-compatible 63-byte ring full when it returns.

This is warning-tier rather than strict: the current harness verdict is not
changed and later GPS epochs can recover, but the transport loss is causal and
independent of the original NMEA overflow.
