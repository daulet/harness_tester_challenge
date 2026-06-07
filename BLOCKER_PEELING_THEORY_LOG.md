# Blocker-Peeling Theory Log

This log preserves every candidate raised by the blocker-peeling campaign,
including adversarial review outcomes and coordinator adjudication. Submission
wording belongs in `BLOCKER_PEELING_ACCEPTED.md`.

## BP-T001 - D3 red and blue cathodes are swapped

- **Status:** ACCEPTED
- **Root cause:** the selected LED's manufacturer pinout disagrees with the
  schematic/PCB footprint color labels.
- **Thesis:** Broadcom `ASMB-KTF0-0A306` uses pin 2 for the red cathode and pin
  4 for the blue cathode, but the design connects D3.2 to `LED_B` and D3.4 to
  `LED_R`. Once the masked LED power, resistor, and GPIO-mode defects are
  repaired, BUSY lights physical red and FAILED lights physical blue.
- **Primary specification evidence:** Broadcom
  `ASMB-KTF0-0A306-DS100`, package pin configuration: pin 1 anode, pin 2 red
  cathode, pin 3 green cathode, pin 4 blue cathode.
  <https://docs.broadcom.com/doc/ASMB-KTF0-0A306-DS100>
- **Source evidence:** `kicad_files/hardware_challenge.kicad_pcb` connects
  D3.2 to `LED_B` at lines 924-931, D3.3 to `LED_G` at lines 933-940, and D3.4
  to `LED_R` at lines 942-949. Firmware `set_status()` sinks `LED_B` for BUSY
  and `LED_R` for FAILED at `firmware/firmware.ino:67-70`.
- **Simulator evidence:** `blocker_peeling_p8_without_mapping` uses the
  source-derived selected-part mapping after repairing only LED power,
  resistance, and GPIO modes. It observes BUSY current in the physical red
  model, GOOD current in green, and FAILED current in blue. The P8 repaired
  mapping control observes blue, green, and red respectively.
- **Negative control:** `P8_without_BOARD_LED_COLOR_MAPPING` differs from P8
  only by retaining the source-derived D3 pin mapping.
- **Duplicate analysis:** independent of A12 (missing current limiting), A16
  (isolated common anode), and A21 (LED pins left as inputs). Correcting all
  three leaves the red/blue assignment unchanged.
- **Prior rejection:** `SIMULATOR_SCENARIO_REVIEW.md` rejected the swap because
  schematic and PCB agreed. That review did not compare either artifact with
  the selected-part manufacturer pinout and is superseded.
- **Source/spec reviewer:** ACCEPT. Green remaining on pin 3 rules out a simple
  package rotation; no contradictory datasheet was identified.
- **Simulator-fidelity reviewer:** REJECT. It argued that physical color names
  in the circuit model are circular and repeated the prior unverified-pinout
  concern. Coordinator disagrees: the official manufacturer pinout is the
  independent color oracle, the model now refuses any D3 value other than
  `ASMB-KTF0-0A306`, and ngspice is used only to prove downstream current after
  blockers are peeled. The rejection also incorrectly assumed BUSY might be
  intended red despite the firmware explicitly assigning BUSY to `LED_B`.
- **Root-cause/duplicate reviewer:** ACCEPT, conditional on official pinout
  verification. The condition is satisfied by the Broadcom source above.
- **Final council:** ACCEPT. The earlier rejection is overturned.
- **Residual caveat:** a deliberately non-standard footprint pad-numbering
  convention could overturn the physical-pad inference, but no source evidence
  supports such a convention and the footprint uses ordinary numbered pads.
