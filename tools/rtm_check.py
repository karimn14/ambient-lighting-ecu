#!/usr/bin/env python3
"""Checks that the SRS and the RTM describe the same set of requirements and that
every requirement has at least one planned test case.

Usage (from the repository root):
    python tools/rtm_check.py
Exit code 0 when consistent, 1 otherwise."""
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRS = ROOT / "docs" / "requirements" / "SRS.md"
RTM = ROOT / "docs" / "test-plans" / "RTM.csv"

ID_RE = re.compile(r"^### ([A-Z]+-\d{3}): ", re.M)


def main() -> int:
    errors = []

    srs_ids = ID_RE.findall(SRS.read_text(encoding="utf-8"))
    if len(srs_ids) != len(set(srs_ids)):
        errors.append("duplicate requirement IDs in SRS")
    srs_set = set(srs_ids)

    rtm_tests = {}
    with RTM.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            rid = row["requirement_id"]
            tid = row["test_case_id"]
            if not tid.strip():
                errors.append(f"{rid}: empty test case id")
            if not row["verification_method"].strip():
                errors.append(f"{rid}: empty verification method")
            if not tid.startswith("TC-" + rid + "-"):
                errors.append(f"{rid}: test case id {tid} does not follow TC-<req>-<nn>")
            rtm_tests.setdefault(rid, []).append(tid)
    rtm_set = set(rtm_tests)

    for rid in sorted(srs_set - rtm_set):
        errors.append(f"{rid}: in SRS but has no test case in RTM")
    for rid in sorted(rtm_set - srs_set):
        errors.append(f"{rid}: in RTM but not in SRS")

    all_tids = [t for ts in rtm_tests.values() for t in ts]
    if len(all_tids) != len(set(all_tids)):
        errors.append("duplicate test case IDs in RTM")

    if errors:
        print("RTM CHECK FAILED")
        for e in errors:
            print("  -", e)
        return 1
    print(f"RTM CHECK PASSED: {len(srs_set)} requirements, {len(all_tids)} test cases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
