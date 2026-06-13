# Blocker-Peeling Simulator Goal

## Objective

Systematically repair only known reachability blockers in generated experimental
variants, then search newly reachable behavior for additional defects. Keep the
original firmware and KiCad sources unchanged.

The simulator is an evidence source, not the definition of correctness. A
simulator-backed finding is admissible only when its relevant model boundary
survives an explicit fidelity challenge.

## Architecture

### 1. Declarative Repair Catalog

Create `simulator/blocker_peeling/repair_catalog.yaml`.

Each repair records:

- A stable repair ID.
- The affected artifact and exact transformation.
- Prerequisite repairs.
- Source, specification, or physical evidence supporting the repair.
- The expected reachability change.
- Whether it modifies firmware, schematic behavior, PCB behavior, or an
  external fixture.

Keep firmware and board repairs as separate axes so observations can be
attributed to the correct source artifact.

### 2. Generated Variants

Generate variants under `build/blocker_peeling/variants/`.

Every generated variant must record:

- Base commit and source hashes.
- Applied repair IDs.
- Generated artifact hashes.
- Scenario and simulator configuration.

Do not maintain ambiguous hand-edited source copies. Generated artifacts must
identify themselves as generated and must never replace the original firmware
or KiCad files.

### 3. Independent Oracle

Build an oracle that does not reuse firmware predicates or helper logic.

The oracle must:

- Derive harness observations from connected components.
- Remove the actively driven pin from its observed component.
- Require every probe row to match its expected observation.
- Define expected button, status, logging, UART, I2C, reset, and SD workflows.
- Derive board facts from KiCad sources or an explicitly documented physical
  model instead of copying candidate bug assumptions.

## Peeling Sequence

| Peel | Repairs applied | Behavior unlocked |
| --- | --- | --- |
| P0 | None | Preserve the unchanged baseline failure |
| P1 | Expander initialization and reset | I2C access |
| P2 | I2C electrical blockers | Reliable expander communication |
| P3 | UART routing and GNSS acquisition blockers | Normal time-lock path |
| P4 | Probe direction, selected output, and 64-bit masks | Complete 40-pin observations |
| P5 | Logical-to-physical harness mapping | Correct channel attribution |
| P6 | Harness verdict aggregation | Successful and failed test completion |
| P7 | Button edge and session handling | Repeated operator workflows |
| P8 | LED GPIO and board electrical path | Observable status behavior |
| P9 | SD and logging blockers | Persistent result workflows |

Apply repairs cumulatively along the main peeling sequence. For each known or
new defect, also generate a leave-one-bug-in counterfactual variant. This
counterfactual must demonstrate that the observed behavior depends on the
specific repair rather than the cumulative patch set.

Structural or manufacturing defects that cannot be repaired meaningfully in a
runtime simulator remain separate prerequisites. Runtime variants may use a
corrected logical component only after the original physical defect has been
preserved and proven independently.

## Discovery Campaigns

Run the following campaigns at every reachable peel:

- Known-good harness behavior.
- Every individual open across all 40 channels.
- Every two-pin short across all channel pairs.
- Generated three-pin and four-pin connected components.
- Mixed open and short cases.
- Correct and incorrect harness combinations.
- Button press, release, hold, bounce, and repeated sessions.
- Valid, invalid, truncated, oversized, CRLF, `$GPRMC`, and `$GNRMC` traffic.
- UART timing, capacity, contention, and overrun boundaries.
- I2C NACK, stuck-low, reset, clock-stretch, and startup sequences.
- SD removal, capacity, partial write, reboot, and append behavior.
- LED and GPIO electrical behavior through closed-loop solved levels.
- Deterministic sanitizer and persisted fuzz-regression campaigns.

At each peel, compare firmware-observed behavior against the independent
oracle. Treat every unexplained mismatch or newly reachable anomalous state as
a bug theory.

Use delta debugging to minimize:

- The repair set needed to expose the mismatch.
- The input and event sequence.
- The harness topology.
- The simulator configuration.

## Finding Admission Pipeline

Every candidate must pass:

1. Precise source or specification contradiction.
2. A minimal causal reproduction.
3. A simulator-fidelity review for every model boundary involved.
4. A leave-one-bug-in or equivalent negative control.
5. Root-cause and duplicate analysis.
6. Multiple independent adversarial reviews.
7. Final council adjudication.

For each candidate, run at least three independent reviewer processes:

- A source and specification reviewer.
- A simulator-fidelity reviewer.
- A root-cause and duplicate reviewer.

Each reviewer must invoke Claude CLI non-interactively and instruct Claude to
focus on disproving the bug thesis. A simulator-backed rejection is valid only
if it identifies a concrete simulator gap, invalid oracle assumption,
non-causal witness, incorrect source interpretation, or duplicate root cause.
Invoke Claude directly with `claude -p --no-session-persistence`; do not create
or use Maestro sessions for discovery, review, or council adjudication.

Scrutinize every rejection. Record whether the coordinating subagent agrees or
disagrees with the rejection and why. Preserve all candidates and review
outcomes regardless of final admission.

## Required Artifacts

- `BLOCKER_PEELING_THEORY_LOG.md`: every theory and complete review history,
  including rejected and disputed candidates.
- `BLOCKER_PEELING_ACCEPTED.md`: submission-grade accepted findings.
- `simulator/blocker_peeling/repair_catalog.yaml`: declarative repairs.
- `simulator/blocker_peeling/dependency_graph.yaml`: reachability
  relationships.
- `simulator/blocker_peeling/scenario_catalog.yaml`: generated and fixed
  scenarios.
- `build/blocker_peeling/variant_manifest.jsonl`: reproducibility metadata.
- `build/blocker_peeling/exposure_matrix.csv`: repair sets, scenarios, and
  observed behavior.
- `reports/BLOCKER_PEELING_REPORT.md`: final council decisions and remaining
  fidelity limitations.

## Implementation Slices

1. Define the protocol, repair catalog, dependency graph, generator, and
   source-integrity checks.
2. Implement the independent oracle and baseline differential runner.
3. Generate firmware repair variants and counterfactual variants.
4. Add board repair overlays and closed-loop electrical feedback.
5. Run generated campaigns and minimize new failures.
6. Run independent direct-CLI Claude admission councils and produce final
   reports.

Each implementation slice must:

- Preserve the unchanged original test path.
- Add targeted positive and negative witnesses.
- Run sanitizer coverage when applicable.
- Run the complete simulator test suite.
- Receive adversarial review before being treated as complete.

## Initial Execution Scope

The first implementation slice delivers:

- The repair catalog and dependency graph.
- A deterministic variant generator and manifest.
- Source-integrity checks proving originals were not modified.
- The independent oracle interface.
- P0 through P3 generated variants.
- Baseline differential tests and counterfactual controls.

## Stopping Rule

Stop only when:

- Every peel is executable and covered by causal tests.
- All bounded harness combinations in the campaign have been evaluated.
- Every unexplained differential result has been converted into an adjudicated
  theory.
- Every accepted root cause has a minimal counterfactual witness.
- Every simulator assumption used as evidence is source-backed or explicitly
  identified as an unresolved fidelity limitation.
- Two consecutive campaign expansions with new scenarios or deterministic
  seeds produce no new candidates.
- Accepted, rejected, and disputed candidates are all present in the logs.
