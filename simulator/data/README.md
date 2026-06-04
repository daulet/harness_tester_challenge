# Board data provenance

The simulator intentionally reads checked-in board data instead of attempting to
parse KiCad files at test time.

`schematic_harness_map.csv` is the schematic view of the harness path: J3
`Pin_1` through `Pin_40` are the `CBL_0` through `CBL_39` nets, and those nets
land on the populated U4 GPort bits listed in the CSV rather than a contiguous
logical bit range. `schematic_io_map.csv` is the corresponding firmware-visible
net map for U2, U3, and U4.

`pcb_fault_overlay.csv` is deliberately separate because the KiCad PCB netlist
still claims the schematic connectivity. The physical copper at J3 does not:
the routed board reaches J3 pad 1 with `CBL_0`, and reaches J3 pad 2 with both
`CBL_1` and `CBL_2`; no routed copper reaches pads 3 through 40. Loading the
overlay replaces the schematic J3 attachments with that physical attachment
set.
