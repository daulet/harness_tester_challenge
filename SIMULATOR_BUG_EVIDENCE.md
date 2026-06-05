# Simulator-Backed Bug Evidence

Working ledger as of 2026-06-04.

This list is intentionally narrower than the full theory log. It contains bugs
that are backed by the corrected host simulator and a fresh non-interactive
Claude review. The simulator no longer carries the fabricated J3 overlay, so no
item below depends on that old artifact.

`firmware_unmasked.ino` is used only for downstream behavior that the original
firmware's initialization and probe bugs would otherwise hide. It fixes the
minimum blockers needed to expose those later behaviors; it is not the source
of the bug thesis.

## Current count

- 11 bugs passed dedicated simulator evidence tests.
- 11/11 were accepted by fresh non-interactive Claude review.
- With ngspice installed, the full simulator suite currently passes 41/41
  tests, including the separate second-pass scenario checks in
  `SIMULATOR_SCENARIO_REVIEW.md` and the electrical fixture matrix.
- The `^bug_` filter contains 13 tests because A03 and A08 are the two
  second-pass additions documented in `SIMULATOR_SCENARIO_REVIEW.md`.

| ID | Bug | Simulator evidence | Fresh Claude review |
| --- | --- | --- | --- |
| A01 | `cy.begin()` is never called | `bug_a01_missing_begin` | ACCEPT: `setup()` never calls `cy.begin()`, so missing Wire2/expander setup follows from source. |
| A02 | GPS SAFEBOOT and reset pins are written without output mode | `bug_a02_missing_output_modes` | ACCEPT: pins 3 and 4 are written without any preceding `pinMode(..., OUTPUT)`. |
| A04 | `$GPRMC` validity/status is ignored | `bug_a04_invalid_status_accepted` | ACCEPT: `%*c` discards the status byte, so `V` parses like `A`. |
| A06 | `set_output()` makes every expander pin an output | `bug_a06_all_outputs` | ACCEPT: `REG_PIN_DIRECTION = 0x00` is written unconditionally for all 8 ports. |
| A07 | `set_pd_inputs()` immediately undoes the selected output | `bug_a07_selected_output_undone` | ACCEPT: `REG_PIN_DIRECTION = 0xFF` is written unconditionally for all 8 ports. |
| A09 | FAILED status is overwritten by GOOD on the next loop | `bug_a09_failed_status_overwritten` | ACCEPT: `loop()` calls `set_status(GOOD)` before the button gate or a new result. |
| A17 | Test button polarity is inverted | `bug_a17_pressed_button_does_not_run` | ACCEPT: physical press is LOW, and firmware returns on LOW. |
| A19 | Firmware only accepts `$GPRMC`, not default `$GNRMC` | `bug_a19_gnrmc_ignored` | ACCEPT: literal `strncmp(..., "$GPRMC", 6)` ignores `GN` talker output. |
| A20 | Harness logical index diverges from expander register-bit index after CBL_19 | `bug_a20_logical_index_gap` | ACCEPT: CBL_20 jumps to raw bit 24 while firmware probes bit 20. |
| A21 | Status LED pins are never configured as outputs | `bug_a21_leds_never_become_outputs` | ACCEPT: no `pinMode(PIN_LED_*, OUTPUT)` exists in source. |
| A23 | A held valid test gate reruns and re-logs indefinitely | `bug_a23_held_gate_relogs` | ACCEPT: level-gated loop has no edge detect, latch, or one-shot state. |

## Fresh Claude review batches

- A01, A02, A04, A06: all ACCEPT.
- A07, A09, A17, A19: all ACCEPT.
- A20, A21, A23: all ACCEPT.

The fresh review was run against the current corrected simulator and explicitly
instructed Claude to reject any claim that depended only on simulator behavior
instead of actual firmware or KiCad source.

## Verification

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Current result with ngspice installed: 41/41 tests passed.
