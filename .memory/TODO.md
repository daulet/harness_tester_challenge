# Active simulator board

- [x] Add deterministic ngspice process boundary and strict fixture loading.
- [x] Add first-pass board rails, logic levels, LED current, and harness R/L/C.
- [x] Add all named nominal and fault fixtures from the active goal.
- [x] Bridge firmware runtime LED state into analog stimuli.
- [ ] Audit whether additional firmware-controlled UART/I2C transitions should
  be exported from `Runtime::analog_stimulus()` in a later fidelity pass.
- [ ] Compare simplified protection/regulator abstractions against selected
  vendor curves if higher electrical fidelity is requested.
