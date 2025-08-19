/**
 * @file simple_threads.h
 * @brief Simplified thread system for Raspberry Pi Pico
 * @author Eurorack Framework
 */

#ifndef __SIMPLE_THREADS_H__
#define __SIMPLE_THREADS_H__

#include "pico/stdlib.h"
#include "hardware/timer.h"

/**
 * @brief Simple cooperative thread base class
 */
class SimpleThread
{
private:
    absolute_time_t last_time;
    uint32_t interval_ms;
    bool enabled;
    const char *name;

public:
    /**
     * @brief Constructor
     */
    SimpleThread(const char *thread_name = "SimpleThread")
    {
        last_time = get_absolute_time();
        interval_ms = 0;
        enabled = true;
        name = thread_name;
    }

    /**
     * @brief Virtual destructor
     */
    virtual ~SimpleThread() = default;

    /**
     * @brief Pure virtual function that must be implemented by derived classes
     */
    virtual void execute() = 0;

    /**
     * @brief Set execution interval
     * @param ms Interval in milliseconds
     */
    void setInterval(uint32_t ms)
    {
        interval_ms = ms;
    }

    /**
     * @brief Check if the thread should run
     * @return true if thread should execute
     */
    bool shouldRun()
    {
        if (!enabled)
            return false;

        if (interval_ms == 0)
            return true; // Run every time

        absolute_time_t current_time = get_absolute_time();
        if (absolute_time_diff_us(last_time, current_time) >= (interval_ms * 1000))
        {
            last_time = current_time;
            return true;
        }
        return false;
    }

    /**
     * @brief Enable/disable the thread
     * @param enable true to enable, false to disable
     */
    void setEnabled(bool enable)
    {
        enabled = enable;
    }

    /**
     * @brief Check if thread is enabled
     * @return true if enabled
     */
    bool isEnabled() const
    {
        return enabled;
    }

    /**
     * @brief Get thread name
     * @return Thread name
     */
    const char *getName() const
    {
        return name;
    }

    /**
     * @brief Run the thread (calls execute if shouldRun returns true)
     */
    void run()
    {
        if (shouldRun())
        {
            execute();
        }
    }
};

/**
 * @brief Simple scheduler for multiple threads
 */
class SimpleScheduler
{
private:
    static const int MAX_THREADS = 16;
    SimpleThread *threads[MAX_THREADS];
    int thread_count;

public:
    /**
     * @brief Constructor
     */
    SimpleScheduler() : thread_count(0)
    {
        for (int i = 0; i < MAX_THREADS; i++)
        {
            threads[i] = nullptr;
        }
    }

    /**
     * @brief Add a thread to the scheduler
     * @param thread Pointer to SimpleThread instance
     * @return true if successful, false if scheduler is full
     */
    bool addThread(SimpleThread *thread)
    {
        if (thread_count < MAX_THREADS)
        {
            threads[thread_count++] = thread;
            return true;
        }
        return false;
    }

    /**
     * @brief Run all threads
     */
    void run()
    {
        for (int i = 0; i < thread_count; i++)
        {
            if (threads[i] != nullptr)
            {
                threads[i]->run();
            }
        }
    }

    /**
     * @brief Remove all threads from scheduler
     */
    void clear()
    {
        thread_count = 0;
        for (int i = 0; i < MAX_THREADS; i++)
        {
            threads[i] = nullptr;
        }
    }

    /**
     * @brief Get the current number of threads
     * @return Current thread count
     */
    int getThreadCount() const
    {
        return thread_count;
    }
};

#endif /* __SIMPLE_THREADS_H__ */
