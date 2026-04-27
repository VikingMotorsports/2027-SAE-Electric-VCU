/*
 * NUCLEO-C092RC: Potentiometer-controlled DC motor via H-bridge (EN + IN1/IN2).
 *
 * - Pot wiper on A0 sets motor speed and direction.
 * - Center of pot travel = stop (with deadband).
 * - Below center = reverse, above center = forward.
 * - Magnitude scales linearly from deadband edge to either endpoint.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>

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

/* ---------- Main loop ---------- */

int main(void)
{
	int rc;
	int16_t sample_buffer;

	struct adc_sequence sequence = {
		.buffer      = &sample_buffer,
		.buffer_size = sizeof(sample_buffer),
	};

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
	}

	return 0;
}
