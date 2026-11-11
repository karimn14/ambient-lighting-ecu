#!/usr/bin/env python3
"""Configure, build and run the host unit tests with one command (SRS TST-002).

Usage (from the repository root):
    python tools/run_unit_tests.py                      normal run
    python tools/run_unit_tests.py --fresh              delete the build folder first
    python tools/run_unit_tests.py --deliberate-failure add one failing test, expect a non-zero exit code

Exit code: 0 when everything passed, non-zero otherwise.
With --deliberate-failure the exit code is 0 only if the run FAILED as it should (proof that failures are not
hidden), and 1 if the failing test was somehow reported as a pass."""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "tests" / "unit"


def run(cmd, cwd=None):
    print("+", " ".join(str(c) for c in cmd), flush=True)
    return subprocess.run([str(c) for c in cmd], cwd=cwd).returncode


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fresh", action="store_true", help="remove the build folder first")
    parser.add_argument("--deliberate-failure", action="store_true",
                        help="add an always-failing test and expect the run to fail")
    args = parser.parse_args()

    build = ROOT / "build" / ("unit_fail" if args.deliberate_failure else "unit")
    if args.fresh and build.exists():
        shutil.rmtree(build)

    if shutil.which("cmake") is None or shutil.which("ctest") is None:
        print("ERROR: cmake and ctest must be on PATH (see handbook step 0.2)")
        return 2

    cfg = ["cmake", "-S", SRC, "-B", build]
    if args.deliberate_failure:
        cfg.append("-DDELIBERATE_FAILURE=ON")
    if run(cfg) != 0:
        print("RESULT: CONFIGURE FAILED")
        return 2
    if run(["cmake", "--build", build, "--config", "Debug"]) != 0:
        print("RESULT: BUILD FAILED (a header may not be self-contained or may include a HAL header)")
        return 2

    rc = run(["ctest", "-C", "Debug", "--output-on-failure"], cwd=build)

    if args.deliberate_failure:
        if rc != 0:
            print("RESULT: OK, the deliberate failure was detected (exit code was non-zero)")
            return 0
        print("RESULT: PROBLEM, the deliberately failing test was reported as a pass")
        return 1

    print("RESULT: ALL TESTS PASSED" if rc == 0 else "RESULT: TESTS FAILED")
    return rc


if __name__ == "__main__":
    sys.exit(main())
