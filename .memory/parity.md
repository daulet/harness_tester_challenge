# Simulator parity ledger

| Boundary | Current model | Reference | Status |
| --- | --- | --- | --- |
| Firmware control flow | Host-compiled unchanged `.ino` | Source | Strong for defined C++ behavior |
| CY8C9560 registers | Compact hand model | Driver usage and selected datasheet facts | Known wrong direction POR |
| GPIO electrical state | Mode/value booleans | Teensy reset/core semantics | Partial; no solved voltage feedback |
| UART | Immediate RX byte queue, nominal TX voltage | Teensy UART + NEO-M8 serial behavior | Weak |
| I2C | Atomic API transaction | Open-drain timed bus | Weak |
| SD | In-memory append-only strings | SD/FAT behavior | Weak |
| Harness digital | Boolean graph for 40 pins | Physical harness connectivity | Topology-only |
| Harness analog | One authored R/L/C channel | 40 physical channels | Representative only |
| Board connectivity | Custom KiCad S-expression/geometry parser | KiCad connectivity/DRC | Useful, approximate |
| Power/protection | Compact authored ngspice deck | Vendor curves and hardware | First-order only |
| RF/EMI/mechanical | None | Physical boundary | Out of scope until explicitly added |

Intentional deviations must be recorded here before a pass closes. A deviation
that merely makes a test easier is not acceptable.
