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
	ADC_DT_SPEC_GET(ZEPHYR_USER);

/*
 * Shared return code variable used for
 * ADC driver function results.
 */
static int rc;

/*
 * Buffer used to store the most recent
 * accelerator pedal ADC sample.
 *
 * Additional buffers can be added here
 * for future sensor inputs.
 */
static int16_t acc_buffer;

/*
 * ADC sequence structure used during
 * ADC conversion operations.
 *
 * The ADC driver stores conversion
 * results into the configured buffer.
 *
 * Additional ADC sequences may be added
 * for future sensors.
 */
static struct adc_sequence sequence = {
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
	/*
	 * Verify ADC device readiness.
	 */
	if (!adc_is_ready_dt(&acc_pot)) {

		printf("ADC not ready\n");
		return -ENODEV;
	}

	/*
	 * Configure ADC channel settings.
	 */
	rc = adc_channel_setup_dt(&acc_pot);

	if (rc) {

		printk("adc_channel_setup_dt failed: %d\n",
		       rc);

		return rc;
	}

	/*
	 * Initialize ADC conversion sequence.
	 */
	rc = adc_sequence_init_dt(&acc_pot,
				  &sequence);

	if (rc) {

		printf("adc_sequence_init_dt failed: %d\n",
		       rc);

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
int pedal_adc_read(int32_t *raw)
{
	/*
	 * Start ADC conversion and store
	 * result into the configured buffer.
	 */
	rc = adc_read_dt(&acc_pot,
			 &sequence);

	if (rc) {

		printk("adc_read_dt failed: %d\n",
		       rc);

		/*
		 * Delay briefly before allowing
		 * another conversion attempt.
		 */
		k_msleep(20);

		return rc;
	}

	/*
	 * Copy ADC sample into user-provided
	 * storage location.
	 */
	*raw = acc_buffer;

	/*
	 * Clamp ADC result to valid range.
	 */
	if (*raw < 0) {
		*raw = 0;
	}

	if (*raw > ADC_MAX) {
		*raw = ADC_MAX;
	}

	return 0;
}