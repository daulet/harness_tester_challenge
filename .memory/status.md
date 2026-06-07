# Current status

- Branch: `simulator`.
- Last completed pass: metamorphic Campaigns C004 and C005.
- Current baseline: 104/104 CTest cases passed against
  `/opt/homebrew/bin/ngspice`.
- Sanitizer boundary: ASan A03 and UBSan A05 expected-failure witnesses pass with
  diagnostic matching on Apple Clang 21.
- Active blocker-peeling item: none. C004 and C005 each produced no candidate,
  bringing the consecutive no-new-candidate streak to two; the stopping rule
  and final exact-diff review are complete.
- Known local-only artifact: untracked `build/`.
- Available boundaries: Apple Clang 21, CMake 4.2.3, clang-format, ngspice.
- Missing boundary at bootstrap: `kicad-cli` is not installed or on `PATH`.
- Required pass-close command:
  `cmake --build build && ctest --test-dir build --output-on-failure`.
- P4-P7/P9 direct Claude review: the first tool-enabled gate stalled; the exact
  staged-diff gate found a non-causal newline-count assumption and an untested
  intermediate `set_output()` direction state. Both were corrected with
  explicit CRLF and isolated direction witnesses before the 89/89 run.
- P8 admission result: direct non-interactive Claude source/spec,
  simulator-fidelity, duplicate/root-cause, and final-council reviews were
  preserved in `BLOCKER_PEELING_THEORY_LOG.md`; the final council accepted
  BP-T001 after the official selected-part pinout resolved the color identity.
- P8 implementation gate: the exact staged diff received a direct
  non-interactive Claude PASS with six hardening observations. Ambiguous mapping
  names, rail-endpoint validation, local shadowing, and the open-anode
  counterfactual were improved; the focused follow-up review returned PASS and
  the final full run remained 93/93.
- Digital harness campaign: the generated P9 path now also peels the already
  known passive-matrix closure and FAILED-status overwrite roots. The bounded
  2,113-scenario campaign completed with zero row, verdict, log, or status
  differentials and wrote `build/blocker_peeling/exposure_matrix.csv`.
- Peripheral council: post-lock partial RMC mutation was accepted as real by
  all three direct Claude reviewers but merged into the existing RMC validation
  root. SD-01 was rejected after target SdFat review exposed byte-granular
  simulator behavior and an unobservable deferred close/sync failure.
- Peripheral/event campaign: 32 scenarios per variant include 384 seeded RMC
  cases plus button, UART, I2C, and SD sequences. The repaired RMC variant has
  zero differentials; the exact P9 counterfactual has nine, all A04/BP-M002.
  BP-C003 through BP-C006 were reviewed by three independent direct-Claude
  roles and final council; none survived as a new admission-bearing root.
- Persisted sanitizer campaign: 842 checked-in cases cover valid, invalid,
  post-lock, and loop-framing RMC behavior. An initial 39 date-suffix
  differentials exposed a repair gap; after delimiter validation, baseline,
  ASan, and UBSan matrices each contain 842 rows and zero differentials.
  BP-C007 merged into A04 after three direct-Claude reviews and final council.
- Metamorphic campaigns: C004 and C005 each run 384 state-projection pairs with
  distinct deterministic seeds across parser, button, UART, I2C, and SD
  relations plus six expected-differential controls. Both replay byte-for-byte
  with zero unexplained differential and zero error; their seed-independent
  descriptor vectors differ and the two-expansion stopping streak is complete.
- Final C004/C005 gate: three fresh direct non-interactive Opus/xhigh reviews
  of the exact staged patch returned no blocker, including the final council.
  The original firmware retains its accepted 64-byte buffer defect; cumulative
  variant `P9_RMC_VALIDATED` includes `FW_NMEA_BOUNDS` and was independently
  verified to define `char nmea_buf[128]`, matching the campaign declaration,
  before commit `03f25ad`.
