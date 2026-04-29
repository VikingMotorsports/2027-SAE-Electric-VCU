/*
 * Copyright (c) 2018 Alexander Wachter
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(motor_pot, LOG_LEVEL_INF);

/* ---------- Devicetree handles ---------- */

#define ZEPHYR_USER DT_PATH(zephyr_user)

static const struct adc_dt_spec adc_pot =
	ADC_DT_SPEC_GET(ZEPHYR_USER);

static const struct pwm_dt_spec pwm_en =
	PWM_DT_SPEC_GET(ZEPHYR_USER);

static const struct gpio_dt_spec in1 =
	GPIO_DT_SPEC_GET(ZEPHYR_USER, in1_gpios);

static const struct gpio_dt_spec in2 =
	GPIO_DT_SPEC_GET(ZEPHYR_USER, in2_gpios);

/* ---------- Tunables ---------- */

#define ADC_RESOLUTION   12
#define ADC_MAX          ((1 << ADC_RESOLUTION) - 1)   /* 4095 */
#define ADC_CENTER       (ADC_MAX / 2)                 /* 2047 */

/* Deadband around the center, in raw ADC counts. ~5% of full range.
 * Increase if your pot is noisy or has mechanical slop at center.
 */
#define ADC_DEADBAND     200

/* Update period for the control loop. */
#define LOOP_PERIOD_MS   20


#define RX_THREAD_STACK_SIZE 512
#define RX_THREAD_PRIORITY 2
#define STATE_POLL_THREAD_STACK_SIZE 512
#define STATE_POLL_THREAD_PRIORITY 2
#define LED_MSG_ID 0x10
#define COUNTER_MSG_ID 0x12345
#define BRAKE_MSG_ID 0x008
#define ACCELERATOR_MSG_ID 0x080
#define STEERING_WHEEL_MSG_ID 0x800
#define SET_LED 1
#define RESET_LED 0
#define SLEEP_TIME K_MSEC(100)

K_THREAD_STACK_DEFINE(rx_thread_stack, RX_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(poll_state_stack, STATE_POLL_THREAD_STACK_SIZE);

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

struct k_thread rx_thread_data;
struct k_thread poll_state_thread_data;
struct k_work_poll change_led_work;
struct k_work state_change_work;
enum can_state current_state;
struct can_bus_err_cnt current_err_cnt;

CAN_MSGQ_DEFINE(change_led_msgq, 2);
CAN_MSGQ_DEFINE(counter_msgq, 2);


static struct k_poll_event change_led_events[1] = {
	K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
					K_POLL_MODE_NOTIFY_ONLY,
					&change_led_msgq, 0)
};

void tx_irq_callback(const struct device *dev, int error, void *arg)
{
	char *sender = (char *)arg;

	ARG_UNUSED(dev);

	if (error != 0) {
		printf("Callback! error-code: %d\nSender: %s\n",
		       error, sender);
	}
}

void rx_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	const struct can_filter filter = {
		.flags = CAN_FILTER_IDE,
		.id = COUNTER_MSG_ID,
		.mask = CAN_EXT_ID_MASK
	};
	const struct can_filter brake_filter = {
		.flags = 0U,
		.id = BRAKE_MSG_ID,
		.mask = CAN_STD_ID_MASK
	};
	const struct can_filter accelerator_filter = {
		.flags = 0U,
		.id = ACCELERATOR_MSG_ID,
		.mask = CAN_STD_ID_MASK
	};
	const struct can_filter steering_wheel_filter = {
		.flags = 0U,
		.id = STEERING_WHEEL_MSG_ID,
		.mask = CAN_STD_ID_MASK
	};
	struct can_frame frame;
	int filter_id;

	filter_id = can_add_rx_filter_msgq(can_dev, &counter_msgq, &filter);
	printf("Counter filter id: %x\n", COUNTER_MSG_ID);
	
	filter_id = can_add_rx_filter_msgq(can_dev, &counter_msgq, &brake_filter);
	printf("brake filter id: %x\n", BRAKE_MSG_ID);
	
	filter_id = can_add_rx_filter_msgq(can_dev, &counter_msgq, &accelerator_filter);
	printf("accelerator filter id: %x\n", ACCELERATOR_MSG_ID);
	
	filter_id = can_add_rx_filter_msgq(can_dev, &counter_msgq, &steering_wheel_filter);
	printf("steering wheel filter id: %x\n", STEERING_WHEEL_MSG_ID);

	while (1) {
		k_msgq_get(&counter_msgq, &frame, K_FOREVER);

		if (IS_ENABLED(CONFIG_CAN_ACCEPT_RTR) && (frame.flags & CAN_FRAME_RTR) != 0U) {
			continue;
		}

		if (frame.dlc != 2U) {
			printf("Wrong data length: %u\n", frame.dlc);
			continue;
		}
		switch (frame.id) {
			case COUNTER_MSG_ID:
				//printf("Counter message received: %u\n", sys_be16_to_cpu(UNALIGNED_GET((uint16_t *)&frame.data)));
				break;
			case BRAKE_MSG_ID:
				printf("%x %u\n", BRAKE_MSG_ID, sys_be16_to_cpu(UNALIGNED_GET((uint16_t *)&frame.data)));
				break;
			case ACCELERATOR_MSG_ID:
				printf("%x %u\n", ACCELERATOR_MSG_ID, sys_be16_to_cpu(UNALIGNED_GET((uint16_t *)&frame.data)));
				break;
			case STEERING_WHEEL_MSG_ID:
				printf("%x %u\n", STEERING_WHEEL_MSG_ID, sys_be16_to_cpu(UNALIGNED_GET((uint16_t *)&frame.data)));
				break;
			default:
				printf("Unknown message received with ID: %u\n", frame.id);
		}

		//printf("message received: %u\n",
		//       sys_be16_to_cpu(UNALIGNED_GET((uint16_t *)&frame.data)));
	}
	
}

void change_led_work_handler(struct k_work *work)
{
	struct can_frame frame;
	int ret;

	while (k_msgq_get(&change_led_msgq, &frame, K_NO_WAIT) == 0) {
		if (IS_ENABLED(CONFIG_CAN_ACCEPT_RTR) && (frame.flags & CAN_FRAME_RTR) != 0U) {
			continue;
		}

		if (led.port == NULL) {
			printf("LED %s\n", frame.data[0] == SET_LED ? "ON" : "OFF");
		} else {
			gpio_pin_set(led.port, led.pin, frame.data[0] == SET_LED ? 1 : 0);
		}
	}

	ret = k_work_poll_submit(&change_led_work, change_led_events,
				 ARRAY_SIZE(change_led_events), K_FOREVER);
	if (ret != 0) {
		printf("Failed to resubmit msgq polling: %d", ret);
	}
}

char *state_to_str(enum can_state state)
{
	switch (state) {
	case CAN_STATE_ERROR_ACTIVE:
		return "error-active";
	case CAN_STATE_ERROR_WARNING:
		return "error-warning";
	case CAN_STATE_ERROR_PASSIVE:
		return "error-passive";
	case CAN_STATE_BUS_OFF:
		return "bus-off";
	case CAN_STATE_STOPPED:
		return "stopped";
	default:
		return "unknown";
	}
}

void poll_state_thread(void *unused1, void *unused2, void *unused3)
{
	struct can_bus_err_cnt err_cnt = {0, 0};
	struct can_bus_err_cnt err_cnt_prev = {0, 0};
	enum can_state state_prev = CAN_STATE_ERROR_ACTIVE;
	enum can_state state;
	int err;

	while (1) {
		err = can_get_state(can_dev, &state, &err_cnt);
		if (err != 0) {
			printf("Failed to get CAN controller state: %d", err);
			k_sleep(K_MSEC(100));
			continue;
		}

		if (err_cnt.tx_err_cnt != err_cnt_prev.tx_err_cnt ||
		    err_cnt.rx_err_cnt != err_cnt_prev.rx_err_cnt ||
		    state_prev != state) {

			err_cnt_prev.tx_err_cnt = err_cnt.tx_err_cnt;
			err_cnt_prev.rx_err_cnt = err_cnt.rx_err_cnt;
			state_prev = state;
			printf("state: %s\n"
			       "rx error count: %d\n"
			       "tx error count: %d\n",
			       state_to_str(state),
			       err_cnt.rx_err_cnt, err_cnt.tx_err_cnt);
		} else {
			k_sleep(K_MSEC(100));
		}
	}
}

void state_change_work_handler(struct k_work *work)
{
	printf("State Change ISR\nstate: %s\n"
	       "rx error count: %d\n"
	       "tx error count: %d\n",
		state_to_str(current_state),
		current_err_cnt.rx_err_cnt, current_err_cnt.tx_err_cnt);
}

void state_change_callback(const struct device *dev, enum can_state state,
			   struct can_bus_err_cnt err_cnt, void *user_data)
{
	struct k_work *work = (struct k_work *)user_data;

	ARG_UNUSED(dev);

	current_state = state;
	current_err_cnt = err_cnt;
	k_work_submit(work);
}

/* ---------- Direction control ---------- */

enum motor_dir {
	DIR_STOP,
	DIR_FORWARD,
	DIR_REVERSE,
};

static int set_direction(enum motor_dir d)
{
	int rc;

	switch (d) {
	case DIR_FORWARD:
		rc = gpio_pin_set_dt(&in1, 1);
		if (rc) return rc;
		rc = gpio_pin_set_dt(&in2, 0);
		break;
	case DIR_REVERSE:
		rc = gpio_pin_set_dt(&in1, 0);
		if (rc) return rc;
		rc = gpio_pin_set_dt(&in2, 1);
		break;
	case DIR_STOP:
	default:
		/* Coast: both inputs low. For active braking, set both high. */
		rc = gpio_pin_set_dt(&in1, 0);
		if (rc) return rc;
		rc = gpio_pin_set_dt(&in2, 0);
		break;
	}
	return rc;
}

/* ---------- Speed control ----------
 *
 * Sets the PWM duty cycle as a fraction of the configured period.
 * `magnitude` is a value in [0, ADC_CENTER - ADC_DEADBAND].
 */
static int set_speed(uint32_t magnitude, uint32_t span)
{
	if (span == 0) {
		return pwm_set_pulse_dt(&pwm_en, 0);
	}
	if (magnitude > span) {
		magnitude = span;
	}

	/* Scale magnitude to the PWM period.
	 * Use 64-bit math to avoid overflow: period in ns can be large.
	 */
	uint64_t pulse = ((uint64_t)pwm_en.period * magnitude) / span;
	return pwm_set_pulse_dt(&pwm_en, (uint32_t)pulse);
}

/* ---------- Setup ---------- */

static int setup(void)
{
	int rc;

	if (!adc_is_ready_dt(&adc_pot)) {
		LOG_ERR("ADC not ready");
		return -ENODEV;
	}
	rc = adc_channel_setup_dt(&adc_pot);
	if (rc) {
		LOG_ERR("adc_channel_setup_dt failed: %d", rc);
		return rc;
	}

	if (!pwm_is_ready_dt(&pwm_en)) {
		LOG_ERR("PWM not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&in1) || !gpio_is_ready_dt(&in2)) {
		LOG_ERR("Direction GPIOs not ready");
		return -ENODEV;
	}
	rc = gpio_pin_configure_dt(&in1, GPIO_OUTPUT_INACTIVE);
	if (rc) {
		LOG_ERR("gpio_pin_configure_dt(in1) failed: %d", rc);
		return rc;
	}
	rc = gpio_pin_configure_dt(&in2, GPIO_OUTPUT_INACTIVE);
	if (rc) {
		LOG_ERR("gpio_pin_configure_dt(in2) failed: %d", rc);
		return rc;
	}

	/* Make sure the motor is stopped before we enter the loop. */
	(void)set_direction(DIR_STOP);
	(void)pwm_set_pulse_dt(&pwm_en, 0);

	return 0;
}

int main(void)
{
	int rc;
	int16_t sample_buffer;

	struct adc_sequence sequence = {
		.buffer      = &sample_buffer,
		.buffer_size = sizeof(sample_buffer),
	};

	const struct can_filter change_led_filter = {
		.flags = 0U,
		.id = LED_MSG_ID,
		.mask = CAN_STD_ID_MASK
	};
	struct can_frame change_led_frame = {
		.flags = 0,
		.id = LED_MSG_ID,
		.dlc = 1
	};
	struct can_frame brake_frame = {
        .flags = 0U,   // standard ID
        .id = BRAKE_MSG_ID,
		.dlc = 2                      // 2-byte payload
    };
	struct can_frame accelerator_frame = {
        .flags = 0U,   // standard ID
        .id = ACCELERATOR_MSG_ID,
        .dlc = 2                      // 2-byte payload
    };
	struct can_frame steering_frame = {
        .flags = 0U,   // standard ID
        .id = STEERING_WHEEL_MSG_ID,
        .dlc = 2                      // 2-byte payload
    };
	struct can_frame counter_frame = {
		.flags = CAN_FRAME_IDE,
		.id = COUNTER_MSG_ID,
		.dlc = 2
	};
	uint8_t toggle = 1;
	uint16_t i = 0;
	k_tid_t rx_tid, get_state_tid;
	int ret;

	rc = setup();
	if (rc) {
		LOG_ERR("setup failed: %d", rc);
		return rc;
	}

	rc = adc_sequence_init_dt(&adc_pot, &sequence);
	if (rc) {
		LOG_ERR("adc_sequence_init_dt failed: %d", rc);
		return rc;
	}

	LOG_INF("Motor controller running. ADC center=%d, deadband=+/-%d",
		ADC_CENTER, ADC_DEADBAND);

	const uint32_t span = ADC_CENTER - ADC_DEADBAND; /* effective half-range */
	
	struct can_timing timing;

	ret = can_calc_timing(can_dev, &timing, 500000, 875);
	if (ret != 0) {
		printf("Error calculating CAN timing [%d]", ret);
		return 0;
	}
	ret = can_set_timing(can_dev, &timing);
	if (ret != 0) {
		printf("Error setting CAN timing [%d]", ret);
		return 0;
	}

	if (!device_is_ready(can_dev)) {
		printf("CAN ERROR! Device %s not ready.\n", can_dev->name);
		return 0;
	}

#ifdef CONFIG_LOOPBACK_MODE
	ret = can_set_mode(can_dev, CAN_MODE_LOOPBACK);
	if (ret != 0) {
		printf("Error setting CAN mode [%d]", ret);
		return 0;
	}
#endif
	ret = can_start(can_dev);
	if (ret != 0) {
		printf("Error starting CAN controller [%d]", ret);
		return 0;
	}

	if (led.port != NULL) {
		if (!gpio_is_ready_dt(&led)) {
			printf("LED: Device %s not ready.\n",
			       led.port->name);
			return 0;
		}
		ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_HIGH);
		if (ret < 0) {
			printf("Error setting LED pin to output mode [%d]",
			       ret);
			led.port = NULL;
		}
	}

	k_work_init(&state_change_work, state_change_work_handler);
	k_work_poll_init(&change_led_work, change_led_work_handler);

	ret = can_add_rx_filter_msgq(can_dev, &change_led_msgq, &change_led_filter);
	if (ret == -ENOSPC) {
		printf("Error, no filter available!\n");
		return 0;
	}

	printf("Change LED filter ID: %d\n", ret);

	ret = k_work_poll_submit(&change_led_work, change_led_events,
				 ARRAY_SIZE(change_led_events), K_FOREVER);
	if (ret != 0) {
		printf("Failed to submit msgq polling: %d", ret);
		return 0;
	}

	rx_tid = k_thread_create(&rx_thread_data, rx_thread_stack,
				 K_THREAD_STACK_SIZEOF(rx_thread_stack),
				 rx_thread, NULL, NULL, NULL,
				 RX_THREAD_PRIORITY, 0, K_NO_WAIT);
	if (!rx_tid) {
		printf("ERROR spawning rx thread\n");
	}

	get_state_tid = k_thread_create(&poll_state_thread_data,
					poll_state_stack,
					K_THREAD_STACK_SIZEOF(poll_state_stack),
					poll_state_thread, NULL, NULL, NULL,
					STATE_POLL_THREAD_PRIORITY, 0,
					K_NO_WAIT);
	if (!get_state_tid) {
		printf("ERROR spawning poll_state_thread\n");
	}

	can_set_state_change_callback(can_dev, state_change_callback, &state_change_work);

	printf("Finished init.\n");

	while (1) {

		rc = adc_read_dt(&adc_pot, &sequence);
		if (rc) {
			LOG_WRN("adc_read_dt failed: %d", rc);
			k_msleep(LOOP_PERIOD_MS);
			continue;
		}

		/* Single-ended channel: clamp to non-negative. */
		int32_t raw = sample_buffer;
		if (raw < 0) raw = 0;
		if (raw > ADC_MAX) raw = ADC_MAX;

		int32_t offset = raw - ADC_CENTER;          /* -2047 .. +2048 */
		uint32_t mag   = (uint32_t)abs(offset);

		enum motor_dir dir;
		uint32_t scaled_mag;

		if (mag <= ADC_DEADBAND) {
			dir        = DIR_STOP;
			scaled_mag = 0;
		} else {
			dir        = (offset > 0) ? DIR_FORWARD : DIR_REVERSE;
			scaled_mag = mag - ADC_DEADBAND; /* 0 at edge of deadband */
		}

		(void)set_direction(dir);
		(void)set_speed(scaled_mag, span);

		LOG_DBG("raw=%4d off=%+5d dir=%d duty=%u/%u",
			raw, offset, dir, scaled_mag, span);

		k_msleep(LOOP_PERIOD_MS);

		//change_led_frame.data[0] = toggle++ & 0x01 ? SET_LED : RESET_LED;
		/* This sending call is none blocking. */
		//can_send(can_dev, &change_led_frame, K_FOREVER,
		//	 tx_irq_callback,
		//	 "LED change");
		k_sleep(SLEEP_TIME);

		/* Generate test values between 0 and 3300 */
        //uint16_t brake_val = (i * 55) % 3301;
        uint16_t accel_val = sample_buffer; //(i * 73) % 3301;
        //uint16_t steer_val = (i * 91) % 3301;

        /* ---------------- Brake Pedal Position ---------------- */
        //brake_frame.data[0] = (brake_val >> 8) & 0xFF;   // MSB
        //brake_frame.data[1] = brake_val & 0xFF;          // LSB

        //if (can_send(can_dev, &brake_frame, K_MSEC(100), NULL, NULL) != 0) {
            //printk("Failed to send brake message\n");
        //} else {
            //printk("Sent BRAKE  ID=0x%03X  value=%u\n", frame.id, brake_val);
        //}

        /* ---------------- Accelerator Pedal Position ---------------- */
        accelerator_frame.data[0] = (accel_val >> 8) & 0xFF;
        accelerator_frame.data[1] = accel_val & 0xFF;

        if (can_send(can_dev, &accelerator_frame, K_MSEC(100), NULL, NULL) != 0) {
            //printk("Failed to send accelerator message\n");
        } else {
            //printk("Sent ACCEL  ID=0x%03X  value=%u\n", frame.id, accel_val);
        }

        /* ---------------- Steering Wheel Position ---------------- */
        //steering_frame.data[0] = (steer_val >> 8) & 0xFF;
        //steering_frame.data[1] = steer_val & 0xFF;

        //if (can_send(can_dev, &steering_frame, K_MSEC(100), NULL, NULL) != 0) {
            //printk("Failed to send steering message\n");
        //} else {
            //printk("Sent STEER  ID=0x%03X  value=%u\n", frame.id, steer_val);
        //}

		//UNALIGNED_PUT(sys_cpu_to_be16(i),
		//	      (uint16_t *)&counter_frame.data[0]);
		/* This sending call is blocking until the message is sent. */
		//can_send(can_dev, &counter_frame, K_MSEC(100), NULL, NULL);
		//i++;
		//k_sleep(SLEEP_TIME);
	}
}
