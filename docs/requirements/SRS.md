# Software Requirements Specification (SRS)

**Project:** Automotive Ambient Lighting ECU with CAN-based Body Control Module
**Document version:** 0.1 (Phase 1 baseline, pending your approval of section 7)
**Date:** 2026-10-21
**Related documents:** `docs/handbook/engineering-handbook.md` v0.2, `docs/architecture/architecture.md`, `docs/test-plans/RTM.csv`

---

## 1. Introduction

### 1.1 Purpose

This document defines what the two-node system shall do and how each statement will be verified. It is the reference for design (Phase 2), implementation (Phases 3 to 5), integration (Phase 6), and testing (Phase 7).

### 1.2 Scope and honesty statement

- The system is an **educational prototype** running at low voltage on a bench. It is not a production ECU and must not be connected to a vehicle electrical system.
- Behavior and numbers in this document are **project-defined targets [D]**. They are not taken from an automotive standard or an OEM specification.
- The layering and vocabulary are **AUTOSAR-inspired**. This is not an AUTOSAR implementation and makes no AUTOSAR or ISO 26262 compliance claim.
- A requirement is only "verified" when a test record exists (TST-004). A design number that a measurement does not confirm is reported as a finding. It is not silently edited to match.

### 1.3 System summary

| Node | Hardware | Responsibility |
|---|---|---|
| BCM | STM32 Blue Pill | Samples simulated vehicle inputs (door, enable, mode, brightness, color, unlock), generates and transmits CAN messages, provides fault injection for tests |
| Lamp ECU | NUCLEO-F446RE | Receives and validates messages, runs the lamp state machine and fades, drives the RGB LED through PWM, reports status and diagnostics |

### 1.4 Priority and verification codes

| Code | Meaning |
|---|---|
| M | Mandatory for the baseline |
| O | Optional |
| A | Advanced, may end as NOT EXECUTED with an honest reason |
| T | Test (execution with observed results) |
| I | Inspection (reading code, configuration, schematics) |
| A (verification) | Analysis |
| D | Demonstration |

Test case levels: UNIT (host PC), INT (target hardware, one or more modules), SYS (two-node system), HW (electrical), PROC (process or documentation).
Test case modes: AUTO (script-driven, repeatable), MANUAL (needs a person to observe or operate), INSP (inspection).

---

## 2. Glossary

| Term | Meaning in this project |
|---|---|
| Valid frame | A received frame that passed all validation steps in section 3.3 |
| Base layer | The lamp output defined by the latest valid LightingCommand (zero when LampEnable = 0) |
| Overlay | A temporary lamp output driven by a vehicle event: courtesy (door) or welcome (unlock) |
| Output | The three PWM duty values (in counts, 0 to 1024) actually applied to the LED |
| Fade | A linear transition of each channel duty from its current value to a target over a time |
| FadeTime | LightingCommand signal in units of 10 ms (0 to 255) |
| COMM_LOST | The Lamp ECU state after a LightingCommand timeout, until recovery |
| Observer | An independent device on the bus (USB-CAN adapter, or an ESP32 with a transceiver) used to timestamp and decode frames |
| Latched fault bit | A fault bit that stays set until the Lamp ECU is reset |
| Active fault bit | A fault bit that mirrors a currently present condition |

---

## 3. Behavioral rules

These rules are normative. The requirements in section 5 refer to them.

### 3.1 Output selection

The Lamp ECU computes one output from four layers:

| Precedence | Layer | Active when |
|---|---|---|
| 1 (highest) | COMM_LOST | LightingCommand timeout is active: output target is 0 for all layers |
| 2 | COURTESY | DoorOpen = 1, or within the 5000 ms hold and 1000 ms fade-out after it returned to 0 |
| 3 | WELCOME | Running a welcome sequence |
| 4 | BASE | Otherwise: commanded color x brightness if LampEnable = 1, else 0 |

Overlay colors do not depend on the commanded color. Courtesy output is fixed white at 80 percent. The welcome target is the commanded color and brightness if LampEnable = 1, otherwise the default welcome color.

Overlays end by fading to the base output that is current at that moment (not the base output at the time the overlay began).

### 3.2 Startup, loss, and recovery

**Lamp ECU startup.** Outputs are 0 and LampState is INIT. After initialization the state is OFF. The base layer stays 0 until the first valid LightingCommand. Overlays are not active until the first valid VehicleStatus.

**First VehicleStatus (after reset or after a VehicleStatus timeout).** It sets the baseline. DoorOpen is applied as a level (door open starts the courtesy output). VehicleLockState is stored as the baseline only and never triggers a welcome sequence.

**Loss of LightingCommand.** After 300 ms without a valid LightingCommand the Lamp ECU enters COMM_LOST, sets CMD_TIMEOUT, increments TimeoutCount, and fades all outputs to 0 over 1000 ms. All overlays are cancelled.

**Recovery.** In COMM_LOST, frames are still received and validated. After 2 consecutive valid LightingCommand frames the Lamp ECU leaves COMM_LOST, clears CMD_TIMEOUT, and fades to the output defined by section 3.1 using the latest command's FadeTime. A rejected frame, or a gap longer than 300 ms, resets the consecutive count to 0.

**Loss of VehicleStatus.** After 300 ms without a valid VehicleStatus the Lamp ECU sets VEH_TIMEOUT, increments TimeoutCount, treats the door as closed, and ends any overlay with a 1000 ms fade-out to the base output. The base layer keeps following valid LightingCommand frames. After the next valid VehicleStatus the baseline rule above applies.

**BCM startup.** The BCM starts transmitting with the defaults of BCM-005 and the door state read from its switch.

### 3.3 Validation order (Lamp ECU, every received frame)

| Step | Check | On failure |
|---|---|---|
| 1 | Identifier is 0x200 or 0x210 | Ignore. No state or counter change |
| 2 | DLC equals the expected length (8 for 0x200, 3 for 0x210) | Reject, DLC_FAIL, RxRejectedCount + 1 |
| 3 | CRC-8 matches the payload | Reject, CRC_FAIL, RxRejectedCount + 1 |
| 4 | AliveCounter differs from the last accepted frame of the same message (first frame always passes this step) | Reject, ALIVE_FAIL, RxRejectedCount + 1 |
| 5 | Every signal is in range and reserved bits are 0 | Reject, INVALID_VALUE, RxRejectedCount + 1 |
| 6 | All checks passed | Accept: update state, reset that message's timeout, RxValidCount + 1, store the AliveCounter |

A rejected frame never changes application state and never resets a timeout. The AliveCounter is stored only on acceptance.

**Known limitation (to appear in the README):** a 4-bit alive counter and a CRC-8 detect corruption, repeated frames, and frozen senders. They do not detect a replayed older frame with a different counter value, a deliberate attacker, or every possible CAN fault.

### 3.4 Fade rules

- Fades are linear per channel in duty counts and are updated every 10 ms.
- Base layer transitions use FadeTime x 10 ms. FadeTime = 0 applies the new target at the next update.
- Courtesy, welcome, and loss transitions use the fixed times in section 4.
- A new target during a fade starts a new fade from the current output value.
- Duty counts range from 0 to 1024. With a timer period of 1024 counts (ARR = 1023) a compare value of 1024 gives a constant high output **[B: confirm on hardware]**.

---

## 4. Project-defined thresholds [D]

All values are proposals pending your approval in section 7. Each has a rationale and a stated way to check it on the real system.

| ID | Parameter | Value | Rationale | How it is checked |
|---|---|---|---|---|
| TH-01 | LightingCommand and VehicleStatus period | 100 ms | Slow enough for a bench setup, fast enough for 300 ms loss detection | Observer capture (SYS-001) |
| TH-02 | Accepted interval range for a 100 ms message | 90 to 110 ms | One 10 ms task period of jitter | Observer capture (SYS-001, COM-005) |
| TH-03 | LampStatus period / DiagnosticStatus period | 100 ms / 500 ms | Status follows commands, diagnostics change slowly | Observer capture (COM-005) |
| TH-04 | Receive timeout T (each message) | 300 ms (detected 300 to 310 ms) | Three cycles: tolerates two lost frames. 10 ms monitor period adds up to 10 ms | COM-001, COM-008 |
| TH-05 | Recovery count N | 2 consecutive valid frames | A single frame could be a stray valid frame | COM-006 |
| TH-06 | Control, fade and monitor task period | 10 ms | Fine enough for smooth fades, coarse enough for a superloop | LMP-005, SYS-005 |
| TH-07 | Debounce time | 30 ms (3 samples at 10 ms) | Typical tactile switch bounce is shorter. Confirm with your buttons | BCM-001 |
| TH-08 | Event to output response | 20 ms from valid frame processing to first output change | Two task periods | LMP-006 |
| TH-09 | PWM frequency / resolution | 2 kHz nominal (1.5 to 2.5 kHz accepted) / 1024 counts | Above visible flicker, and 10 bits gives smooth low-brightness fades. With a 90 MHz timer clock **[B]**: PSC = 43, ARR = 1023 gives 90e6 / (44 x 1024) = about 1997 Hz | LMP-002 |
| TH-10 | Courtesy | White (255,255,255), 80 percent, fade-in 300 ms, hold 5000 ms after door close, fade-out 1000 ms | Typical interior behavior, long enough to observe in tests | LMP-006 |
| TH-11 | Welcome | Fade-in 1000 ms, hold 2000 ms, fade-out 1000 ms. Default color (0,100,255) at 60 percent when LampEnable = 0 | Distinct from courtesy, short enough to test | LMP-007 |
| TH-12 | Breathing | Triangle wave, period 4000 ms (3800 to 4200 accepted), 20 to 100 percent of commanded brightness, starts at minimum | Slow, visible, and trivially computable | LMP-008 |
| TH-13 | Loss fades | 1000 ms for COMM_LOST and for the end of overlays after VehicleStatus loss | A visible, non-abrupt change | COM-001, COM-008 |
| TH-14 | Bus-off restart interval | At least 1000 ms | Avoids flooding a faulty bus | COM-007 |
| TH-15 | Receive ring buffer | 8 frames | Many frames per 10 ms loop pass at this traffic level | COM-010 |
| TH-16 | LED channel current | At most 10 mA | Safe for a 5 mm LED and a small NPN | HW-001 |
| TH-17 | Bus resistance, power off | 54 to 66 ohm | Two 120 ohm in parallel plus meter tolerance | HW-002 |
| TH-18 | Worst-case main-loop pass | Below 5 ms | Keeps the 10 ms tasks on time | SYS-005 |
| TH-19 | BCM event frame latency | At most 20 ms after the debounced change | Two task periods | BCM-004 |
| TH-20 | Bitrate and measured bit width | 500 kbit/s, 1.9 to 2.1 microseconds | Plus or minus 5 percent covers the resolution of a 24 MS/s analyzer | SYS-004 |

## 5.1 System requirements (SYS)

### SYS-001: Cyclic LightingCommand transmission

- **Statement:** The BCM shall transmit LightingCommand (identifier 0x200, DLC 8) every 100 ms (TH-01).
- **Priority:** M
- **Input and expected output:** Input: BCM running with any input state. Output: one 0x200 frame every 100 ms.
- **Verification method:** T
- **Acceptance criteria:** In a 60 s capture on an independent observer, every interval between consecutive 0x200 frames is 90 to 110 ms (TH-02). AliveCounter increases by 1 modulo 16 between consecutive frames. Every CRC-8 is valid.
- **Related software component:** BCM TxScheduler, Protocol
- **Planned test cases:**
  - `TC-SYS-001-01` (SYS, AUTO): 60 s observer capture: intervals, alive counter sequence, CRC validity (MANUAL via UART log if no observer exists)

### SYS-002: Validation before use

- **Statement:** The Lamp ECU shall validate every received LightingCommand and VehicleStatus (identifier, DLC, CRC-8, alive counter, value ranges, reserved bits) in the order defined in section 3.3 before the content affects application state or the timeout monitor.
- **Priority:** M
- **Input and expected output:** Input: valid and invalid frames. Output: only valid frames change application state.
- **Verification method:** T
- **Acceptance criteria:** For each of the six invalid cases (wrong DLC, bad CRC, frozen alive counter, LightingMode out of range, BrightnessPercent above 100, reserved bits not zero): the lamp output and LampState stay unchanged for at least 1 s after injection, RxRejectedCount increases by exactly 1, and the matching latched fault bit is set. The timeout timer is not reset by the invalid frame.
- **Related software component:** CommandValidator, Protocol
- **Planned test cases:**
  - `TC-SYS-002-01` (UNIT, AUTO): Table-driven validator test: one valid vector and the six invalid cases, checking result code and returned fault class
  - `TC-SYS-002-02` (SYS, AUTO): BCM fault-injection sends each invalid frame, Lamp log and DiagnosticStatus checked

### SYS-003: Safe state after reset

- **Statement:** The Lamp ECU shall keep all three PWM outputs at 0 percent from reset until the first valid LightingCommand has been received. The compare registers shall be 0 before the timer outputs are enabled.
- **Priority:** M
- **Input and expected output:** Input: power-up or reset with the BCM absent or silent. Output: LED dark, LampState INIT then OFF.
- **Verification method:** T, I
- **Acceptance criteria:** With the BCM disconnected, for 60 s after reset all three PWM pins read 0 percent duty on a logic analyzer and the LED is dark. Code inspection shows the compare registers are written to 0 before PWM start.
- **Related software component:** LedPwm, LampControl
- **Planned test cases:**
  - `TC-SYS-003-01` (SYS, MANUAL): Reset with BCM disconnected, logic analyzer on the three PWM pins for 60 s
  - `TC-SYS-003-02` (SYS, INSP): Inspect initialization order in LedPwm and main

### SYS-004: Common bitrate

- **Statement:** Both nodes shall use a nominal bitrate of 500 kbit/s with the bit timing documented in the CAN specification (TH-20).
- **Priority:** M
- **Input and expected output:** Input: both nodes running in normal mode. Output: error-free communication at one bitrate.
- **Verification method:** I, T
- **Acceptance criteria:** The documented bit timing matches the generated configuration of both nodes. The shortest dominant pulse on a CAN_TX pin measures 1.9 to 2.1 microseconds (one bit). A 10 minute observer capture under normal operation shows zero error frames (if the adapter reports them).
- **Related software component:** CanDrv, CubeMX configuration
- **Planned test cases:**
  - `TC-SYS-004-01` (SYS, INSP): Compare documented timing with both .ioc files and generated CAN init code
  - `TC-SYS-004-02` (SYS, MANUAL): Measure one bit width on CAN_TX with a logic analyzer of at least 24 MS/s
  - `TC-SYS-004-03` (SYS, AUTO): 10 minute observer capture, count error frames

### SYS-005: Non-blocking scheduling

- **Statement:** The firmware shall run its cyclic work from a 1 ms tick with no blocking waits in application, service, or communication code. The worst-case duration of one main-loop pass shall be below 5 ms (TH-18).
- **Priority:** M
- **Input and expected output:** Input: normal operation for 10 minutes. Output: bounded loop time.
- **Verification method:** I, T
- **Acceptance criteria:** A search of Application, Services and Communication sources finds no call to HAL_Delay or any busy-wait loop. The worst-case loop pass measured over 10 minutes (GPIO toggle on a logic analyzer, or the DWT cycle counter) is below 5 ms.
- **Related software component:** Scheduler
- **Planned test cases:**
  - `TC-SYS-005-01` (SYS, INSP): grep for HAL_Delay and busy-wait patterns in the non-driver source folders
  - `TC-SYS-005-02` (SYS, MANUAL): Measure worst-case main-loop pass over 10 minutes


## 5.2 Lamp ECU requirements (LMP)

### LMP-001: ON and OFF control

- **Statement:** When the latest valid LightingCommand has LampEnable = 1 and no overlay and no communication loss is active, the Lamp ECU shall drive the output to the commanded color scaled by the commanded brightness. With LampEnable = 0 the output shall go to 0.
- **Priority:** M
- **Input and expected output:** Input: LightingCommand with enable 0 or 1. Output: lamp on at the commanded level or off.
- **Verification method:** T
- **Acceptance criteria:** Steady-state compare values equal the LMP-003 result exactly. The transition completes within FadeTime x 10 ms plus 20 ms (see LMP-005).
- **Related software component:** LampControl, FadeEngine
- **Planned test cases:**
  - `TC-LMP-001-01` (UNIT, AUTO): Lamp logic: enable 0 and 1 against expected target output
  - `TC-LMP-001-02` (INT, AUTO): UART console injects enable on and off, compare values read back from the log

### LMP-002: RGB output by hardware PWM

- **Statement:** The Lamp ECU shall drive the three color channels from three hardware PWM channels of one timer, at a nominal PWM frequency of 2 kHz (accepted range 1.5 to 2.5 kHz) with a resolution of 1024 counts (TH-09).
- **Priority:** M
- **Input and expected output:** Input: any lamp output value. Output: three synchronous PWM signals.
- **Verification method:** I, T
- **Acceptance criteria:** The .ioc file shows one timer with three PWM channels and ARR = 1023. A logic analyzer measures a PWM frequency between 1.5 and 2.5 kHz on all three pins.
- **Related software component:** LedPwm
- **Planned test cases:**
  - `TC-LMP-002-01` (HW, INSP): Inspect .ioc and generated timer init
  - `TC-LMP-002-02` (HW, MANUAL): Logic analyzer frequency measurement on the three pins

### LMP-003: Brightness scaling

- **Statement:** For a color value C (0 to 255) and brightness B (0 to 100 percent) the duty in counts shall be round(C x B x 1024 / (255 x 100)) using integer round half up, limited to 1024. A value of 1024 means the output is constantly high (100 percent).
- **Priority:** M
- **Input and expected output:** Input: C and B. Output: duty counts per channel.
- **Verification method:** T
- **Acceptance criteria:** A host unit test with at least 12 vectors passes, including (C,B) = (0,0), (255,0), (0,100), (255,100) giving 1024, (255,50) giving 512, (1,1), (128,100). On hardware each channel alone at 100 percent reads as constantly high on the logic analyzer.
- **Related software component:** Color (pure function), LedPwm
- **Planned test cases:**
  - `TC-LMP-003-01` (UNIT, AUTO): Scaling function against at least 12 vectors including boundaries
  - `TC-LMP-003-02` (INT, AUTO): Each channel alone through the UART console, compare values read back

### LMP-004: Lighting modes

- **Statement:** The Lamp ECU shall support the base lighting modes STATIC (LightingMode 0) and BREATHING (LightingMode 1). Any other LightingMode value shall be rejected (COM-002).
- **Priority:** M
- **Input and expected output:** Input: LightingMode 0 or 1. Output: steady light or breathing light.
- **Verification method:** T
- **Acceptance criteria:** Mode 0 holds a constant output. Mode 1 follows LMP-008. Values 2 to 255 are rejected and counted.
- **Related software component:** LampControl
- **Planned test cases:**
  - `TC-LMP-004-01` (UNIT, AUTO): Mode selection and rejection of out-of-range mode values
  - `TC-LMP-004-02` (SYS, MANUAL): Visual check of both modes on the prototype, recorded as a short video

### LMP-005: Non-blocking linear fades

- **Statement:** Changes of the base layer (enable, color, brightness, mode) shall transition linearly over FadeTime x 10 ms, updated every 10 ms (TH-06). FadeTime 0 shall apply the change at the next update. A new target during a fade shall start a new fade from the current output value without a step discontinuity.
- **Priority:** M
- **Input and expected output:** Input: command change with a FadeTime value. Output: a linear transition.
- **Verification method:** T
- **Acceptance criteria:** In a log of the compare values every 10 ms during a 1000 ms fade, the sequence is monotonic, reaches the target within FadeTime x 10 ms plus 20 ms, and no step exceeds ceil(range / (FadeTime)) + 1 counts. In a retarget test the first value after the new target differs from the last value before it by no more than that same bound.
- **Related software component:** FadeEngine
- **Planned test cases:**
  - `TC-LMP-005-01` (UNIT, AUTO): Fade engine: monotonicity, endpoint timing, step bound, retarget continuity
  - `TC-LMP-005-02` (INT, AUTO): Compare values logged every 10 ms on hardware during a 1000 ms fade

### LMP-006: Courtesy light

- **Statement:** While DoorOpen = 1 in the latest valid VehicleStatus, the Lamp ECU shall show the courtesy output (white 255,255,255 at 80 percent) after a 300 ms fade-in. After DoorOpen returns to 0 it shall hold the courtesy output for 5000 ms and then fade out over 1000 ms to the base output (TH-10). A door reopen during the hold or the fade-out returns to the courtesy output.
- **Priority:** M
- **Input and expected output:** Input: door open, close, reopen. Output: courtesy light behavior.
- **Verification method:** T
- **Acceptance criteria:** From the processing of the valid VehicleStatus, the output starts changing within 20 ms (TH-08). The hold lasts 5000 ms plus or minus 50 ms. The fade-out takes 1000 ms plus or minus 50 ms. A reopen cancels the pending fade-out.
- **Related software component:** LampControl (lamp_fsm), FadeEngine
- **Planned test cases:**
  - `TC-LMP-006-01` (UNIT, AUTO): State machine: open, close, hold timer, reopen during hold, reopen during fade-out
  - `TC-LMP-006-02` (SYS, AUTO): Door sequence with timestamps from the Lamp ECU log

### LMP-007: Welcome light

- **Statement:** On a transition of VehicleLockState from LOCKED to UNLOCKED the Lamp ECU shall run the welcome sequence: fade in over 1000 ms, hold 2000 ms, fade out over 1000 ms (TH-11). The target is the commanded color and brightness if LampEnable = 1, otherwise the default welcome color (0,100,255) at 60 percent. After the sequence the output returns to the base output.
- **Priority:** M
- **Input and expected output:** Input: unlock transition. Output: welcome sequence.
- **Verification method:** T
- **Acceptance criteria:** Fade-in, hold and fade-out durations each match their value within plus or minus 50 ms. No welcome sequence starts on a transition to LOCKED or on a repeated UNLOCKED value.
- **Related software component:** LampControl (lamp_fsm), FadeEngine
- **Planned test cases:**
  - `TC-LMP-007-01` (UNIT, AUTO): State machine: welcome with enable 1, with enable 0, no trigger on LOCKED or repeated UNLOCKED
  - `TC-LMP-007-02` (SYS, AUTO): Unlock sequence with timestamps from the Lamp ECU log

### LMP-008: Breathing waveform

- **Statement:** In BREATHING mode the effective brightness shall follow a triangular waveform with a period of 4000 ms (accepted 3800 to 4200 ms) between 20 percent and 100 percent of the commanded brightness, starting at the minimum (TH-12).
- **Priority:** M
- **Input and expected output:** Input: LightingMode 1 with enable 1. Output: triangular brightness modulation.
- **Verification method:** T
- **Acceptance criteria:** A log of the output every 100 ms over at least 3 periods shows minima and maxima separated by 2000 ms plus or minus 100 ms, with extremes at 20 percent and 100 percent of the commanded value plus or minus 1 count of rounding.
- **Related software component:** FadeEngine, LampControl
- **Planned test cases:**
  - `TC-LMP-008-01` (UNIT, AUTO): Waveform function: period, extremes, start phase
  - `TC-LMP-008-02` (INT, AUTO): Output log over 3 periods on hardware

### LMP-009: Output layer precedence

- **Statement:** The output shall be selected with the precedence COURTESY, then WELCOME, then BASE. A door open during the welcome sequence shall abort it and show the courtesy output. An unlock transition while the courtesy output is active shall be ignored (no welcome sequence afterwards).
- **Priority:** M
- **Input and expected output:** Input: overlapping door and unlock events. Output: deterministic result.
- **Verification method:** T
- **Acceptance criteria:** All four combinations (unlock then door open, door open then unlock, unlock during courtesy hold, door open during welcome fade-out) produce the documented output with no jump larger than the LMP-005 step bound.
- **Related software component:** LampControl (lamp_fsm)
- **Planned test cases:**
  - `TC-LMP-009-01` (UNIT, AUTO): State machine: all four overlap cases
  - `TC-LMP-009-02` (SYS, AUTO): Two overlap cases on hardware with log timestamps


## 5.3 Communication requirements (COM)

### COM-001: LightingCommand timeout and loss behavior

- **Statement:** If no valid LightingCommand has been received for 300 ms (TH-04), the Lamp ECU shall set CMD_TIMEOUT, enter COMM_LOST, and fade all outputs to 0 over 1000 ms (TH-13). It shall stay at 0 until recovery (COM-006).
- **Priority:** M
- **Input and expected output:** Input: BCM stops transmitting. Output: lamp fades off and stays off.
- **Verification method:** T
- **Acceptance criteria:** CMD_TIMEOUT is set between 300 and 310 ms after the last valid frame. The output reaches 0 within 310 ms + 1000 ms + 20 ms of the last valid frame. TimeoutCount increases by 1. Measured values are reported, not the design numbers.
- **Related software component:** TimeoutMonitor, LampControl, FaultManager
- **Planned test cases:**
  - `TC-COM-001-01` (UNIT, AUTO): Timeout monitor with simulated time: boundary at 299, 300, 310 ms
  - `TC-COM-001-02` (SYS, AUTO): Stop BCM TX by fault injection, measure latency with observer and log

### COM-002: Rejection of invalid values

- **Statement:** The Lamp ECU shall reject a frame with an out-of-range or reserved value (LightingMode above 1, BrightnessPercent above 100, VehicleLockState above 1, reserved bits not zero). A rejected frame shall leave the previous valid state in place, shall not reset the timeout, shall increase RxRejectedCount, and shall set INVALID_VALUE.
- **Priority:** M
- **Input and expected output:** Input: frame with invalid values. Output: state unchanged, diagnostics updated.
- **Verification method:** T
- **Acceptance criteria:** For every invalid case, the lamp output and LampState are unchanged for 1 s, RxRejectedCount increases by exactly 1 per frame, INVALID_VALUE is set.
- **Related software component:** CommandValidator
- **Planned test cases:**
  - `TC-COM-002-01` (UNIT, AUTO): Validator ranges at the boundary and one past it for every signal
  - `TC-COM-002-02` (SYS, AUTO): Injected out-of-range frames, check output and counters

### COM-003: Identifier and length handling

- **Statement:** The Lamp ECU shall ignore frames whose identifier is not 0x200 or 0x210 without changing any state or counter. A frame with an expected identifier and an unexpected DLC shall be rejected, counted in RxRejectedCount and set DLC_FAIL.
- **Priority:** M
- **Input and expected output:** Input: wrong identifier, wrong DLC. Output: ignore or reject as stated.
- **Verification method:** T
- **Acceptance criteria:** A frame with identifier 0x201 changes no counter. A 0x200 frame with DLC 7 increases RxRejectedCount by 1 and sets DLC_FAIL.
- **Related software component:** CanDrv (filter), CommandValidator
- **Planned test cases:**
  - `TC-COM-003-01` (INT, AUTO): Inject wrong identifier and wrong DLC frames, compare counters before and after
  - `TC-COM-003-02` (UNIT, INSP): Inspect the filter configuration against the message list

### COM-004: Alive counter and CRC-8 checks

- **Statement:** The Lamp ECU shall reject a frame whose CRC-8 does not match (CRC_FAIL) and a frame whose AliveCounter equals that of the previous accepted frame of the same message (ALIVE_FAIL). The first frame after startup or recovery has no previous value and is accepted if everything else is valid. This is an application-level check and is not AUTOSAR E2E.
- **Priority:** M
- **Input and expected output:** Input: frames with a bad CRC or a repeated counter. Output: rejection and fault bits.
- **Verification method:** T
- **Acceptance criteria:** CRC of ASCII 123456789 equals the documented check value (verify the value with a trusted tool first). A repeated counter is rejected. Counter wrap 15 to 0 is accepted. A counter jump of more than 1 is accepted and does not set ALIVE_FAIL (this is a documented limitation).
- **Related software component:** Protocol, CommandValidator
- **Planned test cases:**
  - `TC-COM-004-01` (UNIT, AUTO): CRC check vector, corrupt each byte in turn, counter repeat, counter wrap
  - `TC-COM-004-02` (SYS, AUTO): Bad CRC and frozen counter injected from the BCM

### COM-005: Status reporting

- **Statement:** The Lamp ECU shall transmit LampStatus (0x300, DLC 4) every 100 ms and DiagnosticStatus (0x310, DLC 8) every 500 ms (TH-03) with the content defined in the CAN specification. OutputBrightnessPercent shall report the actual output.
- **Priority:** M
- **Input and expected output:** Input: running Lamp ECU. Output: cyclic status frames.
- **Verification method:** T
- **Acceptance criteria:** In a 60 s observer capture the LampStatus intervals are 90 to 110 ms and DiagnosticStatus intervals are 450 to 550 ms. The decoded LampState and brightness match the Lamp ECU log at the same time within one update.
- **Related software component:** TxScheduler, Diagnostics
- **Planned test cases:**
  - `TC-COM-005-01` (SYS, AUTO): Observer capture: intervals and decoded content versus the UART log

### COM-006: Recovery rule

- **Statement:** After COMM_LOST, the Lamp ECU shall resume following commands only after N = 2 consecutive valid LightingCommand frames have been received (TH-05). A rejected frame or a gap longer than the timeout between those frames resets the count. After recovery the output fades to the latest valid command using its FadeTime. CMD_TIMEOUT is cleared at recovery.
- **Priority:** M
- **Input and expected output:** Input: BCM resumes transmitting. Output: documented recovery.
- **Verification method:** T
- **Acceptance criteria:** With a valid resume the output stays at 0 until the second valid frame. A rejected frame between the first and second valid frames delays recovery. CMD_TIMEOUT clears at recovery.
- **Related software component:** TimeoutMonitor, LampControl, FaultManager
- **Planned test cases:**
  - `TC-COM-006-01` (UNIT, AUTO): Fault state machine: recovery, interrupted recovery
  - `TC-COM-006-02` (SYS, AUTO): Stop and resume BCM TX, plus a corrupted frame during recovery

### COM-007: Bus error and bus-off handling

- **Statement:** The Lamp ECU shall detect bus-off, set BUSOFF, treat the situation as communication loss (COM-001), and attempt a restart of the CAN peripheral no more often than every 1000 ms (TH-14). Application-level validation shall not be presented as protection against every CAN fault.
- **Priority:** A
- **Input and expected output:** Input: bus-off condition. Output: detection, safe output, restart attempts.
- **Verification method:** T, I
- **Acceptance criteria:** If bus-off can be reproduced with the available equipment: BUSOFF is set, the output is 0, and restart attempts are no closer than 1000 ms. If it cannot be reproduced, the code and logic are reviewed and the test is reported as not executed.
- **Related software component:** CanDrv, FaultManager
- **Planned test cases:**
  - `TC-COM-007-01` (SYS, MANUAL): Attempt to induce bus-off (for example short CANH to CANL briefly with documented safe method), record result honestly
  - `TC-COM-007-02` (SYS, INSP): Code review of the bus-off detection and restart path

### COM-008: VehicleStatus timeout

- **Statement:** If no valid VehicleStatus has been received for 300 ms the Lamp ECU shall set VEH_TIMEOUT, treat the door as closed and the lock state as unchanged, and end any courtesy or welcome overlay with a fade-out over 1000 ms to the base output (TH-13). The base layer continues to follow valid LightingCommand frames.
- **Priority:** M
- **Input and expected output:** Input: VehicleStatus stops while LightingCommand continues. Output: overlays end, base continues.
- **Verification method:** T
- **Acceptance criteria:** VEH_TIMEOUT is set within 300 to 310 ms. A courtesy light that was on at the time ends within 1000 ms plus 20 ms. The base output is unaffected. VEH_TIMEOUT clears after the next valid VehicleStatus.
- **Related software component:** TimeoutMonitor, LampControl
- **Planned test cases:**
  - `TC-COM-008-01` (UNIT, AUTO): Timeout monitor and state machine: VehicleStatus lost during courtesy
  - `TC-COM-008-02` (SYS, AUTO): BCM fault injection stops VehicleStatus only

### COM-009: Startup baseline for events

- **Statement:** The first valid VehicleStatus after reset or after a VehicleStatus timeout shall set the baseline. The door state is applied as a level (DoorOpen = 1 starts the courtesy output). The lock state is only a baseline and shall not trigger the welcome sequence.
- **Priority:** M
- **Input and expected output:** Input: Lamp ECU resets while the door is open and the vehicle is unlocked. Output: courtesy on, no welcome.
- **Verification method:** T
- **Acceptance criteria:** After reset with DoorOpen = 1 the courtesy output starts, and no welcome sequence runs. After reset with the vehicle UNLOCKED and the door closed, nothing starts.
- **Related software component:** LampControl (lamp_fsm)
- **Planned test cases:**
  - `TC-COM-009-01` (UNIT, AUTO): State machine: first VehicleStatus with door open, with unlocked, after timeout
  - `TC-COM-009-02` (SYS, MANUAL): Reset the Lamp ECU with the door switch open and the unlock state set

### COM-010: Interrupt-safe reception

- **Statement:** The CAN receive interrupt shall only copy the frame into a ring buffer of 8 entries (TH-15). Validation and application processing shall run in the main loop. A buffer overflow shall drop the new frame, increase RxRejectedCount, and set RX_OVERFLOW.
- **Priority:** M
- **Input and expected output:** Input: burst of frames. Output: no state corruption, overflow reported.
- **Verification method:** T, I
- **Acceptance criteria:** A host test of the ring buffer shows correct order and overflow behavior. Inspection shows no validation or application call in the interrupt path.
- **Related software component:** RxQueue, CanDrv
- **Planned test cases:**
  - `TC-COM-010-01` (UNIT, AUTO): Ring buffer: ordering, full, empty, overflow count
  - `TC-COM-010-02` (INT, AUTO): Burst of frames from the BCM fault mode, overflow reported without corrupting state


## 5.4 Diagnostic requirements (DIA)

### DIA-001: Diagnostic counters

- **Statement:** The Lamp ECU shall maintain 16-bit wrapping counters: RxValidCount (valid LightingCommand and VehicleStatus), RxRejectedCount (frames with an expected identifier that failed a check, plus ring buffer overflows), and TimeoutCount (each transition into a timeout of either message).
- **Priority:** M
- **Input and expected output:** Input: known sequence of valid, invalid, and lost frames. Output: matching counter values.
- **Verification method:** T
- **Acceptance criteria:** After a scripted sequence of 10 valid frames, 6 invalid frames (one per case) and 1 timeout of each message, the decoded counters are exactly 10, 6 and 2. Counters wrap from 65535 to 0.
- **Related software component:** Diagnostics
- **Planned test cases:**
  - `TC-DIA-001-01` (UNIT, AUTO): Counter increments and wrap
  - `TC-DIA-001-02` (SYS, AUTO): Scripted injection sequence, DiagnosticStatus decoded

### DIA-002: Fault bits

- **Statement:** FaultBits in DiagnosticStatus shall be defined as: bit0 CMD_TIMEOUT (active), bit1 INVALID_VALUE (latched), bit2 CRC_FAIL (latched), bit3 ALIVE_FAIL (latched), bit4 BUSOFF (latched), bit5 VEH_TIMEOUT (active), bit6 DLC_FAIL (latched), bit7 RX_OVERFLOW (latched). Active bits follow the current condition. Latched bits stay set until reset.
- **Priority:** M
- **Input and expected output:** Input: fault conditions. Output: bit set or cleared as defined.
- **Verification method:** T
- **Acceptance criteria:** Each fault is injected alone and only its bit changes. Active bits clear when the condition clears. Latched bits persist until the Lamp ECU is reset.
- **Related software component:** FaultManager, Diagnostics
- **Planned test cases:**
  - `TC-DIA-002-01` (UNIT, AUTO): Fault manager: set, clear, latch behavior for each bit
  - `TC-DIA-002-02` (SYS, AUTO): Single-fault injection, decoded FaultBits compared

### DIA-003: Event log format

- **Statement:** Each ECU shall write each state change, accepted or rejected frame, and fault event to its UART in the form: [<ms since reset>] <MODULE> <event> <key=value ...>.
- **Priority:** M
- **Input and expected output:** Input: normal and fault operation. Output: machine-parsable log lines.
- **Verification method:** I, T
- **Acceptance criteria:** A parser in the test runner extracts 100 percent of the lines produced in a 60 s run without error. Timestamps are non-decreasing.
- **Related software component:** Log
- **Planned test cases:**
  - `TC-DIA-003-01` (UNIT, INSP): Log module output format against the documented pattern
  - `TC-DIA-003-02` (SYS, AUTO): Parse a 60 s capture with the test runner parser


## 5.5 BCM requirements (BCM)

### BCM-001: Input debouncing

- **Statement:** The BCM shall sample each input every 10 ms and change the debounced state only after 3 consecutive equal samples (30 ms) (TH-07).
- **Priority:** M
- **Input and expected output:** Input: bouncing contacts. Output: one clean state change per real press.
- **Verification method:** T
- **Acceptance criteria:** A host test with a 2-sample glitch shows no state change and a 3-sample change shows exactly one. On hardware a press and release of each button produces one event each.
- **Related software component:** Debounce
- **Planned test cases:**
  - `TC-BCM-001-01` (UNIT, AUTO): Debounce: 1 and 2 sample glitches, 3 sample change, long hold, rapid toggling
  - `TC-BCM-001-02` (SYS, MANUAL): 20 presses of each button, count events in the log

### BCM-002: Input mapping

- **Statement:** The BCM shall map inputs as follows: door switch level to DoorOpen; enable button toggles LampEnable; mode button cycles LightingMode 0, 1; brightness button cycles BrightnessPercent through 25, 50, 75, 100; color button cycles the palette white (255,255,255), red (255,0,0), green (0,255,0), blue (0,0,255), amber (255,120,0); unlock button toggles VehicleLockState between LOCKED and UNLOCKED.
- **Priority:** M
- **Input and expected output:** Input: button presses. Output: matching input state.
- **Verification method:** T
- **Acceptance criteria:** Each button press produces exactly the next value in its sequence and wraps from the last to the first.
- **Related software component:** InputState
- **Planned test cases:**
  - `TC-BCM-002-01` (UNIT, AUTO): Input state transitions and wrap for each input
  - `TC-BCM-002-02` (SYS, MANUAL): Press through each sequence on hardware, compare the BCM log

### BCM-003: HAL-independent command generation

- **Statement:** The BCM shall build the LightingCommand and VehicleStatus content in a pure function from the input state (no HAL access), so it can run in a host unit test.
- **Priority:** M
- **Input and expected output:** Input: input state structure. Output: command structure.
- **Verification method:** T, I
- **Acceptance criteria:** The module compiles on a PC with no HAL header. Host tests cover every input value and the default state.
- **Related software component:** BcmCmd
- **Planned test cases:**
  - `TC-BCM-003-01` (UNIT, AUTO): Command builder across all input values
  - `TC-BCM-003-02` (UNIT, INSP): Check includes of the BcmCmd sources

### BCM-004: Transmit scheduling

- **Statement:** The BCM shall transmit LightingCommand and VehicleStatus every 100 ms, and VehicleStatus additionally within 20 ms of a debounced change of door or lock state (TH-19). AliveCounter shall increase by 1 modulo 16 per transmission of that message and CRC-8 shall be computed over the payload.
- **Priority:** M
- **Input and expected output:** Input: input change. Output: immediate VehicleStatus plus the cyclic frames.
- **Verification method:** T
- **Acceptance criteria:** Observer timestamps show an event frame within 20 ms of the BCM log timestamp of the debounced change. The alive counter sequence has no gaps from event frames.
- **Related software component:** TxScheduler, Protocol
- **Planned test cases:**
  - `TC-BCM-004-01` (UNIT, AUTO): Scheduler with simulated time: cyclic and event TX, alive counter
  - `TC-BCM-004-02` (SYS, AUTO): Observer timestamps versus BCM log

### BCM-005: BCM startup defaults

- **Statement:** At startup the BCM shall use: LampEnable 0, LightingMode 0, color white, BrightnessPercent 50, FadeTime 50 (500 ms), VehicleLockState LOCKED, and the door state read from the switch.
- **Priority:** M
- **Input and expected output:** Input: BCM reset. Output: defined first frames.
- **Verification method:** T
- **Acceptance criteria:** The first transmitted LightingCommand after reset contains exactly these values.
- **Related software component:** InputState, BcmCmd
- **Planned test cases:**
  - `TC-BCM-005-01` (UNIT, AUTO): Default input state and the resulting command
  - `TC-BCM-005-02` (SYS, AUTO): Observer capture of the first frames after BCM reset

### BCM-006: Transmit failure handling

- **Statement:** If no CAN transmit mailbox is free, the BCM shall skip that transmission, count it in a TxDroppedCount, and log it. It shall not block.
- **Priority:** M
- **Input and expected output:** Input: transmit mailboxes full. Output: skipped frame, counter, log, no stall.
- **Verification method:** T
- **Acceptance criteria:** With a simulated full mailbox the scheduler returns within one loop pass and the counter increases.
- **Related software component:** TxScheduler, CanDrv
- **Planned test cases:**
  - `TC-BCM-006-01` (UNIT, AUTO): Scheduler with a stub driver returning busy
  - `TC-BCM-006-02` (INT, MANUAL): Run the BCM alone on the bus (no acknowledgement) and observe behavior and log


## 5.6 Hardware requirements (HW)

### HW-001: LED driver current

- **Statement:** Each LED color channel shall be switched by a transistor, and the on-state LED current shall not exceed 10 mA (TH-16). A GPIO pin shall only supply the transistor base current.
- **Priority:** M
- **Input and expected output:** Input: channel on. Output: bounded current.
- **Verification method:** I, T
- **Acceptance criteria:** A multimeter in series measures at most 10 mA per channel at 5 V supply. Base current computed from the schematic is at most 3 mA.
- **Related software component:** Hardware
- **Planned test cases:**
  - `TC-HW-001-01` (HW, MANUAL): Measure current per channel, photograph the setup
  - `TC-HW-001-02` (HW, INSP): Review the schematic against section 5.3 of the handbook

### HW-002: Bus termination

- **Statement:** The CAN bus shall have exactly two 120 ohm terminations at its two physical ends. With power off, the resistance between CANH and CANL shall be 54 to 66 ohm (TH-17).
- **Priority:** M
- **Input and expected output:** Input: assembled bus, power off. Output: about 60 ohm.
- **Verification method:** T
- **Acceptance criteria:** A multimeter reading between CANH and CANL is within 54 to 66 ohm.
- **Related software component:** Hardware
- **Planned test cases:**
  - `TC-HW-002-01` (HW, MANUAL): Multimeter measurement with power off, photograph


## 5.7 Test and process requirements (TST)

### TST-001: Traceability

- **Statement:** Every requirement shall have at least one verification method and at least one planned test case in the traceability matrix.
- **Priority:** M
- **Input and expected output:** Input: SRS and RTM. Output: consistent documents.
- **Verification method:** I
- **Acceptance criteria:** rtm_check.py reports that the requirement IDs in the SRS and the RTM are identical and that each has at least one test case.
- **Related software component:** Process
- **Planned test cases:**
  - `TC-TST-001-01` (PROC, AUTO): Run tools/rtm_check.py

### TST-002: Host unit test run

- **Statement:** All host unit tests shall run with one command and return a non-zero exit code if any test fails.
- **Priority:** M
- **Input and expected output:** Input: clean clone. Output: pass or fail with exit code.
- **Verification method:** T
- **Acceptance criteria:** The command runs on a clean clone and exits 0 on pass. A deliberately broken test makes it exit non-zero.
- **Related software component:** Process
- **Planned test cases:**
  - `TC-TST-002-01` (PROC, AUTO): Run the test command, then break one test on purpose and run again

### TST-003: Automated test report

- **Statement:** A Python test runner should execute the automatable test cases, record PASS or FAIL against expected values, and write a JSON report and a text summary.
- **Priority:** O
- **Input and expected output:** Input: connected hardware and observer. Output: report files.
- **Verification method:** T
- **Acceptance criteria:** One run produces a JSON file with one record per test case (ID, requirement IDs, expected, observed, result, firmware version) and a summary count.
- **Related software component:** tools/test_runner
- **Planned test cases:**
  - `TC-TST-003-01` (PROC, AUTO): Runner self-test with a simulated target

### TST-004: Test records

- **Statement:** Each executed test shall be recorded with the test ID, requirement IDs, date, firmware git hash, procedure, expected result, observed result, and PASS or FAIL. Tests that could not be executed shall be recorded as NOT EXECUTED with the reason.
- **Priority:** M
- **Input and expected output:** Input: executed tests. Output: complete records.
- **Verification method:** I
- **Acceptance criteria:** Every row in the final validation report has all fields filled in.
- **Related software component:** Process
- **Planned test cases:**
  - `TC-TST-004-01` (PROC, INSP): Review the validation report against the required fields

### TST-005: BCM fault injection

- **Statement:** The BCM shall provide UART commands to inject: wrong identifier, wrong DLC, bad CRC, frozen alive counter, out-of-range mode, out-of-range brightness, reserved bits set, stop all transmission, stop VehicleStatus only, and resume normal operation.
- **Priority:** M
- **Input and expected output:** Input: UART command. Output: the matching fault on the bus.
- **Verification method:** T
- **Acceptance criteria:** Each command produces the described frame behavior on the observer. The injection code is excluded from, or clearly marked in, normal-operation builds.
- **Related software component:** BCM FaultInjection
- **Planned test cases:**
  - `TC-TST-005-01` (INT, AUTO): Each injection command, observer capture checked against the expected frame


## 5.8 Documentation requirements (DOC)

### DOC-001: AUTOSAR scope disclosure

- **Statement:** The README shall state that the project is AUTOSAR-inspired and not an AUTOSAR implementation, shall list the standard modules that are not implemented, and shall make no AUTOSAR or ISO 26262 compliance claim.
- **Priority:** M
- **Input and expected output:** Input: README. Output: accurate disclosure.
- **Verification method:** I
- **Acceptance criteria:** The README contains the scope statement and the concept mapping table from the handbook section 10.
- **Related software component:** Documentation
- **Planned test cases:**
  - `TC-DOC-001-01` (PROC, INSP): Review the README against the checklist

### DOC-002: Reproducible build

- **Statement:** The repository shall contain the build and flash instructions, the tool versions, and the package versions needed to reproduce both firmware builds and the host tests from a clean clone.
- **Priority:** M
- **Input and expected output:** Input: clean clone. Output: successful builds.
- **Verification method:** T
- **Acceptance criteria:** Another person (or a fresh machine) builds both firmwares and runs the host tests following only the repository instructions.
- **Related software component:** Documentation
- **Planned test cases:**
  - `TC-DOC-002-01` (PROC, MANUAL): Clean clone build on a second machine or a fresh user account


## 6. Requirement summary

| ID | Title | Pri | Ver | Test cases |
|---|---|---|---|---|
| SYS-001 | Cyclic LightingCommand transmission | M | T | TC-SYS-001-01 |
| SYS-002 | Validation before use | M | T | TC-SYS-002-01, TC-SYS-002-02 |
| SYS-003 | Safe state after reset | M | T, I | TC-SYS-003-01, TC-SYS-003-02 |
| SYS-004 | Common bitrate | M | I, T | TC-SYS-004-01, TC-SYS-004-02, TC-SYS-004-03 |
| SYS-005 | Non-blocking scheduling | M | I, T | TC-SYS-005-01, TC-SYS-005-02 |
| LMP-001 | ON and OFF control | M | T | TC-LMP-001-01, TC-LMP-001-02 |
| LMP-002 | RGB output by hardware PWM | M | I, T | TC-LMP-002-01, TC-LMP-002-02 |
| LMP-003 | Brightness scaling | M | T | TC-LMP-003-01, TC-LMP-003-02 |
| LMP-004 | Lighting modes | M | T | TC-LMP-004-01, TC-LMP-004-02 |
| LMP-005 | Non-blocking linear fades | M | T | TC-LMP-005-01, TC-LMP-005-02 |
| LMP-006 | Courtesy light | M | T | TC-LMP-006-01, TC-LMP-006-02 |
| LMP-007 | Welcome light | M | T | TC-LMP-007-01, TC-LMP-007-02 |
| LMP-008 | Breathing waveform | M | T | TC-LMP-008-01, TC-LMP-008-02 |
| LMP-009 | Output layer precedence | M | T | TC-LMP-009-01, TC-LMP-009-02 |
| COM-001 | LightingCommand timeout and loss behavior | M | T | TC-COM-001-01, TC-COM-001-02 |
| COM-002 | Rejection of invalid values | M | T | TC-COM-002-01, TC-COM-002-02 |
| COM-003 | Identifier and length handling | M | T | TC-COM-003-01, TC-COM-003-02 |
| COM-004 | Alive counter and CRC-8 checks | M | T | TC-COM-004-01, TC-COM-004-02 |
| COM-005 | Status reporting | M | T | TC-COM-005-01 |
| COM-006 | Recovery rule | M | T | TC-COM-006-01, TC-COM-006-02 |
| COM-007 | Bus error and bus-off handling | A | T, I | TC-COM-007-01, TC-COM-007-02 |
| COM-008 | VehicleStatus timeout | M | T | TC-COM-008-01, TC-COM-008-02 |
| COM-009 | Startup baseline for events | M | T | TC-COM-009-01, TC-COM-009-02 |
| COM-010 | Interrupt-safe reception | M | T, I | TC-COM-010-01, TC-COM-010-02 |
| DIA-001 | Diagnostic counters | M | T | TC-DIA-001-01, TC-DIA-001-02 |
| DIA-002 | Fault bits | M | T | TC-DIA-002-01, TC-DIA-002-02 |
| DIA-003 | Event log format | M | I, T | TC-DIA-003-01, TC-DIA-003-02 |
| BCM-001 | Input debouncing | M | T | TC-BCM-001-01, TC-BCM-001-02 |
| BCM-002 | Input mapping | M | T | TC-BCM-002-01, TC-BCM-002-02 |
| BCM-003 | HAL-independent command generation | M | T, I | TC-BCM-003-01, TC-BCM-003-02 |
| BCM-004 | Transmit scheduling | M | T | TC-BCM-004-01, TC-BCM-004-02 |
| BCM-005 | BCM startup defaults | M | T | TC-BCM-005-01, TC-BCM-005-02 |
| BCM-006 | Transmit failure handling | M | T | TC-BCM-006-01, TC-BCM-006-02 |
| HW-001 | LED driver current | M | I, T | TC-HW-001-01, TC-HW-001-02 |
| HW-002 | Bus termination | M | T | TC-HW-002-01 |
| TST-001 | Traceability | M | I | TC-TST-001-01 |
| TST-002 | Host unit test run | M | T | TC-TST-002-01 |
| TST-003 | Automated test report | O | T | TC-TST-003-01 |
| TST-004 | Test records | M | I | TC-TST-004-01 |
| TST-005 | BCM fault injection | M | T | TC-TST-005-01 |
| DOC-001 | AUTOSAR scope disclosure | M | I | TC-DOC-001-01 |
| DOC-002 | Reproducible build | M | T | TC-DOC-002-01 |

Totals: 42 requirements (A: 1, M: 40, O: 1).
Total planned test cases: 75.

---

## 7. Decisions that need your approval

The Phase 1 gate is your approval of these. Reply with "approve all" or list the numbers you want changed and the new value. A change here changes the requirements and the CAN draft, so decide now, before code exists.

| Ref | Decision | Main alternative | Why I chose it | Your answer |
|---|---|---|---|---|
| A1 | LightingMode is STATIC (0) or BREATHING (1). Courtesy and welcome are event-driven overlays driven by VehicleStatus. | BCM commands "welcome" and "courtesy" as modes | The Lamp ECU owns the lamp state machine, which is the interesting software. Events are properties of the vehicle, modes are properties of the lamp | |
| A2 | Door is level-triggered (courtesy while open). Unlock is edge-triggered. | Both edge-triggered | A lost event frame cannot leave the lamp in the wrong state for a level signal | |
| A3 | Courtesy: white, 80 percent, 300 ms fade-in, 5 s hold after close, 1 s fade-out | Different values | See TH-10 | |
| A4 | Welcome: 1 s in, 2 s hold, 1 s out. Default color (0,100,255) at 60 percent when LampEnable = 0 | Welcome only when enabled | Visible even with ambient light off | |
| A5 | Timeout 300 ms, recovery after 2 valid frames. LightingCommand loss turns everything off. VehicleStatus loss only ends overlays | Hold last state | Failure is visible and nothing stale keeps driving the output | |
| A6 | PWM 2 kHz, 1024 counts | 1 kHz, 8 bits | Smoother low-brightness fades, no visible flicker | |
| A7 | Alive counter and CRC-8 are mandatory (promoted from optional). Only a repeated counter is rejected. Jumps are accepted and documented as a limitation | Reject jumps larger than 1 | Rejecting jumps would treat one lost frame as a fault and make recovery harder | |
| A8 | BREATHING is mandatory (promoted from optional) | Keep optional and ship only STATIC | The specification asks for multiple lighting modes. A triangle wave is cheap | |
| A9 | Fault bits: active bits follow the condition, latched bits persist until reset | All bits latched | Active bits show current state, latched bits keep history for the test report | |
| A10 | Door input is a toggle or slide switch. Unlock is a toggle button (each press flips LOCKED and UNLOCKED) | Momentary pulses | Models state, not events | |
| A11 | BCM defaults: disabled, STATIC, white, 50 percent, FadeTime 500 ms, LOCKED | Other defaults | Dark at power-up, same as the Lamp ECU safe state | |
| A12 | COM-007 (bus-off) is Advanced and may end as NOT EXECUTED with a recorded reason | Make it mandatory | Bus-off may not be reproducible safely with this equipment | |

---

## 8. Changes from handbook v0.2

The handbook draft is superseded where it differs from this list.

| Area | Handbook v0.2 | This SRS |
|---|---|---|
| LightingMode values | 0 STATIC, 1 WELCOME, 2 COURTESY, 3 BREATHING | 0 STATIC, 1 BREATHING. Values above 1 are rejected |
| FaultBits | bits 0 to 4 | bit5 VEH_TIMEOUT, bit6 DLC_FAIL, bit7 RX_OVERFLOW added. Active versus latched defined |
| Requirement IDs | LMP-007 was breathing, 25 requirements | LMP-007 is welcome, LMP-008 breathing, LMP-009 precedence. 42 requirements |
| Priorities | Alive counter and CRC optional. Breathing optional | Both mandatory (A7, A8) |
| Test case ID scheme | TC-001, TC-002, ... | TC-<requirement id>-<nn> |
| Duty full scale | 0 to 1023 counts implied | 0 to 1024 counts, where 1024 means constantly high **[B]** |
| Overlay precedence | Not defined | Section 3.1 |
| VehicleStatus timeout | Not defined | COM-008 |
| Startup baseline for events | Not defined | COM-009 |
| Receive path | Ring buffer mentioned | COM-010 |
| BCM requirements | 2 | 6 (BCM-001 to BCM-006) |
| Hardware requirements | None | HW-001, HW-002 |

---

## 9. Dependencies on Phase 0 results (not yet received)

This SRS was written before your Phase 0 results arrived. These assumptions could invalidate parts of it:

| Phase 0 finding | What would change |
|---|---|
| LED is common cathode | The LED driver circuit changes (HW-001 text, handbook section 5.3). PWM polarity handling may need to be inverted in the driver |
| TIM3 clock is not 90 MHz, or the clock tree differs | TH-09 prescaler values, SYS-004 bit timing |
| No observer hardware (no USB-CAN adapter, no ESP32) | Every test case marked AUTO that needs the observer becomes MANUAL or UART-only evidence, and the README must say so |
| Transceiver modules are 5 V parts | Hardware changes before any CAN work |
| Blue Pill is a clone or has different flash size | BCM memory budget (fault injection and logging add code) |
| HSE bypass on the Nucleo does not run the clock as expected | Phase 5 timing, TH-09, TH-20 |
| Button bounce measured longer than 30 ms | TH-07 and BCM-001 |

