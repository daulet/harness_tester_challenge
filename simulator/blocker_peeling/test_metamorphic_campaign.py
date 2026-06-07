#!/usr/bin/env python3

import argparse
import csv
import hashlib
from pathlib import Path
import subprocess
import tempfile


EXPECTED_PAIRS = 384
EXPECTED_CONTROLS = 6
EXPECTED_CATEGORIES = {
    "nmea_fragmentation": 64,
    "parser_neutral_insertion": 64,
    "button_idle_insertion": 64,
    "sd_latency": 64,
    "i2c_clock_stretch": 64,
    "uart_poll_cadence": 64,
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_campaign(executable, output, seed):
    result = subprocess.run(
        [str(executable), str(output), str(seed)],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"metamorphic campaign failed\n{result.stdout}{result.stderr}"
        )
    return result.stdout


def read_rows(path):
    with path.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        rows = list(reader)
        if reader.fieldnames is None:
            raise RuntimeError("metamorphic matrix has no header")
        return reader.fieldnames, rows


def verify_rows(rows, seed):
    if len(rows) != EXPECTED_PAIRS + EXPECTED_CONTROLS:
        raise RuntimeError(
            "expected "
            f"{EXPECTED_PAIRS} metamorphic pairs and {EXPECTED_CONTROLS} controls, "
            f"found {len(rows)} rows"
        )
    categories = {}
    controls = 0
    pair_ids = set()
    for row in rows:
        if row["seed"] != str(seed):
            raise RuntimeError(f"{row['pair_id']} reported seed {row['seed']}")
        if row["pair_id"] in pair_ids:
            raise RuntimeError(f"duplicate pair ID: {row['pair_id']}")
        pair_ids.add(row["pair_id"])
        if row["admission_eligible"] != "0":
            raise RuntimeError(f"{row['pair_id']} unexpectedly became admissible")
        if row["classification"] == "CONTROL_DETECTED":
            controls += 1
            if row["differential"] != "1" or row["error"]:
                raise RuntimeError(
                    f"{row['pair_id']} did not detect its expected differential"
                )
            continue
        categories[row["category"]] = categories.get(row["category"], 0) + 1
        if row["differential"] != "0" or row["classification"] != "NONE":
            raise RuntimeError(
                f"{row['pair_id']} produced an unexplained differential"
            )
        if row["error"]:
            raise RuntimeError(f"{row['pair_id']}: {row['error']}")
    if controls != EXPECTED_CONTROLS:
        raise RuntimeError(
            f"expected {EXPECTED_CONTROLS} detected controls, found {controls}"
        )
    if categories != EXPECTED_CATEGORIES:
        raise RuntimeError(f"metamorphic category counts changed: {categories}")


def descriptor_digest(rows):
    descriptors = [
        "\0".join(
            (
                row["pair_id"],
                row["category"],
                row["transformation"],
                row["projection"],
            )
        )
        for row in rows
        if row["classification"] == "NONE"
    ]
    return hashlib.sha256("\n".join(descriptors).encode()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--seed", required=True, type=int)
    parser.add_argument("--different-seed", type=int)
    args = parser.parse_args()

    root = args.root.resolve()
    source_paths = [
        root / "firmware/firmware.ino",
        root / "firmware/CY8C9560.cpp",
        root / "firmware/CY8C9560.h",
        root / "kicad_files/hardware_challenge.kicad_pcb",
        root / "kicad_files/hardware_challenge.kicad_sch",
    ]
    before = {path: digest(path) for path in source_paths}

    with tempfile.TemporaryDirectory() as temporary:
        temporary = Path(temporary)
        first = temporary / "first.csv"
        second = temporary / "second.csv"
        first_log = run_campaign(args.executable.resolve(), first, args.seed)
        second_log = run_campaign(args.executable.resolve(), second, args.seed)
        if first.read_bytes() != second.read_bytes() or first_log != second_log:
            raise RuntimeError("metamorphic campaign was not byte-for-byte deterministic")
        if f"seed={args.seed} " not in first_log:
            raise RuntimeError("metamorphic campaign reported an unexpected seed")
        if f"controls={EXPECTED_CONTROLS} " not in first_log:
            raise RuntimeError("metamorphic campaign reported an unexpected control count")
        fieldnames, rows = read_rows(first)
        verify_rows(rows, args.seed)
        digest_value = descriptor_digest(rows)

        if args.different_seed is not None:
            different = temporary / "different.csv"
            run_campaign(
                args.executable.resolve(),
                different,
                args.different_seed,
            )
            _, different_rows = read_rows(different)
            verify_rows(different_rows, args.different_seed)
            if digest_value == descriptor_digest(different_rows):
                raise RuntimeError(
                    "distinct seeds generated identical case descriptors"
                )

        args.output.parent.mkdir(parents=True, exist_ok=True)
        temporary_output = args.output.with_suffix(args.output.suffix + ".tmp")
        with temporary_output.open("w", newline="", encoding="utf-8") as sink:
            writer = csv.DictWriter(sink, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)
        temporary_output.replace(args.output)

    after = {path: digest(path) for path in source_paths}
    if before != after:
        raise RuntimeError("metamorphic campaign changed an original source")
    print(first_log.strip())
    print(f"descriptor_sha256={digest_value}")
    print(f"output={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
