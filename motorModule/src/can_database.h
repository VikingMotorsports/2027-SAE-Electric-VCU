#ifndef CAN_DATABASE_H
#define CAN_DATABASE_H

#include <stdint.h>

/*
 * Add new CAN message IDs here
 * 
 * filters and receiving behavior can be added in can_receiver
 * 
 * frame information and format can be added in can_sender
 */

/* ---------- CAN Message IDs ---------- */

#define PEDALS_MSG_ID 0x080
#define MOTOR_DUTY_MSG_ID 0x100

//CAN data structs
typedef struct {
	uint8_t acc_pedal;
	uint8_t brake_pedal;
} pedal_data_t;

#endif