# Phase 2 Guide: Repository, CubeIDE Integration, and Verification

**Version:** 0.1
**Date:** 2026-10-21

Goal of Phase 2: make the repository and both CubeIDE projects ready for implementation, so that Phase 3 can start writing real code test-first.

What I verified myself (on Linux with GCC 13, CMake 4.4, and `arm-none-eabi-gcc` 13.2):

- All 28 headers compile alone and together on the host, with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror`.
- Both include probes compile for Cortex-M4 (Lamp ECU) and Cortex-M3 (BCM) with the same flags.
- The host test run passes (2 test executables), a deliberately failing test is detected, a header that includes a HAL header breaks the build, and a header with a missing include breaks the build.
- The layering checker passes on the tree, and its self-test shows it detects a HAL include, `HAL_Delay`, and an upward include.

What I could **not** verify, and you must (tagged **[B]**): everything that happens inside STM32CubeIDE (menu names, how it stores include paths, linked-folder syntax), because I do not have CubeIDE here. Where a step names a menu, treat the wording as my best knowledge of the current UI and adapt if yours differs.

---

## Step 2.1 Copy the Phase 2 files into your repository

**Why:** the files mirror the repository layout from the handbook, so you can copy them over without renaming.

**Do:** from the `phase2/` folder copy these into your repository root, keeping paths:

```
firmware/common/*.h
firmware/lamp_ecu/Application/*   firmware/lamp_ecu/Services/*   firmware/lamp_ecu/Drivers_App/*   firmware/lamp_ecu/Communication/README.md
firmware/bcm/Application/*        firmware/bcm/Drivers_App/*     firmware/bcm/Communication/README.md
tests/unit/*
tools/run_unit_tests.py   tools/check_layering.py
docs/architecture/*
```

Also keep the Phase 1 files (`docs/requirements/SRS.md`, `docs/test-plans/RTM.csv`, `tools/rtm_check.py`). `docs/architecture/architecture.md` from Phase 2 **replaces** the Phase 1 file.

Your CubeIDE projects already live in `firmware/lamp_ecu/` and `firmware/bcm/` (created in Phase 0). The copy adds the new folders next to `Core/` and `Drivers/`. It must not overwrite anything inside `Core/` or `Drivers/`.

**Expected result:** `git status` shows only new files plus the replaced `architecture.md`.

---

## Step 2.2 Run the three host checks

**Why:** this proves the toolchain on your machine before you touch CubeIDE.

**Do (from the repository root):**

```bash
python tools/check_layering.py
python tools/rtm_check.py
python tools/run_unit_tests.py --fresh
```

**Expected result:**

```
LAYERING CHECK PASSED
RTM CHECK PASSED: 42 requirements, 75 test cases
...
100% tests passed out of 2
RESULT: ALL TESTS PASSED
```

The first run of `run_unit_tests.py` downloads Unity from GitHub, so it needs network access. If you are behind a proxy, set the Git proxy first.

**If it fails:**

| Symptom | Likely cause | Action |
|---|---|---|
| `cmake and ctest must be on PATH` | CMake not installed | Install CMake (handbook step 0.2) |
| `fatal: detected dubious ownership` during the Unity download | Git refuses a folder owned by another user (happens on some shared or mounted folders) | `git config --global --add safe.directory <the path shown in the message>` |
| `No CMAKE_C_COMPILER could be found` | No C compiler | Install GCC (Linux), MSYS2 with `mingw-w64-x86_64-gcc` (Windows), or Xcode command line tools (macOS) |
| Multi-config generator (Visual Studio) prints `-C` warnings | Different generator | Ignore. The script passes `Debug` for both build and test |
| `BUILD FAILED` | A header is not self-contained, or includes a HAL header | Read the compiler error. It names the header |

**Evidence to keep:** a terminal capture of all three commands, saved as `docs/validation-reports/phase2/host-checks.txt`.

---

## Step 2.3 Prove the checks can fail (do this once)

**Why:** a check that has never been seen failing proves nothing.

**Do:**

1. Run `python tools/run_unit_tests.py --deliberate-failure`. Expect `RESULT: OK, the deliberate failure was detected`.
2. Open `firmware/common/status.h`, add the line `#include "stm32f4xx_hal.h"` below `#define STATUS_H`. Run `python tools/check_layering.py` (expect a rule A failure) and `python tools/run_unit_tests.py` (expect `BUILD FAILED`). Undo the edit with `git checkout firmware/common/status.h`.

**Expected result:** both failures are reported, and after the undo everything passes again.

---

## Step 2.4 Configure the Lamp ECU project in CubeIDE **[B]**

**Why:** the new folders contain only hand-written code and the shared headers. CubeIDE must know they are source folders and where to find the headers.

**Do:**

1. Open the workspace that contains `lamp_ecu`. Right-click the project, **Refresh** (F5). The folders `Application`, `Services`, `Communication`, and `Drivers_App` appear next to `Core` and `Drivers`.
2. **Link the shared folder.** Right-click the project, **New, Folder**. Click **Advanced**, tick **Link to alternate location (Linked Folder)**. Name the folder `common`. In the location field enter `PARENT-1-PROJECT_LOC/common`. Click **Finish**.
   - `PARENT-1-PROJECT_LOC` is an Eclipse path variable meaning "one folder above the project location". Because the project is `firmware/lamp_ecu`, this points to `firmware/common`. The link is stored relative, so it still works on another machine.
3. **Add source locations.** Right-click the project, **Properties, C/C++ General, Paths and Symbols, Source Location** tab. Click **Add Folder...** and add `Application`, `Services`, `Drivers_App`, and `common`. (If your project already lists the project root as a source location, the sub-folders may already be included. Check, and do not add duplicates.)
4. **Add include paths.** Same dialog, **Includes** tab, language **GNU C**. Click **Add...** and add these four entries with **Add to all configurations** ticked:
   - `${ProjDirPath}/Application`
   - `${ProjDirPath}/Services`
   - `${ProjDirPath}/Drivers_App`
   - `${ProjDirPath}/common`
5. Apply and close. In **C/C++ Build, Settings, Tool Settings, MCU GCC Compiler, Include paths** you should now see the same four entries. (CubeIDE keeps one list. Either dialog shows it.)
6. Optional but recommended: in the same Tool Settings, **MCU GCC Compiler, Warnings**, enable all common warnings and add `-Wextra` under **Other warning flags**. Do not add `-Werror` yet, because generated code may produce warnings.

**Expected result:** no error dialogs. `Application/include_probe.c` is listed in the project tree under `Application`.

---

## Step 2.5 Build the Lamp ECU **[B]**

**Do:** **Project, Clean...**, then **Project, Build Project**.

**Expected result:** `0 errors`. In the console you see a compile line for `Application/include_probe.c`. The firmware still links, and it still behaves as the Phase 0 smoke test (the probe adds one constant and no code).

**If it fails:**

| Symptom | Likely cause | Action |
|---|---|---|
| `fatal error: can_drv.h: No such file or directory` | `common` include path missing | Step 2.4, item 4 |
| `fatal error: lamp_app.h: No such file or directory` | `Application` include path missing | Step 2.4, item 4 |
| No `include_probe` compile line in the console | `Application` is not a source location | Step 2.4, item 3 |
| `${ProjDirPath}/common` not found | The link was not created or has a wrong variable | Right-click `common`, Properties, Resource. The location must show the real `firmware/common` path |

**Evidence to keep:** screenshot of the build console showing the probe compiled and `0 errors`.

---

## Step 2.6 Prove the probe is meaningful **[B]**

**Why:** if the probe compiles even without the include paths, it proves nothing.

**Do:** temporarily remove the `${ProjDirPath}/common` include path (Step 2.4, item 4). Build. Expect `fatal error: ... No such file or directory` from the probe. Put the path back. Build again and expect `0 errors`.

---

## Step 2.7 Configure and build the BCM project **[B]**

Repeat steps 2.4 to 2.6 for the `bcm` project, with these differences:

| Item | BCM value |
|---|---|
| Source locations | `Application`, `Drivers_App`, `common` (there is no `Services` folder in the BCM) |
| Include paths | `${ProjDirPath}/Application`, `${ProjDirPath}/Drivers_App`, `${ProjDirPath}/common` |
| Probe to check | `Application/include_probe.c` of the BCM |
| Expected result | `0 errors` and the Blue Pill smoke test still behaves as in Phase 0 |

---

## Step 2.8 Re-run the Phase 0 smoke test (regression) **[B]**

**Why:** adding folders and include paths must not break the working Phase 0 firmware.

**Do:** flash each board as in Phase 0 and confirm the LED blinks and the UART prints the boot and tick lines. If Phase 0 is not done yet, do this step after it.

---

## Step 2.9 Commit and tag

```bash
git checkout -b docs/phase2-architecture
git add .
git commit -m "docs: phase 2 module interfaces, conventions, host test build"
git checkout main
git merge docs/phase2-architecture
git tag arch-v0.1
```

Tag only after the acceptance criteria below are met. The commit message pattern is explained in `coding-conventions.md` section 9.

---

## Phase 2 acceptance criteria

| # | Criterion | Evidence |
|---|---|---|
| P2-1 | `check_layering.py`, `rtm_check.py`, and `run_unit_tests.py` all pass on your machine | `host-checks.txt` |
| P2-2 | The deliberate-failure run and the HAL-include break were each seen to fail | Terminal capture |
| P2-3 | Lamp ECU project builds with 0 errors and compiles `include_probe.c` | Console screenshot |
| P2-4 | BCM project builds with 0 errors and compiles `include_probe.c` | Console screenshot |
| P2-5 | Removing the `common` include path makes the probe fail (both projects) | Console screenshot |
| P2-6 | Phase 0 smoke tests still pass after the change (when Phase 0 is done) | Log |
| P2-7 | You have read `module-interfaces.md` section 5 and answered or accepted Q1 to Q7 | Your reply |

Do not start Phase 3 until P2-1 to P2-5 are met and Q1 to Q7 are answered. Phase 3 also needs the Phase 0 results (LED polarity, timer clock, observer decision).

---

## What I need back

1. The output of the three host commands.
2. The two build-console screenshots (or the last lines of each console).
3. Your answers to Q1 to Q7 ("accept all" is fine).
4. Anything from Step 2.4 that did not match my description of the CubeIDE menus, so I can fix this guide.
