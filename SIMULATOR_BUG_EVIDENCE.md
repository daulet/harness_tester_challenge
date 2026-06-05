# Simulator-Backed Bug Evidence

Working ledger as of 2026-06-05.

This list is intentionally narrower than the full theory log. It contains bugs
that are backed by the corrected host simulator and a fresh non-interactive
Claude review. The simulator no longer carries the fabricated J3 overlay, so no
item below depends on that old artifact.

`firmware_unmasked.ino` is used only for downstream behavior that the original
firmware's initialization and probe bugs would otherwise hide. It fixes the
minimum blockers needed to expose those later behaviors; it is not the source
of the bug thesis.

## Current count

- 14 bugs passed dedicated simulator evidence tests.
- 14/14 were accepted by fresh non-interactive Claude review.
- With ngspice installed, the full simulator suite currently passes 55/55
  tests, including the separate second-pass scenario checks in
  `SIMULATOR_SCENARIO_REVIEW.md` and the electrical fixture matrix.
- The `^bug_` filter contains 16 behavioral tests for these 14 bugs because A03
  has synthetic and physical-overrun boundary witnesses and A04 has independent
  status and checksum witnesses. This accepted ledger uses
  source-backed A05 instead of A08; A03 and A05 additionally have
  expected-failure sanitizer witnesses.

| ID | Bug | Simulator evidence | Fresh Claude review |
| --- | --- | --- | --- |
| A01 | `cy.begin()` is never called | `bug_a01_missing_begin` | ACCEPT: `setup()` never calls `cy.begin()`, so missing Wire2/expander setup follows from source. |
| A02 | GPS SAFEBOOT and reset pins are written without output mode | `bug_a02_missing_output_modes` | ACCEPT: pins 3 and 4 are written without any preceding `pinMode(..., OUTPUT)`. |
| A03 | NMEA staging writes past `nmea_buf` | `bug_a03_nmea_buffer_reaches_end_without_guard`, `bug_a03_uart_overrun_path`, `sanitizer_a03_nmea_overflow`, `sanitizer_a03_default_gps_sentence_overflow` | ACCEPT: the direct boundary reaches index 64; the explicitly unmasked operational variant shows its intended timed harness pass overruns the 63-byte Teensy ring and leaves an unterminated fragment at index 63; ASan on unchanged firmware then reports the same global-buffer-overflow after later default GPS traffic. |
| A04 | `$GPRMC` semantic validity is ignored | `bug_a04_invalid_status_accepted`, `bug_a04_checksum_ignored` | ACCEPT: `%*c` discards status and no checksum validation exists, so both status `V` and checksum-invalid GPS-only RMC input set `time_fixed`. |
| A05 | Forty-pin masks use narrow signed shifts | `scenario_a05_narrow_masks`, `sanitizer_a05_narrow_shift` | ACCEPT: unchanged firmware reaches shift exponent 32 for a 32-bit `int`, which UBSan rejects. |
| A06 | `set_output()` makes every expander pin an output | `bug_a06_all_outputs` | ACCEPT: the witness starts from the documented `0xFF` input reset state and observes `REG_PIN_DIRECTION = 0x00` written for all 8 ports. |
| A07 | `set_pd_inputs()` immediately undoes the selected output | `bug_a07_selected_output_undone` | ACCEPT: `REG_PIN_DIRECTION = 0xFF` is written unconditionally for all 8 ports. |
| A09 | FAILED status is overwritten by GOOD on the next loop | `bug_a09_failed_status_overwritten` | ACCEPT: `loop()` calls `set_status(GOOD)` before the button gate or a new result. |
| A17 | Test button polarity is inverted | `bug_a17_pressed_button_does_not_run` | ACCEPT: physical press is LOW, and firmware returns on LOW. |
| A19 | Firmware only accepts `$GPRMC`, not default `$GNRMC` | `bug_a19_gnrmc_ignored`, `gps_startup_cadence_and_checksum` | ACCEPT: literal `strncmp(..., "$GPRMC", 6)` ignores the `GN` main talker produced by the source-backed concurrent-mode receiver profile. |
| A20 | Harness logical index diverges from expander register-bit index after CBL_19 | `bug_a20_logical_index_gap` | ACCEPT: CBL_20 jumps to raw bit 24 while firmware probes bit 20. |
| A21 | Status LED pins are never configured as outputs | `bug_a21_leds_never_become_outputs` | ACCEPT: no `pinMode(PIN_LED_*, OUTPUT)` exists in source. |
| A23 | A held valid test gate reruns and re-logs indefinitely | `bug_a23_held_gate_relogs` | ACCEPT: level-gated loop has no edge detect, latch, or one-shot state. |
| SD-01 | Short log writes are silently accepted | `bug_sd_partial_log_accepted`, `sd_timing_capacity_and_removal` | ACCEPT: unchanged firmware discards all `print`/`println` byte counts, so an authored full-card condition truncates the record without a storage diagnostic. |

## Fresh Claude review batches

- A01, A02, A03, A04, A05, A06: all ACCEPT.
- A07, A09, A17, A19: all ACCEPT.
- A20, A21, A23: all ACCEPT.
- SD-01: ACCEPT as a distinct ignored-short-write defect; the exact truncation
  point is authored, while the unchecked return values are source-portable.

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

Current result with ngspice installed: 55/55 tests passed.
