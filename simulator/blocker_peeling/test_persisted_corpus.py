#!/usr/bin/env python3

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


PLAN_HEADER = (
    "id\tcategory\ttransport\tprelude_hex\tchunks_hex\t"
    "expected_locked\texpected_time\texpected_date\n"
)
RESULT_HEADER = "id\tobserved_locked\tobserved_time\tobserved_date\n"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_corpus(path):
    with path.open("r", encoding="utf-8") as source:
        document = json.load(source)
    if document.get("schema_version") != 1:
        raise RuntimeError("unsupported persisted corpus schema")
    cases = document.get("cases")
    if not isinstance(cases, list) or document.get("case_count") != len(cases):
        raise RuntimeError("persisted corpus count does not match cases")
    ids = [case["id"] for case in cases]
    if len(ids) != len(set(ids)):
        raise RuntimeError("persisted corpus case IDs are not unique")
    return document, cases


def write_plan(path, cases):
    with path.open("w", encoding="utf-8", newline="") as sink:
        sink.write(PLAN_HEADER)
        for case in cases:
            expected = case["expected"]
            chunks = ",".join(case["chunks_hex"])
            fields = [
                case["id"],
                case["category"],
                case["transport"],
                case["prelude_hex"],
                chunks,
                "1" if expected["locked"] else "0",
                expected["time"],
                expected["date"],
            ]
            if any("\t" in field or "\n" in field for field in fields):
                raise RuntimeError(f"{case['id']} contains an unsafe plan field")
            sink.write("\t".join(fields) + "\n")


def runner_environment(label):
    environment = dict(os.environ)
    if label == "asan":
        environment["ASAN_OPTIONS"] = "halt_on_error=1:abort_on_error=1:detect_leaks=0"
    if label == "ubsan":
        environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=0"
    return environment


def run_runner(label, executable, plan):
    result = subprocess.run(
        [str(executable), str(plan)],
        capture_output=True,
        text=True,
        env=runner_environment(label),
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"{label} corpus runner failed\n{result.stdout}{result.stderr}"
        )
    lines = result.stdout.splitlines()
    if not lines or lines[0] + "\n" != RESULT_HEADER:
        raise RuntimeError(f"{label} corpus runner emitted an invalid header")
    observations = {}
    for line in lines[1:]:
        fields = line.split("\t")
        if len(fields) != 4:
            raise RuntimeError(f"{label} corpus runner emitted a malformed row")
        case_id, locked, observed_time, observed_date = fields
        if case_id in observations:
            raise RuntimeError(f"{label} corpus runner repeated case ID {case_id}")
        observations[case_id] = {
            "locked": locked == "1",
            "time": observed_time,
            "date": observed_date,
        }
    return result.stdout, observations


def verify_observations(label, cases, observations):
    expected_ids = {case["id"] for case in cases}
    if set(observations) != expected_ids:
        missing = sorted(expected_ids - set(observations))
        extra = sorted(set(observations) - expected_ids)
        raise RuntimeError(f"{label} corpus IDs changed: missing={missing} extra={extra}")
    rows = []
    for case in cases:
        expected = case["expected"]
        observed = observations[case["id"]]
        differential = observed != expected
        rows.append(
            {
                "runner": label,
                "case_id": case["id"],
                "category": case["category"],
                "mutation": case["mutation"],
                "transport": case["transport"],
                "expected": json.dumps(expected, sort_keys=True, separators=(",", ":")),
                "observed": json.dumps(observed, sort_keys=True, separators=(",", ":")),
                "differential": "1" if differential else "0",
                "classification": "UNEXPLAINED" if differential else "NONE",
                "admission_eligible": "0",
                "error": "corpus expectation mismatch" if differential else "",
            }
        )
    differentials = [row for row in rows if row["differential"] == "1"]
    if differentials:
        raise RuntimeError(
            f"{label} corpus produced unexplained differentials: "
            + ", ".join(row["case_id"] for row in differentials[:20])
        )
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--corpus", required=True, type=Path)
    parser.add_argument("--generator", required=True, type=Path)
    parser.add_argument("--runner-label", required=True)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    root = args.root.resolve()
    corpus_path = args.corpus.resolve()
    document, cases = load_corpus(corpus_path)
    source_paths = [
        root / "firmware/firmware.ino",
        root / "firmware/CY8C9560.cpp",
        root / "firmware/CY8C9560.h",
        root / "kicad_files/hardware_challenge.kicad_pcb",
        root / "kicad_files/hardware_challenge.kicad_sch",
        corpus_path,
    ]
    before = {path: digest(path) for path in source_paths}

    with tempfile.TemporaryDirectory() as temporary:
        temporary = Path(temporary)
        regenerated = temporary / "persisted_corpus.json"
        subprocess.run(
            [
                sys.executable,
                str(args.generator.resolve()),
                "--output",
                str(regenerated),
            ],
            check=True,
        )
        if regenerated.read_bytes() != corpus_path.read_bytes():
            raise RuntimeError("persisted corpus differs from deterministic regeneration")
        plan = temporary / "corpus.tsv"
        write_plan(plan, cases)
        first_output, first = run_runner(
            args.runner_label, args.runner.resolve(), plan
        )
        second_output, second = run_runner(
            args.runner_label, args.runner.resolve(), plan
        )
        if first_output != second_output:
            raise RuntimeError(f"{args.runner_label} corpus replay was not deterministic")
        rows = verify_observations(args.runner_label, cases, first)

    after = {path: digest(path) for path in source_paths}
    if before != after:
        raise RuntimeError("persisted corpus campaign changed an evidence input")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary_output = args.output.with_suffix(args.output.suffix + ".tmp")
    fieldnames = [
        "runner",
        "case_id",
        "category",
        "mutation",
        "transport",
        "expected",
        "observed",
        "differential",
        "classification",
        "admission_eligible",
        "error",
    ]
    with temporary_output.open("w", newline="", encoding="utf-8") as sink:
        writer = csv.DictWriter(sink, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    temporary_output.replace(args.output)
    print(
        f"runner={args.runner_label} seed={document['seed']} "
        f"cases={len(cases)} differentials=0 output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
