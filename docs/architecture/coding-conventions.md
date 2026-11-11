# Coding Conventions and Workflow

**Version:** 0.1 (Phase 2)
**Date:** 2026-10-21

These conventions apply to all hand-written code. CubeMX-generated code is not reformatted and is edited only inside `USER CODE` blocks.

Honest scope note: these are habits borrowed from automotive practice (MISRA-style discipline, fixed-width types, no dynamic memory). The project makes **no MISRA compliance claim** and runs no MISRA checker.

---

## 1. Language and compiler settings

| Item | Rule |
|---|---|
| Language | C11. CubeIDE builds with `gnu11` by default, host tests use `-std=c11` |
| Host warning flags for pure-logic code | `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror` |
| Firmware warning flags | Turn on `-Wall -Wextra` for hand-written folders. Do not add `-Werror` to generated code |
| Dynamic memory | None. No `malloc`, no `free` |
| Recursion, variable-length arrays | None |
| C++ | Not used |

---

## 2. Naming

| Element | Convention | Example |
|---|---|---|
| File | `snake_case.h` and `snake_case.c`, one module per pair | `fade_engine.h` |
| Public function | `Module_Verb`, module in PascalCase | `FadeEngine_SetTarget` |
| Type | PascalCase with `_t` suffix | `LightingCommand_t` |
| Enum value | `UPPER_SNAKE` with a module prefix | `VALIDATION_REJECT_CRC` |
| Macro constant | `UPPER_SNAKE` with a module prefix | `LAMP_CFG_RX_TIMEOUT_MS` |
| Include guard | `FILE_NAME_H` | `FADE_ENGINE_H` |
| File-private function or variable | `static`, variables prefixed `s_` | `s_ledLevel` |
| Local variable, parameter, struct field | `camelCase` | `nowMs`, `aliveCounter` |
| Units in names | Suffix when not obvious | `periodMs`, `brightnessPercent`, `scalePermille` |

---

## 3. Header and source organization

1. Every header starts with a Doxygen block giving `@file`, `@brief`, the **layer**, whether it is **host-testable**, and the requirement IDs it serves.
2. A header is **self-contained**: it includes everything it needs and nothing it does not. The build proves this by compiling every header alone (`header_checks` in `tests/unit/CMakeLists.txt`).
3. In a `.c` file, include in this order: its own header first (proves the header is self-contained), then standard headers, then other project headers.
4. A header contains declarations, types, and constants. Function bodies belong in `.c` files. The one exception is the disabled fault-injection stub (`static inline`), which exists so a normal build contains no injection code.
5. Only Drivers include HAL or device headers. The rule is enforced by `tools/check_layering.py`.

### 3.1 Layers and allowed includes

| Layer (rank) | Folders | May include |
|---|---|---|
| Neutral (0) | `status.h`, `can_frame.h`, `lamp_types.h` in `common/` | nothing from the project |
| Drivers (1) | `Drivers_App/`, `can_drv.h`, `uart_drv.h` | neutral |
| Communication (2) | `crc8.h`, `protocol.h`, `rx_queue.h`, `tx_scheduler.h` in `common/` | ranks 0 to 2 |
| Services (3) | `scheduler.h`, `log.h` in `common/`, `Services/` | ranks 0 to 3 |
| Application (4) | `Application/` | ranks 0 to 4 |

**Rule: calls go downward only, to any lower layer.** A lower layer never includes a header of a higher layer. (The Phase 1 text said "directly below". That was wrong, because the Application layer legitimately calls the drivers. This document replaces it.)

Only Drivers `.c` files include HAL headers. Everything else must be buildable on a PC.

---

## 4. Types and constants

1. Use fixed-width integer types (`uint8_t`, `uint16_t`, `uint32_t`). Use `bool` from `<stdbool.h>`. Do not use plain `int` or `long` for data that crosses a module boundary.
2. No unexplained magic numbers. Every threshold has a named constant that cites its SRS ID (`TH-xx`). Protocol constants live in `protocol.h`. Lamp constants in `lamp_config.h`. BCM constants in `bcm_config.h`.
3. Numbers in tests that are copied from the SRS are intentional duplicates. If an SRS threshold changes, change the header and `test_contracts.c` in the same commit.
4. Time is always `uint32_t` milliseconds passed **into** a function (`nowMs`). Pure modules never call `HAL_GetTick`. Elapsed time is computed as `(uint32_t)(nowMs - thenMs)`, which is wrap-safe.

---

## 5. Error handling

1. A function that can fail returns `Status_t`. A pure computation that cannot fail returns its value.
2. Every pointer argument is checked. NULL gives `STATUS_ERR_NULL_ARG` (or a documented safe result for pure functions).
3. Data from the bus is never trusted. It is validated in the order of SRS 3.3 before it changes any state.
4. Ignoring a return value is written explicitly: `(void)Function();`, and has a comment saying why.
5. No `assert` that stops production firmware. A condition that cannot be handled safely leads to the defined safe state (outputs 0) and a logged fault bit.

---

## 6. Interrupts and shared data

1. An interrupt handler only copies data into a queue and returns. No validation, no logging, no application calls (SRS COM-010).
2. Every variable shared between interrupt and main loop is `volatile` and has **one writer**. The header says who the writer is (see `rx_queue.h`).
3. No blocking waits and no `HAL_Delay` outside driver initialization (SRS SYS-005).

---

## 7. Logging

Line format (SRS DIA-003): `[<ms since reset>] <MODULE> <event> <key=value ...>`

| Part | Rule | Example |
|---|---|---|
| MODULE | Upper case, at most 6 characters | `LAMP`, `FSM`, `RX`, `COMM`, `BCM` |
| event | `UPPER_SNAKE` | `STATE_CHANGE`, `FRAME_REJECT` |
| data | `key=value`, keys in `lower_snake`, values numeric where possible | `from=1 to=4`, `reason=crc` |

Do not log from interrupt context. Log lines are the evidence the test runner parses, so keep the format stable.

---

## 8. Comments

1. Explain why, not what. The code already says what.
2. Public functions have a Doxygen `@brief`. Mention units, ranges, ownership, and what happens on error.
3. Where code implements a requirement, name the ID (`LMP-003`).
4. A design decision that is not obvious gets one sentence and an ID from the decision table (`AD-xx`).

---

## 9. Git workflow

| Item | Rule |
|---|---|
| Main branch | `main` always builds and passes the host checks |
| Work branches | `feat/<REQ-ID>-<short-name>`, `fix/<short-name>`, `docs/<short-name>`, `test/<short-name>` |
| Commit message | Requirement work: `LMP-003: add Color_ScaleChannel`. Other work: `docs:`, `test:`, `chore:`, `fix:` followed by a summary |
| Commit size | One logical change. A commit should not mix generated code regeneration with hand-written changes |
| Tags | `srs-v0.1`, `arch-v0.1`, then `phase3-done` and so on, only when the acceptance criteria of the phase are met |
| Merge | Merge into `main` after the checks below pass |

**Checks before every commit** (run from the repository root):

```bash
python tools/check_layering.py
python tools/rtm_check.py
python tools/run_unit_tests.py
```

Optional: put the same three commands in `.git/hooks/pre-commit` so a commit with a layering violation is refused.

What to commit from CubeIDE projects: `.ioc`, `.project`, `.cproject`, all hand-written sources, and the generated `Core/` and `Drivers/` folders (so a clean clone builds without running CubeMX). Do not commit `Debug/` or `Release/`.

---

## 10. Build and reproducibility

| Item | Rule |
|---|---|
| Host tests | `python tools/run_unit_tests.py` (needs Python, CMake, a C compiler, and network access on the first run to fetch Unity) |
| Unity version | Pinned to release tag `v2.6.0` in `tests/unit/CMakeLists.txt`. Pin to the commit hash for stricter reproducibility |
| Firmware | Built in STM32CubeIDE. The versions of CubeIDE and the Cube MCU packages are recorded in `docs/validation-reports/phase0/toolchain-versions.txt` |
| Python packages | Record the installed versions after Phase 0 (`pip freeze`) in the same folder |
| Every test record | Carries the firmware `git rev-parse --short HEAD` (SRS TST-004) |
