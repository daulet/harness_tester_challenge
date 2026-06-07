#!/usr/bin/env python3

import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def tree_digest(root):
    digest = hashlib.sha256()
    for path in sorted(path for path in root.rglob("*") if path.is_file()):
        digest.update(path.relative_to(root).as_posix().encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def file_digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, expect_success=True):
    result = subprocess.run(command, capture_output=True, text=True)
    if expect_success and result.returncode != 0:
        raise RuntimeError(result.stderr or result.stdout)
    if not expect_success and result.returncode == 0:
        raise RuntimeError("expected generator failure")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    generator = root / "simulator/blocker_peeling/generate_variants.py"
    source_paths = [
        root / "simulator/blocker_peeling/board_electrical_overlay.fixture",
        root / "firmware/firmware.ino",
        root / "firmware/CY8C9560.cpp",
        root / "firmware/CY8C9560.h",
        root / "kicad_files/hardware_challenge.kicad_pcb",
        root / "kicad_files/hardware_challenge.kicad_sch",
    ]
    before = {path: file_digest(path) for path in source_paths}

    with tempfile.TemporaryDirectory() as temporary:
        temporary = Path(temporary)
        first = temporary / "first"
        second = temporary / "second"
        command = [
            sys.executable,
            str(generator),
            "--root",
            str(root),
            "--base-commit",
            "TEST-COMMIT",
        ]
        run(command + ["--output", str(first)])
        run(command + ["--output", str(second)])
        if tree_digest(first) != tree_digest(second):
            raise RuntimeError("two identical generations were not deterministic")
        run(
            [
                sys.executable,
                str(generator),
                "--root",
                str(root),
                "--output",
                str(first),
                "--verify",
            ]
        )

        broken_root = temporary / "broken-root"
        for relative in [
            "simulator/blocker_peeling/board_electrical_overlay.fixture",
            "firmware/firmware.ino",
            "firmware/CY8C9560.cpp",
            "firmware/CY8C9560.h",
            "kicad_files/hardware_challenge.kicad_pcb",
            "kicad_files/hardware_challenge.kicad_sch",
            "simulator/blocker_peeling/repair_catalog.yaml",
            "simulator/blocker_peeling/dependency_graph.yaml",
            "simulator/blocker_peeling/scenario_catalog.yaml",
        ]:
            source = root / relative
            target = broken_root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        firmware = broken_root / "firmware/firmware.ino"
        firmware.write_text(
            firmware.read_text(encoding="utf-8").replace(
                "  // SD card\n", "  // storage\n", 1
            ),
            encoding="utf-8",
        )
        failure = run(
            [
                sys.executable,
                str(generator),
                "--root",
                str(broken_root),
                "--output",
                str(temporary / "broken-output"),
                "--base-commit",
                "TEST-COMMIT",
            ],
            expect_success=False,
        )
        if "expected one match" not in failure.stderr:
            raise RuntimeError("generator failed without an exact-match diagnostic")

    after = {path: file_digest(path) for path in source_paths}
    if before != after:
        raise RuntimeError("generator integrity test changed original sources")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
