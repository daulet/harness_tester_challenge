# Simulator realism quality loop

## Mission

Increase simulator fidelity until firmware, board topology, and electrical
behavior form one source-backed closed loop capable of exposing new design and
firmware failures. Preserve the unchanged firmware path as the primary subject;
variants may isolate downstream behavior but may not silently become the oracle.

## Active-board rule

- Work the highest-leverage open item in `.memory/TODO.md` whose prerequisites
  are complete.
- Each implementation pass must retire one complete vertical slice. Inventory,
  wrappers, or reviewer discussion alone do not close an implementation pass.
- New work discovered outside the active slice is added to the board rather
  than folded into the current diff.

## Evidence rules

- Checked-in firmware, KiCad sources, component documentation, generated tool
  output, simulator traces, and test logs are evidence. Names and fixtures alone
  are not.
- Every witness must prove causality: assert the pre-state, apply the action or
  fault, and assert the changed state. Mutation checks are preferred where a
  witness could otherwise pass without the behavior under test.
- KiCad-derived configuration owns board facts. Hand-authored fixture values may
  express external conditions and tolerances, but must not duplicate board
  topology or component values when those can be parsed.
- Electrical consequences must state whether they come from topology, a compact
  authored model, a vendor model/curve, or physical hardware.
- Never download models or data during configure, build, or test.

## Reviewer policy

- Form a source-backed design before reviewer consultation.
- High-risk design and parity-sensitive passes receive one pre-implementation
  Claude critique using `ultrathink` in a fresh process.
- Every substantial pass receives one adversarial Claude pass-end review in a
  fresh process with only the goal, diff, validation, and relevant evidence.
- Source decides disagreements. Material findings are fixed and rereviewed
  before commit.

## Validation ladder

1. `clang-format --dry-run --Werror` on changed C/C++ files.
2. Targeted CMake build and named CTest cases.
3. Sanitizer build/tests for firmware and runtime changes once the Q1 sanitizer
   targets exist; creating those targets is part of Q1.
4. Full `cmake --build build` and `ctest --test-dir build --output-on-failure`.
5. Boundary tools when relevant: ngspice logs, `kicad-cli` ERC/DRC, Teensy
   cross-compile, or hardware traces.
6. Read logs and verify the intended path ran.

## Commit policy

- Commit each completed pass separately with a conventional commit message.
- Stage only the pass files. The untracked local `build/` directory is never
  committed.
- A pass is committed only after validation and required reviewer closure.
- Do not push without an explicit request.

## Incident stops

- Stop pass closure on a fresh full-suite regression, sanitizer finding in the
  simulator itself, ngspice convergence failure, generated-topology mismatch,
  or authoritative-tool disagreement that affects the active claim.
- Continue read-only diagnosis and local reproduction while an incident is open.

## Quality rules

- C++17, ASCII, narrow APIs, explicit ownership, deterministic tests, and no
  hidden network dependency.
- Prefer replacing duplicated fixture truth with generated source truth over
  adding compatibility layers.
- Do not claim RF, EMI, exact silicon, thermal, mechanical, manufacturing, or
  production fidelity until the corresponding boundary has direct validation.

## Stop condition

Continue until `.memory/TODO.md` has no open legal work, the user redirects, or
an incident stop requires external hardware/tool access that is unavailable.
