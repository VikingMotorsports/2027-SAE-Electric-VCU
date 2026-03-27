/********************************************************************************************
 * Steering Angle Reader using Potentiometer via ADC                                        *
 *                                                                                          *
 * @title: steering_wheel.c                                                                 *
 * @author: Allen Paul                                                                      *                                         
 * @date: 03/26/2026                                                                        *
 *                                                                                          *
 * @description: Reads the potentiometer connected to PA0,                                  *
 * converts the raw ADC value to a steering angle in degrees,                               *
 * and prints it to the console. The angle is calculated based on a linear                  *
 * mapping from the ADC voltage to a specified range of steering angles (-135° to +135°).   *  
 *                                                                                          *
 * Wiring based off of board and Casey's research:                                          *   
 * Potentiometer left leg   → GND  (CN7 pin 20)                                             * 
 * Potentiometer right leg  → 3.3V (CN7 pin 16)                                             *                    
 * Potentiometer wiper      → PA0  (CN7 pin 28)                                             *
 *                                                                                          *
 ********************************************************************************************/

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

// Definitions
#define SAMPLE_INTERVAL_MS   100     /* 10 Hz — lower for faster response */
#define ADC_RESOLUTION       12      /* 12-bit: values 0–4095 */
#define ADC_VREF_MV          3300    /* Internal reference voltage: 3.3V */

/*
 * Steering angle range (degrees multiplied by 10 to avoid float values)
 * A 270° pot centred at 0 → -1350 to +1350 (-135.0° to +135.0°)
 */
#define STEERING_MIN_DEG_X10   (-1350)   /* Full left  */
#define STEERING_MAX_DEG_X10   (1350)   /* Full right */

// ADC channel from overlay
static const struct adc_dt_spec adc_steering =
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

// Heartbeat LED (LD2, on-board)
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

// Convert raw ADC count to millivolts
static int32_t raw_to_mv(int32_t raw)
{
    return (raw * ADC_VREF_MV) / ((1 << ADC_RESOLUTION) - 1);
}


 /* Maping millivolts to steering angle (degrees × 10).
 * 0 mV            → STEERING_MIN_DEG_X10  (full left)
 * 3300 mV (3.3V)  → STEERING_MAX_DEG_X10  (full right)
 * 1650 mV (1.65V) → 0°                    (center)
 */
static int32_t mv_to_angle(int32_t mv)
{
    int32_t range_deg = STEERING_MAX_DEG_X10 - STEERING_MIN_DEG_X10;
    return STEERING_MIN_DEG_X10 + (mv * range_deg) / ADC_VREF_MV;
}

/* --- Sample ADC, return raw count --- */
static int read_steering(int32_t *out_raw)
{
    int16_t buf = 0;
    struct adc_sequence seq = {
        .buffer      = &buf,
        .buffer_size = sizeof(buf),
    };

    int ret = adc_sequence_init_dt(&adc_steering, &seq);
    if (ret < 0) {
        printf("ERR: adc_sequence_init (%d)\n", ret);
        return ret;
    }

    ret = adc_read_dt(&adc_steering, &seq);
    if (ret < 0) {
        printf("ERR: adc_read (%d)\n", ret);
        return ret;
    }

    *out_raw = (int32_t)buf;
    return 0;
}

int main(void)
{
    int ret;
    bool led_state = false;

    printf("=== Steering Angle Reader ===\n");
    printf("Pot wiper → PA0 (CN7 pin 28) | Range: %d.%d to %d.%d deg\n\n",
           STEERING_MIN_DEG_X10 / 10, abs(STEERING_MIN_DEG_X10 % 10),
           STEERING_MAX_DEG_X10 / 10, STEERING_MAX_DEG_X10 % 10);

    // Feedback User LED 
    if (gpio_is_ready_dt(&led)) {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    }

    // ADC initialization
    if (!adc_is_ready_dt(&adc_steering)) {
        printf("FATAL: ADC not ready. Check overlay and wiring.\n");
        return 0;
    }
    ret = adc_channel_setup_dt(&adc_steering);
    if (ret < 0) {
        printf("FATAL: ADC channel setup failed (%d)\n", ret);
        return 0;
    }

    printf("%-10s %-12s %-14s\n", "Raw", "Voltage(mV)", "Angle(deg)");
    printf("--------------------------------------\n");

    while (1) {
        int32_t raw = 0;

        ret = read_steering(&raw);
        if (ret == 0) {
            int32_t mv    = raw_to_mv(raw);
            int32_t angle = mv_to_angle(mv);

            printf("%-10d %-12d %+d.%d\n",
                   raw, mv,
                   angle / 10,
                   abs(angle % 10));
        }

        // Toggle LED each sample
        led_state = !led_state;
        gpio_pin_set_dt(&led, led_state ? 1 : 0);

        k_msleep(SAMPLE_INTERVAL_MS);
    }

    return 0;
}