/*
 * CAN Interface module for the VCU system.
 *
 * This module is responsible for:
 *  - Owning the CAN controller device handle
 *  - Initializing the CAN peripheral
 *  - Configuring CAN bus timing and operating mode
 *  - Starting the CAN controller
 *
 * The purpose of this module is to isolate all low-level CAN hardware
 * configuration from the application layer. Other modules should access
 * the CAN device through the provided interface functions rather than
 * directly declaring or configuring CAN hardware themselves.
 *
 * This creates a cleaner architecture and makes the CAN subsystem easier
 * to maintain, debug, and expand in the future.
 */

#ifndef CAN_INTERFACE_H
#define CAN_INTERFACE_H

#include <stdint.h>
#include <zephyr/drivers/can.h>

/* ---------- CAN Message IDs ---------- */

/*
 * Standard CAN identifier used for accelerator pedal messages.
 *
 * Additional message IDs for:
 *  - brake pedal
 *  - steering wheel
 *  - dashboard
 *  - battery management
 *  - inverter communication
 *
 * can also be added here as the VCU system grows.
 */
#define ACCELERATOR_MSG_ID 0x080

/* ---------- Public Interface ---------- */

/*
 * Returns a pointer to the CAN controller device used by the system.
 *
 * This allows other modules (CAN transmitter, receiver, diagnostics, etc.)
 * to access the initialized CAN hardware without directly owning the
 * global device handle.
 *
 * Returns:
 *  Pointer to Zephyr CAN device structure.
 */
const struct device *can_interface_get_device(void);

/*
 * Initializes the CAN controller hardware.
 *
 * This function performs:
 *  - CAN timing calculation
 *  - bitrate configuration
 *  - optional loopback mode configuration
 *  - CAN controller startup
 *
 * Parameters:
 *  can_dev - Pointer to the CAN device to initialize.
 *
 * Returns:
 *   0  -> Initialization successful
 *  <0  -> Error occurred during setup
 */
int can_interface_init(const struct device *can_dev);

#endif /* CAN_INTERFACE_H */