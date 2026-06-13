# Independent Council Member B Reconciliation

Date: 2026-06-06

Repository commit:
`069f7243d9ca87b31333d873bb33b08cfb54042a`

## Scope

This is a read-only reconstruction of which 19 submitted findings were most
likely credited, followed by an independent search for omitted roots that
could plausibly move the result above 22.

The challenge admission standard is taken directly from `README.md:31-41`:
the bug must be intentional, independent, and show-stopping; improvements and
fixes are out of scope.

Public solution lists, issue comments, other entrants' reports, and leaderboard
popularity were not used as proof. The only inputs used for technical verdicts
were:

- original firmware and KiCad source;
- executable scratch reproducers under `audit_work/`;
- primary component and target-platform specifications;
- hostile Claude reviews that were instructed to disprove the candidate.

The score of 19 does not reveal which 19 rows were accepted. Therefore the
exact mapping below is a constrained best fit, not a claim that organizer
attribution has been recovered with certainty.

## Executive Verdict

The best-fit 19 credited findings are:

`1, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 18, 21, 23, 25, 26`

The seven best-fit noncredits are:

`2, 4, 16, 19, 20, 22, 24`

That exact partition should not be overread. The most plausible organizer-score
swaps are:

1. Submitted 16 may have been credited instead of 18 or 21.
2. Submitted 22 may have been credited instead of 9, 18, or 21.
3. Submitted 4 may have been credited if "GPS lock" was interpreted as
   requiring navigation-valid RMC status rather than valid UTC only.

The strongest source-backed route above 22 is:

1. Keep the mechanically strong submitted roots.
2. Promote submitted 16 with the selected module's documented default talker.
3. Replace submitted 19's comment-based wording with the transitive-closure
   impossibility proof.
4. Re-submit 21 using the selected Broadcom physical pin table, not the generic
   KiCad symbol names.
5. Treat 22 as a deliberate boundary candidate with its exact reverse-current
   path.
6. Add C7 only as a rated-BOM impossibility, not as a guaranteed immediate
   short.

This supports a defensible hard core around 21 and a plausible, disputed
ceiling of 23-24. The audited source does not support 35 independent,
show-stopping roots.

## Numbered Submitted-Finding Verdict Matrix

Legend:

- **Likely credit**: included in the best-fit 19.
- **Likely no credit**: excluded from the best-fit 19.
- **Boundary**: technically real or strongly arguable, but organizer credit is
  uncertain because of conditionality, submitted wording, or admission scope.

| No. | Best-fit verdict | Source evidence and council reasoning | Explicit dedupe |
|---:|---|---|---|
| 1 | **Likely credit** | `firmware/firmware.ino:63,97-116` never calls `cy.begin()`. `firmware/CY8C9560.cpp:3-15` shows that `begin()` supplies XRES, `Wire2.begin()`, clock setup, and device-ID validation before the first access at `firmware/firmware.ino:145`. | Independent initialization root. Do not split reset, Wire startup, and ID check into separate findings. |
| 2 | **Likely no credit** | `firmware/firmware.ino:106-107` writes SAFEBOOT and RESET without `pinMode(OUTPUT)`, but Teensy input-mode `digitalWrite()` selects weak pulls. RESET_N is released high, and SAFEBOOT is applied after the concurrently powered GPS has normally sampled startup state. No deterministic shipped failure was established. Hostile review: `audit_work/council/final_reconcile/member_a/claude/02_gps_pin_modes.output.txt`. | SAFEBOOT wrong-level and missing-output-mode variants belong to one GPS-control cluster. Neither earns a second count. |
| 3 | **Likely credit** | `nmea_buf[64]` at `firmware/firmware.ino:118` is filled by unchecked `nmea_idx++` at `:123`; a normal u-blox RMC sentence can exceed 64 bytes. Existing ASan repro records an out-of-bounds write. | Exact-64 terminator overflow and longer-line overflow are the same capacity/bounds root. |
| 4 | **Likely no credit; boundary** | `firmware/firmware.ino:76-80` discards A/V with `%*c` and latches `time_fixed`. The parser accepts a well-formed V sentence, but RMC V denotes invalid navigation data, not necessarily invalid UTC. For this time-only product the submitted "bogus timestamp" consequence is not deterministic. Hostile review: `audit_work/council/final_reconcile/member_a/claude/04_rmc_void_status.output.txt`. | Checksum, field-shape, range, and A/V checks are one RMC-validation cluster unless a separate normal-path effect is proven. |
| 5 | **Likely credit** | `1 << i` and `1 << j` at `firmware/firmware.ino:144,152` use signed native-width integers in a 40-pin loop. UBSan reaches invalid shift exponent 32; bit 31 also sign-extends when converted to `uint64_t`. | One mask-width root; output-mask and print-mask manifestations are not separate. |
| 6 | **Likely credit** | `firmware/CY8C9560.cpp:77-83` writes direction `0x00` for every port, ignoring the selected mask and making all physical GPIOs outputs. It can independently create connected-pin contention before finding 7 changes direction again. Hostile review: `audit_work/council/final_reconcile/member_a/claude/06_all_outputs.output.txt`. | Transient contention and full output-latch rewrite effects merge here. Separate from 7 because repairing either helper leaves the other failure live. |
| 7 | **Likely credit** | `firmware/CY8C9560.cpp:61-66` writes direction `0xFF` for every port. It runs immediately after `set_output()` at `firmware/firmware.ino:145-146`, converting the selected driven probe back into an input before sampling. | Stale mode and selected-output removal effects merge here. Separate from 6 by repair and effect. |
| 8 | **Likely credit** | `passed` starts false at `firmware/firmware.ino:142` and is only latched true at `:155-157`; later mismatches cannot clear it. One matching row passes a 40-row harness. | Independent aggregation root. Do not merge with bad matrix contents or probe-direction failures. |
| 9 | **Likely credit** | A failed test sets FAILED at `firmware/firmware.ino:163`; the next post-lock loop unconditionally sets GOOD at `:135` before button handling. The result LED therefore settles on GOOD after a failed harness. Hostile reviews: `audit_work/council/final_reconcile/member_a/claude/09_failed_status.output.txt` and `audit_work/council/deep_firmware_spec_20260606/claude/outputs/DF31.txt`. | Independent result-latch root. LED hardware defects mask visibility but have different fixes. |
| 10 | **Likely credit** | The Teensy and NEO-M8 use the same `UBX-TXD` and `UBX-RXD` labels on like-direction pins in `kicad_files/hardware_challenge.kicad_sch:10218,10614,10966,11802`. The physical UART is TX-to-TX and RX-to-RX. | Independent hardware path. Does not merge with parser or talker-ID failures. |
| 11 | **Likely credit** | R3 is 4.7 kohm from `CY_SDA` to GND at `kicad_files/hardware_challenge.kicad_sch:16298-16350`; SCL has the proper pull-up through R2. SDA is held low instead of idling high. | One I2C-bias root. The disproved "SCL also lacks a pull-up" theory merges nowhere. |
| 12 | **Likely credit** | D3's `LED_R`, `LED_G`, and `LED_B` nets directly join Teensy GPIOs with no series component in `kicad_files/hardware_challenge.kicad_sch:9954-10416,10922-12022`. | Independent current-limiting root. D3 anode open, GPIO mode, and color swap remain separate repairs. |
| 13 | **Likely credit** | D3 pad 1 route and via terminate in a separate In1.Cu `+3.3V` island. `audit_work/pcb_continuity/key_net_summary.txt` places D3.1 in a different full-copper component from the main +3.3V users. | One routed-open root. Do not split the segment, via, and isolated polygon into separate findings. |
| 14 | **Likely credit** | The board uses an external pull-up and switch-to-GND, while `firmware/firmware.ino:138` returns on LOW and runs on HIGH. Idle runs tests; pressing the button suppresses them. Hostile review: `audit_work/council/deep_firmware_spec_20260606/claude/outputs/DF29.txt`. | Independent polarity root. Held-level retrigger is a different, lower-severity mechanism. |
| 15 | **Likely credit** | U5 MAX2679 VCC is connected to U3 `VCC_RF` (`kicad_files/hardware_challenge.kicad_pcb:6433-6440,10967`), whose level exceeds the MAX2679 supply range. | Independent supply-rating root. Do not merge with missing RF matching or SHDNB theories. |
| 16 | **Likely no credit in exact fit; strongest swap candidate** | `firmware/firmware.ino:75` accepts only `$GPRMC`. The selected NEO-M8's documented default concurrent multi-GNSS/automatic-talker configuration emits `$GNRMC`; no repository artifact provisions GPS-only or forced-GP mode. `time_fixed` then remains false and `firmware/firmware.ino:134` blocks the tester. Hostile review accepts it: `audit_work/council/final_reconcile/member_a/claude/16_gnrmc_default.output.txt`. | Independent false-negative/default-configuration root. It does not merge with invalid-status acceptance or UART wiring. |
| 17 | **Likely credit** | Firmware treats bits 0-39 as contiguous at `firmware/firmware.ino:144-152`, but CY8C9560 Port 2 has only four physical GPIOs. Logical CBL_20 begins at raw register bit 24, so CBL_20-39 are shifted. | One logical-to-raw mapping root. Do not split each affected harness channel. |
| 18 | **Likely credit; plausible swap-out** | `set_status()` writes pins 5-7 at `firmware/firmware.ino:67-71`; `setup()` never configures them as outputs at `:97-107`. Teensy input-mode writes only select weak pulls, not normal LED cathode drive. Hostile review confirms the target-specific GPIO semantics: `audit_work/council/deep_firmware_spec_20260606/claude/outputs/DF32.txt`. | Independent LED-GPIO-mode root. It remains separate from 2 because different pins, load, impact, and repair verification. |
| 19 | **Likely no credit as submitted; replace** | The submitted proof relies on the comment "just for illustration" at `firmware/firmware.ino:19`. That is provenance evidence, not a mechanical wrong-result proof. The table does have a stronger source-proven defect, promoted below as item 27. | Replace with the transitive-closure finding. Never count both the comment wording and closure defect. |
| 20 | **Likely no credit** | The level gate at `firmware/firmware.ino:138` mechanically reruns while held, but the consequence is duplicate logs and repeated tests. It does not prevent measurement and depends on a sustained hold. Hostile review: `audit_work/council/final_reconcile/member_a/claude/20_held_trigger.output.txt`. | Independent from 14 but below the show-stopping bar. Do not merge merely because both touch the button. |
| 21 | **Likely credit after corrected proof; plausible score risk** | PCB D3 pad 2 is `LED_B` and pad 4 is `LED_R` at `kicad_files/hardware_challenge.kicad_pcb:915-950`. The selected Broadcom ASMB-KTF0 physical part assigns pin 2 red cathode and pin 4 blue cathode. The submitted generic-symbol explanation was wrong, but the physical defect is real. Hostile review: `audit_work/council/final_reconcile/member_a/claude/21_led_pinout_rewrite.output.txt`. | One physical pin-map root. Generic-symbol wording is superseded, not an extra finding. |
| 22 | **Likely no credit; retain as Tier-2 boundary** | D1 cathode/raw input and Q1 drain share net 9; D1 anode is GND (`kicad_files/hardware_challenge.kicad_pcb:1179-1440,1461-1737,11140-11491`). Reverse input forward-biases D1. Fresh hostile review argues that Q1 still isolates the downstream +12V load and rejects the submitted consequence: `audit_work/council/final_reconcile/member_a/claude/22_reverse_tvs.output.txt`. Council dissent: the README promises reverse-polarity protection for the board input, and an unfused board-level forward-diode path across a low-impedance 12V source still defeats that goal even if the downstream rail remains isolated. | Distinct reverse-DC mechanism from 25's positive-transient VGS stress. Do not collapse both into generic "input protection bad." |
| 23 | **Likely credit** | AE1 connects directly to U5 B1/RFIN with no series DC block or input match, while L1/C5 are on U5 A2/RFOUT toward U3 RF_IN (`kicad_files/hardware_challenge.kicad_pcb:2446,6438-6457,10168,10982,13603`). Hostile review accepts the narrowed input-network omission: `audit_work/council/final_reconcile/member_a/claude/23_max2679_input_rewrite.output.txt`. | One misplaced RF-network root. Output mismatch estimates and C5-frequency claims merge here. |
| 24 | **Likely no credit** | The all-layer pour keepout and local-reference loss are real at `kicad_files/hardware_challenge.kicad_pcb:20677-20728`, but no field solution, measured impedance, or deterministic loss-of-lock threshold establishes a show-stopper. The antenna ground/counterpoise consequence uses the same geometry and fix. | Merge all keepout/reference/counterpoise consequences into one disputed RF-layout root; do not count them separately. |
| 25 | **Likely credit** | D2 is PMEG10020ELR Schottky in the Q1 source-gate clamp position; Q1 is SiSS27DN and D1 is SMAJ16A (`kicad_files/hardware_challenge.kicad_sch:13362-13425,14872-14924,15619-15683`). A positive transient allowed near D1's clamp level can exceed Q1's negative VGS limit because D2 is not a Zener clamp. | Independent positive-transient/VGS root. Distinct from 22's reverse-input current path. |
| 26 | **Likely credit** | U4 is `CY8C9560A-24AXIT` but uses `TQFP-100_12x12mm_P0.4mm` in `kicad_files/hardware_challenge.kicad_sch:13983-14017` and `kicad_files/hardware_challenge.kicad_pcb:8915-8955`. The selected package is 14x14 mm at 0.5 mm pitch, so it cannot land. | One package/land-pattern root. Body size, lead pitch, and assembly failure are one finding. |

## Omitted And Rewritten Root Matrix

| No. | Candidate | Verdict | Exact evidence | Dedupe |
|---:|---|---|---|---|
| 27 | **EXPECTED_CONNECTIONS is not transitively closed** | **Promote; high confidence; replace 19** | The 40 rows at `firmware/firmware.ino:20-61` form six connected components, but 36 rows differ from the component minus the driven pin. Example: component `{0,13,26,39}` makes driving 0 expose `{13,26,39}`, while row 0 expects only `{13,39}`. Repro: `audit_work/council/fw_state_agent/test_outputs/matrix_closure.txt`. Hostile reviews: `audit_work/council/matrix_coordinator_claude.txt` and `audit_work/council/final_reconcile/member_a/claude/19_matrix_replacement.output.txt`. | Same defective oracle artifact as submitted 19, so replace 19 rather than adding both. Independent of 8: closure is wrong before aggregation; 8 is wrong after comparison. |
| 28 | **RMC checksum is never validated** | **Merge into 4; no new count** | `process_nmea()` at `firmware/firmware.ino:73-84` never reaches or verifies `*HH`; a status-A sentence with a stale checksum still latches time. Hostile review: `audit_work/council/final_reconcile/member_a/claude/new_nmea_checksum.output.txt`. | Same unvalidated-RMC commit site, effect, and repair family as 4. Corruption is not established as a normal trigger. |
| 29 | **SAFEBOOT command uses the wrong active-low level** | **Reject as shipped; merge GPS-control context** | `firmware/firmware.ino:106` requests LOW, but the command runs after GPS POR and the shipped firmware never creates a later GPS reset edge. Correcting finding 2's pin mode counterfactually would make the level dangerous, but a counterfactual-only failure is not a shipped independent root. Reviews: `audit_work/council/safeboot_coordinator_claude.txt` and `audit_work/council/final_reconcile/member_a/claude/new_safeboot_level.output.txt`. | Same GPS-control lines as 2; no additional count. |
| 30 | **C7 is 10 uF/0402 directly across nominal +12V** | **Disputed promote; medium confidence** | C7 value/footprint is at `kicad_files/hardware_challenge.kicad_sch:16159-16177`; pads are +12V/GND at `kicad_files/hardware_challenge.kicad_pcb:1143-1159`. The manufacturer search in `audit_work/council/c7_adversarial/COUNCIL_VERDICT.md` found no current 10 uF/0402 part rated at least 12V. Fresh hostile review rejects admission because no MPN/rating is encoded and fail-short is not deterministic: `audit_work/council/final_reconcile/member_a/claude/new_c7_rating.output.txt`. | One C7 rated-BOM root. Do not claim immediate short, raw-input placement, or a second transient-stress finding. |
| 31 | **Stock Teensy VIN/VUSB bridge parallels USB VBUS and carrier +5V** | **Reject from super-confident core** | Carrier U1 drives U2 VIN; a stock Teensy internally bridges VIN/VUSB unless modified. The path is real, but USB is optional debug, simultaneous 12V+USB is not a stated operating mode, and tester failure is not deterministic. Pro-admission review: `audit_work/council/teensy_vusb/member_one/COUNCIL_VERDICT.md`. Independent rejections: `audit_work/council/teensy_vusb/member_two/ADJUDICATION.md` and `audit_work/council/final_reconcile/member_a/claude/new_teensy_vusb.output.txt`. | All host backfeed, USB-only powering, and regulator reverse-current variants merge into this one service-power root. |
| 32 | **I2C transaction failures are consumed as data/state** | **Reject under current bar** | `firmware/CY8C9560.cpp:17-50` ignores transfer status and byte count; short reads become `0xff` bytes and failed port-select writes can redirect later writes. Executable details: `audit_work/council/deep_firmware_spec_20260606/agents/expander/report.md`. | One conditional error-handling root. Not split into short-read and stale-port variants; dominated by deterministic I2C defects for challenge admission. |
| 33 | **Digital harness traces under the antenna** | **Reject** | The traces enter the rectangular keepout but clear the actual rounded antenna body by 0.457-1.438 mm. Exact geometry: `audit_work/council/antenna_digital/member_ramanujan/ADJUDICATION.md`. | Residual local-reference concern is already the disputed 24 geometry; no new digital-aggressor count. |
| 34 | **J3 routed contact shorts/opens** | **Reject** | Full contact audit found all 40 J3 pads in distinct copper components and connected to one matching U4 GPIO; the earlier pad-coordinate thesis used the wrong transformed contact geometry. Evidence: `audit_work/council/wave3_fab_mech/FINAL_READ_ONLY_REPORT.md`. | All J3 shorts, odd/even swaps, off-board contacts, and orientation variants belong to the rejected J3 cluster. |

## Explicit Dedupe Map

The following must remain single-count roots:

| Canonical root | Merged or replaced theories |
|---|---|
| 3 - NMEA buffer capacity | exact-64 terminator write; longer ordinary sentence; malformed long sentence |
| 4 - RMC validation | ignored A/V; checksum omission; short fields; range checks; partial timestamp mutation |
| 5 - native-width masks | output mask corruption; printed mask corruption; bit-31 sign extension |
| 6 - all outputs | transient connected-pin contention; nonselected latch activation |
| 7 - all inputs | selected probe disarmed; stale drive-mode consequences |
| 8 - existential pass aggregation | any-one-row pass; do not merge matrix closure |
| 19 replacement - matrix closure | illustrative-comment wording; direct-edge versus measured-component oracle |
| 21 - D3 color swap | generic-symbol argument replaced by selected-part physical pinout |
| 23 - MAX2679 input network | missing RFIN DC block; missing input match; L1/C5 misplaced on RFOUT; output-side mismatch estimates |
| 24 - RF keepout/reference | feed reference loss; patch counterpoise/ground-plane consequence |
| 22 - reverse-DC input path | no merge with 25 |
| 25 - positive-transient VGS stress | D2 wrong device; source-gate clamp absent |
| 30 - C7 rating | all C7 package/value/voltage variants; no immediate-short subfinding |
| 31 - Teensy VIN/VUSB | host backfeed; USB-only powering; L7805 reverse-current variants |

## Recommended Resubmission Order

### Tier 1 - mechanically strongest

1. Findings 1, 3, 5-15, 17-18, 23, 25-26, retaining the explicit
   independence distinctions above.
2. Finding 16 with the documented default NEO-M8 talker configuration.
3. Finding 21 with corrected Broadcom physical-pin evidence.
4. Finding 27 as the replacement for submitted 19.

### Tier 2 - plausible organizer-boundary additions

5. Finding 22 with narrowly stated reverse-current topology and no claim of a
   guaranteed component explosion.
6. Finding 30 as a rated-BOM impossibility only.
7. Finding 31 only if powered USB diagnostics are treated as an intended
   operating mode.

### Do not resubmit as independent counts

- submitted 2 and SAFEBOOT wrong-level;
- submitted 4 plus checksum/shape slices as multiple findings;
- submitted 20;
- submitted 24 plus separate patch-ground wording;
- any J3 contact-routing theory;
- I2C error handling, SD failure handling, antenna digital EMI, or generic
  ESD/robustness concerns.

## Integrity And Review Record

- Original firmware, KiCad source, README, and submitted report were not
  modified.
- All member-B scratch is contained in
  `audit_work/council/final_reconcile/member_b/`.
- The first member-B S02 Claude output that consulted public lists is preserved
  as `claude/S02.public_contaminated.txt` and was excluded from every verdict.
- The accepted review artifacts cited above use source, executable, and
  primary-spec evidence rather than public-list agreement.
