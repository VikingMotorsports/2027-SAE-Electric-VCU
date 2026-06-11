/*
 * motor_controller_pwm.h
 *
 * PWM motor controller interface for the VCU module.
 *
 * This module provides a simple abstraction layer for controlling
 * a DC motor driver using:
 *
 *  - One PWM output for speed control
 *  - Two GPIO outputs for direction control
 *
 * The implementation is designed for Zephyr RTOS using devicetree-
 * configured peripherals.
 *
 * Responsibilities of this module:
 *  - Configure PWM hardware
 *  - Configure motor direction GPIOs
 *  - Set motor direction
 *  - Set motor speed using PWM duty cycle
 *  - Stop the motor safely during initialization
 *
 * The application should interact with this module instead of
 * directly controlling PWM or GPIO peripherals.
 */

#ifndef MOTOR_CONTROLLER_PWM_H
#define MOTOR_CONTROLLER_PWM_H

#include <stdint.h>




/*
 * Motor direction states.
 *
 * DIR_STOP:
 *     Both direction pins are driven low.
 *     The motor coasts to a stop.
 *
 * DIR_FORWARD:
 *     Motor rotates in the forward direction.
 *
 * DIR_REVERSE:
 *     Motor rotates in the reverse direction.
 */
enum motor_dir {
	DIR_STOP,
	DIR_FORWARD,
	DIR_REVERSE,
};


/*
 * Initialize the motor controller hardware.
 *
 * This function:
 *  - Verifies PWM readiness
 *  - Verifies GPIO readiness
 *  - Configures GPIO direction pins
 *  - Stops the motor initially
 *  - Sets PWM duty cycle to 0%
 *
 * Must be called before using any other motor
 * control functions.
 *
 * Returns:
 *     0 on success.
 *     Negative Zephyr error code on failure.
 */
int motor_pwm_setup(void);

/*
 * Set the motor rotation direction.
 *
 * Parameters:
 *     d - Desired motor direction.
 *
 * Returns:
 *     0 on success.
 *     Negative Zephyr error code on failure.
 */
int set_direction(enum motor_dir d);

/*
 * Set the motor PWM duty cycle.
 *
 * The duty cycle controls motor speed by adjusting
 * the percentage of time the PWM signal is active.
 *
 * Parameters:
 *     duty_cycle - Desired PWM duty cycle percentage.
 *                  Valid range: 0.0f to 100.0f
 *
 * Returns:
 *     0 on success.
 *     Negative Zephyr error code on failure.
 */
int set_speed(float duty_cycle);


#endif /* MOTOR_CONTROLLER_PWM_H */