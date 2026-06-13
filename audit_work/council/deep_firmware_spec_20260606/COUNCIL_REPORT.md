# Deep Firmware/Spec Council Final Report

Date: 2026-06-06

Scope: executable failure modes in `firmware/firmware.ino`,
`firmware/CY8C9560.cpp`, `firmware/CY8C9560.h`, and directly relevant Teensy
4.1, Wire, SD, libc, and CY8C9560 behavior. Production source and shared logs
were not edited.

## Final Count

| Verdict | Count |
| --- | ---: |
| ACCEPT_NEW | 0 |
| ACCEPT_EXISTING | 13 |
| ACCEPT_REPLACEMENT | 1 |
| MERGE | 6 |
| REJECT | 44 |
| DISPUTED | 0 |
| Total | 64 |

**No genuinely new independent accepted firmware root survived.**

DF26 is the only replacement: use the executable 36-of-40 transitive-closure
mismatch as the evidence for existing finding 19. It strengthens that finding;
it does not add a count.

## Accepted Roots

| ID | Verdict | Exact source | Reproduction | Existing target |
| --- | --- | --- | --- | --- |
| DF01 | ACCEPT_EXISTING | `firmware/firmware.ino:63,97-116`; `firmware/CY8C9560.cpp:3-15` | `agents/state/evidence/02_setup-trace.txt` shows `cy.begin.calls=0`. | Finding 1 |
| DF03 | ACCEPT_EXISTING | `firmware/firmware.ino:73-76,118-126` | `agents/parser/asan_vendor_rmc.txt` and `evidence/overflow_asan.txt` catch the ordinary 75-byte RMC overflow. | Finding 3 |
| DF04 | ACCEPT_EXISTING | `firmware/firmware.ino:73-80` | `evidence/parser.txt` shows `void_status_time_fixed=1`. | Finding 4 |
| DF07 | ACCEPT_EXISTING | `firmware/firmware.ino:73-84,133-134` | `evidence/parser.txt` shows GNRMC rejected with `fixed=0`. | Finding 16 |
| DF13 | ACCEPT_EXISTING | `firmware/firmware.ino:143-152` | `evidence/shift_ubsan.txt` fails at exponent 32; target object evidence shows bit-31 sign extension. | Finding 5 |
| DF14 | ACCEPT_EXISTING | `firmware/CY8C9560.cpp:77-84` | `agents/expander/evidence.txt` records all-port `0x00` direction writes. | Finding 6 |
| DF15 | ACCEPT_EXISTING | `firmware/firmware.ino:143-148`; `firmware/CY8C9560.cpp:61-67` | The same trace records immediate all-port `0xFF` direction writes. | Finding 7 |
| DF25 | ACCEPT_EXISTING | `firmware/firmware.ino:143-152`; `firmware/CY8C9560.cpp:25-84` | Expander model maps logical 20 to nonexistent raw bit 20 instead of raw bit 24. | Finding 17 |
| DF26 | ACCEPT_REPLACEMENT | `firmware/firmware.ino:18-61,148-155` | `evidence/matrix_closure.txt` finds 6 components and 36/40 row mismatches. | Replace finding 19 evidence |
| DF27 | ACCEPT_EXISTING | `firmware/firmware.ino:142,155-163` | `evidence/state.txt` shows `one_match_passed=1`. | Finding 8 |
| DF29 | ACCEPT_EXISTING | `firmware/firmware.ino:103-104,137-139` | `agents/state/evidence/03_button-polarity.txt`: pressed LOW runs 0 tests; released HIGH runs 1. | Finding 14 |
| DF30 | ACCEPT_EXISTING | `firmware/firmware.ino:137-163` | `evidence/state.txt`: three held iterations produce three tests and logs. | Finding 20 |
| DF31 | ACCEPT_EXISTING | `firmware/firmware.ino:133-139,160-163` | `agents/state/evidence/05_failed-persistence.txt`: FAILED becomes GOOD next loop. | Finding 9 |
| DF32 | ACCEPT_EXISTING | `firmware/firmware.ino:67-71,97-107` | `agents/state/evidence/02_setup-trace.txt` shows every LED command with INPUT modes. | Finding 18 |

DF30's Claude verdict was `MERGE` because it is a verbatim duplicate of
finding 20. The council labels it `ACCEPT_EXISTING` under the protocol's
definition: it is an independent executable root already present in the
finding set. This does not change the count.

## Compact Verdict Matrix

The complete 64-row matrix, including the hostile Claude verdict, dedupe target,
and strongest disproof, is in `FINAL_VERDICTS.tsv`. Summary by group:

- `ACCEPT_EXISTING`: DF01, DF03, DF04, DF07, DF13-DF15, DF25, DF27,
  DF29-DF32.
- `ACCEPT_REPLACEMENT`: DF26.
- `MERGE`: DF05, DF08, DF12, DF16, DF39, DF55.
- `REJECT`: DF02, DF06, DF09-DF11, DF17-DF24, DF28, DF33-DF38,
  DF40-DF54, DF56-DF64.
- `ACCEPT_NEW`: none.
- `DISPUTED`: none.

## Claude Gate

Every DF01-DF64 candidate received a fresh noninteractive hostile-disproof run:

```text
claude -p --model opus --effort xhigh --no-session-persistence
```

Results:

- Completed reviews: 64/64
- Exit status 0: 64/64
- Authentication failures: 0
- Timeouts: 0
- Empty outputs: 0
- Missing verdict artifacts: 0

Per-candidate prompts, outputs, stderr, and statuses are under `claude/`.
The mechanical gate inventory is `evidence/claude_gate_summary.txt`.

## Verification

- Parser harness: passed; both expected ASan overflow cases were observed.
- State harness: `PASS: 19 deterministic checks`.
- Runtime suite: `all runtime evidence checks passed`.
- Expander harness: `ALL REPRO ASSERTIONS PASSED`.
- Production source SHA-256 remained:
  - `firmware/firmware.ino`: `1d87ab9551aebbd2fa5b7c3cb66356eee98762d83edaeaeb001e009bc6e1cd0b`
  - `firmware/CY8C9560.cpp`: `5d8c1f226d5aace89d65fd2d1ec7a953c2dc4329fd49e90813f4b500f1f235de`
  - `firmware/CY8C9560.h`: `7cc88fe8864088c8bd772ce1e11af1d1be4829aceea0daea6224a97371ed2a1d`

Conditional media faults, reset injection, unchecked transaction errors,
optional build profiles, and bounded latency were retained in the matrix but
did not inflate the accepted count.
