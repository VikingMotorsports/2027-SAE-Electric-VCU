# Formula SAE EV Vehicle Control System

A distributed vehicle control system (VCS) for a Formula SAE electric racecar.

The system is built around STM32 microcontrollers running Zephyr RTOS, communicating over a CAN bus. A Raspberry Pi provides the driver dashboard and broadcasts vehicle data to a remote pit display using MQTT.

> **Project Status:** Active Development

---

## Overview

The Vehicle Control System is responsible for collecting driver inputs, communicating vehicle state over CAN, and controlling the motor interface.

The current system consists of:

- **Pedal Input Module** — Reads accelerator and brake pedal inputs and transmits them over CAN.
- **Motor Controller Module** — Receives pedal commands over CAN and generates PWM signals for the motor H-bridge.
- **Raspberry Pi Dashboard** — Receives CAN data through a CAN interface and displays driver inputs in real time.
- **Remote Pit Dashboard** — Receives vehicle data from the Raspberry Pi over MQTT.
- **CAN Network** — Provides communication between the vehicle control modules.
- **H-Bridge Interface** — Receives PWM and control signals from the motor controller.

### System Architecture

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