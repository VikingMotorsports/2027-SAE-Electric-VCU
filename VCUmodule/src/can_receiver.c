/*
 * CAN Receiver module implementation for the VCU system.
 *
 * This module creates a dedicated thread responsible for:
 *
 *  - configuring CAN receive filters
 *  - receiving CAN frames from the bus
 *  - decoding incoming CAN messages
 *  - handling message-specific processing
 *
 * Incoming CAN messages are placed into a Zephyr CAN message queue
 * by the CAN driver. The receive thread blocks while waiting for
 * new messages and processes them as they arrive.
 *
 * This architecture separates CAN reception logic from the main
 * application loop and improves system organization.
 */

#include <stdio.h>

#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#include "can_receiver.h"
#include "can_interface.h"

/*
 * Allocate stack memory for the CAN receive thread.
 *
 * Zephyr threads require statically allocated stack memory.
 */
K_THREAD_STACK_DEFINE(rx_thread_stack, RX_THREAD_STACK_SIZE);

/*
 * CAN message queue used to store received CAN frames.
 *
 * Parameters:
 *  - queue name
 *  - maximum number of queued frames
 *
 * Incoming CAN messages matching configured filters are placed
 * into this queue by the CAN driver.
 */
CAN_MSGQ_DEFINE(VCU_msgq, 2);

/*
 * Thread control structure used internally by Zephyr to manage
 * the CAN receive thread.
 */
static struct k_thread rx_thread_data;

/*
 * Thread ID returned by Zephyr when the RX thread is created.
 *
 * Can be used later for:
 *  - thread management
 *  - suspension
 *  - monitoring
 *  - debugging
 */
static k_tid_t rx_tid;

/*
 * Main CAN receive thread.
 *
 * This thread:
 *  1. Configures CAN receive filters
 *  2. Waits for incoming CAN frames
 *  3. Processes received messages
 *  4. Decodes CAN payload data
 *
 * Parameters:
 *  can_dev - Pointer to initialized CAN device
 *  unused2 - Unused thread argument
 *  unused3 - Unused thread argument
 */
void rx_thread(void *can_dev, void *unused2, void *unused3)
{
	ARG_UNUSED(unused2);
	ARG_UNUSED(unused3);

	/*
	 * CAN receive filter for accelerator pedal messages.
	 *
	 * Only frames matching:
	 *   ID = ACCELERATOR_MSG_ID
	 *
	 * will be accepted into the receive queue.
	 */
	const struct can_filter accelerator_filter = {
		.flags = 0U,
		.id = ACCELERATOR_MSG_ID,
		.mask = CAN_STD_ID_MASK
	};

	struct can_frame frame;

	int filter_id;

	/*
	 * Register receive filter with the CAN controller.
	 *
	 * Matching frames are automatically pushed into VCU_msgq.
	 */
	filter_id = can_add_rx_filter_msgq(
		can_dev,
		&VCU_msgq,
		&accelerator_filter
	);

	printf("Accelerator filter added: 0x%X\n",
	       ACCELERATOR_MSG_ID);

	/*
	 * Main receive loop.
	 *
	 * Wait indefinitely for incoming CAN frames.
	 */
	while (1) {

		/* Block until a CAN frame is received */
		k_msgq_get(&VCU_msgq, &frame, K_FOREVER);

		/*
		 * Ignore Remote Transmission Request (RTR) frames
		 * if enabled in the CAN configuration.
		 */
		if (IS_ENABLED(CONFIG_CAN_ACCEPT_RTR) &&
		    (frame.flags & CAN_FRAME_RTR) != 0U) {
			continue;
		}

		/*
		 * Process received message based on CAN ID.
		 */
		switch (frame.id) {

		case ACCELERATOR_MSG_ID:

			/*
			 * Extract 16-bit big-endian accelerator value
			 * from CAN payload.
			 */
			printf(
				"ACCELERATOR: %u\n",
				sys_be16_to_cpu(
					UNALIGNED_GET(
						(uint16_t *)&frame.data
					)
				)
			);

			break;

		default:

			/*
			 * Unknown or unhandled CAN message.
			 */
			printf(
				"Unknown message received with ID: %u\n",
				frame.id
			);
		}
	}
}

/*
 * Creates and starts the CAN receive thread.
 *
 * The thread immediately begins running and starts listening
 * for CAN traffic.
 *
 * Parameters:
 *  can_dev - Pointer to initialized CAN device
 *
 * Returns:
 *   0  -> Thread successfully created
 *  <0  -> Thread creation failed
 */
int rx_thread_create(const struct device *can_dev)
{
	/*
	 * Create Zephyr thread for CAN reception.
	 */
	rx_tid = k_thread_create(
		&rx_thread_data,
		rx_thread_stack,
		K_THREAD_STACK_SIZEOF(rx_thread_stack),
		rx_thread,
		(void *)can_dev,
		NULL,
		NULL,
		RX_THREAD_PRIORITY,
		0,
		K_NO_WAIT
	);

	/*
	 * Verify thread creation succeeded.
	 */
	if (!rx_tid) {

		printf("ERROR spawning RX thread\n");

		return -ENOENT;
	}

	printf("CAN RX thread started\n");

	return 0;
}