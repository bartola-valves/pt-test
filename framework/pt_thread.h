/**
 * @file pt_thread.h
 * @brief C++ wrapper for protothreads with Raspberry Pi Pico SDK integration
 *
 * This header provides a C++ interface to the protothreads library,
 * specifically designed for Eurorack module development on the
 * Raspberry Pi Pico. It includes support for:
 * - Thread management and scheduling
 * - Interrupt-driven events
 * - Hardware abstraction for encoders, buttons, LEDs
 * - CV and gate I/O handling
 * - Screen management
 * - Timing and synchronization
 */

#ifndef __PT_THREAD_H__
#define __PT_THREAD_H__

extern "C"
{
#include "protothreads/pt.h"
#include "protothreads/pt-sem.h"
}

#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"

#include <functional>
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>

// Forward declarations
class PTThread;
class PTScheduler;

/**
 * @brief Event types for interrupt-driven programming
 */
enum class PTEventType
{
    NONE = 0,
    ENCODER_TURN,
    BUTTON_PRESS,
    BUTTON_RELEASE,
    GATE_RISING,
    GATE_FALLING,
    TIMER_TICK,
    ADC_READY,
    SCREEN_REFRESH,
    SEQUENCE_STEP,
    CV_CHANGE,
    USER_EVENT
};

/**
 * @brief Event structure for interrupt handling
 */
struct PTEvent
{
    PTEventType type;
    uint32_t data;      // Event-specific data
    uint32_t timestamp; // Time of event occurrence
    bool processed;     // Event processing flag

    PTEvent() : type(PTEventType::NONE), data(0), timestamp(0), processed(false) {}
    PTEvent(PTEventType t, uint32_t d = 0) : type(t), data(d), timestamp(time_us_32()), processed(false) {}
};

/**
 * @brief Event queue for managing interrupt-driven events
 */
class PTEventQueue
{
private:
    static const size_t MAX_EVENTS = 32;
    std::array<PTEvent, MAX_EVENTS> events;
    volatile size_t head;
    volatile size_t tail;
    volatile size_t count;

public:
    PTEventQueue() : head(0), tail(0), count(0) {}

    bool push(const PTEvent &event)
    {
        if (count >= MAX_EVENTS)
            return false;

        uint32_t irq_state = save_and_disable_interrupts();
        events[head] = event;
        head = (head + 1) % MAX_EVENTS;
        count++;
        restore_interrupts(irq_state);
        return true;
    }

    bool pop(PTEvent &event)
    {
        if (count == 0)
            return false;

        uint32_t irq_state = save_and_disable_interrupts();
        event = events[tail];
        tail = (tail + 1) % MAX_EVENTS;
        count--;
        restore_interrupts(irq_state);
        return true;
    }

    bool isEmpty() const { return count == 0; }
    size_t size() const { return count; }
    void clear()
    {
        uint32_t irq_state = save_and_disable_interrupts();
        head = tail = count = 0;
        restore_interrupts(irq_state);
    }
};

/**
 * @brief Base class for protothreads with C++ integration
 */
class PTThread
{
private:
    struct pt thread_pt;
    bool active;
    const char *name;
    uint32_t last_run_time;
    uint32_t run_count;

protected:
    PTEventQueue *event_queue;

public:
    PTThread(const char *thread_name = "PTThread")
        : active(true), name(thread_name), last_run_time(0), run_count(0), event_queue(nullptr)
    {
        PT_INIT(&thread_pt);
    }

    virtual ~PTThread() = default;

    /**
     * @brief Main thread function - must be implemented by derived classes
     * @return Thread status (PT_WAITING, PT_YIELDED, PT_EXITED, PT_ENDED)
     */
    virtual int run() = 0;

    /**
     * @brief Execute the thread - internal scheduler interface
     */
    int execute()
    {
        if (!active)
            return PT_EXITED;

        last_run_time = time_us_32();
        int result = run();
        run_count++;

        if (result == PT_ENDED || result == PT_EXITED)
        {
            active = false;
        }

        return result;
    }

    /**
     * @brief Initialize/restart the thread
     */
    void init()
    {
        PT_INIT(&thread_pt);
        active = true;
        run_count = 0;
    }

    /**
     * @brief Stop the thread
     */
    void stop() { active = false; }

    /**
     * @brief Check if thread is active
     */
    bool isActive() const { return active; }

    /**
     * @brief Get thread name
     */
    const char *getName() const { return name; }

    /**
     * @brief Get run statistics
     */
    uint32_t getRunCount() const { return run_count; }
    uint32_t getLastRunTime() const { return last_run_time; }

    /**
     * @brief Set event queue for interrupt handling
     */
    void setEventQueue(PTEventQueue *queue) { event_queue = queue; }

    // Protothread control structure access
    struct pt *getPT() { return &thread_pt; }
};

/**
 * @brief Scheduler for managing multiple protothreads
 */
class PTScheduler
{
private:
    static const size_t MAX_THREADS = 16;
    std::array<PTThread *, MAX_THREADS> threads;
    size_t thread_count;
    PTEventQueue global_event_queue;
    uint32_t scheduler_ticks;
    bool running;

public:
    PTScheduler() : thread_count(0), scheduler_ticks(0), running(false) {}

    /**
     * @brief Add a thread to the scheduler
     */
    bool addThread(PTThread *thread)
    {
        if (thread_count >= MAX_THREADS || thread == nullptr)
        {
            return false;
        }

        threads[thread_count] = thread;
        thread->setEventQueue(&global_event_queue);
        thread_count++;
        return true;
    }

    /**
     * @brief Remove a thread from the scheduler
     */
    bool removeThread(PTThread *thread)
    {
        for (size_t i = 0; i < thread_count; i++)
        {
            if (threads[i] == thread)
            {
                // Shift remaining threads
                for (size_t j = i; j < thread_count - 1; j++)
                {
                    threads[j] = threads[j + 1];
                }
                thread_count--;
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Run one scheduler cycle
     */
    void runOnce()
    {
        scheduler_ticks++;

        for (size_t i = 0; i < thread_count; i++)
        {
            PTThread *thread = threads[i];
            if (thread && thread->isActive())
            {
                int result = thread->execute();

                // Remove ended threads
                if (result == PT_ENDED || result == PT_EXITED)
                {
                    removeThread(thread);
                    i--; // Adjust index after removal
                }
            }
        }
    }

    /**
     * @brief Start the scheduler (runs indefinitely)
     */
    void run()
    {
        running = true;
        while (running && thread_count > 0)
        {
            runOnce();
            tight_loop_contents(); // Pico SDK yield
        }
    }

    /**
     * @brief Stop the scheduler
     */
    void stop() { running = false; }

    /**
     * @brief Get scheduler statistics
     */
    size_t getThreadCount() const { return thread_count; }
    uint32_t getSchedulerTicks() const { return scheduler_ticks; }

    /**
     * @brief Get global event queue
     */
    PTEventQueue *getEventQueue() { return &global_event_queue; }

    /**
     * @brief Post an event to the global queue
     */
    bool postEvent(PTEventType type, uint32_t data = 0)
    {
        return global_event_queue.push(PTEvent(type, data));
    }
};

// Helper macros for protothread implementation in derived classes
#define PT_THREAD_BEGIN(pt_ptr) PT_BEGIN((pt_ptr)->getPT())
#define PT_THREAD_END(pt_ptr) PT_END((pt_ptr)->getPT())
#define PT_THREAD_WAIT_UNTIL(pt_ptr, condition) PT_WAIT_UNTIL((pt_ptr)->getPT(), condition)
#define PT_THREAD_WAIT_WHILE(pt_ptr, condition) PT_WAIT_WHILE((pt_ptr)->getPT(), condition)
#define PT_THREAD_YIELD(pt_ptr) PT_YIELD((pt_ptr)->getPT())
#define PT_THREAD_YIELD_UNTIL(pt_ptr, condition) PT_YIELD_UNTIL((pt_ptr)->getPT(), condition)
#define PT_THREAD_EXIT(pt_ptr) PT_EXIT((pt_ptr)->getPT())
#define PT_THREAD_RESTART(pt_ptr) PT_RESTART((pt_ptr)->getPT())

// Event handling helpers
#define PT_WAIT_EVENT(pt_ptr, event_var) \
    PT_THREAD_WAIT_UNTIL(pt_ptr, (pt_ptr)->event_queue && (pt_ptr)->event_queue->pop(event_var))

#define PT_WAIT_EVENT_TYPE(pt_ptr, event_var, event_type) \
    do                                                    \
    {                                                     \
        PT_WAIT_EVENT(pt_ptr, event_var);                 \
    } while ((event_var).type != event_type)

#endif // __PT_THREAD_H__
