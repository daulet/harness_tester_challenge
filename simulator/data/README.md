# Legacy reference data

The runtime board model no longer reads these CSVs. It parses
`kicad_files/hardware_challenge.kicad_pcb` and
`kicad_files/hardware_challenge.kicad_sch` directly, with PCB connectivity used
for simulation and schematic connectivity retained as comparison evidence.

The CSVs remain only as legacy reference material for the original challenge
write-up. They are not authoritative runtime inputs.
