/*
 * Main application for the VCU (Vehicle Control Unit) module.
 *
 * This application demonstrates a modular embedded software architecture
 * using the Zephyr RTOS. The system currently performs:
 *
 *  - accelerator pedal ADC acquisition
 *  - PWM motor control
 *  - CAN bus initialization
 *  - CAN message transmission
 *  - CAN message reception using a dedicated thread
 *
 * System Overview:
 *
 *   pedal_adc module
 *        ↓
 *   main control loop
 *        ↓
 *   motor_controller_pwm module
 *        ↓
 *   can_sender module
 *
 * Meanwhile:
 *
 *   can_receiver module
 *        ↓
 *   background CAN receive thread
 *
 * The main loop continuously:
 *  1. reads accelerator pedal position
 *  2. applies pedal deadband logic
 *  3. calculates PWM duty cycle
 *  4. drives the motor controller
 *  5. transmits pedal position over CAN
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "pedal_adc.h"
#include "motor_controller_pwm.h"
#include "can_interface.h"
#include "can_receiver.h"
#include "can_sender.h"
#include "can_database.h"

/*
 * Main control loop execution period in milliseconds.
 *
 * This determines how frequently:
 *  - ADC values are sampled
 *  - motor outputs are updated
 *  - CAN messages are transmitted
 */
#define LOOP_PERIOD_MS 33


/*
 * Main application entry point.
 *
 * This function performs:
 *  - subsystem initialization
 *  - CAN interface startup
 *  - CAN receiver thread creation
 *  - continuous vehicle control execution
 *
 * Returns:
 *   0  -> normal operation (typically never reached)
 *  <0  -> initialization failure
 */
int main(void)
{
	static int rc;
	int loop_ctr = 0;
	pedal_data_t pedals;
	pedals.acc_pedal = 0;
	pedals.brake_pedal = 0;
	/*
	 * Retrieve pointer to initialized CAN controller device.
	 *
	 * The CAN hardware itself is owned by the CAN interface
	 * module and accessed through this helper function.
	 */
	const struct device *can_dev =
		can_interface_get_device();

	/* ---------- Subsystem Initialization ---------- */
	/*
	 * Initialize PWM motor controller subsystem.
	 */
	rc = motor_pwm_setup();
	if (rc) {
		printk("motor_pwm_setup failed: %d\n", rc);
		return rc;
	}

	/*
	 * Initialize CAN controller hardware.
	 */
	rc = can_interface_init(can_dev);
	if (rc) {
		printk("can_interface_init failed: %d\n", rc);
		return rc;
	}

	/*
	 * Create and start background CAN receive thread.
	 */
	rc = rx_thread_create(can_dev, (void *)&pedals);
	if (rc) {
		printk("rx_thread_create failed: %d\n", rc);
		return rc;
	}

	printf("Finished init.\n");

	/* ---------- Main Vehicle Control Loop ---------- */

	while (1) {
		/* ---------- Motor Control ---------- */
		// * Motor direction command
		motor_dir dir = DIR_FORWARD;
		motor_side side = BOTH;
		float duty = pedals.acc_pedal;

		if (pedals.brake_pedal) {
			dir = DIR_STOP;
			duty = pedals.brake_pedal;
		}
		else if (pedals.acc_pedal) {
			dir = DIR_FORWARD;
			duty = pedals.acc_pedal;
		}
		else {
			dir = DIR_COAST;
			duty = 0;
		}

		// * Apply motor direction command.
		(void)set_direction(dir, side);

		// * Apply PWM duty cycle command.
		(void)set_speed(duty, side);

		

		if (can_send_sensor_data(can_dev, (uint16_t)duty, MOTOR_DUTY_MSG_ID)) {
			printk("can_send_sensor_data failed: %d\n", rc);
		}

		if ((loop_ctr % 100) == 0) {
			printf("main loop: duty=%d\npedals.acc_pedal=%d\npedals.brake_pedal=%d\nmotor_dir=%s\n",
				(uint16_t)duty,
				pedals.acc_pedal,
				pedals.acc_pedal,
				(dir==DIR_FORWARD) ? "DIR_FORWARD" : 
				(dir==DIR_REVERSE) ? "DIR_REVERSE" : 
				(dir==DIR_STOP) ? "DIR_STOP" : 
				(dir==DIR_COAST) ? "DIR_COAST" : "UNKNOWN");
		}
		loop_ctr++;
		// * Wait until next control loop iteration.
		k_msleep(LOOP_PERIOD_MS);
	}
};