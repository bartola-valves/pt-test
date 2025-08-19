/**
 * @file eurorack_hardware.h
 * @brief Hardware abstraction layer for Eurorack modules
 *
 * This header provides high-level C++ interfaces for common Eurorack
 * hardware components using the Raspberry Pi Pico SDK and protothreads.
 * Designed specifically for interrupt-driven, event-based programming.
 */

#ifndef __EURORACK_HARDWARE_H__
#define __EURORACK_HARDWARE_H__

#include "pt_thread.h"
#include "eurorack_utils.h"
#include <functional>
#include <climits>
#include <cstdlib>

/**
 * @brief Encoder interface with interrupt support
 */
class PTEncoder
{
private:
    uint pin_a, pin_b, pin_button;
    volatile int32_t position;
    volatile bool button_state;
    volatile uint32_t last_change_time;
    PTEventQueue *event_queue;
    bool button_enabled;

    // Interrupt handlers
    static void gpio_irq_handler(uint gpio, uint32_t events);
    static PTEncoder *instances[4]; // Support up to 4 encoders
    static uint8_t instance_count;
    uint8_t instance_id;

public:
    PTEncoder(uint pin_a, uint pin_b, uint pin_button = UINT_MAX)
        : pin_a(pin_a), pin_b(pin_b), pin_button(pin_button),
          position(0), button_state(false), last_change_time(0),
          event_queue(nullptr), button_enabled(pin_button != UINT_MAX),
          instance_id(instance_count++)
    {

        if (instance_count <= 4)
        {
            instances[instance_id] = this;
        }
        init();
    }

    ~PTEncoder()
    {
        if (instance_id < 4)
        {
            instances[instance_id] = nullptr;
        }
    }

    void init()
    {
        // Configure encoder pins
        gpio_init(pin_a);
        gpio_init(pin_b);
        gpio_set_dir(pin_a, GPIO_IN);
        gpio_set_dir(pin_b, GPIO_IN);
        gpio_pull_up(pin_a);
        gpio_pull_up(pin_b);

        // Configure button pin if enabled
        if (button_enabled)
        {
            gpio_init(pin_button);
            gpio_set_dir(pin_button, GPIO_IN);
            gpio_pull_up(pin_button);

            gpio_set_irq_enabled_with_callback(pin_button,
                                               GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);
        }

        // Enable interrupts for encoder pins
        gpio_set_irq_enabled_with_callback(pin_a,
                                           GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);
    }

    void setEventQueue(PTEventQueue *queue) { event_queue = queue; }

    int32_t getPosition() const { return position; }
    void setPosition(int32_t pos) { position = pos; }
    bool getButtonState() const { return button_state; }

    void handleEncoderChange()
    {
        bool a_state = gpio_get(pin_a);
        bool b_state = gpio_get(pin_b);

        // Simple quadrature decoding
        static bool last_a = false;
        if (a_state != last_a)
        {
            if (a_state == b_state)
            {
                position++;
            }
            else
            {
                position--;
            }
            last_change_time = time_us_32();

            if (event_queue)
            {
                event_queue->push(PTEvent(PTEventType::ENCODER_TURN, position));
            }
        }
        last_a = a_state;
    }

    void handleButtonChange()
    {
        if (!button_enabled)
            return;

        bool new_state = !gpio_get(pin_button); // Active low
        if (new_state != button_state)
        {
            button_state = new_state;
            last_change_time = time_us_32();

            if (event_queue)
            {
                PTEventType event_type = new_state ? PTEventType::BUTTON_PRESS : PTEventType::BUTTON_RELEASE;
                event_queue->push(PTEvent(event_type, instance_id));
            }
        }
    }
};

/**
 * @brief Button interface with debouncing and interrupt support
 */
class PTButton
{
private:
    uint pin;
    volatile bool current_state;
    volatile bool last_state;
    volatile uint32_t last_change_time;
    volatile uint32_t press_time;
    PTEventQueue *event_queue;
    uint32_t debounce_time_us;
    bool active_low;

    static void gpio_irq_handler(uint gpio, uint32_t events);
    static PTButton *instances[8]; // Support up to 8 buttons
    static uint8_t instance_count;
    uint8_t instance_id;

public:
    PTButton(uint pin, bool active_low = true, uint32_t debounce_us = 50000)
        : pin(pin), current_state(false), last_state(false),
          last_change_time(0), press_time(0), event_queue(nullptr),
          debounce_time_us(debounce_us), active_low(active_low),
          instance_id(instance_count++)
    {

        if (instance_count <= 8)
        {
            instances[instance_id] = this;
        }
        init();
    }

    ~PTButton()
    {
        if (instance_id < 8)
        {
            instances[instance_id] = nullptr;
        }
    }

    void init()
    {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);

        gpio_set_irq_enabled_with_callback(pin,
                                           GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);
    }

    void setEventQueue(PTEventQueue *queue) { event_queue = queue; }

    bool isPressed() const { return current_state; }
    uint32_t getPressTime() const { return press_time; }

    void handleChange()
    {
        uint32_t now = time_us_32();
        bool raw_state = gpio_get(pin);
        bool new_state = active_low ? !raw_state : raw_state;

        // Debouncing
        if (now - last_change_time > debounce_time_us)
        {
            if (new_state != current_state)
            {
                last_state = current_state;
                current_state = new_state;
                last_change_time = now;

                if (current_state)
                {
                    press_time = now;
                }

                if (event_queue)
                {
                    PTEventType event_type = current_state ? PTEventType::BUTTON_PRESS : PTEventType::BUTTON_RELEASE;
                    event_queue->push(PTEvent(event_type, instance_id));
                }
            }
        }
    }
};

/**
 * @brief Gate input with edge detection and timing
 */
class PTGateInput
{
private:
    uint pin;
    volatile bool current_state;
    volatile uint32_t last_edge_time;
    volatile uint32_t gate_duration;
    PTEventQueue *event_queue;
    bool active_high;

    static void gpio_irq_handler(uint gpio, uint32_t events);
    static PTGateInput *instances[4];
    static uint8_t instance_count;
    uint8_t instance_id;

public:
    PTGateInput(uint pin, bool active_high = true)
        : pin(pin), current_state(false), last_edge_time(0), gate_duration(0),
          event_queue(nullptr), active_high(active_high),
          instance_id(instance_count++)
    {

        if (instance_count <= 4)
        {
            instances[instance_id] = this;
        }
        init();
    }

    ~PTGateInput()
    {
        if (instance_id < 4)
        {
            instances[instance_id] = nullptr;
        }
    }

    void init()
    {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_down(pin); // Typically no pull for gate inputs

        gpio_set_irq_enabled_with_callback(pin,
                                           GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);
    }

    void setEventQueue(PTEventQueue *queue) { event_queue = queue; }

    bool getState() const { return current_state; }
    uint32_t getLastEdgeTime() const { return last_edge_time; }
    uint32_t getGateDuration() const { return gate_duration; }

    void handleEdge()
    {
        uint32_t now = time_us_32();
        bool raw_state = gpio_get(pin);
        bool new_state = active_high ? raw_state : !raw_state;

        if (new_state != current_state)
        {
            if (current_state)
            {
                // Falling edge - calculate gate duration
                gate_duration = now - last_edge_time;
            }

            current_state = new_state;
            last_edge_time = now;

            if (event_queue)
            {
                PTEventType event_type = current_state ? PTEventType::GATE_RISING : PTEventType::GATE_FALLING;
                event_queue->push(PTEvent(event_type, instance_id));
            }
        }
    }
};

/**
 * @brief CV input using ADC with change detection
 */
class PTCVInput
{
private:
    uint adc_pin;
    uint adc_input;
    volatile uint16_t current_value;
    volatile uint16_t last_value;
    volatile uint32_t last_read_time;
    PTEventQueue *event_queue;
    uint16_t change_threshold;

public:
    PTCVInput(uint adc_pin, uint16_t threshold = 50)
        : adc_pin(adc_pin), current_value(0), last_value(0),
          last_read_time(0), event_queue(nullptr), change_threshold(threshold)
    {

        // Convert GPIO pin to ADC input
        if (adc_pin >= 26 && adc_pin <= 28)
        {
            adc_input = adc_pin - 26;
        }
        else if (adc_pin == 29)
        {
            adc_input = 3; // VSYS/3
        }
        else
        {
            adc_input = 0; // Default to ADC0
        }

        init();
    }

    void init()
    {
        adc_init();
        adc_gpio_init(adc_pin);
        adc_select_input(adc_input);
    }

    void setEventQueue(PTEventQueue *queue) { event_queue = queue; }

    uint16_t getValue() const { return current_value; }
    float getVoltage() const
    {
        return EurorackUtils::CV::adcToEurorackVoltage(current_value);
    }

    void update()
    {
        uint32_t now = time_us_32();
        adc_select_input(adc_input);
        uint16_t new_value = adc_read();

        if (abs((int16_t)new_value - (int16_t)current_value) > change_threshold)
        {
            last_value = current_value;
            current_value = new_value;
            last_read_time = now;

            if (event_queue)
            {
                event_queue->push(PTEvent(PTEventType::CV_CHANGE, adc_input));
            }
        }
    }
};

/**
 * @brief CV output using PWM
 */
class PTCVOutput
{
private:
    uint pin;
    uint slice;
    uint channel;
    uint16_t current_level;

public:
    PTCVOutput(uint pin) : pin(pin), current_level(0)
    {
        init();
    }

    void init()
    {
        gpio_set_function(pin, GPIO_FUNC_PWM);
        slice = pwm_gpio_to_slice_num(pin);
        channel = pwm_gpio_to_channel(pin);

        pwm_config config = pwm_get_default_config();
        pwm_config_set_clkdiv(&config, 1.0f);
        pwm_config_set_wrap(&config, 65535); // 16-bit resolution
        pwm_init(slice, &config, true);
    }

    void setVoltage(float voltage)
    {
        current_level = EurorackUtils::CV::eurorackVoltageToDAC(voltage);
        pwm_set_chan_level(slice, channel, current_level);
    }

    void setLevel(uint16_t level)
    {
        current_level = level;
        pwm_set_chan_level(slice, channel, level);
    }

    uint16_t getLevel() const { return current_level; }
    float getVoltage() const
    {
        return EurorackUtils::CV::dacToEurorackVoltage(current_level);
    }
};

/**
 * @brief Gate output with timing control
 */
class PTGateOutput
{
private:
    uint pin;
    volatile bool current_state;
    volatile uint32_t gate_start_time;
    uint32_t gate_duration_us;
    bool active_high;

public:
    PTGateOutput(uint pin, bool active_high = true, uint32_t duration_us = 10000)
        : pin(pin), current_state(false), gate_start_time(0),
          gate_duration_us(duration_us), active_high(active_high)
    {
        init();
    }

    void init()
    {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, active_high ? false : true); // Start inactive
    }

    void trigger()
    {
        current_state = true;
        gate_start_time = time_us_32();
        gpio_put(pin, active_high ? true : false);
    }

    void setHigh()
    {
        current_state = true;
        gpio_put(pin, active_high ? true : false);
    }

    void setLow()
    {
        current_state = false;
        gpio_put(pin, active_high ? false : true);
    }

    bool getState() const { return current_state; }

    void update()
    {
        if (current_state && gate_duration_us > 0)
        {
            uint32_t now = time_us_32();
            if (now - gate_start_time >= gate_duration_us)
            {
                setLow();
            }
        }
    }

    void setDuration(uint32_t duration_us) { gate_duration_us = duration_us; }
    uint32_t getDuration() const { return gate_duration_us; }
};

// Static member initializations (must be defined in a .cpp file in practice)
PTEncoder *PTEncoder::instances[4] = {nullptr};
uint8_t PTEncoder::instance_count = 0;

PTButton *PTButton::instances[8] = {nullptr};
uint8_t PTButton::instance_count = 0;

PTGateInput *PTGateInput::instances[4] = {nullptr};
uint8_t PTGateInput::instance_count = 0;

// Interrupt handler implementations
void PTEncoder::gpio_irq_handler(uint gpio, uint32_t events)
{
    for (uint8_t i = 0; i < instance_count; i++)
    {
        if (instances[i] && (instances[i]->pin_a == gpio || instances[i]->pin_b == gpio))
        {
            instances[i]->handleEncoderChange();
            break;
        }
        else if (instances[i] && instances[i]->button_enabled && instances[i]->pin_button == gpio)
        {
            instances[i]->handleButtonChange();
            break;
        }
    }
}

void PTButton::gpio_irq_handler(uint gpio, uint32_t events)
{
    for (uint8_t i = 0; i < instance_count; i++)
    {
        if (instances[i] && instances[i]->pin == gpio)
        {
            instances[i]->handleChange();
            break;
        }
    }
}

void PTGateInput::gpio_irq_handler(uint gpio, uint32_t events)
{
    for (uint8_t i = 0; i < instance_count; i++)
    {
        if (instances[i] && instances[i]->pin == gpio)
        {
            instances[i]->handleEdge();
            break;
        }
    }
}

#endif // __EURORACK_HARDWARE_H__
