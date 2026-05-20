/*
 * CAN Receiver module for the VCU system.
 *
 * This module is responsible for:
 *  - Creating and managing the CAN receive thread
 *  - Configuring CAN receive filters
 *  - Receiving CAN frames from the bus
 *  - Parsing incoming CAN messages
 *  - Dispatching received data to the application layer
 *
 * The receiver runs as a dedicated Zephyr thread which continuously
 * waits for incoming CAN traffic using a CAN message queue.
 *
 * Separating CAN receive functionality into its own module improves:
 *  - modularity
 *  - scalability
 *  - readability
 *  - maintainability
 *
 * Additional CAN message filters and handlers can be added here
 * as more VCU subsystems are implemented.
 */

#ifndef CAN_RECEIVER_H
#define CAN_RECEIVER_H

#include <zephyr/device.h>

/* ---------- RX Thread Configuration ---------- */

/*
 * Stack size allocated for the CAN receive thread.
 *
 * Increase this value if:
 *  - more complex parsing is added
 *  - large local variables are used
 *  - additional processing is added to the RX thread
 */
#define RX_THREAD_STACK_SIZE 512

/*
 * Zephyr thread priority for the CAN receive thread.
 *
 * Lower numerical values correspond to higher priority.
 */
#define RX_THREAD_PRIORITY 2

#define NUM_CAN_FILTERS 2

/*
 * struct containing all CAN message filters
 * 
 * NOTES:
 * Message IDs should first be defined in can_database.h
 * Add new CAN messages here if you will need to receive them
 * Be sure to update NUM_CAN_FILTERS appropriately in can_receiver.h
 * Receiving behavior, if desired, needs to be defined in rx_thread below
 */
//const struct can_filter vcu_filters[NUM_CAN_FILTERS];

/* ---------- Public Interface ---------- */

/*
 * Main CAN receive thread function.
 *
 * This thread:
 *  - configures CAN receive filters
 *  - waits for incoming CAN messages
 *  - processes received CAN frames
 *  - prints or dispatches decoded data
 *
 * Parameters:
 *  can_dev - Pointer to CAN device passed during thread creation
 *  unused2 - Unused Zephyr thread parameter
 *  unused3 - Unused Zephyr thread parameter
 */
void rx_thread(void *can_dev, void *unused2, void *unused3);

/*
 * Creates and starts the CAN receive thread.
 *
 * Parameters:
 *  can_dev - Pointer to initialized CAN device
 *
 * Returns:
 *   0  -> Thread successfully started
 *  <0  -> Thread creation failed
 */
int rx_thread_create(const struct device *can_dev);

#endif /* CAN_RECEIVER_H */