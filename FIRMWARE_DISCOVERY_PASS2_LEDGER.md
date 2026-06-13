# Firmware Discovery Pass 2 Ledger

Scope: `firmware/firmware.ino`, `firmware/CY8C9560.cpp`,
`firmware/CY8C9560.h`, the directly connected KiCad nets, and target-specific
Teensy 4.1 APIs. Scratch reproducers live under `audit_work/`; original source
was not modified.

Status meanings:

- **Accepted new**: source-backed, distinct, and not present in the submitted
  26 findings.
- **Merged**: technically valid, but already represented by an existing
  finding or the same root cause.
- **Rejected**: false, conditional, improvement-only, or insufficiently
  source-proven as an intentional show-stopper.

## Accepted New

### N01 - The expected connection rows are not electrically transitive

- Status: **Accepted new**, recommended as the replacement for submitted
  finding 19 rather than as an additional version of that weak wording.
- Source evidence: `EXPECTED_CONNECTIONS` is defined in
  `firmware/firmware.ino:20-61`; the measured row is compared directly with
  `EXPECTED_CONNECTIONS[i]` at `firmware/firmware.ino:148-155`.
- Reproduction: `audit_work/council/fw_state_agent/test_outputs/matrix_closure.txt`
  independently parses all 40 rows. The table is symmetric and forms six
  connected components, but 36 of 40 rows differ from the component minus the
  driven pin. For component `{0,13,26,39}`, row 0 expects only `{13,39}`;
  physical continuity also exposes pin 26.
- Root cause: the table stores direct graph edges, while a passive continuity
  measurement observes the complete connected component. Requiring all rows,
  fixing the pin map, and fixing expander direction handling do not repair this
  oracle mismatch.
- Claude status:
  - Fresh state-agent Claude: `ACCEPT_DISTINCT`.
  - Coordinator Claude review 1: `ACCEPT`.
  - Coordinator Claude review 2: technical claim accepted, but recommended
    merge with broad submitted finding 19.
- Council disposition: accept the concrete closure thesis and replace the
  comment-based "illustrative table" finding. Do not count both.

## Merged With Existing Findings

| Candidate | Disposition | Exact evidence | Claude status |
| --- | --- | --- | --- |
| CY8C9560 initialization omitted | Merge with finding 1 | `setup()` at `firmware/firmware.ino:97-116` never calls `cy.begin()`; initialization is at `firmware/CY8C9560.cpp:3-15`. | Prior admission reviews accepted. Fresh agent CLI returned `Not logged in`. |
| NMEA buffer overflow | Merge with finding 3 | `nmea_buf[64]` and unbounded `nmea_idx++` at `firmware/firmware.ino:118-126`; ASan repro reports a write past byte 64, and an ordinary u-blox RMC example is 75 bytes. | Prior admission accepted. Fresh agent CLI unavailable. |
| Invalid RMC status accepted | Merge with finding 4 | `%*c` discards the A/V status before setting `time_fixed` at `firmware/firmware.ino:75-80`; scratch parser accepts a `V` sentence. | Prior admission accepted. |
| Missing NMEA checksum validation | Merge with finding 4, not a separate count | No checksum code exists in `process_nmea()` at `firmware/firmware.ino:73-84`; scratch parser accepts a deliberately wrong checksum. | Fresh coordinator Claude: `REJECT` as distinct, because it is the same unvalidated-RMC root and requires corruption. |
| Malformed short time/date accepted | Merge with finding 4 | `%10[^,]` and `%6[^,]` cap maximum length but impose no minimum or range at `firmware/firmware.ino:76`; scratch parser accepts time `1`, date `2`. | Fresh CLI invocation failed; local result retained only as parser-validation detail. |
| Only `$GPRMC` accepted | Merge with finding 16 | Prefix check at `firmware/firmware.ino:75`; default multi-GNSS u-blox M8 talker is `GN`. | Prior admission accepted. |
| 32-bit shifts in a 40-pin loop | Merge with finding 5 | `1 << i` and `1 << j` at `firmware/firmware.ino:144,152`; UBSan reports shift exponent 32, and ARM output sign-extends bit 31. | Prior admission accepted. |
| `set_output()` makes every port output | Merge with finding 6 | Direction `0x00` is written for all eight ports at `firmware/CY8C9560.cpp:77-83`; scratch trace records eight zero direction writes. | Prior admission accepted. |
| `set_pd_inputs()` removes the selected output | Merge with finding 7 | Direction `0xFF` is written for all ports at `firmware/CY8C9560.cpp:61-66`, immediately after `set_output()` at `firmware/firmware.ino:145-146`. | Prior admission accepted. |
| One matching row passes the harness | Merge with finding 8 | `passed` starts false and is only set true at `firmware/firmware.ino:142,155-157`; scratch result shows one of forty rows leaves pass true. | Prior admission accepted. |
| FAILED is immediately replaced by GOOD | Merge with finding 9 | `set_status(GOOD)` runs on every post-lock loop at `firmware/firmware.ino:133-139`; scratch state changes FAILED to GOOD. | Prior admission accepted. |
| Logical-to-register gap after CBL_19 | Merge with finding 17 | Firmware uses contiguous mask bits at `firmware/firmware.ino:144-152`, while CY8C9560 port 2 has only four pins. | Prior admission accepted. |
| LED pins are never configured as outputs | Merge with finding 18 | `set_status()` writes pins 5-7 at `firmware/firmware.ino:67-71`, but `setup()` has no LED `pinMode(OUTPUT)` calls. Teensy input-mode writes only select weak pulls. | Prior admission accepted. |
| Button polarity inverted | Merge with finding 14 | Board R4 is a 10 kOhm pull-up and SW1 shorts `BTN_TEST` to GND; firmware returns on LOW at `firmware/firmware.ino:137-139`. | Prior admission accepted. |
| Held button retriggers and relogs | Merge with finding 20 | The button is only level-tested at `firmware/firmware.ino:137-163`; scratch loop records three tests for three held-high iterations. | Prior admission accepted, but this remains lower confidence as an intentional show-stopper. |
| Later malformed RMC can partially update timestamp buffers | Merge with finding 4 | `sscanf()` executes before `&& !time_fixed` at `firmware/firmware.ino:76`, so later lines can modify globals even after lock. | Prior review: disputed/derivative; not an independent count. |
| D3 red and blue channels are swapped | Merge with finding 21, with corrected evidence | PCB pad 2 is `LED_B` and pad 4 is `LED_R` at `kicad_files/hardware_challenge.kicad_pcb:915-948`. The selected Broadcom ASMB-KTF0-0A306 datasheet assigns physical pin 2 to red cathode and pin 4 to blue cathode. | Primary-datasheet review confirms the title; earlier symbol-only evidence was inadequate. |

## Rejected

| Candidate | Rejection reason and exact evidence | Claude status |
| --- | --- | --- |
| Submitted finding 2: GPS writes have no electrical effect because pins are inputs | Incorrect target semantics. Teensy 4.1 `digitalWrite()` on an input selects a pull-up or pull-down. Lines `firmware/firmware.ino:106-107` therefore weakly bias SAFEBOOT low and RESET_N high rather than doing nothing. Missing active output drive alone is not proven show-stopping. | Dedicated Claude runs did not return before freeze. |
| SAFEBOOT input pull-down deterministically prevents GNSS boot | Source mechanism is plausible: line 106 requests LOW, U3 pad 1 is `UBX-SAFEBOOT` at `kicad_files/hardware_challenge.kicad_pcb:10882-10890`, and u-blox says SAFEBOOT_N low at startup enters safe boot. Rejected as deterministic because the files do not prove the relative Teensy firmware/GPS startup sampling timing or net voltage. | Three fresh Claude processes timed out/no completed verdict. |
| Timestamp freezes after first fix | False. `sscanf()` is evaluated before `!time_fixed` at `firmware/firmware.ino:76`; scratch output shows the second sentence updates both fields. | Locally disproved before Claude. |
| CRLF double frame is operationally harmful | Both CR and LF call the parser at `firmware/firmware.ino:123-126`, but the second call receives an empty frame and cannot match `$GPRMC`. | Earlier Claude: `REJECT`. Fresh invocation failed. |
| Button input floats | False on this board. R4 is a populated 10 kOhm pull-up on `BTN_TEST`; SW1 connects the net to GND when pressed. | Local source disproof. |
| `FILE_WRITE` overwrites previous results | False for the Teensy SD library. Its `SD.open(..., FILE_WRITE)` uses `O_RDWR | O_CREAT | O_AT_END`; firmware closes each record at `firmware/firmware.ino:86-91`. | Local primary-library disproof; no completed fresh Claude result. |
| SD initialization failure remaining BUSY is a hidden design bug | `while (1)` at `firmware/firmware.ino:109-113` is poor failure handling, but requires a missing/failed card and is not a deterministic source or board fault. | Prior strict review rejected similar runtime error-handling claims. |
| `SD.open()` failure silently loses a result | Conditional media/runtime failure handling, not an intentional deterministic defect. The code does print an error at `firmware/firmware.ino:92-94`. | Prior strict Claude rerun: `REJECT`. |
| Serial1 overflow during the probe is a separate show-stopper | A 100 kHz transaction estimate and the Teensy 64-byte RX buffer make overflow plausible while testing, but the GPS fix is already acquired, UART parsing resynchronizes on line endings, and no distinct test-result failure was proven. | Fresh agent Claude unavailable. |
| `%f` scanning is unavailable on Teensy | False. The target-compatible scratch `sscanf` repro parsed both skipped float fields and returned two assigned fields. | Local runtime disproof. |
| Unchecked I2C short reads are a separate bug | `Wire.read()` can return `-1`, becoming `0xFF` in `firmware/CY8C9560.cpp:17-34`, but this is generic error handling and is dominated by the deterministic missing initialization and SDA pull-down faults. | Earlier Claude: `REJECT` as a separate challenge bug. |
| Repeated-start sequence is unsupported | False. Teensy Wire supports `endTransmission(false)` followed by `requestFrom()`, exactly as used at `firmware/CY8C9560.cpp:17-22`. | Primary library disproof; fresh Claude unavailable. |
| Register burst byte order/autoincrement is wrong | No contradiction found. The code writes and reads consecutive byte registers least-significant byte first at `firmware/CY8C9560.cpp:25-50`, matching the port-register ordering. | Candidate was intentionally skeptical; fresh Claude unavailable. |
| Drive-mode registers must all be cleared first | False. CY8C9560 drive-mode registers use last-written-one priority; writing a one in the desired mode changes that pin's active mode. | Primary datasheet disproof; fresh Claude unavailable. |
| CY8C9560 reset polarity is inverted | False. XRES is active high; `HIGH`, delay, then `LOW` at `firmware/CY8C9560.cpp:5-9` is the correct reset pulse. | Prior adversarial review: `REJECT`. |
| Device-ID extraction is wrong | False. `read_register(0x2E) >> 4 & 0x0F` at `firmware/CY8C9560.cpp:53-55` matches the CY8C9560 family ID field. | Primary datasheet disproof; fresh Claude unavailable. |
| I2C address includes the R/W bit | False. `0b0100000` at `firmware/CY8C9560.h:8` is the correct seven-bit default address with A0 low. | Primary datasheet disproof; fresh Claude unavailable. |
| Wire2 uses the wrong Teensy pins | False. U2 pads 16/17 are pins 24/SCL2 and 25/SDA2 at `kicad_files/hardware_challenge.kicad_pcb:7841-7861`, matching Teensy `Wire2`. | Prior source review: `REJECT`. |

## Submission Impact

- New source-backed thesis beyond the submitted wording: **N01 matrix
  transitive-closure mismatch**.
- Replace submitted finding 19 with N01; do not submit both.
- Retain submitted finding 21, but cite the selected Broadcom part rather than
  the generic KiCad symbol: board pad 2 is `LED_B` although the physical part's
  pin 2 is red; board pad 4 is `LED_R` although physical pin 4 is blue.
- Submitted finding 2 is not supported by Teensy 4.1 semantics as written.
  Do not replace it with SAFEBOOT-low unless startup timing and voltage are
  measured or otherwise proven.
- Net result of this pass: one strong replacement, two submitted claims
  downgraded, and no justified path to 35 high-confidence independent firmware
  bugs.
