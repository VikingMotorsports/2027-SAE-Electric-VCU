# Potentiometer-controlled DC motor — NUCLEO-C092RC (Zephyr)

Reads a potentiometer on Arduino A0 and drives an EN + IN1/IN2 H-bridge
to control a DC motor. Center pot = stop, left = reverse, right = forward.

## Wiring

| Function          | Arduino pin | STM32 pin | Peripheral     |
| ----------------- | ----------- | --------- | -------------- |
| Pot wiper (input) | A0          | PA0       | ADC1 channel 0 |
| H-bridge EN (PWM) | D9          | PA8       | TIM1 CH1       |
| H-bridge IN1      | D8          | PA9       | GPIO out       |
| H-bridge IN2      | D7          | PC7       | GPIO out       |
| GND               | GND         | —         | common ground  |

Connect the pot's two outer legs between 3V3 and GND; wiper to A0.
**Power the motor from a separate supply** that shares ground with the
Nucleo. Do not try to run a motor off the Nucleo's 3V3 or 5V rails.

## Build & flash

From a Zephyr workspace (with `west` available):

```sh
west build -b nucleo_c092rc path/to/motor_pot_demo
west flash
```

## Tuning

Edit `src/main.c`:

- `ADC_DEADBAND` — raw ADC counts of "stop zone" around center. Increase
  if the motor twitches at rest.
- `LOOP_PERIOD_MS` — how often the control loop runs (default 20 ms = 50 Hz).
- The PWM period is set in `boards/nucleo_c092rc.overlay` (default
  `PWM_USEC(50)` = 20 kHz, above audible range; raise to ~1 kHz if your
  H-bridge can't keep up at 20 kHz, e.g. classic L298N).

## Behaviour notes

- "Stop" = coast (both IN low, EN PWM 0). To brake instead, change
  `DIR_STOP` in `set_direction()` to drive both IN1 and IN2 high.
- The control loop is open-loop. There's no current limit, no soft-start,
  no acceleration limiting. Add a slew-rate limit on `scaled_mag` if your
  mechanics or supply can't tolerate instant full-throttle reversals.
