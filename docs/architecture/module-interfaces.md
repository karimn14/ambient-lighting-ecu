# Module Interface Map

**Version:** 0.1 (Phase 2, interfaces only, no implementations yet)
**Date:** 2026-10-21

Every module below has a header with its contract in `firmware/`. Phase 2 writes the interfaces. Implementations come in Phases 3 to 5, test-first for the host-testable modules.

Layer ranks: Neutral (0), Drivers (1), Communication (2), Services (3), Application (4). Calls go downward only (coding-conventions.md section 3.1).

---

## 1. Shared modules (`firmware/common/`)

| Header | Layer | Used by | Host-testable | Hardware access | Requirements served |
|---|---|---|---|---|---|
| `status.h` | Neutral | both | n/a (types) | no | all (error convention) |
| `can_frame.h` | Neutral | both | n/a (types) | no | SYS-004, COM-003 |
| `lamp_types.h` | Neutral | both | n/a (types) | no | LMP-002, LMP-003 |
| `crc8.h` | Communication | both | yes | no | COM-004 |
| `protocol.h` | Communication | both | yes | no | SYS-001, COM-004, COM-005, DIA-002 |
| `rx_queue.h` | Communication | Lamp | yes | no | COM-010 |
| `tx_scheduler.h` | Communication | both | yes (stub send) | no | SYS-001, COM-005, BCM-004, BCM-006 |
| `scheduler.h` | Services | both | yes | no | SYS-005 |
| `log.h` | Services | both | yes (stub sink) | no | DIA-003 |
| `can_drv.h` | Drivers (interface) | both | stub | implementation in `Drivers_App` | SYS-004, COM-003, COM-007 |
| `uart_drv.h` | Drivers (interface) | both | stub | implementation in `Drivers_App` | DIA-003, TST-005 |

## 2. Lamp ECU (`firmware/lamp_ecu/`)

| Header | Folder | Layer | Host-testable | Hardware access | Requirements served |
|---|---|---|---|---|---|
| `lamp_config.h` | Application | Application | yes (constants) | no | all TH values |
| `color.h` | Application | Application | yes | no | LMP-003 |
| `fade_engine.h` | Application | Application | yes | no | LMP-005, LMP-008 |
| `lamp_fsm.h` | Application | Application | yes | no | LMP-001, LMP-004, LMP-006, LMP-007, LMP-009, COM-008, COM-009 |
| `command_validator.h` | Application | Application | yes | no | SYS-002, COM-002, COM-003, COM-004 |
| `fault_manager.h` | Application | Application | yes | no | COM-001, COM-006, COM-007, COM-008, DIA-002 |
| `lamp_app.h` | Application | Application (composition) | partly (orchestration needs stubs) | via drivers | SYS-003, SYS-005, COM-010 |
| `timeout_monitor.h` | Services | Services | yes | no | COM-001, COM-008 |
| `diagnostics.h` | Services | Services | yes | no | DIA-001, DIA-002 |
| `led_pwm.h` | Drivers_App | Drivers (interface) | stub | TIM3 PWM | LMP-002, SYS-003, HW-001 |

## 3. BCM (`firmware/bcm/`)

| Header | Folder | Layer | Host-testable | Hardware access | Requirements served |
|---|---|---|---|---|---|
| `bcm_config.h` | Application | Application | yes (constants) | no | BCM-001, BCM-002, BCM-004, BCM-005 |
| `debounce.h` | Application | Application | yes | no | BCM-001 |
| `input_state.h` | Application | Application | yes | no | BCM-002, BCM-005 |
| `bcm_cmd.h` | Application | Application | yes | no | BCM-003 |
| `fault_injection.h` | Application | Application | yes (when enabled) | no | TST-005 |
| `bcm_app.h` | Application | Application (composition) | partly | via drivers | BCM-004, BCM-006 |
| `gpio_in.h` | Drivers_App | Drivers (interface) | stub | GPIO inputs | BCM-001, BCM-002 |

---

## 4. Execution order (decision AD-07)

The composition modules fix the order of work in each 10 ms pass. A fixed order makes behavior deterministic and testable.

**Lamp ECU** (`LampApp_Run10ms`):

1. Drain the RxQueue. For each frame: validate, then update FaultManager, Diagnostics, TimeoutMonitor, and LampFsm. When a monitor enters its lost state, forget that message's alive counter.
2. Update the timeout monitors and report transitions.
3. Handle new queue overflows and the CAN driver status (bus-off).
4. Step the LampFsm, apply its request to the FadeEngine, compute the output, write it with LedPwm.
5. Run the TxScheduler.
6. Log, then flush the log.

**BCM** (`BcmApp_Run10ms`): sample and debounce inputs, update InputState, request an event VehicleStatus on a door or lock change, read the console, run the TxScheduler, flush the log.

---

## 5. Open design questions found while writing the interfaces

These are not blocking Phase 2. Each must be decided before the module that depends on it is implemented. None changes an approved SRS requirement without your say.

| # | Question | Where it matters | My proposal |
|---|---|---|---|
| Q1 | When the LightingCommand monitor goes from COMM_RECOVERING back to COMM_LOST because of a gap, is that a new timeout for TimeoutCount? | DIA-001, `diagnostics.h` | Yes. Count every transition into a timeout (documented in `diagnostics.h`). Needs a line in the SRS |
| Q2 | The LampStatus message has no CRC (only 0x200 and 0x210 do). Is that acceptable? | `protocol.h`, Phase 5 CAN spec | Yes for the baseline, because the BCM only logs it. State it in the CAN spec |
| Q3 | Breathing and fades both change the output. The interface lets LampFsm request a fade target plus a breathing flag, and applies the breathing scale on top of the faded value. Is it acceptable that a fade into BREATHING mode is a fade of the unscaled base and breathing starts at the minimum scale? | `lamp_fsm.h`, LMP-004, LMP-008 | Yes. Decide the exact blend in Phase 3 with a host test |
| Q4 | The RxQueue relies on atomic 8-bit and 16-bit accesses and on the compiler not reordering the index update. Is a memory barrier needed? | `rx_queue.h`, COM-010 | Review the generated code in Phase 5 [B] and add a barrier if needed |
| Q5 | An event VehicleStatus increments the same alive counter as the cyclic one, so the receiver never sees a repeated counter. Confirm that this is intended | BCM-004 | Yes. One counter per message, incremented on every transmission |
| Q6 | The log is a singleton with one global buffer. Fine for two small firmwares, but it limits tests to one log instance at a time | `log.h` | Acceptable. `Log_Init` resets it for each host test |
| Q7 | `firmware/common/` mixes modules of different layers. The layer of each header is declared in `tools/check_layering.py`. Is that acceptable or should `common/` be split into per-layer folders? | Repository layout | Keep one folder for now, because it keeps the CubeIDE linked-folder setup to one entry |

---

## 6. What Phase 2 deliberately does not do

- No implementation of any module. The only `.c` files are two include probes, which define one constant each.
- No interface for the UART console parser, because it is part of Phase 3 and Phase 7 and is not yet specified.
- No claim that the interfaces are final. A Phase 3 or 5 implementation may reveal a missing parameter. A header change then needs the same care as a code change: update the header, the contract test if needed, and this table in one commit.
