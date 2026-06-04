<div align="center">
<h1>Harness Tester Challenge</h1>
<h3>
  <a href="https://comma.ai/leaderboard">Leaderboard</a>
  <span> · </span>
  <a href="https://comma.ai/jobs">comma.ai/jobs</a>
  <span> · </span>
  <a href="https://discord.comma.ai">Discord</a>
  <span> · </span>
  <a href="https://x.com/comma_ai">X</a>
</h3>
</div>

We (mock) designed a simple harness testing PCB and wrote some firmware for it. Unfortunately, this board was designed and firmware was written on a Monday morning.

<b>Can you figure out everything wrong with it?</b>

## The Harness Tester
We currently ship hundreds of [car-specific harnesses](https://www.comma.ai/shop/car-harness) to users every month, consisting of more than [40 different types](https://github.com/commaai/hardware/tree/master/harness/v3).

As a simple wiring harness, we expect them to have a near-zero failure rate, so good testing is critical.
This tester is designed to be capable of quickly verifying which pins are connected to which other pins.

A quick list of design goals is listed here:
- 12V DC input with transient and reverse polarity protection
- compatible with harnesses with up to 40 pins (through a pin header)
- start button and RGB LED are the user interface
- microSD card to store test results along with the current time (for statistical purposes)
- receive time by acquiring a GPS lock (*ok, yes, this is just to make the board and firmware more interesting*)

## The challenge
The challenge consists of 3 distinct parts, each riddled with more than a few bugs:
- Schematic ([PDF](./schematic.pdf) / [Web](https://kicanvas.org/?github=https%3A%2F%2Fgithub.com%2Fcommaai%2Fharness_tester_challenge%2Fblob%2Fmaster%2Fkicad_files%2Fhardware_challenge.kicad_sch))
- PCB layout ([Gerbers](./gerbers.zip) / [Web](https://kicanvas.org/?github=https%3A%2F%2Fgithub.com%2Fcommaai%2Fharness_tester_challenge%2Fblob%2Fmaster%2Fkicad_files%2Fhardware_challenge.kicad_pcb))
- Firmware ([Source](./firmware/))

The board is designed in [KiCad 8](https://www.kicad.org/), and the full project is available in an [exported archive](./kicad_project.zip).

There are a handful of intentional, show-stopping bugs hidden in each part. Find them, and [send](https://forms.gle/US88Hg7UR6bBuW3BA) them over to us for verification!

We're not looking for improvements or fixes; just submit a list of bugs.

![PCB](./imgs/board.png)

## Host simulator

This repo now includes a source-level host simulator under `simulator/`. It compiles
the original `firmware/firmware.ino` unchanged against small Arduino/Teensy API
shims and a digital board model. It does not emulate the Teensy binary or analog
behavior.

The board model is split deliberately:

- `simulator/data/schematic_harness_map.csv` contains the schematic-derived J3 to
  CY8C9560 mapping.
- `simulator/data/schematic_io_map.csv` contains the firmware-visible GPIO/I2C
  mapping.
- `simulator/data/pcb_fault_overlay.csv` contains the layout-only J3 fault. The
  schematic says all 40 contacts attach to their matching `CBL_*` nets, while
  the routed PCB only attaches J3 pad 1 to `CBL_0` and J3 pad 2 to both
  `CBL_1` and `CBL_2`.

Build and run:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/harness_simulator_original
```

The test suite runs the unchanged firmware against both known-good and broken
harness fixtures, then runs the explicit `simulator/variants/firmware_unmasked.ino`
variant separately. That variant fixes only the initialization/probe blockers so
the hidden schematic wiring mistake and the PCB overlay become observable. The
patched schematic-only case reaches the expander and shows that logical probe
20 drives an unconnected physical bit, while the same patched case on the
as-drawn overlay shows the J3 pad-2 `CBL_1`/`CBL_2` short.
