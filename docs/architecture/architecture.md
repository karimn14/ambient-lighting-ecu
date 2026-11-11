# Architecture

**Version:** 0.2 (Phase 2: layer rule corrected, composition modules and decisions added)
**Date:** 2026-10-21
**Related:** `docs/requirements/SRS.md`, `docs/architecture/module-interfaces.md`, `docs/architecture/coding-conventions.md`, `docs/handbook/engineering-handbook.md` v0.2

**Supersedes** the Phase 1 `architecture.md`. Changes are listed in section 9.

All diagrams are Mermaid text, so they are versioned with the code and render on GitHub. The architecture is **AUTOSAR-inspired**. It is not an AUTOSAR implementation.

---

## 1. System block diagram

```mermaid
flowchart LR
    subgraph INPUTS["Simulated vehicle inputs"]
        DOOR["Door switch"]
        BTN["Buttons: enable, mode, brightness, color, unlock"]
    end

    subgraph BCMN["BCM - STM32 Blue Pill"]
        BCMFW["BCM firmware"]
    end

    subgraph BUS["CAN bus 500 kbit/s"]
        T1["Transceiver 1"]
        T2["Transceiver 2"]
        OBS["Observer: USB-CAN adapter or ESP32"]
    end

    subgraph LAMPN["Lamp ECU - NUCLEO-F446RE"]
        LAMPFW["Lamp ECU firmware"]
    end

    subgraph OUTS["Output stage"]
        DRV["3x NPN low-side switch"]
        LED["RGB LED, common anode, 5 V"]
    end

    DOOR --> BCMFW
    BTN --> BCMFW
    BCMFW <-->|"TX PB9, RX PB8"| T1
    T1 <-->|"CANH, CANL"| T2
    T2 <-->|"TX PB9, RX PB8"| LAMPFW
    OBS -.->|"passive tap"| T2
    LAMPFW -->|"TIM3 PWM x3"| DRV
    DRV --> LED

    BCMFW -->|"UART log"| PC["PC: logs, Python runner"]
    LAMPFW -->|"UART log via ST-LINK VCP"| PC
    OBS --> PC
```

The observer is passive. It must never transmit during normal tests.

---

## 2. ECU interaction diagram

```mermaid
flowchart LR
    BCM["BCM"]
    LAMP["Lamp ECU"]
    OBSV["Observer"]

    BCM -->|"LightingCommand 0x200, 100 ms"| LAMP
    BCM -->|"VehicleStatus 0x210, 100 ms plus on change"| LAMP
    LAMP -->|"LampStatus 0x300, 100 ms"| BCM
    LAMP -->|"DiagnosticStatus 0x310, 500 ms"| BCM

    BCM -.->|"all frames"| OBSV
    LAMP -.->|"all frames"| OBSV
```

Responsibilities across the interface:

| Information | Owner | Consumer | Notes |
|---|---|---|---|
| Door state, lock state | BCM | Lamp ECU | Vehicle facts. The BCM does not decide lamp behavior for door and unlock |
| Lamp enable, mode, color, brightness, fade time | BCM | Lamp ECU | User preference, set by the BCM inputs |
| Lamp state machine, fades, overlay precedence | Lamp ECU | none | Lamp behavior is entirely inside the Lamp ECU |
| Actual output, lamp state, faults | Lamp ECU | BCM, observer | The BCM only logs them in this baseline |

---

## 3. Software architecture

### 3.1 Lamp ECU layers

```mermaid
flowchart TB
    subgraph APP["Application (no HAL includes)"]
        LA["LampApp: composition, fixed order of work"]
        LC["LampControl and lamp_fsm"]
        FE["FadeEngine"]
        CV["CommandValidator"]
        FM["FaultManager"]
        COL["Color (pure scaling functions)"]
    end

    subgraph SVC["Services (no HAL includes)"]
        SCH["Scheduler: 1 ms tick"]
        TM["TimeoutMonitor"]
        DIAG["Diagnostics"]
        LOG["Log"]
    end

    subgraph COM["Communication (no HAL includes)"]
        PROTO["Protocol: pack, unpack, CRC-8"]
        RXQ["RxQueue: ring buffer, 8 frames"]
        TXS["TxScheduler"]
    end

    subgraph DRV["Drivers (the only layer that includes HAL)"]
        LEDPWM["LedPwm"]
        CAND["CanDrv"]
        UARTD["UartDrv"]
    end

    HAL["STM32 HAL and CubeMX Core"]

    SCH --> LA
    LA --> LC
    LA --> TM
    LA --> TXS
    LA --> RXQ
    LC --> FE
    LC --> COL
    LC --> CV
    LC --> FM
    CV --> PROTO
    FM --> DIAG
    TM --> FM
    RXQ --> CV
    TXS --> PROTO
    LC --> LEDPWM
    RXQ --> CAND
    TXS --> CAND
    LOG --> UARTD
    LEDPWM --> HAL
    CAND --> HAL
    UARTD --> HAL
```

### 3.2 BCM layers

```mermaid
flowchart TB
    subgraph BAPP["Application (no HAL includes)"]
        BA["BcmApp: composition, fixed order of work"]
        DEB["Debounce"]
        IST["InputState"]
        BC["BcmCmd: pure command builder"]
        FI["FaultInjection"]
    end

    subgraph BSVC["Services (no HAL includes)"]
        BSCH["Scheduler: 1 ms tick"]
        BLOG["Log"]
    end

    subgraph BCOM["Communication (no HAL includes)"]
        BPROTO["Protocol: pack, CRC-8"]
        BTXS["TxScheduler"]
    end

    subgraph BDRV["Drivers (the only layer that includes HAL)"]
        GPIOD["GpioIn"]
        BCAND["CanDrv"]
        BUARTD["UartDrv"]
    end

    BHAL["STM32 HAL and CubeMX Core"]

    BSCH --> BA
    BA --> DEB
    BA --> BTXS
    DEB --> IST
    IST --> BC
    BC --> BTXS
    FI --> BTXS
    BTXS --> BPROTO
    BTXS --> BCAND
    DEB --> GPIOD
    BLOG --> BUARTD
    GPIOD --> BHAL
    BCAND --> BHAL
    BUARTD --> BHAL
```

### 3.3 Layer rules

1. Calls go **downward only**: a module may use modules of its own layer or of any lower layer, and never a higher layer. Layer ranks: Drivers 1, Communication 2, Services 3, Application 4. (The Phase 1 wording said "directly below". That contradicted the diagrams, where the Application layer calls the drivers. This is the corrected rule.) The rule is enforced by `tools/check_layering.py`.
2. Application, Services, and Communication never include `stm32*.h` or any HAL header. Only Drivers do.
3. Modules that can run on a PC (and therefore have host unit tests): `Protocol`, `crc8`, `Color`, `FadeEngine`, `CommandValidator`, `lamp_fsm`, `TimeoutMonitor`, `FaultManager`, `Diagnostics`, `RxQueue`, `Debounce`, `InputState`, `BcmCmd`, `TxScheduler` (with a stub driver).
4. The CAN receive interrupt only copies the frame into `RxQueue`. It never validates or touches application state.
5. No blocking waits and no `HAL_Delay` outside driver initialization.
6. `firmware/common/` holds modules of several layers. The layer of each header is declared in `tools/check_layering.py`.
7. The composition modules `LampApp` and `BcmApp` sit at the top of the Application layer. They are the only modules that know all the others and they define the order of work in each 10 ms pass (AD-07).

### 3.4 Module responsibilities

| Module | Node | Does | Does not |
|---|---|---|---|
| LedPwm | Lamp | Writes compare values to the timer channels, enforces 0 before start | Know colors, modes, or fades |
| Color | Lamp | Computes duty counts from color and brightness (LMP-003) | Touch hardware |
| FadeEngine | Lamp | Linear fades, breathing waveform, retarget continuity | Decide when to fade |
| lamp_fsm (in LampControl) | Lamp | Chooses the target output from layers (SRS 3.1) and runs overlay timers | Parse frames or write PWM |
| CommandValidator | Lamp | Applies the validation order (SRS 3.3) and returns a result code | Update application state |
| TimeoutMonitor | Lamp | Tracks the time since the last valid frame per message | Decide the lamp output |
| FaultManager | Lamp | Owns COMM_OK, COMM_LOST, COMM_RECOVERING and the fault bits | Drive outputs directly |
| Diagnostics | Lamp | Counters, packing DiagnosticStatus | Read hardware |
| Protocol | Both | Pack and unpack signals, CRC-8 (shared in `firmware/common/`) | Know about state |
| RxQueue | Lamp | Ring buffer between interrupt and main loop | Validate |
| TxScheduler | Both | Cyclic and event timing for outgoing frames, mailbox-busy handling | Compute payload content |
| CanDrv | Both | Init, filters, start, add TX message, fetch RX message, error flags | Know signal layouts |
| Debounce, InputState | BCM | Clean inputs, hold the input state | Know CAN |
| BcmCmd | BCM | Build command content from input state | Touch hardware |
| FaultInjection | BCM | Alter outgoing frames on UART command | Run in normal operation unless commanded |
| Log | Both | Format and buffer log lines | Block |
| LampApp | Lamp | Wires the modules together and runs them in a fixed order every 10 ms | Contain behavior that belongs to a module |
| BcmApp | BCM | Same for the BCM | Contain behavior that belongs to a module |
| Scheduler | Both | Runs periodic tasks from the 1 ms tick, records the worst lateness | Know what the tasks do |

An **application function** decides what should happen ("fade to 40 percent over 500 ms"). A **hardware driver** performs it on this chip ("write 410 to the TIM3 channel 1 compare register"). The line between them is the reason the application can be tested on a PC.

---

## 4. CAN communication sequences

### 4.1 Door opens and closes

```mermaid
sequenceDiagram
    participant IN as Door switch
    participant BCM
    participant BUS as CAN bus
    participant LAMP as Lamp ECU

    IN->>BCM: contact closes
    Note over BCM: debounce 30 ms
    BCM->>BUS: VehicleStatus 0x210, DoorOpen=1 (event frame)
    BUS->>LAMP: frame
    Note over LAMP: validate: id, DLC, CRC, alive, range
    Note over LAMP: state COURTESY, fade in 300 ms
    LAMP->>BUS: LampStatus 0x300 (next cycle)
    BCM->>BUS: LightingCommand 0x200 (every 100 ms)
    IN->>BCM: contact opens
    BCM->>BUS: VehicleStatus 0x210, DoorOpen=0 (event frame)
    Note over LAMP: hold 5000 ms, then fade out 1000 ms
```

### 4.2 Communication loss and recovery

```mermaid
sequenceDiagram
    participant BCM
    participant BUS as CAN bus
    participant LAMP as Lamp ECU

    BCM->>BUS: LightingCommand (valid)
    BUS->>LAMP: valid, timeout timer reset
    Note over BCM: stops transmitting
    Note over LAMP: no valid frame for 300 ms
    Note over LAMP: CMD_TIMEOUT set, state COMM_LOST
    Note over LAMP: fade to 0 over 1000 ms
    Note over BCM: resumes transmitting
    BCM->>BUS: LightingCommand (valid, 1st)
    Note over LAMP: COMM_RECOVERING, output stays 0
    BCM->>BUS: LightingCommand (valid, 2nd)
    Note over LAMP: COMM_OK, CMD_TIMEOUT cleared
    Note over LAMP: fade to commanded output
```

### 4.3 Invalid frame

```mermaid
sequenceDiagram
    participant BCM
    participant BUS as CAN bus
    participant LAMP as Lamp ECU

    BCM->>BUS: LightingCommand with BrightnessPercent=150
    BUS->>LAMP: frame
    Note over LAMP: steps 1 to 4 pass, step 5 fails
    Note over LAMP: reject, INVALID_VALUE set, RxRejectedCount plus 1
    Note over LAMP: state and timeout unchanged
    BCM->>BUS: LightingCommand (valid)
    Note over LAMP: accepted, timeout reset
```

---

## 5. Lamp state machine

LampState values reported in LampStatus: 0 INIT, 1 OFF, 2 ON, 3 WELCOME, 4 COURTESY, 5 COURTESY_FADEOUT, 6 COMM_LOST.

```mermaid
stateDiagram-v2
    [*] --> INIT
    INIT --> OFF: initialization done

    OFF --> ON: valid command with enable 1
    ON --> OFF: valid command with enable 0

    OFF --> WELCOME: unlock transition
    ON --> WELCOME: unlock transition
    WELCOME --> ON: sequence done and enable 1
    WELCOME --> OFF: sequence done and enable 0

    OFF --> COURTESY: door open
    ON --> COURTESY: door open
    WELCOME --> COURTESY: door open aborts welcome
    COURTESY --> COURTESY_FADEOUT: door closed and 5000 ms hold elapsed
    COURTESY_FADEOUT --> COURTESY: door reopened
    COURTESY_FADEOUT --> ON: fade done and enable 1
    COURTESY_FADEOUT --> OFF: fade done and enable 0

    WELCOME --> ON: VehicleStatus timeout and enable 1
    WELCOME --> OFF: VehicleStatus timeout and enable 0
    COURTESY --> ON: VehicleStatus timeout and enable 1
    COURTESY --> OFF: VehicleStatus timeout and enable 0

    OFF --> COMM_LOST: LightingCommand timeout
    ON --> COMM_LOST: LightingCommand timeout
    WELCOME --> COMM_LOST: LightingCommand timeout
    COURTESY --> COMM_LOST: LightingCommand timeout
    COURTESY_FADEOUT --> COMM_LOST: LightingCommand timeout
    COMM_LOST --> OFF: recovery complete after 2 valid frames
```

Notes:

- An unlock transition while in COURTESY or COURTESY_FADEOUT is ignored (LMP-009).
- After `COMM_LOST --> OFF` the normal `OFF --> ON` rule applies at once if the latest valid command has enable 1.
- Overlay end states (`ON` or `OFF`) are chosen from the base command current at that moment.
- In COURTESY and WELCOME the fade phases are internal to the state. They are not separate LampState values.

---

## 6. Communication fault-handling state machine

Two monitors run independently. The LightingCommand monitor owns COMM_LOST. The VehicleStatus monitor only sets VEH_TIMEOUT.

```mermaid
stateDiagram-v2
    [*] --> COMM_STARTUP
    COMM_STARTUP --> COMM_OK: first valid LightingCommand
    COMM_OK --> COMM_LOST: no valid LightingCommand for 300 ms
    COMM_LOST --> COMM_RECOVERING: first valid LightingCommand
    COMM_RECOVERING --> COMM_OK: second consecutive valid LightingCommand
    COMM_RECOVERING --> COMM_LOST: rejected frame or gap over 300 ms
```

```mermaid
stateDiagram-v2
    [*] --> VEH_UNKNOWN
    VEH_UNKNOWN --> VEH_OK: first valid VehicleStatus sets baseline
    VEH_OK --> VEH_LOST: no valid VehicleStatus for 300 ms
    VEH_LOST --> VEH_OK: valid VehicleStatus sets baseline again
```

Bus-off (COM-007, advanced) is an additional input to the LightingCommand monitor: when the CAN peripheral reports bus-off, BUSOFF is latched and the monitor behaves as if the timeout had occurred. Restart attempts are at least 1000 ms apart.

Fault bits (DiagnosticStatus byte 6): bit0 CMD_TIMEOUT (active), bit1 INVALID_VALUE (latched), bit2 CRC_FAIL (latched), bit3 ALIVE_FAIL (latched), bit4 BUSOFF (latched), bit5 VEH_TIMEOUT (active), bit6 DLC_FAIL (latched), bit7 RX_OVERFLOW (latched).

---

## 7. Requirement traceability

The traceability matrix is `docs/test-plans/RTM.csv`. It is generated together with the SRS and checked with `python tools/rtm_check.py`. A row links one requirement to one planned test case with its level (UNIT, INT, SYS, HW, PROC) and mode (AUTO, MANUAL, INSP). The `status` column starts as `planned` and moves to `passed`, `failed`, or `not executed` when real results exist.

---

## 8. Architecture decisions

| ID | Decision | Reason | Cost |
|---|---|---|---|
| AD-01 | Superloop with a 1 ms tick. No RTOS | Predictable, easy to test on a PC | No preemption. Tasks must be short (SYS-005) |
| AD-02 | One shared protocol module for both ECUs | One definition of every signal | Both CubeIDE projects need the shared include path |
| AD-03 | Overlays and base layer are separate, with a fixed precedence | Simple, testable behavior | A new feature needs an explicit precedence decision |
| AD-04 | The interrupt only enqueues | Keeps interrupt time short and state changes in one place | An 8-entry buffer can overflow on a burst, counted by COM-010 |
| AD-05 | Fault monitors are independent per message | A partial loss is visible | Two timers to test |
| AD-06 | Fault injection lives in the BCM firmware | Repeatable negative tests without extra hardware | Test code ships in the same binary. It must be compiled out or clearly gated in a normal build |
| AD-07 | The composition modules run the work in a fixed order each 10 ms pass (module-interfaces.md section 4) | Deterministic, testable behavior | The order is a design commitment. Changing it needs a test and a note |
| AD-08 | Header layer membership is declared in one place (`check_layering.py`) and checked automatically | The architecture rules stay true as the code grows | A new header must be added to the map, otherwise the checker ignores its rank |

---

## 9. Changes from the Phase 1 architecture

| Area | Phase 1 | Phase 2 |
|---|---|---|
| Layer rule | Calls only to the layer directly below | Calls only downward to any lower layer. Enforced by a script |
| Composition modules | Not shown | LampApp and BcmApp added to the diagrams and the module table |
| Scheduler | Listed only for the Lamp ECU | Shared by both ECUs (in `firmware/common/`) |
| Decisions | AD-01 to AD-06 | AD-07 (fixed order of work), AD-08 (layer map checked by script) |

