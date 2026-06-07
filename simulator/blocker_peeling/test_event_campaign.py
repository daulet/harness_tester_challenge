#!/usr/bin/env python3

import argparse
import csv
import hashlib
from pathlib import Path
import subprocess
import tempfile


EXPECTED_ROWS_PER_VARIANT = 32
EXPECTED_COUNTERFACTUAL_DIFFERENTIALS = 9
KNOWN_CLASSIFICATIONS = {
    "A04_RMC_VALIDATION",
    "BP_M002_ATOMICITY",
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_campaign(executable, output):
    result = subprocess.run(
        [str(executable), str(output)],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"{executable.name} failed\n{result.stdout}{result.stderr}"
        )
    return result.stdout


def read_matrix(path):
    with path.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        rows = list(reader)
        if reader.fieldnames is None:
            raise RuntimeError(f"{path} has no CSV header")
        return reader.fieldnames, rows


def verify_rows(rows, repaired):
    if len(rows) != EXPECTED_ROWS_PER_VARIANT:
        raise RuntimeError(
            f"expected {EXPECTED_ROWS_PER_VARIANT} rows, found {len(rows)}"
        )
    for row in rows:
        if row["error"]:
            raise RuntimeError(
                f"{row['variant']} {row['scenario_id']}: {row['error']}"
            )
        if row["classification"] == "UNEXPLAINED":
            raise RuntimeError(
                f"{row['variant']} {row['scenario_id']} is unexplained"
            )

    differentials = [row for row in rows if row["differential"] == "1"]
    if repaired:
        if differentials:
            raise RuntimeError(
                "repaired event campaign retained a differential: "
                + ", ".join(row["scenario_id"] for row in differentials)
            )
        return

    if len(differentials) != EXPECTED_COUNTERFACTUAL_DIFFERENTIALS:
        raise RuntimeError(
            "counterfactual differential count changed: "
            f"expected {EXPECTED_COUNTERFACTUAL_DIFFERENTIALS}, "
            f"found {len(differentials)}"
        )
    unexpected = [
        row
        for row in differentials
        if row["classification"] not in KNOWN_CLASSIFICATIONS
    ]
    if unexpected:
        raise RuntimeError(
            "counterfactual produced an off-allowlist differential: "
            + ", ".join(row["scenario_id"] for row in unexpected)
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--repaired", required=True, type=Path)
    parser.add_argument("--counterfactual", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
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
        matrices = {}
        logs = {}
        for name, executable in (
            ("repaired", args.repaired.resolve()),
            ("counterfactual", args.counterfactual.resolve()),
        ):
            first = temporary / f"{name}-first.csv"
            second = temporary / f"{name}-second.csv"
            logs[name] = run_campaign(executable, first)
            run_campaign(executable, second)
            if first.read_bytes() != second.read_bytes():
                raise RuntimeError(
                    f"{name} campaign was not byte-for-byte deterministic"
                )
            matrices[name] = read_matrix(first)

        repaired_header, repaired_rows = matrices["repaired"]
        counterfactual_header, counterfactual_rows = matrices[
            "counterfactual"
        ]
        if repaired_header != counterfactual_header:
            raise RuntimeError("campaign variants emitted different CSV schemas")
        verify_rows(repaired_rows, repaired=True)
        verify_rows(counterfactual_rows, repaired=False)

        args.output.parent.mkdir(parents=True, exist_ok=True)
        temporary_output = args.output.with_suffix(args.output.suffix + ".tmp")
        with temporary_output.open("w", newline="", encoding="utf-8") as sink:
            writer = csv.DictWriter(sink, fieldnames=repaired_header)
            writer.writeheader()
            writer.writerows(repaired_rows)
            writer.writerows(counterfactual_rows)
        temporary_output.replace(args.output)

    after = {path: digest(path) for path in source_paths}
    if before != after:
        raise RuntimeError("event campaign changed an original source")

    print(logs["repaired"].strip())
    print(logs["counterfactual"].strip())
    print(
        "combined_rows="
        f"{len(repaired_rows) + len(counterfactual_rows)} "
        f"known_counterfactual_differentials="
        f"{EXPECTED_COUNTERFACTUAL_DIFFERENTIALS} "
        f"output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
