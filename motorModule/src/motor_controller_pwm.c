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
static const struct pwm_dt_spec pwmLeft = PWM_DT_SPEC_GET_BY_IDX(ZEPHYR_USER, 0);
static const struct pwm_dt_spec pwmRight = PWM_DT_SPEC_GET_BY_IDX(ZEPHYR_USER, 1);

/*
 * GPIO specification for motor direction pins
 */
static const struct gpio_dt_spec leftA = GPIO_DT_SPEC_GET(ZEPHYR_USER, in1_gpios);
static const struct gpio_dt_spec leftB = GPIO_DT_SPEC_GET(ZEPHYR_USER, in2_gpios);
static const struct gpio_dt_spec righta = GPIO_DT_SPEC_GET(ZEPHYR_USER, in3_gpios);
static const struct gpio_dt_spec rightb = GPIO_DT_SPEC_GET(ZEPHYR_USER, in4_gpios);

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
	if (!pwm_is_ready_dt(&pwmLeft) ||
		!pwm_is_ready_dt(&pwmRight)) {
		
		printf("PWM not ready\n");
		return -ENODEV;
	}

	/* Verify GPIO hardware is ready */
	if (!gpio_is_ready_dt(&leftA) ||
	    !gpio_is_ready_dt(&leftB) ||
		!gpio_is_ready_dt(&righta) ||
		!gpio_is_ready_dt(&rightb)) {

		printf("Direction GPIOs not ready\n");
		return -ENODEV;
	}

	/* Configure IN1 as output */
	if (((rc = gpio_pin_configure_dt(&leftA, GPIO_OUTPUT_INACTIVE)) != 0) ||
		((rc = gpio_pin_configure_dt(&leftB, GPIO_OUTPUT_INACTIVE)) != 0) ||
		((rc = gpio_pin_configure_dt(&righta, GPIO_OUTPUT_INACTIVE)) != 0) ||
		((rc = gpio_pin_configure_dt(&rightb, GPIO_OUTPUT_INACTIVE)) != 0)) {
		printf("gpio_pin_configure_dt failed: %d\n", rc);
		return rc;
	}

	printf("Motor initialized successfully\n");


	/*
	 * Ensure the motor starts in a safe state.
	 */
	enum motor_side side = BOTH;

	(void)set_direction(DIR_STOP, side);

	/*
	 * Set PWM duty cycle to 0%.
	 */
	(void)pwm_set_pulse_dt(&pwmLeft, 0);

	(void)pwm_set_pulse_dt(&pwmRight, 0);

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
int set_direction(enum motor_dir d, enum motor_side side)
{
	int a = 0, b = 0;

	switch (d) {
		case DIR_STOP:
			a = 1;
			b = 1;
			break;
		case DIR_COAST:
			a = 0;
			b = 0;
			break;
		case DIR_FORWARD:
			a = 1;
			b = 0;
			break;
		case DIR_REVERSE:
			a = 0;
			b = 1;
			break;
	}

	if (side == LEFT){
		if (((rc = gpio_pin_set_dt(&leftA, a)) != 0) ||
				((rc = gpio_pin_set_dt(&leftB, b)) != 0) ) {
					return rc;
				}
	}
	else if (side == RIGHT){
		if (((rc = gpio_pin_set_dt(&righta, a)) != 0) ||
				((rc = gpio_pin_set_dt(&rightb, b)) != 0) ) {
					return rc;
				}
	}
	else{
		if (((rc = gpio_pin_set_dt(&leftA, a)) != 0) ||
				((rc = gpio_pin_set_dt(&leftB, b)) != 0) ||
				((rc = gpio_pin_set_dt(&righta, a)) != 0) ||
				((rc = gpio_pin_set_dt(&rightb, b)) != 0) ) {
					return rc;
				}
	}

	return 0;
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
int set_speed(float duty_cycle, enum motor_side side)
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
		(uint64_t)((duty_cycle / 100.0f) * pwmLeft.period);
	
	/*
	 * Apply PWM pulse width to hardware.
	 */
	if (side == LEFT) return pwm_set_pulse_dt(&pwmLeft, (uint32_t)pulse);
	else if (side == RIGHT) return pwm_set_pulse_dt(&pwmRight, (uint32_t)pulse);
	else {
		int rc = 0;
		if (((rc = pwm_set_pulse_dt(&pwmLeft, (uint32_t)pulse)) != 0) ||
			((rc = pwm_set_pulse_dt(&pwmLeft, (uint32_t)pulse)) != 0) ) {
				return rc;
			}
		else return 0;
	}
	
};

