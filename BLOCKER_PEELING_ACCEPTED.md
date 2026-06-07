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
