#!/usr/bin/env python3

import argparse
import calendar
import json
from pathlib import Path
import random


SEED = 0x43525033
VALID_CASES = 128
INVALID_REPETITIONS = 32
POST_LOCK_CASES = 128


def nmea_sentence(body, corrupt=False, lowercase=False, ending=b"\r\n"):
    checksum = 0
    for value in body.encode("ascii"):
        checksum ^= value
    if corrupt:
        checksum ^= 0xFF
    checksum_text = f"{checksum:02x}" if lowercase else f"{checksum:02X}"
    return b"$" + body.encode("ascii") + b"*" + checksum_text.encode("ascii") + ending


def valid_fields(random_source):
    year = random_source.randrange(100)
    month = random_source.randrange(1, 13)
    day = random_source.randrange(1, calendar.monthrange(2000 + year, month)[1] + 1)
    hour = random_source.randrange(24)
    minute = random_source.randrange(60)
    second = random_source.randrange(61)
    fraction_digits = random_source.randrange(4)
    fraction = ""
    if fraction_digits:
        fraction = "." + "".join(
            str(random_source.randrange(10)) for _ in range(fraction_digits)
        )
    return {
        "talker": random_source.choice(["GN", "GP"]),
        "time": f"{hour:02d}{minute:02d}{second:02d}{fraction}",
        "status": "A",
        "latitude": "4807.03800",
        "latitude_hemisphere": "N",
        "longitude": "01131.00000",
        "longitude_hemisphere": "E",
        "speed": "0.004",
        "course": "77.52",
        "date": f"{day:02d}{month:02d}{year:02d}",
        "tail": ",,A,V",
    }


def rmc_body(fields):
    return (
        f"{fields['talker']}RMC,{fields['time']},{fields['status']},"
        f"{fields['latitude']},{fields['latitude_hemisphere']},"
        f"{fields['longitude']},{fields['longitude_hemisphere']},"
        f"{fields['speed']},{fields['course']},{fields['date']},"
        f"{fields['tail']}"
    )


def record(case_id, category, mutation, chunks, expected, prelude=b"", transport="direct"):
    return {
        "id": case_id,
        "category": category,
        "mutation": mutation,
        "transport": transport,
        "prelude_hex": prelude.hex(),
        "chunks_hex": [chunk.hex() for chunk in chunks],
        "expected": expected,
    }


def rejected(case_id, mutation, payload, prelude=b""):
    if prelude:
        expected = {"locked": True, "time": "123519.00", "date": "230394"}
        category = "post_lock_atomicity"
    else:
        expected = {"locked": False, "time": "", "date": ""}
        category = "invalid_rmc"
    return record(case_id, category, mutation, [payload], expected, prelude=prelude)


def invalid_payload(fields, mutation):
    changed = dict(fields)
    if mutation == "void_status":
        changed["status"] = "V"
        return nmea_sentence(rmc_body(changed))
    if mutation == "checksum_corrupt":
        return nmea_sentence(rmc_body(changed), corrupt=True)
    if mutation == "hour_24":
        changed["time"] = "24" + changed["time"][2:]
        return nmea_sentence(rmc_body(changed))
    if mutation == "minute_60":
        changed["time"] = changed["time"][:2] + "60" + changed["time"][4:]
        return nmea_sentence(rmc_body(changed))
    if mutation == "second_61":
        changed["time"] = changed["time"][:4] + "61" + changed["time"][6:]
        return nmea_sentence(rmc_body(changed))
    if mutation == "date_day_zero":
        changed["date"] = "00" + changed["date"][2:]
        return nmea_sentence(rmc_body(changed))
    if mutation == "date_day_too_high":
        changed["date"] = "32" + changed["date"][2:]
        return nmea_sentence(rmc_body(changed))
    if mutation == "date_month_zero":
        changed["date"] = changed["date"][:2] + "00" + changed["date"][4:]
        return nmea_sentence(rmc_body(changed))
    if mutation == "date_month_13":
        changed["date"] = changed["date"][:2] + "13" + changed["date"][4:]
        return nmea_sentence(rmc_body(changed))
    if mutation == "time_nondigit":
        changed["time"] = changed["time"][:1] + "X" + changed["time"][2:]
        return nmea_sentence(rmc_body(changed))
    if mutation == "date_nondigit":
        changed["date"] = changed["date"][:2] + "X" + changed["date"][3:]
        return nmea_sentence(rmc_body(changed))
    if mutation == "missing_checksum":
        return b"$" + rmc_body(changed).encode("ascii") + b"\r\n"
    if mutation == "nonhex_checksum":
        sentence = nmea_sentence(rmc_body(changed))
        return sentence[:-4] + b"GG\r\n"
    if mutation == "trailing_checksum_junk":
        sentence = nmea_sentence(rmc_body(changed))
        return sentence[:-2] + b"X\r\n"
    if mutation == "truncated_after_status":
        return nmea_sentence(
            f"{changed['talker']}RMC,{changed['time']},{changed['status']},"
        )
    if mutation == "date_suffix":
        changed["date"] += "X"
        return nmea_sentence(rmc_body(changed))
    if mutation == "missing_latitude":
        changed["latitude"] = ""
        return nmea_sentence(rmc_body(changed))
    if mutation == "embedded_nul":
        payload = nmea_sentence(rmc_body(changed))
        return payload[:12] + b"\0" + payload[13:]
    raise ValueError(f"unknown mutation: {mutation}")


def generate_cases():
    random_source = random.Random(SEED)
    cases = []

    for index in range(VALID_CASES):
        fields = valid_fields(random_source)
        payload = nmea_sentence(
            rmc_body(fields),
            lowercase=index % 3 == 0,
        )
        cases.append(
            record(
                f"valid-{index:03d}",
                "valid_rmc",
                "generated_valid",
                [payload],
                {
                    "locked": True,
                    "time": fields["time"],
                    "date": fields["date"],
                },
            )
        )

    mutations = [
        "void_status",
        "checksum_corrupt",
        "hour_24",
        "minute_60",
        "second_61",
        "date_day_zero",
        "date_day_too_high",
        "date_month_zero",
        "date_month_13",
        "time_nondigit",
        "date_nondigit",
        "missing_checksum",
        "nonhex_checksum",
        "trailing_checksum_junk",
        "truncated_after_status",
        "date_suffix",
        "missing_latitude",
        "embedded_nul",
    ]
    invalid_cases = []
    for repetition in range(INVALID_REPETITIONS):
        for mutation in mutations:
            fields = valid_fields(random_source)
            payload = invalid_payload(fields, mutation)
            invalid_cases.append(
                rejected(
                    f"invalid-{mutation}-{repetition:02d}",
                    mutation,
                    payload,
                )
            )
    cases.extend(invalid_cases)

    prelude = nmea_sentence(
        rmc_body(
            {
                "talker": "GN",
                "time": "123519.00",
                "status": "A",
                "latitude": "4807.03800",
                "latitude_hemisphere": "N",
                "longitude": "01131.00000",
                "longitude_hemisphere": "E",
                "speed": "0.004",
                "course": "77.52",
                "date": "230394",
                "tail": ",,A,V",
            }
        )
    )
    for index, invalid_case in enumerate(invalid_cases[:POST_LOCK_CASES]):
        cases.append(
            rejected(
                f"post-lock-{invalid_case['mutation']}-{index:03d}",
                invalid_case["mutation"],
                bytes.fromhex(invalid_case["chunks_hex"][0]),
                prelude=prelude,
            )
        )

    valid_loop = nmea_sentence(
        "GNRMC,235959.00,A,4807.03800,N,01131.00000,E,0.004,77.52,311224,,,A,V"
    )
    invalid_loop = nmea_sentence(
        "GNRMC,235959.00,V,4807.03800,N,01131.00000,E,0.004,77.52,311224,,,A,V"
    )
    non_rmc = nmea_sentence(
        "GNGGA,123519.00,4807.03800,N,01131.00000,E,1,08,0.9"
    )
    loop_expected = {"locked": True, "time": "235959.00", "date": "311224"}
    cases.extend(
        [
            record(
                "loop-valid-crlf",
                "loop_framing",
                "crlf",
                [valid_loop],
                loop_expected,
                transport="loop",
            ),
            record(
                "loop-valid-cr",
                "loop_framing",
                "cr_only",
                [valid_loop[:-2] + b"\r"],
                loop_expected,
                transport="loop",
            ),
            record(
                "loop-valid-lf",
                "loop_framing",
                "lf_only",
                [valid_loop[:-2] + b"\n"],
                loop_expected,
                transport="loop",
            ),
            record(
                "loop-fragmented",
                "loop_framing",
                "three_chunks",
                [valid_loop[:11], valid_loop[11:47], valid_loop[47:]],
                loop_expected,
                transport="loop",
            ),
            record(
                "loop-non-rmc-then-valid",
                "loop_framing",
                "multiple_sentences",
                [non_rmc + valid_loop],
                loop_expected,
                transport="loop",
            ),
            record(
                "loop-invalid-then-valid",
                "loop_framing",
                "invalid_then_valid",
                [invalid_loop + valid_loop],
                loop_expected,
                transport="loop",
            ),
            record(
                "loop-overlong-recovery",
                "loop_framing",
                "overlong_delimited_then_valid",
                [b"X" * 160 + b"\r\n" + valid_loop],
                loop_expected,
                transport="loop",
            ),
            record(
                "loop-overlong-swallow",
                "loop_framing",
                "overlong_undelimited_suffix",
                [b"X" * 160 + valid_loop],
                {"locked": False, "time": "", "date": ""},
                transport="loop",
            ),
            record(
                "loop-valid-then-invalid",
                "loop_framing",
                "post_lock_invalid",
                [valid_loop + invalid_loop],
                loop_expected,
                transport="loop",
            ),
            record(
                "loop-empty-lines-then-valid",
                "loop_framing",
                "empty_lines",
                [b"\r\n\n\r" + valid_loop],
                loop_expected,
                transport="loop",
            ),
        ]
    )
    return cases


def generated_document():
    cases = generate_cases()
    return {
        "schema_version": 1,
        "seed": SEED,
        "case_count": len(cases),
        "cases": cases,
    }


def serialize(document):
    return (json.dumps(document, indent=2, sort_keys=True) + "\n").encode("utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    rendered = serialize(generated_document())
    if args.verify:
        if not args.output.exists() or args.output.read_bytes() != rendered:
            raise SystemExit("persisted corpus does not match deterministic generator")
        return 0
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
