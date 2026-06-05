# Simulator quality loop

- Preserve the unchanged source-level firmware execution path and existing
  digital tests.
- Treat checked-in KiCad files, ngspice circuits/models, fixtures, and test logs
  as source evidence.
- New electrical behavior requires an explicit fixture and named CTest case.
- Never download models or data during configure, build, or test.
- Validate with targeted analog tests, then the full CMake/build/CTest flow.
- Do not claim hardware, RF, EMI, thermal, or production fidelity from this
  first-pass simulator.
