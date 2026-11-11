#!/usr/bin/env python3
"""Enforces the architecture rules of docs/architecture/architecture.md section 3.3 on the source tree.

Rule A (HAL isolation): files in common/, Application/, Services/ and Communication/ must not include any
        HAL, device or CubeMX header and must not use HAL_ or LL_ identifiers (comments are ignored).
Rule B (no blocking waits): the same files must not call HAL_Delay (SRS SYS-005, test case TC-SYS-005-01).
Rule C (downward calls only): a header may include only headers of its own layer or a lower layer.
        Layers, low to high: Drivers(1) < Communication(2) < Services(3) < Application(4).
        Neutral headers (types, status) have rank 0 and may be included by everyone.

Usage (from the repository root):
    python tools/check_layering.py              check firmware/
    python tools/check_layering.py --self-test  prove that violations are detected
Exit code 0 when clean, 1 when violations are found."""
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

RANK_NEUTRAL, RANK_DRIVERS, RANK_COMM, RANK_SERVICES, RANK_APP = 0, 1, 2, 3, 4

# Layer of every header in firmware/common (the folder mixes layers, so it is listed explicitly).
COMMON_LAYER = {
    "status.h": RANK_NEUTRAL, "can_frame.h": RANK_NEUTRAL, "lamp_types.h": RANK_NEUTRAL,
    "crc8.h": RANK_COMM, "protocol.h": RANK_COMM, "rx_queue.h": RANK_COMM, "tx_scheduler.h": RANK_COMM,
    "scheduler.h": RANK_SERVICES, "log.h": RANK_SERVICES,
    "can_drv.h": RANK_DRIVERS, "uart_drv.h": RANK_DRIVERS,
}
FOLDER_LAYER = {"Application": RANK_APP, "Services": RANK_SERVICES,
                "Communication": RANK_COMM, "Drivers_App": RANK_DRIVERS}

# Files checked for rules A and B.
HAL_FREE_FOLDERS = {"common", "Application", "Services", "Communication"}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)
FORBIDDEN_INCLUDE_RE = re.compile(r'(^|/)(stm32[a-z0-9_]*|main|cmsis[a-z0-9_]*|core_cm\d+[a-z]*)\.h$', re.I)
HAL_IDENT_RE = re.compile(r'\b(HAL_[A-Za-z0-9_]+|__HAL_[A-Za-z0-9_]+|LL_[A-Z0-9_]+)\b')


def strip_comments(text: str) -> str:
    text = re.sub(r'/\*.*?\*/', lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def check(fw: Path):
    problems = []
    # Map every project header name to its rank.
    rank_of = dict(COMMON_LAYER)
    for folder, rank in FOLDER_LAYER.items():
        for h in fw.glob("*/" + folder + "/*.h"):
            rank_of[h.name] = rank

    for path in sorted(fw.rglob("*")):
        if path.suffix not in (".c", ".h"):
            continue
        rel = path.relative_to(fw)
        parts = rel.parts
        if "Core" in parts or ("Drivers" in parts):
            continue  # CubeMX generated code is out of scope
        folder = parts[0] if parts[0] == "common" else (parts[1] if len(parts) > 2 else "")
        if folder not in HAL_FREE_FOLDERS and folder != "Drivers_App":
            continue

        text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))

        if folder in HAL_FREE_FOLDERS:
            for inc in INCLUDE_RE.findall(text):
                if FORBIDDEN_INCLUDE_RE.search(inc):
                    problems.append(f"{rel}: rule A, includes HAL or device header '{inc}'")
            for ident in sorted(set(HAL_IDENT_RE.findall(text))):
                if ident == "HAL_Delay":
                    problems.append(f"{rel}: rule B, calls HAL_Delay")
                else:
                    problems.append(f"{rel}: rule A, uses HAL identifier '{ident}'")

        # Rule C
        if path.suffix == ".h" or path.suffix == ".c":
            if folder == "common":
                own = COMMON_LAYER.get(path.name) if path.suffix == ".h" else rank_of.get(path.with_suffix(".h").name)
            else:
                own = FOLDER_LAYER.get(folder)
            if own is None:
                continue
            for inc in INCLUDE_RE.findall(text):
                base = Path(inc).name
                if base in rank_of and rank_of[base] > own:
                    problems.append(f"{rel}: rule C, includes '{base}' from a higher layer")
    return problems


def self_test() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        fw = Path(tmp) / "firmware"
        (fw / "common").mkdir(parents=True)
        (fw / "lamp_ecu" / "Application").mkdir(parents=True)
        (fw / "lamp_ecu" / "Services").mkdir(parents=True)
        (fw / "common" / "status.h").write_text("/* ok */\n")
        (fw / "lamp_ecu" / "Application" / "app.h").write_text("#include \"status.h\"\n")
        # Violations: HAL include, HAL_Delay, upward include
        (fw / "lamp_ecu" / "Application" / "bad_hal.c").write_text(
            '#include "stm32f4xx_hal.h"\nvoid f(void){ HAL_Delay(5); }\n')
        (fw / "lamp_ecu" / "Services" / "bad_up.h").write_text('#include "app.h"\n')
        # A comment mentioning HAL must NOT be flagged
        (fw / "lamp_ecu" / "Services" / "fine.h").write_text("/* uses HAL_Delay in a comment */\n")
        problems = check(fw)
        text = "\n".join(problems)
        expected = ["rule A, includes HAL or device header", "rule B, calls HAL_Delay", "rule C, includes 'app.h'"]
        ok = all(e in text for e in expected) and "fine.h" not in text
        print("\n".join(problems))
        print("SELF-TEST", "PASSED" if ok else "FAILED")
        return 0 if ok else 1


def main() -> int:
    if "--self-test" in sys.argv:
        return self_test()
    problems = check(ROOT / "firmware")
    if problems:
        print("LAYERING CHECK FAILED")
        for p in problems:
            print("  -", p)
        return 1
    print("LAYERING CHECK PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
