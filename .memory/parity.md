# Simulator parity ledger

| Boundary | Current model | Reference | Status |
| --- | --- | --- | --- |
| Firmware control flow | Host-compiled unchanged `.ino` plus ASan/UBSan witnesses | Source and C++ runtime diagnostics | Strong for defined behavior; known A03/A05 UB dynamically proven |
| CY8C9560 registers | Compact hand model with active-high XRES/POR state | Infineon 38-12036 Rev. M and driver behavior | POR defaults, held-reset high-Z, firmware reset timeline, address response, and register transactions are source-backed |
| GPIO electrical state | Mode/value booleans plus scheduled button waveforms | Teensy reset/core semantics | Button timing is deterministic; no solved voltage feedback |
| UART | KiCad-routed timed 8N1 frames plus explicit direct-injection path | Teensy UART + NEO-M8 serial behavior | Direction, baud availability, and digital contention modeled; analog levels remain compact |
| I2C | Timed controller transactions, parsed-board stuck-low state, NACKs, and clock stretching | NXP UM10204 and Arduino Wire status behavior | Strong for single-controller digital sequencing; no rise time, arbitration, bus clear, or solved-voltage feedback |
| SD | In-memory append-only strings | SD/FAT behavior | Weak |
| Harness digital | Boolean graph for 40 pins | Physical harness connectivity | Topology-only |
| Harness analog | One authored R/L/C channel | 40 physical channels | Representative only |
| Board connectivity and component metadata | Custom KiCad S-expression/geometry parser | KiCad connectivity/DRC | Scoped values/footprints are structural; geometry remains approximate |
| Power/protection | Compact authored ngspice deck | Vendor curves and hardware | First-order only |
| RF/EMI/mechanical | None | Physical boundary | Out of scope until explicitly added |

Intentional deviations must be recorded here before a pass closes. A deviation
that merely makes a test easier is not acceptable.
