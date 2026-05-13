/*
 * motor_controller_pwm.c
 *
 * PWM motor controller implementation for the VCU module.
 *
 * This module controls a DC motor driver using:
 *
 *  - PWM output for speed control
 *  - Two GPIO outputs for direction control
 *
 * Hardware configuration is obtained from the Zephyr
 * devicetree using the zephyr_user node.
 *
 * Direction control:
 *     IN1 = 1, IN2 = 0 -> Forward
 *     IN1 = 0, IN2 = 1 -> Reverse
 *     IN1 = 0, IN2 = 0 -> Coast/Stop
 *
 * PWM control:
 *     The PWM duty cycle determines average motor voltage
 *     and therefore motor speed.
 */

#include "motor_controller_pwm.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

/*
 * Obtain the zephyr_user devicetree node.
 *
 * This node contains all application-specific
 * hardware mappings.
 */
#ifndef ZEPHYR_USER
#define ZEPHYR_USER DT_PATH(zephyr_user)
#endif

/*
 * PWM specification for the motor enable pin.
 *
 * This structure contains:
 *  - PWM controller device
 *  - PWM channel
 *  - PWM period
 *  - PWM flags
 */
static const struct pwm_dt_spec pwm_en = PWM_DT_SPEC_GET(ZEPHYR_USER);

/*
 * GPIO specification for motor direction input 1.
 */
static const struct gpio_dt_spec in1 = GPIO_DT_SPEC_GET(ZEPHYR_USER, in1_gpios);

/*
 * GPIO specification for motor direction input 2.
 */
static const struct gpio_dt_spec in2 = GPIO_DT_SPEC_GET(ZEPHYR_USER, in2_gpios);

/*
 * Shared return code variable used throughout
 * the module for hardware API calls.
 */
static int rc;

/*
 * Initialize motor PWM and GPIO hardware.
 *
 * This function must be called before
 * attempting to control the motor.
 *
 * Initialization sequence:
 *  1. Verify PWM device readiness
 *  2. Verify GPIO readiness
 *  3. Configure GPIO pins as outputs
 *  4. Stop the motor
 *  5. Set PWM duty cycle to 0%
 *
 * Returns:
 *     0 on success.
 *     Negative Zephyr error code on failure.
 */
int motor_pwm_setup(void)
{
	/* Verify PWM hardware is ready */
	if (!pwm_is_ready_dt(&pwm_en)) {
		printf("PWM not ready\n");
		return -ENODEV;
	}

	/* Verify GPIO hardware is ready */
	if (!gpio_is_ready_dt(&in1) ||
	    !gpio_is_ready_dt(&in2)) {

		printf("Direction GPIOs not ready\n");
		return -ENODEV;
	}

	/* Configure IN1 as output */
	rc = gpio_pin_configure_dt(&in1,
				   GPIO_OUTPUT_INACTIVE);

	if (rc) {
		printf("gpio_pin_configure_dt(in1) failed: %d\n",
		       rc);
		return rc;
	}

	/* Configure IN2 as output */
	rc = gpio_pin_configure_dt(&in2,
				   GPIO_OUTPUT_INACTIVE);

	if (rc) {
		printf("gpio_pin_configure_dt(in2) failed: %d\n",
		       rc);
		return rc;
	}

	printf("Motor initialized successfully\n");


	/*
	 * Ensure the motor starts in a safe state.
	 */
	(void)set_direction(DIR_STOP);

	/*
	 * Set PWM duty cycle to 0%.
	 */
	(void)pwm_set_pulse_dt(&pwm_en, 0);

	return 0;
};

/*
 * Set the motor direction.
 *
 * Depending on the selected direction, the GPIO
 * direction pins are driven appropriately.
 *
 * Forward:
 *     IN1 = HIGH
 *     IN2 = LOW
 *
 * Reverse:
 *     IN1 = LOW
 *     IN2 = HIGH
 *
 * Stop:
 *     IN1 = LOW
 *     IN2 = LOW
 *
 * Parameters:
 *     d - Desired motor direction.
 *
 * Returns:
 *     0 on success.
 *     Negative Zephyr error code on failure.
 */
int set_direction(enum motor_dir d)
{
	switch (d) {

	case DIR_FORWARD:

		/* Drive motor forward */
		rc = gpio_pin_set_dt(&in1, 1);
		if (rc) {
			return rc;
		}

		rc = gpio_pin_set_dt(&in2, 0);
		break;

	case DIR_REVERSE:

		/* Drive motor in reverse */
		rc = gpio_pin_set_dt(&in1, 0);
		if (rc) {
			return rc;
		}

		rc = gpio_pin_set_dt(&in2, 1);
		break;

	case DIR_STOP:
	default:

		/*
		 * Stop motor by disabling both
		 * direction inputs.
		 *
		 * This allows the motor to coast.
		 * Active braking could be implemented
		 * by setting both inputs HIGH instead.
		 */
		rc = gpio_pin_set_dt(&in1, 0);
		if (rc) {
			return rc;
		}

		rc = gpio_pin_set_dt(&in2, 0);
		break;
	}

	return rc;
};

/*
 * Set motor speed using PWM duty cycle.
 *
 * The PWM duty cycle determines how long the PWM
 * signal remains active during each period.
 *
 * 0%  -> motor off
 * 100% -> full speed
 *
 * The duty cycle is automatically clamped
 * to the valid range.
 *
 * Parameters:
 *     duty_cycle - Desired duty cycle percentage.
 *
 * Returns:
 *     0 on success.
 *     Negative Zephyr error code on failure.
 */
int set_speed(float duty_cycle)
{
	/* Clamp duty cycle to valid range */
	if (duty_cycle < 0.0f) {
		duty_cycle = 0.0f;

	} else if (duty_cycle > 100.0f) {
		duty_cycle = 100.0f;
	}

	/*
	 * Convert duty cycle percentage into
	 * PWM pulse width in nanoseconds.
	 */
	uint64_t pulse =
		(uint64_t)((duty_cycle / 100.0f) * pwm_en.period);
	
	/*
	 * Apply PWM pulse width to hardware.
	 */
	return pwm_set_pulse_dt(&pwm_en, (uint32_t)pulse);
};

