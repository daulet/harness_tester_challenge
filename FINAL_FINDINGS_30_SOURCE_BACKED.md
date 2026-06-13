# Harness Tester Challenge: 30 Source-Backed Findings and Mechanisms

This is the complete research list, not the recommended submission list. The
organizer-targeted list is `FINAL_FINDINGS_24_SUBMISSION.md`; it merges several
source sites that are split here and excludes disputed roots. Every item below
is source-backed or simulator-reproduced, but the stated caveats matter for
organizer counting. Evidence paths are relative to the project root.

1. **CY8C9560 initialization is omitted.**
   Evidence: `firmware/firmware.ino:63,97-116`;
   `firmware/CY8C9560.cpp:3-15`.

2. **NMEA framing can overflow the 64-byte receive buffer.**
   Evidence: unchecked appends at `firmware/firmware.ino:118-126` and the
   unreserved terminator write at `firmware/firmware.ino:73-74`.

3. **The electrical output-mask expression uses a narrow signed shift.**
   Evidence: `firmware/firmware.ino:143-145`.

4. **The serial connectivity-rendering expression independently uses a narrow
   signed shift.**
   Evidence: `firmware/firmware.ino:150-153`.
   Caveat: organizers may merge items 3 and 4 as one shift-width finding.

5. **`set_output()` makes every expander GPIO an output.**
   Evidence: `firmware/CY8C9560.cpp:77-83`.

6. **`set_pd_inputs()` removes the selected probe output before reading.**
   Evidence: `firmware/firmware.ino:145-148`;
   `firmware/CY8C9560.cpp:61-66`.

7. **Any one matching row passes the whole harness.**
   Evidence: `firmware/firmware.ino:142,155-161`.

8. **FAILED is overwritten by GOOD on the next loop.**
   Evidence: `firmware/firmware.ino:133-139,160-163`.

9. **The GPS UART is wired same-direction.**
    Evidence: `kicad_files/hardware_challenge.kicad_pcb:7687-7705,11072-11092`.

10. **CY8C9560 SDA is pulled to GND.**
    Evidence: `kicad_files/hardware_challenge.kicad_pcb:6877-7072`.

11. **The RGB LED has no channel current limiting.**
    Evidence: `kicad_files/hardware_challenge.kicad_sch:13431-13505`.

12. **D3's common-anode route terminates on an isolated copper island.**
    Evidence: `kicad_files/hardware_challenge.kicad_pcb:915-922,15754-15832`;
    `audit_work/council/pcb_blind/evidence/gerber_connectivity.json`.

13. **The test-button polarity is inverted.**
    Evidence: `firmware/firmware.ino:137-139` plus the R4/SW1 topology.

14. **MAX2679 VCC is driven above absolute maximum.**
    Evidence: `kicad_files/hardware_challenge.kicad_pcb:6433-6441,10962-10970`.

15. **The parser rejects default `$GNRMC` output.**
    Evidence: `firmware/firmware.ino:73-83`.

16. **Logical harness indexes diverge from raw expander bits after CBL_19.**
    Evidence: `firmware/firmware.ino:143-152`;
    `kicad_files/hardware_challenge.kicad_pcb:9189-9418`.

17. **RGB status pins are never configured as outputs.**
    Evidence: `firmware/firmware.ino:67-71,97-108`.

18. **The expected harness matrix violates passive transitive closure.**
    Evidence: `firmware/firmware.ino:20-61`;
    `audit_work/council/matrix_closure/check_matrix.py`.
    Caveat: the table is described as illustrative, and organizer credit is
    uncertain.

19. **The physical red and blue LED dies are swapped.**
    Evidence: `kicad_files/hardware_challenge.kicad_pcb:915-950`.

20. **MAX2679 RFIN lacks its datasheet-required input network.**
    Evidence: `kicad_files/hardware_challenge.kicad_pcb:1951,2446,6443-6452,10168,10982,13603`.
    Caveat: the public winner report classifies RF matching as clean.

21. **D2 is not a Q1 gate-source Zener clamp.**
    Evidence: `kicad_files/hardware_challenge.kicad_sch:14872-14924,15619-15683`.

22. **U4 uses an incompatible package footprint.**
    Evidence: `kicad_files/hardware_challenge.kicad_sch:13983-14017`;
    `kicad_files/hardware_challenge.kicad_pcb:8915-8955`.

23. **RMC status, checksum, shape, and atomic commit are not validated.**
    Evidence: `firmware/firmware.ino:73-82`.

24. **Post-open SD write-call failures are silently accepted.**
    Evidence: `firmware/firmware.ino:86-94`.

25. **The stock Teensy VIN/VUSB bridge permits powered-USB backfeed.**
    Evidence: `kicad_files/hardware_challenge.kicad_pcb:8193-8213`.
    Caveat: requires simultaneous USB and external power.

26. **GPS SAFEBOOT and RESET writes remain weak input-mode pulls.**
    Evidence: `firmware/firmware.ino:103-107`.
    Caveat: the mode defect is exact, but deterministic boot failure is not.

27. **A held trigger repeats complete tests and SD records.**
    Evidence: `firmware/firmware.ino:137-163`.
    Caveat: deterministic after polarity correction, but below the strict
    show-stopper threshold.

28. **The antenna keepout removes the feed reference and patch counterpoise.**
    Evidence: `kicad_files/hardware_challenge.kicad_pcb:20677-20705`.
    Caveat: geometry is exact; a guaranteed loss-of-lock threshold is not.

29. **The complete scan overruns the GPS UART receive ring.**
    Evidence: `firmware/firmware.ino:121-130,143-158`; the repaired P9
    simulator regression `production_scan_uart_loss` isolates the loss from
    the original NMEA-capacity defect.
    Caveat: byte loss is real, but the next receiver epoch can recover.

30. **A partial post-lock RMC tears the live timestamp pair.**
    Evidence: `firmware/firmware.ino:73-82,86-90`; the dedicated regression
    `bug_post_lock_timestamp_tearing` logs a new UTC value with the prior date.
    Caveat: normally merged into the broad RMC-validation repair.

## Additional Logged Theories

The C7 10 uF/0402/+12V tuple, reverse-input D1 conduction, and ignored-I2C
fault consequences remain in `FINAL_COUNCIL_LEDGER.md` and
`BUG_THEORY_LOG.md`. They are not promoted above because their component
assumptions or fault-gated impact did not survive the same admission bar.
