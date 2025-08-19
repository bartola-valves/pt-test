/**
 * @file pt-test-eurorack.cpp
 * @brief Complete Eurorack module framework using SimpleThreads
 *
 * This example demonstrates a comprehensive framework for Eurorack modules with:
 * - Encoder handling for parameter control
 * - Button interface for mode switching
 * - LED status indicators
 * - CV input/output for audio signals
 * - Gate input/output for triggering
 * - Sequencer logic with timing
 * - Event-driven programming model
 * - Non-blocking cooperative threading
 */

#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include <cstdio>
#include <cstdlib>

#include "framework/simple_threads.h"
#include "framework/eurorack_utils.h"

// Hardware pin definitions (adjust for your hardware)
#define ENCODER1_A_PIN 2
#define ENCODER1_B_PIN 3
#define ENCODER1_BTN_PIN 4

#define BUTTON1_PIN 5
#define BUTTON2_PIN 6

#define LED1_PIN 25 // Onboard LED
#define LED2_PIN 15 // External LEDs
#define LED3_PIN 16

#define CV_IN1_PIN 26  // ADC0
#define CV_IN2_PIN 27  // ADC1
#define CV_OUT1_PIN 20 // PWM
#define CV_OUT2_PIN 21 // PWM

#define GATE_IN_PIN 7
#define GATE_OUT_PIN 8

// Event system for inter-thread communication
struct EurorackEvent
{
    enum Type
    {
        NONE = 0,
        ENCODER_TURN,
        BUTTON_PRESS,
        BUTTON_RELEASE,
        GATE_RISING,
        GATE_FALLING,
        SEQUENCE_STEP,
        CV_CHANGE
    };

    Type type;
    uint32_t data;
    uint32_t timestamp;
};

class EurorackEventQueue
{
private:
    static const size_t MAX_EVENTS = 16;
    EurorackEvent events[MAX_EVENTS];
    volatile size_t head = 0;
    volatile size_t tail = 0;
    volatile size_t count = 0;
    spin_lock_t *lock;

public:
    EurorackEventQueue()
    {
        lock = spin_lock_init(spin_lock_claim_unused(true));
    }
    bool push(EurorackEvent::Type type, uint32_t data = 0)
    {
        if (count >= MAX_EVENTS)
            return false;

        uint32_t irq_state = spin_lock_blocking(lock);
        events[head] = {type, data, time_us_32()};
        head = (head + 1) % MAX_EVENTS;
        count++;
        spin_unlock(lock, irq_state);
        return true;
    }

    bool pop(EurorackEvent &event)
    {
        if (count == 0)
            return false;

        uint32_t irq_state = spin_lock_blocking(lock);
        event = events[tail];
        tail = (tail + 1) % MAX_EVENTS;
        count--;
        spin_unlock(lock, irq_state);
        return true;
    }

    bool isEmpty() const { return count == 0; }
    size_t size() const { return count; }
};

// Global event queue
EurorackEventQueue g_event_queue;

// Global state variables
volatile float g_tempo_bpm = 120.0f;
volatile bool g_sequencer_running = false;
volatile uint8_t g_current_step = 0;
volatile uint8_t g_sequence_length = 8;
volatile float g_sequence_voltages[16] = {0.0f}; // Up to 16 step sequence

// Encoder state tracking
struct EncoderState
{
    volatile int32_t position = 0;
    volatile bool button_pressed = false;
    volatile uint32_t last_change = 0;
    bool last_a = false;
    bool last_b = false;

    void update()
    {
        bool a_state = gpio_get(ENCODER1_A_PIN);
        bool b_state = gpio_get(ENCODER1_B_PIN);
        bool btn_state = !gpio_get(ENCODER1_BTN_PIN); // Active low

        // Quadrature decoding
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
            last_change = time_us_32();
            g_event_queue.push(EurorackEvent::ENCODER_TURN, position);
        }
        last_a = a_state;
        last_b = b_state;

        // Button state change
        if (btn_state != button_pressed)
        {
            button_pressed = btn_state;
            if (btn_state)
            {
                g_event_queue.push(EurorackEvent::BUTTON_PRESS, 0);
            }
            else
            {
                g_event_queue.push(EurorackEvent::BUTTON_RELEASE, 0);
            }
        }
    }
};

EncoderState g_encoder1;

// Button state tracking
struct ButtonState
{
    uint pin;
    volatile bool pressed = false;
    volatile uint32_t last_change = 0;
    uint32_t debounce_time = 50000; // 50ms

    ButtonState(uint p) : pin(p) {}

    void update()
    {
        bool state = !gpio_get(pin); // Active low
        uint32_t now = time_us_32();

        if (state != pressed && (now - last_change > debounce_time))
        {
            pressed = state;
            last_change = now;

            if (state)
            {
                g_event_queue.push(EurorackEvent::BUTTON_PRESS, pin);
            }
            else
            {
                g_event_queue.push(EurorackEvent::BUTTON_RELEASE, pin);
            }
        }
    }
};

ButtonState g_button1(BUTTON1_PIN);
ButtonState g_button2(BUTTON2_PIN);

// Gate input tracking
struct GateInputState
{
    volatile bool current_state = false;
    volatile uint32_t last_edge_time = 0;

    void update()
    {
        bool new_state = gpio_get(GATE_IN_PIN);
        if (new_state != current_state)
        {
            current_state = new_state;
            last_edge_time = time_us_32();

            if (new_state)
            {
                g_event_queue.push(EurorackEvent::GATE_RISING);
            }
            else
            {
                g_event_queue.push(EurorackEvent::GATE_FALLING);
            }
        }
    }
};

GateInputState g_gate_input;

// Gate output control
struct GateOutputState
{
    volatile bool active = false;
    volatile uint32_t trigger_time = 0;
    uint32_t gate_duration = 10000; // 10ms default

    void trigger()
    {
        active = true;
        trigger_time = time_us_32();
        gpio_put(GATE_OUT_PIN, true);
    }

    void update()
    {
        if (active)
        {
            uint32_t now = time_us_32();
            if (now - trigger_time >= gate_duration)
            {
                active = false;
                gpio_put(GATE_OUT_PIN, false);
            }
        }
    }

    void setHigh()
    {
        active = true;
        gpio_put(GATE_OUT_PIN, true);
    }

    void setLow()
    {
        active = false;
        gpio_put(GATE_OUT_PIN, false);
    }
};

GateOutputState g_gate_output;

// CV output control
struct CVOutputState
{
    uint pin;
    uint slice;
    uint channel;
    uint16_t current_level = 32767; // Mid-range

    CVOutputState(uint p) : pin(p)
    {
        gpio_set_function(pin, GPIO_FUNC_PWM);
        slice = pwm_gpio_to_slice_num(pin);
        channel = pwm_gpio_to_channel(pin);

        pwm_config config = pwm_get_default_config();
        pwm_config_set_clkdiv(&config, 1.0f);
        pwm_config_set_wrap(&config, 65535);
        pwm_init(slice, &config, true);

        setLevel(current_level);
    }

    void setVoltage(float voltage)
    {
        // Convert -5V to +5V range to 0-65535
        voltage = EurorackUtils::Math::constrain(voltage, -5.0f, 5.0f);
        current_level = (uint16_t)((voltage + 5.0f) / 10.0f * 65535.0f);
        pwm_set_chan_level(slice, channel, current_level);
    }

    void setLevel(uint16_t level)
    {
        current_level = level;
        pwm_set_chan_level(slice, channel, level);
    }

    float getVoltage() const
    {
        return (current_level / 65535.0f) * 10.0f - 5.0f;
    }
};

CVOutputState g_cv_out1(CV_OUT1_PIN);
CVOutputState g_cv_out2(CV_OUT2_PIN);

/**
 * @brief Hardware Input Polling Thread
 * Polls all hardware inputs and generates events
 */
class InputPollingThread : public SimpleThread
{
private:
    uint32_t last_poll_time = 0;
    const uint32_t poll_interval = 1000; // 1ms polling

public:
    InputPollingThread() : SimpleThread("InputPolling") {}

    void execute() override
    {
        uint32_t now = time_us_32();

        if (now - last_poll_time >= poll_interval)
        {
            last_poll_time = now;

            // Poll all hardware inputs
            g_encoder1.update();
            g_button1.update();
            g_button2.update();
            g_gate_input.update();

            // Sample CV inputs
            adc_select_input(0); // CV_IN1
            uint16_t cv1 = adc_read();
            adc_select_input(1); // CV_IN2
            uint16_t cv2 = adc_read();

            // Generate CV change events if significant change
            static uint16_t last_cv1 = 0, last_cv2 = 0;
            if (std::abs((int16_t)cv1 - (int16_t)last_cv1) > 50)
            {
                last_cv1 = cv1;
                g_event_queue.push(EurorackEvent::CV_CHANGE, 1);
            }
            if (std::abs((int16_t)cv2 - (int16_t)last_cv2) > 50)
            {
                last_cv2 = cv2;
                g_event_queue.push(EurorackEvent::CV_CHANGE, 2);
            }
        }
    }
};

/**
 * @brief User Interface Thread
 * Processes user input events and updates parameters
 */
class UIThread : public SimpleThread
{
private:
    int32_t last_encoder_pos = 0;
    bool encoder_button_pressed = false;
    uint32_t led_blink_time = 0;

public:
    UIThread() : SimpleThread("UI") {}

    void execute() override
    {
        EurorackEvent event;

        // Process all pending events
        while (g_event_queue.pop(event))
        {
            switch (event.type)
            {
            case EurorackEvent::ENCODER_TURN:
            {
                int32_t new_pos = (int32_t)event.data;
                int32_t delta = new_pos - last_encoder_pos;
                last_encoder_pos = new_pos;

                if (!encoder_button_pressed)
                {
                    // Adjust tempo
                    g_tempo_bpm += delta * 5.0f;
                    g_tempo_bpm = EurorackUtils::Math::constrain(g_tempo_bpm, 60.0f, 200.0f);
                }
                else
                {
                    // Adjust sequence length
                    g_sequence_length += delta;
                    g_sequence_length = EurorackUtils::Math::constrain((float)g_sequence_length, 1.0f, 16.0f);
                }

                // Blink LED to indicate parameter change
                gpio_put(LED1_PIN, true);
                led_blink_time = time_us_32();
                break;
            }

            case EurorackEvent::BUTTON_PRESS:
            {
                if (event.data == ENCODER1_BTN_PIN)
                {
                    encoder_button_pressed = true;
                }
                else if (event.data == BUTTON1_PIN)
                {
                    // Start/Stop sequencer
                    g_sequencer_running = !g_sequencer_running;
                    gpio_put(LED2_PIN, g_sequencer_running);
                }
                else if (event.data == BUTTON2_PIN)
                {
                    // Reset sequencer
                    g_current_step = 0;
                    gpio_put(LED3_PIN, true);
                    led_blink_time = time_us_32();
                }
                break;
            }

            case EurorackEvent::BUTTON_RELEASE:
            {
                if (event.data == ENCODER1_BTN_PIN)
                {
                    encoder_button_pressed = false;
                }
                break;
            }

            default:
                break;
            }
        }

        // Turn off LED blink after 100ms
        if (led_blink_time > 0 && (time_us_32() - led_blink_time) > 100000)
        {
            gpio_put(LED1_PIN, false);
            gpio_put(LED3_PIN, false);
            led_blink_time = 0;
        }
    }
};

/**
 * @brief Sequencer Thread
 * Generates timing and step progression for the sequencer
 */
class SequencerThread : public SimpleThread
{
private:
    uint32_t last_step_time = 0;
    uint32_t step_interval_us = 500000; // 500ms = 120 BPM

    void updateStepInterval()
    {
        // Convert BPM to microseconds per step
        float steps_per_second = g_tempo_bpm / 60.0f;
        step_interval_us = (uint32_t)(1000000.0f / steps_per_second);
    }

public:
    SequencerThread() : SimpleThread("Sequencer") {}

    void execute() override
    {
        if (!g_sequencer_running)
        {
        }

        uint32_t now = time_us_32();
        updateStepInterval();

        if (now - last_step_time >= step_interval_us)
        {
            last_step_time = now;

            // Advance sequence step
            g_current_step = (g_current_step + 1) % g_sequence_length;

            // Output CV for current step
            g_cv_out1.setVoltage(g_sequence_voltages[g_current_step]);

            // Trigger gate output
            g_gate_output.trigger();

            // Generate sequence step event
            g_event_queue.push(EurorackEvent::SEQUENCE_STEP, g_current_step);
        }
    }
};

/**
 * @brief Gate Input Processing Thread
 * Handles external gate input for sync and tempo detection
 */
class GateInputThread : public SimpleThread
{
private:
    uint32_t last_gate_time = 0;

public:
    GateInputThread() : SimpleThread("GateInput") {}

    void execute() override
    {
        EurorackEvent event;

        while (g_event_queue.pop(event))
        {
            if (event.type == EurorackEvent::GATE_RISING)
            {
                uint32_t now = event.timestamp;

                if (last_gate_time > 0)
                {
                    // Calculate tempo from gate interval
                    uint32_t interval = now - last_gate_time;
                    if (interval > 100000 && interval < 2000000)
                    { // Valid range: 0.1-2 seconds
                        float detected_bpm = 60000000.0f / interval;
                        g_tempo_bpm = detected_bpm;
                    }
                }
                last_gate_time = now;

                // External sync mode - advance sequencer if not running internally
                if (!g_sequencer_running)
                {
                    g_current_step = (g_current_step + 1) % g_sequence_length;
                    g_cv_out1.setVoltage(g_sequence_voltages[g_current_step]);
                    g_gate_output.trigger();
                }
            }
        }
    }
};

/**
 * @brief CV Processing Thread
 * Handles CV input processing and sequence voltage updates
 */
class CVProcessingThread : public SimpleThread
{
private:
    uint32_t last_sample_time = 0;
    const uint32_t sample_interval = 5000; // 5ms sampling

public:
    CVProcessingThread() : SimpleThread("CVProcessing") {}

    void execute() override
    {
        uint32_t now = time_us_32();

        if (now - last_sample_time >= sample_interval)
        {
            last_sample_time = now;

            // Sample CV inputs and update sequence
            adc_select_input(0);
            uint16_t cv1_raw = adc_read();
            float cv1_voltage = EurorackUtils::CV::adcToEurorackVoltage(cv1_raw);

            // Update current step voltage with CV input 1
            if (g_current_step < 16)
            {
                g_sequence_voltages[g_current_step] = cv1_voltage;
            }

            // Process CV input 2 for modulation (could control tempo, etc.)
            adc_select_input(1);
            uint16_t cv2_raw = adc_read();
            float cv2_voltage = EurorackUtils::CV::adcToEurorackVoltage(cv2_raw);

            // Use CV2 to modulate tempo (example)
            float tempo_mod = cv2_voltage * 10.0f; // Scale factor
            float modulated_tempo = EurorackUtils::Math::constrain(g_tempo_bpm + tempo_mod, 60.0f, 200.0f);
            // Apply modulation gradually
            g_tempo_bpm = g_tempo_bpm * 0.99f + modulated_tempo * 0.01f;
        }
    }
};

/**
 * @brief Hardware Maintenance Thread
 * Updates hardware outputs and performs background tasks
 */
class MaintenanceThread : public SimpleThread
{
private:
    uint32_t last_update_time = 0;
    const uint32_t update_interval = 10000; // 10ms

public:
    MaintenanceThread() : SimpleThread("Maintenance") {}

    void execute() override
    {
        uint32_t now = time_us_32();

        if (now - last_update_time >= update_interval)
        {
            last_update_time = now;

            // Update gate outputs (for timed gates)
            g_gate_output.update();

            // Update status LED based on sequencer state
            if (g_sequencer_running)
            {
                // Blink LED in sync with sequence steps
                bool led_state = (g_current_step % 2) == 0;
                gpio_put(LED2_PIN, led_state);
            }

            // Output second CV channel with modulated value
            float cv2_out = g_sequence_voltages[g_current_step] * 0.5f; // Harmonic
            g_cv_out2.setVoltage(cv2_out);
        }
    }
};

/**
 * @brief Status Display Thread
 * Updates status information (could drive OLED/LCD display)
 */
class StatusThread : public SimpleThread
{
private:
    uint32_t last_display_time = 0;
    const uint32_t display_interval = 250000; // 250ms

public:
    StatusThread() : SimpleThread("Status") {}

    void execute() override
    {
        uint32_t now = time_us_32();

        if (now - last_display_time >= display_interval)
        {
            last_display_time = now;

            // Print status information (in real implementation, update display)
            static uint32_t status_count = 0;
            if ((status_count++ % 4) == 0)
            { // Print every 4th update (1 second)
                printf("Tempo: %.1f BPM | Step: %d/%d | Running: %s | CV1: %.2fV\n",
                       g_tempo_bpm,
                       g_current_step + 1,
                       g_sequence_length,
                       g_sequencer_running ? "YES" : "NO",
                       g_cv_out1.getVoltage());
            }
        }
    }
};

// Hardware initialization
void init_hardware()
{
    stdio_init_all();

    // Initialize GPIO pins
    gpio_init(LED1_PIN);
    gpio_init(LED2_PIN);
    gpio_init(LED3_PIN);
    gpio_set_dir(LED1_PIN, GPIO_OUT);
    gpio_set_dir(LED2_PIN, GPIO_OUT);
    gpio_set_dir(LED3_PIN, GPIO_OUT);

    // Initialize encoder pins
    gpio_init(ENCODER1_A_PIN);
    gpio_init(ENCODER1_B_PIN);
    gpio_init(ENCODER1_BTN_PIN);
    gpio_set_dir(ENCODER1_A_PIN, GPIO_IN);
    gpio_set_dir(ENCODER1_B_PIN, GPIO_IN);
    gpio_set_dir(ENCODER1_BTN_PIN, GPIO_IN);
    gpio_pull_up(ENCODER1_A_PIN);
    gpio_pull_up(ENCODER1_B_PIN);
    gpio_pull_up(ENCODER1_BTN_PIN);

    // Initialize button pins
    gpio_init(BUTTON1_PIN);
    gpio_init(BUTTON2_PIN);
    gpio_set_dir(BUTTON1_PIN, GPIO_IN);
    gpio_set_dir(BUTTON2_PIN, GPIO_IN);
    gpio_pull_up(BUTTON1_PIN);
    gpio_pull_up(BUTTON2_PIN);

    // Initialize gate pins
    gpio_init(GATE_IN_PIN);
    gpio_init(GATE_OUT_PIN);
    gpio_set_dir(GATE_IN_PIN, GPIO_IN);
    gpio_set_dir(GATE_OUT_PIN, GPIO_OUT);
    gpio_pull_down(GATE_IN_PIN);
    gpio_put(GATE_OUT_PIN, false);

    // Initialize ADC for CV inputs
    adc_init();
    adc_gpio_init(CV_IN1_PIN);
    adc_gpio_init(CV_IN2_PIN);

    // Initialize sequence with default values (chromatic scale)
    for (int i = 0; i < 16; i++)
    {
        g_sequence_voltages[i] = (float)i / 12.0f; // 1V per octave, semitone steps
    }
}

int main()
{
    init_hardware();

    printf("Eurorack Module Framework Starting...\n");
    printf("Features: Encoder, Buttons, CV I/O, Gate I/O, Sequencer\n");
    printf("Thread-based architecture with event system\n\n");

    // Create scheduler and threads
    SimpleScheduler scheduler;

    // Create all thread instances
    InputPollingThread input_thread;
    UIThread ui_thread;
    SequencerThread sequencer_thread;
    GateInputThread gate_thread;
    CVProcessingThread cv_thread;
    MaintenanceThread maintenance_thread;
    StatusThread status_thread;

    // Add threads to scheduler
    scheduler.addThread(&input_thread);
    scheduler.addThread(&ui_thread);
    scheduler.addThread(&sequencer_thread);
    scheduler.addThread(&gate_thread);
    scheduler.addThread(&cv_thread);
    scheduler.addThread(&maintenance_thread);
    scheduler.addThread(&status_thread);

    printf("Starting scheduler with %d threads...\n", scheduler.getThreadCount());

    // Run the scheduler
    scheduler.run();

    return 0;
}
