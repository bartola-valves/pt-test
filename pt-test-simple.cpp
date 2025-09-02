/**
 * @file pt-test-simple.cpp
 * @brief Simple protothread example with LED patterns and inter-thread communication
 *
 * This example demonstrates:
 * - Two different LED blinking patterns using protothreads
 * - Inter-thread communication using events
 * - Pattern switching based on signals between threads
 * - State machine approach for reliable operation
 */

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "framework/pt_thread.h"
#include <cstdio>

// LED pin definition (using onboard LED)
#define LED_PIN PICO_DEFAULT_LED_PIN

// Custom event types for pattern switching
#define PATTERN_SWITCH_EVENT PTEventType::USER_EVENT

/**
 * @brief Fast blink pattern thread using state machine approach
 * Creates rapid LED blinking with short pauses
 */
class FastBlinkThread : public PTThread
{
private:
    enum State
    {
        INIT,
        BLINK_ON,
        BLINK_OFF,
        SEQUENCE_PAUSE,
        WAIT_FOR_RESUME
    };

    State state;
    uint32_t timer;
    uint32_t blink_count;
    uint32_t sequence_count;
    int current_blink;

public:
    FastBlinkThread() : PTThread("FastBlink"), state(INIT), timer(0),
                        blink_count(0), sequence_count(0), current_blink(0) {}

    int run() override
    {
        uint32_t now = time_us_32();

        switch (state)
        {
        case INIT:
            printf("FastBlinkThread: Starting fast blink pattern\n");
            state = BLINK_ON;
            current_blink = 0;
            timer = now;
            break;

        case BLINK_ON:
            if ((now - timer) >= 100000)
            { // 100ms
                gpio_put(LED_PIN, 1);
                state = BLINK_OFF;
                timer = now;
            }
            break;

        case BLINK_OFF:
            if ((now - timer) >= 100000)
            { // 100ms
                gpio_put(LED_PIN, 0);
                blink_count++;
                current_blink++;

                if (current_blink >= 6)
                { // 6 blinks per sequence
                    state = SEQUENCE_PAUSE;
                    timer = now;
                }
                else
                {
                    state = BLINK_ON;
                    timer = now;
                }
            }
            break;

        case SEQUENCE_PAUSE:
            if ((now - timer) >= 1000000)
            { // 1 second pause
                current_blink = 0;
                sequence_count++;

                if (sequence_count >= 3)
                {
                    printf("FastBlinkThread: Switching to slow pattern (sent %lu blinks)\n", blink_count);
                    if (event_queue)
                    {
                        PTEvent switch_event(PATTERN_SWITCH_EVENT, 1); // data=1 means switch to slow
                        event_queue->push(switch_event);
                    }
                    sequence_count = 0;
                    state = WAIT_FOR_RESUME;
                }
                else
                {
                    state = BLINK_ON;
                    timer = now;
                }
            }
            break;

        case WAIT_FOR_RESUME:
            if (checkForResumeEvent())
            {
                printf("FastBlinkThread: Resuming fast pattern\n");
                state = BLINK_ON;
                timer = now;
            }
            break;
        }

        return PT_WAITING; // Always continue
    }

private:
    bool checkForResumeEvent()
    {
        if (!event_queue)
            return false;

        PTEvent event;
        while (event_queue->pop(event))
        {
            if (event.type == PATTERN_SWITCH_EVENT && event.data == 0) // data=0 means switch to fast
            {
                return true;
            }
        }
        return false;
    }
};

/**
 * @brief Slow pulse pattern thread using state machine approach
 * Creates gentle breathing-like LED pattern
 */
class SlowPulseThread : public PTThread
{
private:
    enum State
    {
        WAITING,
        PULSE_ON,
        PULSE_OFF,
        SEQUENCE_PAUSE
    };

    State state;
    uint32_t timer;
    uint32_t pulse_count;
    uint32_t sequence_count;
    int current_pulse;

public:
    SlowPulseThread() : PTThread("SlowPulse"), state(WAITING), timer(0),
                        pulse_count(0), sequence_count(0), current_pulse(0) {}

    int run() override
    {
        uint32_t now = time_us_32();

        switch (state)
        {
        case WAITING:
            if (checkForActivationEvent())
            {
                printf("SlowPulseThread: Activated - starting slow pulse pattern\n");
                state = PULSE_ON;
                current_pulse = 0;
                timer = now;
            }
            break;

        case PULSE_ON:
            if ((now - timer) >= 800000)
            { // 800ms
                gpio_put(LED_PIN, 1);
                state = PULSE_OFF;
                timer = now;
            }
            break;

        case PULSE_OFF:
            if ((now - timer) >= 200000)
            { // 200ms
                gpio_put(LED_PIN, 0);
                pulse_count++;
                current_pulse++;

                if (current_pulse >= 4)
                { // 4 pulses per sequence
                    state = SEQUENCE_PAUSE;
                    timer = now;
                }
                else
                {
                    state = PULSE_ON;
                    timer = now;
                }
            }
            break;

        case SEQUENCE_PAUSE:
            if ((now - timer) >= 1500000)
            { // 1.5 second pause
                current_pulse = 0;
                sequence_count++;

                if (sequence_count >= 2)
                {
                    printf("SlowPulseThread: Switching back to fast pattern (sent %lu pulses)\n", pulse_count);
                    if (event_queue)
                    {
                        PTEvent switch_event(PATTERN_SWITCH_EVENT, 0); // data=0 means switch to fast
                        event_queue->push(switch_event);
                    }
                    sequence_count = 0;
                    state = WAITING;
                }
                else
                {
                    state = PULSE_ON;
                    timer = now;
                }
            }
            break;
        }

        return PT_WAITING; // Always continue
    }

private:
    bool checkForActivationEvent()
    {
        if (!event_queue)
            return false;

        PTEvent event;
        while (event_queue->pop(event))
        {
            if (event.type == PATTERN_SWITCH_EVENT && event.data == 1) // data=1 means switch to slow
            {
                return true;
            }
        }
        return false;
    }
};

/**
 * @brief Status reporting thread using simple timer
 * Periodically reports system status
 */
class StatusThread : public PTThread
{
private:
    uint32_t last_report_time;
    uint32_t report_count;

public:
    StatusThread() : PTThread("Status"), last_report_time(0), report_count(0) {}

    int run() override
    {
        uint32_t now = time_us_32();

        if (last_report_time == 0)
        {
            last_report_time = now;
        }

        if ((now - last_report_time) >= 10000000)
        { // 10 seconds
            report_count++;
            printf("\n=== Status Report #%lu ===\n", report_count);
            printf("Uptime: %.1f seconds\n", (float)now / 1000000.0f);
            printf("LED State: %s\n", gpio_get(LED_PIN) ? "ON" : "OFF");
            printf("Event Queue Size: %zu\n", event_queue ? event_queue->size() : 0);
            printf("==========================\n\n");
            last_report_time = now;
        }

        return PT_WAITING; // Always continue
    }
};
int main()
{
    // Initialize system
    stdio_init_all();

    // Initialize LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    // Wait a bit for serial connection
    sleep_ms(2000);

    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║         Protothread LED Demo             ║\n");
    printf("║      Inter-thread Communication          ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("\n");
    printf("This demo shows:\n");
    printf("• Two different LED blinking patterns\n");
    printf("• Protothread cooperative multitasking\n");
    printf("• Event-driven pattern switching\n");
    printf("• Inter-thread communication\n");
    printf("• Real protothread syntax (PT_BEGIN, PT_WAIT_UNTIL, PT_END)\n");
    printf("\nWatch the onboard LED for pattern changes!\n");
    printf("==========================================\n\n");

    // Create thread instances
    FastBlinkThread fast_thread;
    SlowPulseThread slow_thread;
    StatusThread status_thread;

    // Create scheduler and add threads
    PTScheduler scheduler;
    scheduler.addThread(&fast_thread);
    scheduler.addThread(&slow_thread);
    scheduler.addThread(&status_thread);

    printf("All threads initialized and added to scheduler.\n");
    printf("Starting main execution loop...\n\n");

    // Main execution loop
    uint32_t loop_count = 0;
    while (true)
    {
        scheduler.runOnce();

        // Small delay to prevent overwhelming the system
        tight_loop_contents();

        // Optional: count main loop iterations (for debugging)
        loop_count++;
        if (loop_count % 1000000 == 0)
        {
            // This will print occasionally to show the main loop is running
            printf("Main loop: %lu iterations\n", loop_count);
        }
    }

    return 0;
}
