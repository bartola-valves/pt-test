/**
 * @file pt-test-full.cpp
 * @brief Complete protothreads example for Eurorack module
 *
 * This example demonstrates the full protothreads framework with:
 * - Encoder handling for parameter control
 * - Button interface for mode switching
 * - LED status indicators
 * - CV input/output for audio signals
 * - Gate input/output for triggering
 * - Screen management for UI
 * - Sequencer logic with timing
 * - Interrupt-driven event processing
 */

#include "pico/stdlib.h"
#include "hardware/timer.h"
#include <cstdio>

#include "framework/pt_thread.h"
#include "framework/eurorack_hardware.h"
#include "framework/eurorack_utils.h"

// Hardware pin definitions (adjust for your hardware)
#define ENCODER1_A_PIN 2
#define ENCODER1_B_PIN 3
#define ENCODER1_BTN_PIN 4

#define BUTTON1_PIN 5
#define BUTTON2_PIN 6

#define LED1_PIN 25 // Onboard LED
#define LED2_PIN 15
#define LED3_PIN 16

#define CV_IN1_PIN 26  // ADC0
#define CV_IN2_PIN 27  // ADC1
#define CV_OUT1_PIN 20 // PWM
#define CV_OUT2_PIN 21 // PWM

#define GATE_IN_PIN 7
#define GATE_OUT_PIN 8

// Global hardware objects
PTEncoder encoder1(ENCODER1_A_PIN, ENCODER1_B_PIN, ENCODER1_BTN_PIN);
PTButton button1(BUTTON1_PIN);
PTButton button2(BUTTON2_PIN);
PTCVInput cv_in1(CV_IN1_PIN);
PTCVInput cv_in2(CV_IN2_PIN);
PTCVOutput cv_out1(CV_OUT1_PIN);
PTCVOutput cv_out2(CV_OUT2_PIN);
PTGateInput gate_in(GATE_IN_PIN);
PTGateOutput gate_out(GATE_OUT_PIN);

// Global variables
volatile float tempo_bpm = 120.0f;
volatile bool sequencer_running = false;
volatile uint8_t current_step = 0;
volatile uint8_t sequence_length = 8;
volatile float sequence_voltages[16] = {0.0f}; // Up to 16 step sequence

/**
 * @brief User Interface Thread
 * Handles encoder and button input for parameter control
 */
class UIThread : public PTThread
{
private:
    PTEvent event;
    int32_t last_encoder_pos = 0;
    bool encoder_button_pressed = false;
    uint32_t led_blink_time = 0;

public:
    UIThread() : PTThread("UI") {}

    int run() override
    {
        PT_THREAD_BEGIN(this);

        while (true)
        {
            // Wait for UI events (encoder, buttons)
            PT_WAIT_EVENT(this, event);

            switch (event.type)
            {
            case PTEventType::ENCODER_TURN:
            {
                int32_t new_pos = (int32_t)event.data;
                int32_t delta = new_pos - last_encoder_pos;
                last_encoder_pos = new_pos;

                if (!encoder_button_pressed)
                {
                    // Adjust tempo
                    tempo_bpm += delta * 5.0f;
                    tempo_bpm = EurorackUtils::Math::clamp(tempo_bpm, 60.0f, 200.0f);
                }
                else
                {
                    // Adjust sequence length
                    sequence_length += delta;
                    sequence_length = EurorackUtils::Math::clamp((int)sequence_length, 1, 16);
                }

                // Blink LED to indicate parameter change
                gpio_put(LED1_PIN, true);
                led_blink_time = time_us_32();
                break;
            }

            case PTEventType::BUTTON_PRESS:
            {
                if (event.data == 0)
                { // Encoder button
                    encoder_button_pressed = true;
                }
                else if (event.data == 1)
                { // Button 1 - Start/Stop
                    sequencer_running = !sequencer_running;
                    gpio_put(LED2_PIN, sequencer_running);
                }
                else if (event.data == 2)
                { // Button 2 - Reset
                    current_step = 0;
                    gpio_put(LED3_PIN, true);
                    led_blink_time = time_us_32();
                }
                break;
            }

            case PTEventType::BUTTON_RELEASE:
            {
                if (event.data == 0)
                { // Encoder button
                    encoder_button_pressed = false;
                }
                break;
            }

            default:
                break;
            }

            // Turn off LED blink after 100ms
            if (led_blink_time > 0 && (time_us_32() - led_blink_time) > 100000)
            {
                gpio_put(LED1_PIN, false);
                gpio_put(LED3_PIN, false);
                led_blink_time = 0;
            }

            PT_THREAD_YIELD(this);
        }

        PT_THREAD_END(this);
    }
};

/**
 * @brief CV Input Processing Thread
 * Samples CV inputs and updates sequence values
 */
class CVInputThread : public PTThread
{
private:
    uint32_t last_sample_time = 0;
    const uint32_t sample_interval = 1000; // 1ms sampling

public:
    CVInputThread() : PTThread("CVInput") {}

    int run() override
    {
        PT_THREAD_BEGIN(this);

        while (true)
        {
            // Sample CV inputs at regular intervals
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - last_sample_time) >= sample_interval);
            last_sample_time = time_us_32();

            // Update CV inputs
            cv_in1.update();
            cv_in2.update();

            // Update current sequence step with CV input 1
            if (sequencer_running && current_step < sequence_length)
            {
                sequence_voltages[current_step] = cv_in1.getVoltage();
            }

            PT_THREAD_YIELD(this);
        }

        PT_THREAD_END(this);
    }
};

/**
 * @brief Sequencer Thread
 * Generates timing and triggers based on tempo
 */
class SequencerThread : public PTThread
{
private:
    uint32_t last_step_time = 0;
    uint32_t step_interval_us = 500000; // 500ms = 120 BPM
    PTEvent event;

    void updateStepInterval()
    {
        // Convert BPM to microseconds per step
        float steps_per_second = tempo_bpm / 60.0f;
        step_interval_us = (uint32_t)(1000000.0f / steps_per_second);
    }

public:
    SequencerThread() : PTThread("Sequencer") {}

    int run() override
    {
        PT_THREAD_BEGIN(this);

        while (true)
        {
            // Wait for sequencer to be enabled
            PT_THREAD_WAIT_UNTIL(this, sequencer_running);

            updateStepInterval();

            // Wait for next step time
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - last_step_time) >= step_interval_us);
            last_step_time = time_us_32();

            // Advance sequence step
            current_step = (current_step + 1) % sequence_length;

            // Output CV for current step
            cv_out1.setVoltage(sequence_voltages[current_step]);

            // Trigger gate output
            gate_out.trigger();

            // Post sequence step event
            if (event_queue)
            {
                event_queue->push(PTEvent(PTEventType::SEQUENCE_STEP, current_step));
            }

            PT_THREAD_YIELD(this);
        }

        PT_THREAD_END(this);
    }
};

/**
 * @brief Gate Input Monitoring Thread
 * Processes external gate triggers and sync
 */
class GateInputThread : public PTThread
{
private:
    PTEvent event;
    uint32_t last_gate_time = 0;

public:
    GateInputThread() : PTThread("GateInput") {}

    int run() override
    {
        PT_THREAD_BEGIN(this);

        while (true)
        {
            // Wait for gate events
            PT_WAIT_EVENT_TYPE(this, event, PTEventType::GATE_RISING);

            uint32_t now = time_us_32();
            if (last_gate_time > 0)
            {
                // Calculate tempo from gate interval
                uint32_t interval = now - last_gate_time;
                if (interval > 100000 && interval < 2000000)
                { // Valid range: 0.1-2 seconds
                    float detected_bpm = 60000000.0f / interval;
                    tempo_bpm = detected_bpm;
                }
            }
            last_gate_time = now;

            // External sync mode - advance sequencer
            if (!sequencer_running)
            {
                current_step = (current_step + 1) % sequence_length;
                cv_out1.setVoltage(sequence_voltages[current_step]);
            }

            PT_THREAD_YIELD(this);
        }

        PT_THREAD_END(this);
    }
};

/**
 * @brief Hardware Maintenance Thread
 * Updates hardware state and performs background tasks
 */
class MaintenanceThread : public PTThread
{
private:
    uint32_t last_update_time = 0;
    const uint32_t update_interval = 10000; // 10ms

public:
    MaintenanceThread() : PTThread("Maintenance") {}

    int run() override
    {
        PT_THREAD_BEGIN(this);

        while (true)
        {
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - last_update_time) >= update_interval);
            last_update_time = time_us_32();

            // Update gate outputs (for timed gates)
            gate_out.update();

            // Update status LED based on sequencer state
            if (sequencer_running)
            {
                // Blink LED in sync with sequence
                bool led_state = (current_step % 2) == 0;
                gpio_put(LED2_PIN, led_state);
            }

            PT_THREAD_YIELD(this);
        }

        PT_THREAD_END(this);
    }
};

/**
 * @brief Screen Update Thread (placeholder for future OLED/LCD integration)
 * Updates display with current parameters and status
 */
class ScreenThread : public PTThread
{
private:
    uint32_t last_refresh_time = 0;
    const uint32_t refresh_interval = 100000; // 100ms refresh rate
    PTEvent event;

public:
    ScreenThread() : PTThread("Screen") {}

    int run() override
    {
        PT_THREAD_BEGIN(this);

        while (true)
        {
            // Wait for screen refresh time or screen refresh event
            PT_THREAD_WAIT_UNTIL(this,
                                 (time_us_32() - last_refresh_time) >= refresh_interval ||
                                     (event_queue && event_queue->pop(event) && event.type == PTEventType::SCREEN_REFRESH));

            last_refresh_time = time_us_32();

            // Screen update logic would go here
            // For now, we'll just demonstrate the threading structure

            // Placeholder: Print status to console (in real implementation, update LCD/OLED)
            static uint32_t screen_updates = 0;
            if ((screen_updates++ % 10) == 0)
            { // Print every 10th update
                printf("Tempo: %.1f BPM, Step: %d/%d, Running: %s\n",
                       tempo_bpm, current_step + 1, sequence_length,
                       sequencer_running ? "YES" : "NO");
            }

            PT_THREAD_YIELD(this);
        }

        PT_THREAD_END(this);
    }
};

int main()
{
    stdio_init_all();

    // Initialize hardware
    gpio_init(LED1_PIN);
    gpio_init(LED2_PIN);
    gpio_init(LED3_PIN);
    gpio_set_dir(LED1_PIN, GPIO_OUT);
    gpio_set_dir(LED2_PIN, GPIO_OUT);
    gpio_set_dir(LED3_PIN, GPIO_OUT);

    // Initialize sequence with default values
    for (int i = 0; i < 16; i++)
    {
        sequence_voltages[i] = (float)i / 12.0f; // Chromatic scale
    }

    printf("Eurorack Module Starting...\n");
    printf("Full Protothreads Framework Demo\n");
    printf("Features: Encoder, Buttons, CV I/O, Gate I/O, Sequencer, Events\n\n");

    // Create scheduler and threads
    PTScheduler scheduler;

    // Create thread instances
    UIThread ui_thread;
    CVInputThread cv_thread;
    SequencerThread seq_thread;
    GateInputThread gate_thread;
    MaintenanceThread maint_thread;
    ScreenThread screen_thread;

    // Set event queue for all hardware
    PTEventQueue *event_queue = scheduler.getEventQueue();
    encoder1.setEventQueue(event_queue);
    button1.setEventQueue(event_queue);
    button2.setEventQueue(event_queue);
    cv_in1.setEventQueue(event_queue);
    cv_in2.setEventQueue(event_queue);
    gate_in.setEventQueue(event_queue);

    // Add threads to scheduler
    scheduler.addThread(&ui_thread);
    scheduler.addThread(&cv_thread);
    scheduler.addThread(&seq_thread);
    scheduler.addThread(&gate_thread);
    scheduler.addThread(&maint_thread);
    scheduler.addThread(&screen_thread);

    printf("Starting scheduler with %zu threads...\n", scheduler.getThreadCount());

    // Run the scheduler
    scheduler.run();

    return 0;
}
