# Simulator parity ledger

| Boundary | Current model | Reference | Status |
| --- | --- | --- | --- |
| Firmware control flow | Host-compiled unchanged `.ino` plus ASan/UBSan witnesses | Source and C++ runtime diagnostics | Strong for defined behavior; known A03/A05 UB dynamically proven |
| CY8C9560 registers | Compact hand model with active-high XRES/POR state | Infineon 38-12036 Rev. M and driver behavior | POR defaults, held-reset high-Z, firmware reset timeline, address response, and register transactions are source-backed |
| GPIO electrical state | Mode/value booleans plus scheduled button waveforms | Teensy reset/core semantics | Button timing is deterministic event replay; no contact-bounce generation, RC network, or solved voltage feedback |
| UART | KiCad-routed timed 8N1 frames, 63-byte Teensy Serial1 receive capacity, drop-new overruns, and explicit direct-injection path | PJRC Teensy 4.x core + NEO-M8 serial behavior | Direction, baud availability, contention, finite software buffering, and lost-byte accounting modeled; no hardware FIFO/ISR latency, flow control, or solved analog levels |
| NEO-M8 output | Standalone deterministic receiver over timed UART plus a spec-anchored atomic RMC campaign repair | NEO-M8 data sheet UBX-13003366 and protocol spec UBX-13003221 | Source-backed startup TXT, 1 Hz default NMEA set, GN/GSV talkers, fix status, checksum behavior, 384 in-process mutations, and an 842-case persisted baseline/ASan/UBSan corpus; no RF/acquisition physics or satellite state |
| I2C | Timed controller transactions plus source-derived PCB pull resistances passed to ngspice | NXP UM10204, Arduino Wire behavior, parsed R2/R3 values/nets, and Teensy 1.59 Wire source | Successful sequencing and static pull-network voltage are source-backed; stuck-low faults return immediately instead of consuming the target's timeout, and there is no rise time, arbitration, bus clear, or solved-voltage feedback into firmware |
| SD | Timed FILE_WRITE create/append, presence/reinsertion state, aggregate capacity, and deterministic all-or-zero write-call failures | Arduino SD/SdFat write semantics | Write-call returns and reinitialization direction match the target contract; deferred close/sync failure, reads, FAT geometry, wear, power-loss corruption, and measured latency remain unmodeled |
| Harness digital | Boolean graph for 40 pins | Physical harness connectivity | Topology-only |
| Harness analog | One authored R/L/C channel | 40 physical channels | Representative only |
| Board connectivity and component metadata | Custom KiCad S-expression/geometry parser plus generated I2C electrical config | KiCad connectivity/DRC | Component collections, PCB values, and pad nets drive I2C pulls; remaining electrical topology and geometry are still partial |
| Power/protection | Compact authored ngspice deck | Vendor curves and hardware | First-order only |
| RF/EMI/mechanical | None | Physical boundary | Out of scope until explicitly added |

Intentional deviations must be recorded here before a pass closes. A deviation
that merely makes a test easier is not acceptable.

Parser-only corpus cases call `process_nmea()` directly to isolate field
semantics. Ten separate loop cases cover CR/LF terminators, fragmentation,
multiple sentences, and overlong-line discard/recovery; neither path is used as
evidence for RF or physical UART error rates.
