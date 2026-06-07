#!/usr/bin/env python3

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def load_json(path):
    with path.open("r", encoding="utf-8") as source:
        return json.load(source)


def canonical_json(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def sha256_bytes(value):
    return hashlib.sha256(value).hexdigest()


def sha256_file(path):
    return sha256_bytes(path.read_bytes())


def source_commit(root, explicit):
    if explicit:
        return explicit
    result = subprocess.run(
        ["git", "-C", str(root), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def adapt_newlines(value, newline):
    return value.replace("\n", newline)


def replace_once(source, operation, newline):
    old = adapt_newlines(operation["old"], newline)
    new = adapt_newlines(operation["new"], newline)
    count = source.count(old)
    if count != 1:
        raise ValueError(
            f"replace_once expected one match, found {count}: {operation['old']!r}"
        )
    return source.replace(old, new, 1)


def replace_after(source, operation, newline):
    offset = 0
    for anchor_value in operation["anchors"]:
        anchor = adapt_newlines(anchor_value, newline)
        match = source.find(anchor, offset)
        if match < 0:
            raise ValueError(f"replace_after anchor not found: {anchor_value!r}")
        offset = match + len(anchor)

    old = adapt_newlines(operation["old"], newline)
    new = adapt_newlines(operation["new"], newline)
    scope_end = min(len(source), offset + operation["max_distance"])
    matches = []
    match = source.find(old, offset, scope_end)
    while match >= 0:
        matches.append(match)
        match = source.find(old, match + len(old), scope_end)
    if not matches:
        raise ValueError(f"replace_after target not found: {operation['old']!r}")
    if len(matches) != 1:
        raise ValueError(f"replace_after target is ambiguous: {operation['old']!r}")
    match = matches[0]
    return source[:match] + new + source[match + len(old) :]


def replace_initializer(source, operation, newline):
    anchor = adapt_newlines(operation["anchor"], newline)
    matches = []
    offset = source.find(anchor)
    while offset >= 0:
        matches.append(offset)
        offset = source.find(anchor, offset + len(anchor))
    if len(matches) != 1:
        raise ValueError(
            f"replace_initializer expected one anchor, found {len(matches)}: "
            f"{operation['anchor']!r}"
        )

    start = matches[0]
    end = source.find(adapt_newlines("};", newline), start + len(anchor))
    if end < 0:
        raise ValueError(
            f"replace_initializer terminator not found: {operation['anchor']!r}"
        )
    end += len("};")
    rows = "".join(f"  {row}," + newline for row in operation["rows"])
    replacement = anchor + newline + rows + "};"
    return source[:start] + replacement + source[end:]


def apply_repair(source, repair):
    newline = "\r\n" if "\r\n" in source else "\n"
    for operation in repair["operations"]:
        if operation["type"] == "replace_once":
            source = replace_once(source, operation, newline)
        elif operation["type"] == "replace_after":
            source = replace_after(source, operation, newline)
        elif operation["type"] == "replace_initializer":
            source = replace_initializer(source, operation, newline)
        else:
            raise ValueError(f"unknown operation type: {operation['type']}")
    return source


def write_bytes(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(value)


def validate_catalog(catalog, graph, scenarios):
    if catalog.get("schema_version") != 1:
        raise ValueError("unsupported repair catalog schema")
    if graph.get("schema_version") != 1:
        raise ValueError("unsupported dependency graph schema")
    if scenarios.get("schema_version") != 1:
        raise ValueError("unsupported scenario catalog schema")

    repairs = catalog["repairs"]
    repair_ids = [repair["id"] for repair in repairs]
    if len(repair_ids) != len(set(repair_ids)):
        raise ValueError("repair IDs must be unique")

    scenario_ids = {scenario["id"] for scenario in scenarios["scenarios"]}
    variant_ids = set()
    for variant in graph["variants"]:
        if variant["id"] in variant_ids:
            raise ValueError(f"duplicate variant ID: {variant['id']}")
        variant_ids.add(variant["id"])
        unknown_repairs = set(variant["repairs"]) - set(repair_ids)
        if unknown_repairs:
            raise ValueError(
                f"{variant['id']} references unknown repairs: "
                f"{sorted(unknown_repairs)}"
            )
        unknown_scenarios = set(variant["scenario_ids"]) - scenario_ids
        if unknown_scenarios:
            raise ValueError(
                f"{variant['id']} references unknown scenarios: "
                f"{sorted(unknown_scenarios)}"
            )


def generate(root, output, explicit_commit):
    catalog_path = root / "simulator/blocker_peeling/repair_catalog.yaml"
    graph_path = root / "simulator/blocker_peeling/dependency_graph.yaml"
    scenarios_path = root / "simulator/blocker_peeling/scenario_catalog.yaml"
    catalog = load_json(catalog_path)
    graph = load_json(graph_path)
    scenarios = load_json(scenarios_path)
    validate_catalog(catalog, graph, scenarios)

    artifact_paths = {
        key: root / relative for key, relative in catalog["sources"].items()
    }
    source_paths = dict(artifact_paths)
    source_paths.update(
        {
            key: root / relative
            for key, relative in catalog.get("immutable_sources", {}).items()
        }
    )
    source_bytes = {key: path.read_bytes() for key, path in source_paths.items()}
    source_hashes = {
        key: sha256_bytes(value) for key, value in source_bytes.items()
    }
    source_text = {
        key: source_bytes[key].decode("utf-8") for key in artifact_paths
    }
    repairs = {repair["id"]: repair for repair in catalog["repairs"]}
    scenarios_by_id = {
        scenario["id"]: scenario for scenario in scenarios["scenarios"]
    }

    output.mkdir(parents=True, exist_ok=True)
    temporary_variants = output / f".variants.tmp-{os.getpid()}"
    if temporary_variants.exists():
        shutil.rmtree(temporary_variants)
    temporary_variants.mkdir(parents=True)

    base_commit = source_commit(root, explicit_commit)
    catalog_hashes = {
        "repair_catalog": sha256_file(catalog_path),
        "dependency_graph": sha256_file(graph_path),
        "scenario_catalog": sha256_file(scenarios_path),
    }
    manifest_entries = []

    try:
        for variant in graph["variants"]:
            contents = dict(source_text)
            for repair_id in variant["repairs"]:
                repair = repairs[repair_id]
                artifact = repair["artifact"]
                contents[artifact] = apply_repair(contents[artifact], repair)

            variant_dir = temporary_variants / variant["id"]
            generated_paths = {
                artifact: variant_dir / catalog["sources"][artifact]
                for artifact in contents
            }
            artifact_hashes = {}
            for artifact, path in generated_paths.items():
                value = contents[artifact].encode("utf-8")
                write_bytes(path, value)
                artifact_hashes[artifact] = sha256_bytes(value)

            metadata = {
                "schema_version": 1,
                "generated": True,
                "generator": "simulator/blocker_peeling/generate_variants.py",
                "variant_id": variant["id"],
                "base_commit": base_commit,
                "repairs": variant["repairs"],
                "counterfactual_for": variant.get("counterfactual_for"),
                "source_sha256": source_hashes,
                "artifact_sha256": artifact_hashes,
                "catalog_sha256": catalog_hashes,
                "scenario_ids": variant["scenario_ids"],
                "simulator_config": [
                    scenarios_by_id[scenario_id]
                    for scenario_id in variant["scenario_ids"]
                ],
            }
            metadata_bytes = (
                json.dumps(metadata, indent=2, sort_keys=True) + "\n"
            ).encode("utf-8")
            metadata_path = variant_dir / "variant.generated.json"
            write_bytes(metadata_path, metadata_bytes)
            manifest_entry = dict(metadata)
            manifest_entry["metadata_sha256"] = sha256_bytes(metadata_bytes)
            manifest_entries.append(manifest_entry)

        variants_path = output / "variants"
        if variants_path.exists():
            shutil.rmtree(variants_path)
        temporary_variants.rename(variants_path)

        manifest_bytes = "".join(
            canonical_json(entry) + "\n" for entry in manifest_entries
        ).encode("utf-8")
        manifest_temp = output / f".variant_manifest.tmp-{os.getpid()}"
        write_bytes(manifest_temp, manifest_bytes)
        manifest_temp.replace(output / "variant_manifest.jsonl")
        (output / ".generated.stamp").write_text(
            sha256_bytes(manifest_bytes) + "\n", encoding="ascii"
        )
    finally:
        if temporary_variants.exists():
            shutil.rmtree(temporary_variants)

    after_hashes = {
        key: sha256_file(path) for key, path in source_paths.items()
    }
    if source_hashes != after_hashes:
        raise RuntimeError("source integrity failure: generator modified an input")


def verify(root, output):
    catalog = load_json(root / "simulator/blocker_peeling/repair_catalog.yaml")
    manifest_path = output / "variant_manifest.jsonl"
    if not manifest_path.exists():
        raise ValueError("variant manifest does not exist")
    entries = [
        json.loads(line)
        for line in manifest_path.read_text(encoding="utf-8").splitlines()
        if line
    ]
    if not entries:
        raise ValueError("variant manifest is empty")

    for entry in entries:
        variant_dir = output / "variants" / entry["variant_id"]
        paths = {
            artifact: variant_dir / relative
            for artifact, relative in catalog["sources"].items()
        }
        for artifact, path in paths.items():
            actual = sha256_file(path)
            expected = entry["artifact_sha256"][artifact]
            if actual != expected:
                raise ValueError(
                    f"{entry['variant_id']} {artifact} hash mismatch"
                )
        metadata_path = variant_dir / "variant.generated.json"
        if sha256_file(metadata_path) != entry["metadata_sha256"]:
            raise ValueError(f"{entry['variant_id']} metadata hash mismatch")

    source_paths = {
        key: root / relative for key, relative in catalog["sources"].items()
    }
    source_paths.update(
        {
            key: root / relative
            for key, relative in catalog.get("immutable_sources", {}).items()
        }
    )
    expected_sources = entries[0]["source_sha256"]
    for key, path in source_paths.items():
        if sha256_file(path) != expected_sources[key]:
            raise ValueError(f"source hash changed for {key}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--base-commit")
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()

    try:
        if args.verify:
            verify(args.root.resolve(), args.output.resolve())
        else:
            generate(
                args.root.resolve(),
                args.output.resolve(),
                args.base_commit,
            )
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"blocker-peeling generator: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
