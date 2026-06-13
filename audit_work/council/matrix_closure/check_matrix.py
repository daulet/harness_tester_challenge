#!/usr/bin/env python3
"""Check whether the firmware's expected edge rows match measurable continuity."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "firmware" / "firmware.ino"


def parse_rows() -> list[int]:
    text = SOURCE.read_text(encoding="utf-8")
    block = re.search(
        r"EXPECTED_CONNECTIONS\[NUM_HARNESS_PINS\]\s*=\s*\{(.*?)\};",
        text,
        re.DOTALL,
    )
    if block is None:
        raise RuntimeError("EXPECTED_CONNECTIONS initializer not found")
    return [int(bits, 2) for bits in re.findall(r"0b([01]+)", block.group(1))]


def component(start: int, rows: list[int]) -> set[int]:
    seen = {start}
    pending = [start]
    while pending:
        current = pending.pop()
        neighbors = {bit for bit in range(len(rows)) if rows[current] & (1 << bit)}
        for neighbor in neighbors - seen:
            seen.add(neighbor)
            pending.append(neighbor)
    return seen


def main() -> None:
    rows = parse_rows()
    assert len(rows) == 40, len(rows)

    asymmetric = []
    closure_mismatches = []
    components: set[tuple[int, ...]] = set()
    for pin, row in enumerate(rows):
        for other in range(len(rows)):
            if bool(row & (1 << other)) != bool(rows[other] & (1 << pin)):
                asymmetric.append((pin, other))

        connected = component(pin, rows)
        components.add(tuple(sorted(connected)))
        observed = sum(1 << other for other in connected if other != pin)
        if observed != row:
            closure_mismatches.append((pin, row, observed, tuple(sorted(connected))))

    print(f"rows={len(rows)}")
    print(f"asymmetric_edges={len(asymmetric)}")
    print(f"connected_components={len(components)}")
    print(f"rows_not_equal_to_electrical_closure={len(closure_mismatches)}")
    for pin, expected, observed, connected in closure_mismatches:
        print(
            f"pin={pin:02d} component={connected} "
            f"table=0x{expected:010x} observable=0x{observed:010x}"
        )


if __name__ == "__main__":
    main()
