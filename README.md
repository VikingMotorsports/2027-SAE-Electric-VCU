# Formula SAE EV Vehicle Control System

A distributed **Vehicle Control System (VCS)** for a Formula SAE Electric racecar. The system uses STM32 microcontrollers running Zephyr RTOS for real-time vehicle control, a CAN bus for communication, and a Raspberry Pi for driver and remote telemetry displays.

> **Status:** Active Development

---

## System Overview

The VCS is currently built around two **STM32C092** modules and a Raspberry Pi.

- **Pedal Input Module** — Reads accelerator and brake pedal inputs and transmits them over CAN.
- **Motor Controller Module** — Receives pedal commands over CAN and generates PWM signals for the motor H-bridge.
- **Raspberry Pi Dashboard** — Receives CAN data and displays accelerator and brake inputs to the driver.
- **Remote Pit Dashboard** — Receives vehicle data from the Raspberry Pi over MQTT.

### Architecture

```text

     ┌──────────────────────┐			┌──────────────────────┐
     │   Brake Pedal        │			│   Accelerator Pedal  │
     └──────────┬───────────┘			└──────────┬───────────┘
				│								   │
				└─────────────────┬────────────────┘
                                  │
                                  ▼
                    ┌──────────────────────────┐
                    │   STM32C092              │
                    │   Pedal Input Module     │
                    │                          │
                    │   Zephyr RTOS            │
                    │   ADC → CAN              │
                    └────────────┬─────────────┘
                                 │
                                 │ CAN Bus
                                 │
                ┌────────────────┴────────────────┐
                │                                 │
                ▼                                 ▼
     ┌──────────────────────┐          ┌──────────────────────┐
     │   STM32C092          │          │   Raspberry Pi       │
     │   Motor Controller   │          │   Dashboard          │
     │                      │          │                      │
     │   CAN → PWM          │          │   CAN → Display      │
     └──────────┬───────────┘          └──────────┬───────────┘
                │                                 │
                │ PWM / Control                   │ MQTT
                ▼                                 ▼
     ┌──────────────────────┐          ┌──────────────────────┐
     │   Motor H-Bridge     │          │   Remote Pit         │
     │                      │          │   Dashboard          │
     └──────────────────────┘          └──────────────────────┘
	 

````

---

## Hardware & Software

| Component        | Description                                                |
| ---------------- | ---------------------------------------------------------- |
| **STM32C092**    | Embedded vehicle control modules                           |
| **Zephyr RTOS**  | Real-time operating system for STM32 modules               |
| **CAN Bus**      | Communication between vehicle modules                      |
| **Raspberry Pi** | Dashboard and vehicle telemetry gateway                    |
| **Vulcan**       | CAN interface between the vehicle network and Raspberry Pi |
| **MQTT**         | Remote telemetry communication                             |
| **KiCad**        | Electrical and PCB design                                  |

---

## Repository Structure

```text
.
├── Dashboard/              # Raspberry Pi driver dashboard
├── Dashboard Webdisplay/   # Remote pit dashboard
├── Developers_Guide/       # Development documentation
├── PCB/                  	# PCB and electrical design files
├── MQTT/                   # MQTT communication
├── Vulcan/                 # CAN interface and configuration
├── Zephyr_apps/            # STM32C092 Zephyr applications
├── Zephyr_Scripts/         # Build, flash, and development scripts
├── .gitignore
└── README.md
```

---

## Communication

### CAN

The STM32 modules communicate over CAN. The Pedal Input Module sends accelerator and brake values to the CAN network, where they are received by the Motor Controller Module and Raspberry Pi.

```text
Pedal Module
     │
     ├── Accelerator
     └── Brake
          │
          ▼
       CAN Bus
       ┌────┴────┐
       ▼         ▼
Motor Controller  Raspberry Pi
       │             │
       ▼             ├──> Driver Dashboard
    PWM              │
       │             └──> MQTT
       ▼                    │
   H-Bridge                 ▼
                     Remote Pit Dashboard
```

### MQTT

The Raspberry Pi republishes selected CAN data over MQTT, allowing the pit crew to monitor vehicle information remotely.

---

## Current Functionality

The current system provides an end-to-end control and telemetry path:

* Accelerator and brake inputs are read by the Pedal Input Module.
* Pedal values are transmitted over CAN.
* The Motor Controller receives the commands and generates PWM output.
* The Raspberry Pi receives and displays pedal data on the driver dashboard.
* The Raspberry Pi broadcasts the same data over MQTT.
* A remote pit dashboard displays the transmitted vehicle data.

---

## Development

The system is designed to be modular, allowing additional vehicle control and monitoring functionality to be added as development continues.

### Planned Development

* [ ] Expand vehicle telemetry
* [ ] Add additional CAN messages
* [ ] Expand driver dashboard
* [ ] Expand remote pit monitoring
* [ ] Implement fault detection and reporting
* [ ] Add vehicle safety interlocks
* [ ] Develop dedicated VCS hardware
* [ ] Improve CAN diagnostics and logging
* [ ] Complete system-level testing

---

## Project Status

**Early Development / System Integration**

The basic vehicle control and telemetry pipeline is currently operational:

```text
Pedals
  ↓
STM32 Pedal Module
  ↓
CAN Bus
  ├──> STM32 Motor Controller ──> PWM ──> H-Bridge
  │
  └──> Raspberry Pi
          ├──> Driver Dashboard
          │
          └──> MQTT ──> Remote Pit Dashboard
```

Developed as part of a **Formula SAE Electric** racecar project.

```
```
