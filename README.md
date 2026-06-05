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

This repo includes a source-level host simulator under `simulator/`. It compiles
the original `firmware/firmware.ino` unchanged against small Arduino/Teensy API
shims. The C++ runtime remains the control plane: firmware pin state is evaluated
by the existing digital `BoardModel`, and selected digital stimuli can be passed
to a deterministic ngspice process for first-pass electrical observations.

The board model reads the KiCad sources directly:

- `kicad_files/hardware_challenge.kicad_pcb` is the runtime connectivity source.
- `kicad_files/hardware_challenge.kicad_sch` is the intended-design reference.
- `simulator/tests/board_model_cases.cpp` checks J3, U4, U2, zones, vias, and
  keepout evidence before exercising the runtime board model.

Schematic-versus-PCB disagreements are reported as evidence, but the runtime
continues on the PCB-derived graph rather than replacing it with schematic
connectivity.

Build and run:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/harness_simulator_original
```

The analog tests require an installed `ngspice` executable. CMake discovers it
from `PATH`; when it is absent, the named analog tests are reported as skipped.
On macOS the local requirement can be installed with `brew install ngspice`.
The simulator never downloads models or fixture data at configure or test time.

The checked-in analog layer lives under `simulator/ngspice/` and models:

- 12V input, reverse blocking, a unidirectional transient clamp, and first-order
  5V/3.3V rail propagation;
- pull-up, pull-down, open-drain I2C, UART voltage levels, and RGB LED current;
- one representative harness channel with series R/L, shunt C, leakage, contact
  resistance, open/short behavior, and a deterministic intermittent interval.

Run only the electrical fixtures with:

```sh
ctest --test-dir build -R '^analog_' --output-on-failure
```

Named fixtures cover nominal operation, reverse polarity, transient clamping,
open circuit, unexpected short, leakage, elevated contact resistance,
intermittent connection, bad pull network, LED overcurrent/no-current, UART
level mismatch, and I2C pull failure. See `simulator/ngspice/README.md` for the
interface, fixture schema, model provenance, thresholds, and explicit limits.

This first pass is not RF, antenna, GPS acquisition, EMI, full MCU silicon,
connector wear/vibration, or production physical validation. The regulator,
protection, logic-driver, LED, and harness models are intentionally simplified;
their purpose is deterministic host regression and fault observability.

The dedicated source-backed bug evidence cases can be run with:

```sh
ctest --test-dir build -R '^bug_' --output-on-failure
```

The broader board-scenario checks can be run with:

```sh
ctest --test-dir build -R '^(scenario_|bug_a03|bug_a08)' --output-on-failure
```

When the compiler supports AddressSanitizer and UndefinedBehaviorSanitizer, the
build also registers two expected-failure firmware witnesses:

```sh
ctest --test-dir build -R '^sanitizer_' --output-on-failure
```

These execute the unchanged firmware and pass only when the process terminates
with the intended diagnostic: the NMEA global-buffer overflow for A03, or the
invalid narrow shift for A05. The wrapper rejects a clean exit or an unrelated
crash. CMake reports explicitly when either sanitizer is unavailable.

The test suite runs the unchanged firmware against both known-good and broken
harness fixtures, then runs the explicit `simulator/variants/firmware_unmasked.ino`
variant separately. That variant fixes only the initialization/probe blockers so
the hidden schematic wiring mistake becomes observable. The patched case reaches
the expander and shows that logical probe 20 drives an unconnected physical bit.
