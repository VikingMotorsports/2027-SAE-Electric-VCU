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
#define LOOP_PERIOD_MS 20

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
	 * Initialize accelerator pedal ADC subsystem.
	 */
	rc = pedal_adc_setup();
	if (rc) {
		printk("pedal_adc_setup failed: %d\n", rc);
		return rc;
	}

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
	rc = rx_thread_create(can_dev);
	if (rc) {
		printk("rx_thread_create failed: %d\n", rc);
		return rc;
	}

	printf("Finished init.\n");

	/* ---------- Main Vehicle Control Loop ---------- */

	while (1) {

		// * Raw accelerator pedal ADC reading.
		int32_t acc_value;
		int32_t brake_value;

		// * Read accelerator pedal position from ADC.
		rc = pedal_adc_read(&acc_value, &brake_value);
		if (rc) {
			printk("pedal_adc_read failed: %d\n", rc);
			return rc;
		}

		/* ---------- Pedal Processing ---------- */

		// * Motor direction command.
		enum motor_dir dir;

		// * Adjusted pedal value after deadband removal.
		uint32_t adj_acc_value;
		uint32_t adj_brake_value;

		// * PWM duty cycle percentage.
		float duty;

		// * Apply pedal deadband.
		if (acc_value <= ADC_DEADBAND) {
			dir = DIR_STOP;
			adj_acc_value = 0;
		} 
		else {
			/*
			 * Current configuration uses reverse direction.
			 *
			 * This may vary
			 */
			dir = DIR_REVERSE;

			// * Remove deadband offset from pedal value.
			adj_acc_value =
				(acc_value - ADC_DEADBAND);
		}
		if (brake_value <= ADC_DEADBAND) {
			adj_brake_value = 0;
		} 
		else {
			// * Remove deadband offset from pedal value.
			adj_brake_value =
				(brake_value - ADC_DEADBAND);
		}


		// * Convert adjusted pedal position into PWM duty cycle percentage.
		duty =
			((float)adj_acc_value / ADC_SPAN)
			* 100.0f;

		/* ---------- Motor Control ---------- */

		// * Apply motor direction command.
		(void)set_direction(dir);

		// * Apply PWM duty cycle command.
		(void)set_speed(duty);

		/* ---------- CAN Transmission ---------- */

		// * Convert duty cycle percentage into integer
		// * value for CAN transmission.
		uint16_t acc_pedal_percent = (uint16_t)duty;
		uint16_t brake_pedal_percent = (uint16_t)(((float)adj_brake_value / ADC_SPAN)
			* 100.0f);

		// * Transmit accelerator pedal position over CAN bus.
		if (can_send_sensor_data(can_dev, acc_pedal_percent, ACCELERATOR_MSG_ID)) {
			printk("can_send_sensor_data failed: %d\n", rc);
		}
		if (can_send_sensor_data(can_dev, brake_pedal_percent, BRAKE_MSG_ID)) {
			printk("can_send_sensor_data failed: %d\n", rc);
		}

		// * Wait until next control loop iteration.
		k_msleep(LOOP_PERIOD_MS);
	}
};