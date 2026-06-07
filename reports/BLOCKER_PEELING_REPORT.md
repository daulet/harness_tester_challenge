# Blocker-Peeling Campaign Report

## Scope

This campaign generated repaired experimental variants while preserving
`firmware/` and `kicad_files/` as immutable evidence inputs. The simulator was
used to expose behavior, not to define correctness. Findings were admitted only
when source/specification evidence, a causal counterfactual, model fidelity, and
duplicate analysis all survived adversarial review.

## Completed Peels

P0 through P9 are generated from:

- `simulator/blocker_peeling/repair_catalog.yaml`
- `simulator/blocker_peeling/dependency_graph.yaml`
- `simulator/blocker_peeling/scenario_catalog.yaml`

Every generated variant records source hashes, applied repairs, generated
artifact hashes, scenarios, and simulator configuration in
`build/blocker_peeling/variant_manifest.jsonl`.

The sequence reaches:

- expander initialization and physical I2C;
- corrected UART routing and default NEO-M8 RMC reception;
- complete 40-channel probing and logical/raw mapping;
- all-row verdict and passive matrix closure;
- one-session-per-press operation;
- closed-loop LED current and persistent result status;
- observable all-or-zero SD write-call failure.

Leave-one-repair-out variants preserve causal witnesses for each peel.

## Discovery Campaigns

### C001 - Digital harness topology

`build/blocker_peeling/exposure_matrix.csv` contains 2,113 scenarios:

- one declared baseline;
- all 40 single opens;
- all 780 double opens;
- all 780 pair additions;
- 256 seeded mixed open/short cases;
- 128 synthetic three-pin components;
- 128 synthetic four-pin components.

The repaired firmware and independent connected-component oracle had zero row,
verdict, log, or status differentials.

### C002 - Peripheral and event sequences

`build/blocker_peeling/peripheral_exposure_matrix.csv` contains 64 rows: 32
scenarios against an atomic RMC-validation variant and the same 32 against its
exact P9 counterfactual.

Coverage includes:

- valid, invalid, corrupt, truncated, oversized, fragmented, and post-lock RMC;
- 384 deterministic seeded valid/invalid parser cases;
- press, hold, release/repress, and authored bounce sequences;
- physical UART success, baud mismatch, and bounded overrun;
- normal, NACK, and stuck-low I2C sequences;
- SD append, capacity, removal, in-flight removal, and reinsertion.

The repaired variant had zero differentials. The counterfactual had exactly
nine: seven in the existing A04 RMC-validation root and two in BP-M002 atomic
state composition. No off-allowlist differential survived.

## Accepted Finding

`BLOCKER_PEELING_ACCEPTED.md` contains one newly admitted root:

1. D3 red and blue cathodes are swapped relative to the selected
   `ASMB-KTF0-0A306` manufacturer pinout.

The closed-loop P8 counterfactual isolates this from missing LED power,
resistance, and GPIO-mode defects. The independent color oracle is the official
Broadcom `ASMB-KTF0-0A306-DS100` pin configuration:
<https://docs.broadcom.com/doc/ASMB-KTF0-0A306-DS100>.

## Rejected And Merged Theories

`BLOCKER_PEELING_THEORY_LOG.md` preserves all outcomes:

- expected-matrix closure is a duplicate of an existing accepted root;
- post-lock partial RMC mutation merges into A04;
- the original SD partial-write thesis was rejected after target-stack
  fidelity correction;
- release bounce was rejected for lack of a selected-part/product oracle;
- ignored generated `begin()` failure was rejected as an A01 repair artifact;
- I2C-fault misdiagnosis was rejected independently and retained as an A11
  impact witness plus simulator timing limitation;
- SD reinsertion recovery was rejected as unspecified conditional media policy.

## Remaining Fidelity Limits

- Harness truth is derived from source-declared topology; no external harness
  specification is available.
- Solved electrical readback is closed for LEDs and source-derived I2C pulls,
  not all 40 harness channels or MCU/expander pins.
- I2C stuck-low failures return immediately in the runtime; target Teensy Wire
  waits for timeout.
- The button runtime replays authored boolean edges and has no contact/RC model
  or selected switch bounce specification.
- SD deferred close/sync failure, FAT geometry, wear, and power-loss corruption
  remain unmodeled.
- RF acquisition, thermal limits, mechanical durability, EMI, and production
  manufacturing variation remain outside the simulator boundary.
- `kicad-cli`, Teensy target cross-compilation, and hardware/reference traces
  are not available in the current local validation boundary.

## Stopping Rule Status

C001 and C002 produced no new accepted root, and every differential is
adjudicated. C002 nevertheless produced BP-C003 through BP-C006 as new
candidates, so the stricter `goal.md` stopping rule is not yet met. Two further
campaign expansions must produce no new candidates. Rejected and merged
theories remain in the log, and unresolved model boundaries are explicit above.

## Validation

The local validation gate completed on June 7, 2026:

- variant generation and manifest verification;
- deterministic double execution of both campaign matrices;
- AddressSanitizer and UndefinedBehaviorSanitizer witnesses;
- ngspice electrical fixtures;
- complete build and 99/99 passing CTests.
