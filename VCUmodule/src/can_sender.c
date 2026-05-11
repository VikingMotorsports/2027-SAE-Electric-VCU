/*
 * CAN Sender module implementation for the VCU system.
 *
 * This module handles:
 *  - CAN frame construction
 *  - sensor data encoding
 *  - CAN message transmission
 *  - transmission completion callbacks
 *
 * The purpose of this module is to isolate CAN transmission
 * logic from the main application layer.
 *
 * Instead of manually constructing CAN frames throughout the
 * application, the application can simply call helper functions
 * such as:
 *
 *   can_send_sensor_data(...)
 *
 * This produces cleaner and more maintainable code.
 */

#include <stdio.h>

#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#include "can_interface.h"
#include "can_sender.h"

/*
 * CAN transmit completion callback.
 *
 * This function is automatically called by the Zephyr CAN driver
 * after a CAN transmission completes.
 *
 * Parameters:
 *  dev   - Pointer to CAN controller device
 *  error - Transmission result code
 *  arg   - User-defined argument passed into can_send()
 *
 * The callback is useful for:
 *  - detecting failed transmissions
 *  - debugging CAN traffic
 *  - asynchronous communication handling
 *
 * NOTE:
 *  This callback may execute in interrupt context, so heavy
 *  processing should be avoided here.
 */
void tx_irq_callback(
	const struct device *dev,
	int error,
	void *arg
)
{
	ARG_UNUSED(dev);

	/*
	 * User-provided argument passed from can_send().
	 *
	 * Typically used to identify which message
	 * completed transmission.
	 */
	char *sender = (char *)arg;

	/*
	 * Only print information if transmission failed.
	 */
	if (error != 0) {

		printf(
			"CAN TX callback error [%d]\n"
			"Sender: %s\n",
			error,
			sender
		);
	}
}

/*
 * Sends a 16-bit sensor value over CAN.
 *
 * This function:
 *  1. Creates a CAN frame
 *  2. Stores the sensor value into the payload
 *  3. Converts data to big-endian format
 *  4. Sends the CAN frame
 *
 * Parameters:
 *  can_dev      - Pointer to initialized CAN device
 *  sensor_value - 16-bit sensor value to transmit
 *  sensor_id    - CAN identifier for this message
 *
 * Returns:
 *   0  -> Message successfully queued for transmission
 *  <0  -> Transmission failed
 */
int can_send_sensor_data(
	const struct device *can_dev,
	uint16_t sensor_value,
	uint32_t sensor_id
)
{
	/*
	 * Create CAN frame structure.
	 *
	 * Current configuration:
	 *  - standard 11-bit CAN identifier
	 *  - 2-byte payload
	 */
	struct can_frame sensor_frame = {
		.flags = 0U,
		.id = sensor_id,
		.dlc = 2
	};

	/*
	 * Store sensor value into CAN payload.
	 *
	 * Data is converted to big-endian format to ensure
	 * consistent communication across devices.
	 */
	UNALIGNED_PUT(
		sys_cpu_to_be16(sensor_value),
		(uint16_t *)&sensor_frame.data[0]
	);

	/*
	 * Transmit CAN frame.
	 *
	 * Parameters:
	 *  - CAN device
	 *  - frame pointer
	 *  - timeout
	 *  - TX completion callback
	 *  - user argument passed into callback
	 */
	return can_send(
		can_dev,
		&sensor_frame,
		K_MSEC(100),
		tx_irq_callback,
		"sensor_tx"
	);
}