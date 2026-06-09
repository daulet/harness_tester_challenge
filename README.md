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

The runtime uses a deterministic microsecond event queue. Arduino `delay()`
advances simulated time without sleeping, and physical GPS UART transmissions
use KiCad-derived U2/U3 pad nets, 8N1 frame timing, configured baud, and
contention checks. Physical `Serial1` reception follows the PJRC Teensy 4.x
[`Serial1` allocation](https://github.com/PaulStoffregen/cores/blob/master/teensy4/HardwareSerial1.cpp)
and [ring implementation](https://github.com/PaulStoffregen/cores/blob/master/teensy4/HardwareSerial.cpp):
the default 64-entry software ring has 63 usable bytes, newly arriving bytes are
dropped when full, and the runtime records the lost-byte count. Tests can
configure a larger receive capacity to represent added receive memory.
`inject_serial1_rx_bypass_capacity()` remains an explicit helper for isolated
parser and sanitizer witnesses; end-to-end scenarios use `transmit_gps()`
instead.

`GpsReceiver` layers a deterministic NEO-M8 source profile over that UART
transport: startup TXT, one-second navigation epochs, the factory-enabled NMEA
sentence set, concurrent-mode `GN` navigation talkers with GNSS-specific GSV talkers,
pre-fix/post-fix status, and generated or intentionally corrupted checksums.
The default cold-acquisition interval is 26 seconds. This models documented
receiver output behavior, not RF acquisition, satellite dynamics, ephemeris,
leap seconds, hardware FIFO/interrupt latency, or flow control.

Button input changes can be scheduled as exact press/bounce/release waveforms.
The CY8C9560 model follows the selected silicon rather than the misleading
`CY_RST_N` net name: XRES is active high, POR powers up deasserted with factory
pull-ups, held-reset pins are high impedance, and the unchanged driver advances
through its 10 ms reset pulse plus 100 ms recovery delay in simulated time.

I2C controller calls now consume bit-level bus time and report Arduino-compatible
address-NACK, data-NACK, and other-error statuses. Focused physical tests can
attach the ngspice feedback backend: Runtime derives SDA/SCL pulls from its own
parsed board only when the resistor pads are physically connected to the bus
at the MCU and to its rail, solves released line voltages before START, and
feeds a typed electrical snapshot back into the firmware-visible Wire result.
Peripheral-side SDA/SCL reachability is checked separately so a downstream open
produces an address NACK instead of erasing a controller-side pull.
Unsupported indeterminate or overvoltage line levels fail explicitly before
either line can mask the other. The default fast path retains the narrow parsed
R3-to-GND approximation for large campaigns, while explicit ideal mode isolates
downstream firmware behavior. Tests can also inject SCL/SDA stuck-low faults,
one-shot NACKs, and deterministic clock stretching. Neither path models rise
time, arbitration, or bus clear.

The SD shim models the firmware-used `FILE_WRITE` create/append path with
configurable initialization/open/write/close time, finite aggregate capacity,
card removal/reinsertion, and deterministic all-or-zero write-call failures,
matching the target SdFat return contract. Neutral defaults remain zero-time
and effectively unlimited because the design does not specify a card. The model
does not yet implement deferred close/sync failure, file reads, FAT allocation
geometry, wear, power-loss corruption, or measured card latency distributions.

Build and run:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/harness_simulator_original
```

The blocker-peeling generator keeps original firmware and KiCad files
immutable, emits hashed variants under `build/blocker_peeling/`, and runs
bounded discovery matrices:

```sh
ctest --test-dir build \
  -R '^blocker_peeling_(harness|peripheral_event|metamorphic)_campaign' \
  --output-on-failure
```

`exposure_matrix.csv` records 2,113 digital harness topologies.
`peripheral_exposure_matrix.csv` records 32 peripheral/event scenarios per
variant against an atomic RMC-validation repair and its exact P9
counterfactual, including a 384-case deterministic parser corpus. See
`reports/BLOCKER_PEELING_REPORT.md` for admission decisions and explicit
fidelity limits.

The checked-in
`simulator/blocker_peeling/corpus/nmea_peripheral_regression.json` adds 842
replayable valid, invalid, post-lock, and loop-framing cases. CTest regenerates
the corpus byte-for-byte and runs it through baseline, AddressSanitizer, and
UndefinedBehaviorSanitizer builds when those sanitizers are available.

The C004 and C005 metamorphic campaigns each run 384 paired transformations
across parser fragmentation and neutral insertion, button idle insertion,
bounded SD latency, I2C clock stretching, and lossless physical-UART polling.
Each also runs six expected-differential controls. They use distinct
deterministic seeds, verify that the generated descriptor vectors differ, and
write
`metamorphic_matrix_c004.csv` and `metamorphic_matrix_c005.csv`.

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

`BoardElectricalConfig` now scans parsed two-terminal resistor components and
PCB pad nets to replace the fixture's SDA/SCL pull parameters. On the checked-in
board this derives R2 as a 4.7 kOhm `CY_SCL` pull-up and R3 as a 4.7 kOhm
`CY_SDA` pull-down. The remaining rail, protection, LED, UART, and harness
parameters are still authored fixture inputs.

Run only the electrical fixtures with:

```sh
ctest --test-dir build -R '^analog_' --output-on-failure
```

Named fixtures cover nominal operation, reverse polarity, transient clamping,
open circuit, unexpected short, leakage, elevated contact resistance,
intermittent connection, bad pull network, LED overcurrent/no-current, UART
level mismatch, I2C pull failure, and source-derived board pulls. See
`simulator/ngspice/README.md` for the interface, fixture schema, model
provenance, thresholds, and explicit limits.

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
