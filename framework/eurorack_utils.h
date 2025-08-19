/**
 * @file eurorack_utils.h
 * @brief Utility functions for Eurorack modules using Raspberry Pi Pico
 * @author Eurorack Framework
 */

#ifndef __EURORACK_UTILS_H__
#define __EURORACK_UTILS_H__

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"

namespace EurorackUtils
{

    /**
     * @brief Initialize the Pico for Eurorack module use
     */
    inline void init()
    {
        stdio_init_all();

        // Initialize onboard LED
        const uint LED_PIN = PICO_DEFAULT_LED_PIN;
        gpio_init(LED_PIN);
        gpio_set_dir(LED_PIN, GPIO_OUT);

        // Initialize ADC for CV inputs
        adc_init();
    }

    /**
     * @brief LED control utilities
     */
    namespace LED
    {
        const uint ONBOARD_PIN = PICO_DEFAULT_LED_PIN;

        inline void on()
        {
            gpio_put(ONBOARD_PIN, 1);
        }

        inline void off()
        {
            gpio_put(ONBOARD_PIN, 0);
        }

        inline void toggle()
        {
            gpio_put(ONBOARD_PIN, !gpio_get(ONBOARD_PIN));
        }

        inline bool getState()
        {
            return gpio_get(ONBOARD_PIN);
        }
    }

    /**
     * @brief CV (Control Voltage) utilities
     */
    namespace CV
    {
        /**
         * @brief Initialize a GPIO pin for CV input (ADC)
         * @param pin GPIO pin number (26, 27, 28 for ADC0, ADC1, ADC2)
         */
        inline void initInput(uint pin)
        {
            adc_gpio_init(pin);
        }

        /**
         * @brief Read CV input value
         * @param adc_channel ADC channel (0-2)
         * @return Raw ADC value (0-4095)
         */
        inline uint16_t readRaw(uint adc_channel)
        {
            adc_select_input(adc_channel);
            return adc_read();
        }

        /**
         * @brief Read CV input as voltage (0-3.3V)
         * @param adc_channel ADC channel (0-2)
         * @return Voltage in volts
         */
        inline float readVoltage(uint adc_channel)
        {
            return (readRaw(adc_channel) * 3.3f) / 4096.0f;
        }

        /**
         * @brief Read CV input scaled to Eurorack range (-5V to +5V theoretical)
         * Note: Pico can only read 0-3.3V, so this assumes appropriate conditioning
         * @param adc_channel ADC channel (0-2)
         * @return Scaled voltage (-5.0 to +5.0)
         */
        inline float readEurorackVoltage(uint adc_channel)
        {
            float voltage = readVoltage(adc_channel);
            return (voltage - 1.65f) * (10.0f / 3.3f); // Scale and center
        }

        /**
         * @brief Convert ADC reading to Eurorack voltage
         * @param adc_value 12-bit ADC value (0-4095)
         * @return Voltage in range -5V to +5V
         */
        inline float adcToEurorackVoltage(uint16_t adc_value)
        {
            // ADC gives 0-4095 for 0-3.3V input
            // Scale to -5V to +5V range
            float voltage = (adc_value / 4095.0f) * 3.3f; // 0-3.3V
            return (voltage - 1.65f) * (10.0f / 3.3f);    // Scale to -5V to +5V
        }

        /**
         * @brief Convert Eurorack voltage to DAC value
         * @param voltage Voltage in range -5V to +5V
         * @return 16-bit DAC value (0-65535)
         */
        inline uint16_t eurorackVoltageToDAC(float voltage)
        {
            // Clamp voltage to valid range
            if (voltage < -5.0f)
                voltage = -5.0f;
            if (voltage > 5.0f)
                voltage = 5.0f;

            // Scale from -5V to +5V to 0-65535
            return (uint16_t)((voltage + 5.0f) / 10.0f * 65535.0f);
        }

        /**
         * @brief Convert DAC value to Eurorack voltage
         * @param dac_value 16-bit DAC value (0-65535)
         * @return Voltage in range -5V to +5V
         */
        inline float dacToEurorackVoltage(uint16_t dac_value)
        {
            return (dac_value / 65535.0f) * 10.0f - 5.0f;
        }
    }

    /**
     * @brief Gate/Trigger utilities
     */
    namespace Gate
    {
        /**
         * @brief Initialize a GPIO pin for gate input
         * @param pin GPIO pin number
         */
        inline void initInput(uint pin)
        {
            gpio_init(pin);
            gpio_set_dir(pin, GPIO_IN);
            gpio_pull_down(pin); // Pull down for gate detection
        }

        /**
         * @brief Initialize a GPIO pin for gate output
         * @param pin GPIO pin number
         */
        inline void initOutput(uint pin)
        {
            gpio_init(pin);
            gpio_set_dir(pin, GPIO_OUT);
            gpio_put(pin, 0); // Start low
        }

        /**
         * @brief Read gate input state
         * @param pin GPIO pin number
         * @return true if gate is high
         */
        inline bool read(uint pin)
        {
            return gpio_get(pin);
        }

        /**
         * @brief Set gate output state
         * @param pin GPIO pin number
         * @param state true for high, false for low
         */
        inline void write(uint pin, bool state)
        {
            gpio_put(pin, state);
        }

        /**
         * @brief Toggle gate output
         * @param pin GPIO pin number
         */
        inline void toggle(uint pin)
        {
            gpio_put(pin, !gpio_get(pin));
        }
    }

    /**
     * @brief Timing utilities
     */
    namespace Timing
    {
        /**
         * @brief Get current time in microseconds
         * @return Time in microseconds since boot
         */
        inline uint64_t getMicros()
        {
            return to_us_since_boot(get_absolute_time());
        }

        /**
         * @brief Get current time in milliseconds
         * @return Time in milliseconds since boot
         */
        inline uint32_t getMillis()
        {
            return to_ms_since_boot(get_absolute_time());
        }

        /**
         * @brief Non-blocking delay check
         * @param last_time Reference to last time variable
         * @param delay_ms Delay in milliseconds
         * @return true if delay has elapsed
         */
        inline bool delayElapsed(uint32_t &last_time, uint32_t delay_ms)
        {
            uint32_t current_time = getMillis();
            if ((current_time - last_time) >= delay_ms)
            {
                last_time = current_time;
                return true;
            }
            return false;
        }
    }

    /**
     * @brief Math utilities for audio/CV processing
     */
    namespace Math
    {
        /**
         * @brief Map a value from one range to another
         * @param value Input value
         * @param in_min Input range minimum
         * @param in_max Input range maximum
         * @param out_min Output range minimum
         * @param out_max Output range maximum
         * @return Mapped value
         */
        inline float map(float value, float in_min, float in_max, float out_min, float out_max)
        {
            return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
        }

        /**
         * @brief Constrain a value to a range
         * @param value Input value
         * @param min_val Minimum value
         * @param max_val Maximum value
         * @return Constrained value
         */
        inline float constrain(float value, float min_val, float max_val)
        {
            if (value < min_val)
                return min_val;
            if (value > max_val)
                return max_val;
            return value;
        }

        /**
         * @brief Clamp a value to a range (alias for constrain)
         */
        template <typename T>
        inline T clamp(T value, T min_val, T max_val)
        {
            if (value < min_val)
                return min_val;
            if (value > max_val)
                return max_val;
            return value;
        }
    }
}

#endif /* __EURORACK_UTILS_H__ */
