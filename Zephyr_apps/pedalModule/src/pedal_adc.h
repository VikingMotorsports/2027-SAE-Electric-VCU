/*
 * pedal_adc.h
 *
 * Accelerator pedal ADC interface for the VCU module.
 *
 * This module provides a hardware abstraction layer for
 * reading analog pedal position sensors using the Zephyr
 * ADC subsystem.
 *
 * Current functionality:
 *  - Initialize ADC hardware
 *  - Read accelerator pedal position
 *  - Clamp ADC readings to a valid range
 *
 * Future expansion:
 *  - Brake pedal sensor
 *  - Steering position sensor
 *  - Dual redundant pedal sensors
 *  - Sensor plausibility checking
 *  - ADC filtering and averaging
 *
 * The application should use this module instead of
 * directly interacting with ADC peripherals.
 */

#ifndef PEDAL_ADC_H
#define PEDAL_ADC_H

#include <stdint.h>

/* ---------- ADC Configuration ---------- */

/*
 * ADC resolution in bits.
 *
 * 12-bit resolution produces values in the range:
 *      0 -> 4095
 */
#define ADC_RESOLUTION   12

/*
 * Maximum expected accelerator pedal value.
 *
 * This value is intentionally limited below the
 * full 12-bit range to account for:
 *  - sensor tolerances
 *  - voltage variation
 *  - calibration limits
 */
#define ADC_MAX          3100

/*
 * Deadband at the beginning of pedal travel.
 *
 * Any ADC reading below this value is treated
 * as zero throttle input.
 *
 * This helps reduce:
 *  - electrical noise
 *  - potentiometer jitter
 *  - accidental small inputs
 */
#define ADC_DEADBAND     1100

 //Effective usable pedal range after deadband
 //removal.
#define ADC_SPAN         (ADC_MAX - ADC_DEADBAND)

/*
 * Initialize pedal ADC hardware.
 *
 * This function:
 *  - Verifies ADC hardware readiness
 *  - Configures ADC channels
 *  - Initializes ADC sequence structures
 *
 * Must be called before attempting to read
 * pedal values.
 *
 * Returns:
 *     0 on success.
 *     Negative Zephyr error code on failure.
 */
int pedal_adc_setup(void);

/*
 * Read accelerator pedal ADC value.
 *
 * The raw ADC value is read from the configured
 * accelerator pedal potentiometer channel.
 *
 * The returned value is automatically clamped
 * to the valid range:
 *
 *      0 -> ADC_MAX
 *
 * Parameters:
 *     raw - Pointer to storage location for
 *           the ADC reading.
 *
 * Returns:
 *     0 on success.
 *     Negative Zephyr error code on failure.
 */
int pedal_adc_read(int32_t *acc_raw, int32_t *brake_raw);

#endif /* PEDAL_ADC_H */