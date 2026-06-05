# Deterministic electrical simulator

`NgSpiceSimulator` keeps source-level firmware execution in the C++ `Runtime`.
It writes fixture parameters and runtime stimuli into a temporary driver
netlist, invokes the installed `ngspice` binary directly with `fork`/`exec`, and
parses named `.meas` results back into `AnalogObservation`. Temporary files are
removed after each run, and `LC_ALL=C` fixes numeric parsing.

The permanent checked-in inputs are:

- `board_harness.cir`: board rails, representative logic nets, RGB LED loads,
  and one harness channel;
- `models/first_pass_models.lib`: compact diode, LED, and switch models;
- `fixtures/*.fixture`: complete numeric inputs for nominal and fault cases.

No test downloads data or obtains a model from the network.

## Model boundary

The 12V source includes source resistance, a simplified unidirectional TVS, and
a simplified reverse-blocking element. The protected rail feeds behavioral 5V
and 3.3V regulators with finite output resistance, capacitance, and loads. The
harness has series resistance and inductance, contact resistance, shunt
capacitance, receiver loading, insulation leakage, optional shorts, an open
resistance, and a timed contact switch.

I2C uses 4.7 kohm nominal pulls and finite-resistance open-drain switches. UART
uses finite source impedances so conflicting or overvoltage peers produce a
measurable line voltage. Each LED channel uses a diode model, configurable
series resistance, a current-sense source, and a firmware-controlled sink.

`Runtime::analog_stimulus()` currently exports active-low RGB LED state and
released I2C state. Additional firmware-controlled electrical signals should be
added there rather than bypassing the C++ control plane.

## Provenance

The model library is authored for this simulator and is not copied from a
vendor SPICE library. Part identities and nominal intent come from
`kicad_files/hardware_challenge.kicad_sch`:

- D1 is identified as Littelfuse `SMAJ16A`; the first-pass TVS uses a 17.8V
  breakdown knee and finite series resistance based on the schematic-linked
  SMAJ datasheet.
- Q1 is identified as Vishay `SiSS27DN`; this pass represents only reverse
  blocking and path resistance, not the MOSFET transfer curve or PCB orientation
  defect.
- U1 is identified as `L7805`; it is represented by a bounded behavioral 5V
  source, not a vendor macromodel.
- D3 is identified as `ASMB-KTF0-0A306`; its three colors use generic diode
  equations chosen to produce distinct first-pass forward drops.

These choices are regression abstractions. They do not establish surge-energy,
thermal, regulator-stability, component-tolerance, or absolute-maximum safety.

## Reproduction

Each fixture is a strict `key=value` file. It specifies every electrical
parameter; unknown, duplicate, or missing keys fail before ngspice runs. The
driver adds optional `AnalogStimulus` overrides, includes the checked-in model
and circuit files by absolute path, then runs a fixed 5ms Gear transient with
fixed tolerances and named measurements.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build -R '^analog_' --output-on-failure
```

The named CTest cases are the acceptance thresholds. Voltage classifications
default to low at or below 0.8V, high at or above 2.0V, indeterminate between
those limits, and overvoltage above 3.6V.

## Out of scope

This layer does not model RF paths, antennas, GPS lock physics, EMI, distributed
PCB impedance/return paths, full MCU or peripheral silicon, thermal behavior,
mechanical wear/vibration, manufacturing tolerances, or production validation.
