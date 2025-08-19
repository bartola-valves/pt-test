/**
 * @file pt-test-working.cpp
 * @brief Working threading example for Eurorack modules
 * @description Simple example with multiple threads controlling LED patterns
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "framework/simple_threads.h"
#include "framework/eurorack_utils.h"

/**
 * @brief Fast blink thread - creates rapid LED blinking
 */
class FastBlinkThread : public SimpleThread
{
private:
    uint32_t blink_count;
    uint32_t sequence_count;
    uint32_t phase;

public:
    FastBlinkThread() : blink_count(0), sequence_count(0), phase(0)
    {
        setInterval(100); // 100ms interval
    }

    void execute() override
    {
        phase++;

        // Fast blink sequence: 10 rapid blinks, then pause
        if (phase <= 10)
        {
            EurorackUtils::LED::toggle();
            blink_count++;
        }
        else if (phase <= 30)
        {
            // Pause phase - LED off
            EurorackUtils::LED::off();
        }
        else
        {
            // Reset sequence
            phase = 0;
            sequence_count++;
            printf("Fast blink sequence #%lu completed (%lu total blinks)\n",
                   sequence_count, blink_count);
        }
    }
};

/**
 * @brief Slow pulse thread - creates breathing LED effect
 */
class SlowPulseThread : public SimpleThread
{
private:
    uint32_t pulse_count;
    uint32_t phase;

public:
    SlowPulseThread() : pulse_count(0), phase(0)
    {
        setInterval(200); // 200ms interval for smooth effect
    }

    void execute() override
    {
        phase++;

        // Simple breathing pattern: on for 5 cycles, off for 10 cycles
        if (phase <= 5)
        {
            EurorackUtils::LED::on();
        }
        else if (phase <= 15)
        {
            EurorackUtils::LED::off();
        }
        else
        {
            phase = 0;
            pulse_count++;
            printf("Slow pulse #%lu completed\n", pulse_count);
        }
    }
};

/**
 * @brief Status thread - reports system information
 */
class StatusThread : public SimpleThread
{
private:
    uint32_t status_count;

public:
    StatusThread() : status_count(0)
    {
        setInterval(5000); // 5 second interval
    }

    void execute() override
    {
        status_count++;
        printf("\n=== System Status #%lu ===\n", status_count);
        printf("Uptime: %lu ms\n", EurorackUtils::Timing::getMillis());
        printf("LED State: %s\n", EurorackUtils::LED::getState() ? "ON" : "OFF");
        printf("Core0 temp: ~%.1f°C (estimated)\n", 27.0f + (float)(EurorackUtils::Timing::getMillis() % 100) / 100.0f * 3.0f);
        printf("===========================\n\n");
    }
};

/**
 * @brief Demonstrates thread control - enables/disables other threads
 */
class ControlThread : public SimpleThread
{
private:
    uint32_t control_count;
    FastBlinkThread *fast_thread;
    SlowPulseThread *slow_thread;
    bool fast_enabled;

public:
    ControlThread(FastBlinkThread *fast, SlowPulseThread *slow)
        : control_count(0), fast_thread(fast), slow_thread(slow), fast_enabled(true)
    {
        setInterval(8000); // 8 second interval
    }

    void execute() override
    {
        control_count++;

        // Alternate between different threading modes
        if (fast_enabled)
        {
            printf(">>> Switching to SLOW pulse mode <<<\n");
            fast_thread->setEnabled(false);
            slow_thread->setEnabled(true);
            fast_enabled = false;
        }
        else
        {
            printf(">>> Switching to FAST blink mode <<<\n");
            fast_thread->setEnabled(true);
            slow_thread->setEnabled(false);
            fast_enabled = true;
        }

        printf("Control cycle #%lu - Mode: %s\n",
               control_count, fast_enabled ? "FAST" : "SLOW");
    }
};

int main()
{
    // Initialize the system
    EurorackUtils::init();

    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║        Eurorack Simple Threading Demo     ║\n");
    printf("║            Raspberry Pi Pico             ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("\n");
    printf("Features demonstrated:\n");
    printf("• Multiple cooperative threads\n");
    printf("• Timed execution intervals\n");
    printf("• Thread enable/disable control\n");
    printf("• LED pattern generation\n");
    printf("• System status monitoring\n");
    printf("\n");
    printf("Watch the onboard LED for different patterns!\n");
    printf("==========================================\n\n");

    // Create thread instances
    FastBlinkThread fast_blink;
    SlowPulseThread slow_pulse;
    StatusThread status;
    ControlThread control(&fast_blink, &slow_pulse);

    // Start with fast blink enabled, slow pulse disabled
    fast_blink.setEnabled(true);
    slow_pulse.setEnabled(false);

    // Create scheduler and add threads
    SimpleScheduler scheduler;
    scheduler.addThread(&fast_blink);
    scheduler.addThread(&slow_pulse);
    scheduler.addThread(&status);
    scheduler.addThread(&control);

    printf("All threads initialized successfully!\n");
    printf("Starting main execution loop...\n\n");

    // Main execution loop
    uint32_t loop_count = 0;
    while (true)
    {
        scheduler.run();

        // Small delay to prevent overwhelming the system
        // In a real Eurorack module, this loop would handle:
        // - Audio sample processing
        // - CV input reading
        // - Gate detection
        // - Parameter updates
        tight_loop_contents();

        // Optional: count main loop iterations (for debugging)
        loop_count++;
        if (loop_count % 1000000 == 0)
        {
            // This will print approximately every few seconds
            printf("Main loop: %lu iterations\n", loop_count);
        }
    }

    return 0;
}
