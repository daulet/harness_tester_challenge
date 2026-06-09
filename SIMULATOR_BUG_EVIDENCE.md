# Simulator-Backed Bug Evidence

Working ledger as of 2026-06-08.

This list is intentionally narrower than the full theory log. It contains bugs
that are backed by the corrected host simulator and a fresh non-interactive
Claude review. The simulator no longer carries the fabricated J3 overlay, so no
item below depends on that old artifact.

`firmware_unmasked.ino` is used only for downstream behavior that the original
firmware's initialization and probe bugs would otherwise hide. It fixes the
minimum blockers needed to expose those later behaviors; it is not the source
of the bug thesis.

## Current count

- 16 simulator-backed bugs or independently repaired source sites remain
  accepted in this ledger after target-stack fidelity review.
- The former byte-granular SD-01 witness remains rejected, but a corrected
  all-or-zero post-open write-call witness is accepted as C006-01.
- With ngspice installed, the full simulator suite contains 120 tests,
  including the separate second-pass scenario checks in
  `SIMULATOR_SCENARIO_REVIEW.md` and the electrical fixture matrix.
- The `^bug_` filter contains 17 behavioral tests: 15 witnesses for 13 accepted
  roots, the merged post-lock A04 witness, and the retained rejected A08
  witness. A03 has synthetic and physical-overrun boundaries, while A04 has
  independent status and checksum witnesses. This accepted ledger uses
  source-backed A05 instead of A08; A03 and A05 additionally have
  expected-failure sanitizer witnesses.

| ID | Bug | Simulator evidence | Fresh Claude review |
| --- | --- | --- | --- |
| A01 | `cy.begin()` is never called | `bug_a01_missing_begin` | ACCEPT: `setup()` never calls `cy.begin()`, so missing Wire2/expander setup follows from source. |
| A02 | GPS SAFEBOOT and reset pins are written without output mode | `bug_a02_missing_output_modes` | ACCEPT: pins 3 and 4 are written without any preceding `pinMode(..., OUTPUT)`. |
| A03 | NMEA staging writes past `nmea_buf` | `bug_a03_nmea_buffer_reaches_end_without_guard`, `bug_a03_uart_overrun_path`, `sanitizer_a03_nmea_overflow`, `sanitizer_a03_default_gps_sentence_overflow` | ACCEPT: the direct boundary reaches index 64; the explicitly unmasked operational variant shows its intended timed harness pass overruns the 63-byte Teensy ring and leaves an unterminated fragment at index 63; ASan on unchanged firmware then reports the same global-buffer-overflow after later default GPS traffic. |
| A04 | `$GPRMC` semantic validity is ignored | `bug_a04_invalid_status_accepted`, `bug_a04_checksum_ignored` | ACCEPT: `%*c` discards status and no checksum validation exists, so both status `V` and checksum-invalid GPS-only RMC input set `time_fixed`. |
| A05a | Electrical output masks use a narrow signed shift | `scenario_a05_narrow_masks`, `sanitizer_a05_narrow_shift` | ACCEPT: unchanged firmware reaches shift exponent 32 while constructing the physical stimulus mask. |
| A05b | Serial connection rendering independently uses a narrow signed shift | `blocker_peeling_p4_debug_shift` | ACCEPT_SPLIT: after changing only the stimulus expression to `1ULL << i`, UBSan still aborts at the untouched `1 << j` diagnostic expression. The final direct-Claude chair accepted the split because the source sites, dataflow, product surfaces, and local edits are independent. |
| A06 | `set_output()` makes every expander pin an output | `bug_a06_all_outputs` | ACCEPT: the witness first establishes the all-input state left by a prior probe, then observes `REG_PIN_DIRECTION = 0x00` written for all 8 ports. The corrected runtime separately models the documented POR direction `0x00`, output latch `0xFF`, and pull-up mode. |
| A07 | `set_pd_inputs()` immediately undoes the selected output | `bug_a07_selected_output_undone` | ACCEPT: `REG_PIN_DIRECTION = 0xFF` is written unconditionally for all 8 ports. |
| A09 | FAILED status is overwritten by GOOD on the next loop | `bug_a09_failed_status_overwritten` | ACCEPT: `loop()` calls `set_status(GOOD)` before the button gate or a new result. |
| A17 | Test button polarity is inverted | `bug_a17_pressed_button_does_not_run` | ACCEPT: physical press is LOW, and firmware returns on LOW. |
| A19 | Firmware only accepts `$GPRMC`, not default `$GNRMC` | `bug_a19_gnrmc_ignored`, `gps_startup_cadence_and_checksum` | ACCEPT: literal `strncmp(..., "$GPRMC", 6)` ignores the `GN` main talker produced by the source-backed concurrent-mode receiver profile. |
| A20 | Harness logical index diverges from expander register-bit index after CBL_19 | `bug_a20_logical_index_gap` | ACCEPT: CBL_20 jumps to raw bit 24 while firmware probes bit 20. |
| A21 | Status LED pins are never configured as outputs | `bug_a21_leds_never_become_outputs` | ACCEPT: no `pinMode(PIN_LED_*, OUTPUT)` exists in source. |
| A23 | A held valid test gate reruns and re-logs indefinitely | `bug_a23_held_gate_relogs` | ACCEPT: level-gated loop has no edge detect, latch, or one-shot state. |
| SD-OLD | Byte-granular partial SD writes are silently accepted | historical `sd_timing_capacity_and_removal` witness | REJECT: Teensy SdFat small writes return the requested count or zero, so the old byte-prefix model overstated target fidelity. |
| C006-01 | Post-open SD write-call failure is silently accepted | `bug_sd_partial_log_accepted` | ACCEPT: an explicit persistent media fault after four successful write calls leaves `230394 - 123519: `, later result/newline calls return zero, and firmware ignores every return count plus the discarded close result. This proves the target API failure boundary without claiming FAT allocation geometry. Final direct-Claude chair accepted this as a new root distinct from open failure. |
| C007-01 | Post-lock partial RMC can tear live timestamp state | `bug_post_lock_timestamp_tearing` | MERGE: the dedicated regression proves new UTC combined with the old date in a logged record, but all reviewers merged it into A04's parse/validate/atomic-commit root. |
| C007-02 | A complete repaired workflow scan overruns Serial1 RX | `production_scan_uart_loss` | ACCEPT_WARNING: the cumulative P9 variant still fills the 63-byte ring and records overruns solely because the blocking 40-row scan does not service UART. Strict reviewers reject only the impact level, not the mechanism or simulator fidelity. |

## Fresh Claude review batches

- A01, A02, A03, A04, A05, A06: all ACCEPT.
- A07, A09, A17, A19: all ACCEPT.
- A20, A21, A23: all ACCEPT.
- SD-OLD: REJECT after target-stack review because byte-granular writes were
  not faithful.
- C006-01: ACCEPT after corrected all-or-zero write modeling, direct Teensy
  SD/SdFat source inspection, an explicit persistent write-call fault, and a
  final direct-Claude chair given the actual accepted ledger.

The same review accepted checksum-invalid RMC as part of A04's existing
no-validation root cause. It narrowed CRLF's separate empty parser pass to a
non-defect observation. Finite RX capacity now demonstrates harmful loss, but
the resulting memory corruption remains evidence for A03 rather than a separate
accepted defect; `firmware_uart_polling_control` proves frequent service avoids
the overrun under the same default stream.

The fresh review was run against the current corrected simulator and explicitly
instructed Claude to reject any claim that depended only on simulator behavior
instead of actual firmware or KiCad source.

## Verification

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Current result with ngspice installed: 120/120 registered tests passed on
2026-06-08.
