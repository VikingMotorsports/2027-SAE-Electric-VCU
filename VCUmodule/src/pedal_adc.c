/*
 * pedal_adc.c
 *
 * Accelerator pedal ADC implementation for the VCU module.
 *
 * This module handles analog pedal position acquisition
 * using the Zephyr ADC subsystem.
 *
 * Hardware configuration is obtained from the
 * zephyr_user devicetree node.
 *
 * Current functionality:
 *  - Initialize ADC hardware
 *  - Read raw accelerator pedal position
 *  - Clamp readings to valid limits
 *
 * Future expansion may include:
 *  - Multiple pedal sensors
 *  - Signal filtering
 *  - Sensor diagnostics
 *  - Redundant safety channels
 */

#include "pedal_adc.h"

#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <stdio.h>

/* ---------- Devicetree Handles ---------- */

/*
 * Obtain the zephyr_user devicetree node.
 *
 * This node stores application-specific
 * peripheral mappings.
 */
#ifndef ZEPHYR_USER
#define ZEPHYR_USER DT_PATH(zephyr_user)
#endif

/*
 * ADC specification for the accelerator
 * pedal potentiometer.
 *
 * This structure contains:
 *  - ADC device reference
 *  - ADC channel configuration
 *  - acquisition settings
 */
static const struct adc_dt_spec acc_pot =
	ADC_DT_SPEC_GET_BY_IDX(ZEPHYR_USER, 0);

static const struct adc_dt_spec brake_pot =
	ADC_DT_SPEC_GET_BY_IDX(ZEPHYR_USER, 1);

static int rc;

/*
 * Additional buffers can be added here
 * for future sensor inputs.
 */
static int16_t brake_buffer;
static int16_t acc_buffer;

/*
 * Additional ADC sequences may be added
 * for future sensors.
 */
static struct adc_sequence brake_sequence = {
	.buffer      = &brake_buffer,
	.buffer_size = sizeof(brake_buffer),
};
static struct adc_sequence acc_sequence = {
	.buffer      = &acc_buffer,
	.buffer_size = sizeof(acc_buffer),
};

/*
 * Initialize pedal ADC hardware.
 *
 * Initialization sequence:
 *  1. Verify ADC hardware readiness
 *  2. Configure ADC channel settings
 *  3. Initialize ADC conversion sequence
 *
 * This function must be called before
 * attempting to read pedal values.
 *
 * Returns:
 *     0 on success.
 *     Negative Zephyr error code on failure.
 */
int pedal_adc_setup(void)
{
	//Verify ADC device readiness.
	if (!adc_is_ready_dt(&acc_pot)) {
		printf("ADC not ready\n");
		return -ENODEV;
	}
	if (!adc_is_ready_dt(&brake_pot)) {
		printf("ADC not ready\n");
		return -ENODEV;
	}

	//Configure ADC channel settings.
	rc = adc_channel_setup_dt(&acc_pot);
	if (rc) {
		printk("adc_channel_setup_dt failed: %d\n", rc);
		return rc;
	}
	rc = adc_channel_setup_dt(&brake_pot);
	if (rc) {
		printk("adc_channel_setup_dt failed: %d\n", rc);
		return rc;
	}

	//Initialize ADC conversion sequence.
	rc = adc_sequence_init_dt(&acc_pot, &acc_sequence);
	if (rc) {
		printf("adc_sequence_init_dt failed: %d\n", rc);
		return rc;
	}
	rc = adc_sequence_init_dt(&brake_pot, &brake_sequence);
	if (rc) {
		printf("adc_sequence_init_dt failed: %d\n", rc);
		return rc;
	}

	return 0;
}

/*
 * Read accelerator pedal position.
 *
 * This function performs an ADC conversion
 * using the configured accelerator pedal
 * channel.
 *
 * The resulting ADC value is:
 *  - stored in the provided variable
 *  - clamped to the valid range
 *
 * Valid range:
 *      0 -> ADC_MAX
 *
 * Parameters:
 *     raw - Pointer to destination variable
 *           for ADC result storage.
 *
 * Returns:
 *     0 on success.
 *     Negative Zephyr error code on failure.
 */
int pedal_adc_read(int32_t *acc_raw, int32_t *brake_raw)
{
	//Read ADC into buffer
	rc = adc_read_dt(&acc_pot, &acc_sequence);
	if (rc) {
		printk("adc_read_dt failed: %d\n", rc);
		//small delay after failed reading
		k_msleep(20);
		return rc;
	}
	rc = adc_read_dt(&brake_pot, &brake_sequence);
	if (rc) {
		printk("adc_read_dt failed: %d\n", rc);
		//small delay after failed reading
		k_msleep(20);
		return rc;
	}

	//copy data to location passed in
	*acc_raw = acc_buffer;
	*brake_raw = brake_buffer;

	//Clamp ADC result to valid range.
	if (*acc_raw < 0) *acc_raw = 0;
	if (*acc_raw > ADC_MAX) *acc_raw = ADC_MAX;

	if (*brake_raw < 0) *brake_raw = 0;
	if (*brake_raw > ADC_MAX) *brake_raw = ADC_MAX;

	return 0;
}