# Simulator realism lessons

## 2026-06-05 - A passing witness can be causally empty

- Symptom: `bug_a06_all_outputs` passed even if `set_output()` did nothing.
- Root cause: the simulator reset direction was already `0x00`, equal to the
  asserted post-state and opposite the real input/high-impedance default.
- Source evidence: `Runtime::reset_expander_state()` and the A06 review in
  `SIMULATOR_20_ISSUES.md`.
- Permanent rule: every state-transition witness asserts a different pre-state
  and, where practical, a negative control or mutation that removes the action.

## 2026-06-05 - Parallel model layers hide emergent failures

- Symptom: firmware tests, board topology tests, and ngspice fixtures each prove
  isolated facts but cannot expose their combined behavior.
- Root cause: `Runtime::analog_stimulus()` exports only LED booleans, released
  I2C lines, and a nominal UART voltage; solved electrical levels never feed back
  into firmware reads.
- Source evidence: `simulator/src/runtime.cpp` and
  `simulator/include/host_simulator/analog.h`.
- Permanent rule: board facts are generated from KiCad, firmware produces pin
  drives, the electrical solver resolves nets, and resolved levels return to the
  firmware runtime.

## 2026-06-05 - Host execution is not target execution

- Symptom: host behavior can corroborate invalid shifts but cannot define their
  target result.
- Root cause: undefined behavior and host/target integer or core differences.
- Source evidence: A05 and the host-only CMake targets.
- Permanent rule: use sanitizers for language UB and a Teensy-target compile or
  trace for target-specific claims.
