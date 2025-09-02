# Simple Threads Library Tutorial
*A Complete Guide to Cooperative Threading for Eurorack Development*

## Table of Contents
1. [Introduction](#introduction)
2. [Getting Started](#getting-started)
3. [Basic Concepts](#basic-concepts)
4. [Step-by-Step Tutorial](#step-by-step-tutorial)
5. [Class Reference](#class-reference)
6. [Best Practices](#best-practices)
7. [Common Patterns](#common-patterns)
8. [Troubleshooting](#troubleshooting)
9. [Performance Considerations](#performance-considerations)
10. [Examples and Use Cases](#examples-and-use-cases)

---

## Introduction

The Simple Threads library (`framework/simple_threads.h`) provides a lightweight, cooperative threading system designed specifically for embedded applications on the Raspberry Pi Pico. It's particularly well-suited for Eurorack synthesizer modules where you need to handle multiple concurrent tasks like audio processing, LED patterns, CV monitoring, and user interface updates.

### Why Use Simple Threads?

- **Lightweight**: Minimal memory overhead and CPU usage
- **Deterministic**: Predictable execution timing for audio applications
- **Simple**: Easy to understand and debug compared to preemptive threading
- **Safe**: No race conditions or deadlocks when used properly
- **Flexible**: Time-based or continuous execution modes

---

## Getting Started

### Prerequisites

- Raspberry Pi Pico SDK installed and configured
- Basic understanding of C++ and object-oriented programming
- VS Code with the project properly set up (see main README.md)

### Including the Library

```cpp
#include "framework/simple_threads.h"
#include "framework/eurorack_utils.h" // For LED and timing utilities
```

---

## Basic Concepts

### Cooperative Multitasking

Simple Threads uses **cooperative multitasking**, meaning:
- Threads voluntarily yield control back to the scheduler
- No interruptions or context switching
- Each thread runs to completion before the next thread executes
- Perfect for real-time audio applications

### Thread Lifecycle

1. **Creation**: Instantiate your thread class
2. **Configuration**: Set interval and initial state
3. **Registration**: Add to scheduler
4. **Execution**: Scheduler calls `run()` on each thread
5. **Cleanup**: Threads are automatically managed

---

## Step-by-Step Tutorial

### Step 1: Create Your First Thread

Every thread must inherit from `SimpleThread` and implement the `execute()` method:

```cpp
#include "framework/simple_threads.h"

class MyFirstThread : public SimpleThread
{
public:
    MyFirstThread() : SimpleThread("MyFirst")
    {
        setInterval(1000); // Execute every 1000ms (1 second)
    }

    void execute() override
    {
        printf("Hello from MyFirstThread!\n");
    }
};
```

**Key Points:**
- Constructor calls `SimpleThread("MyFirst")` with a thread name
- `setInterval(1000)` makes the thread execute every second
- `execute()` contains your thread's main logic

### Step 2: Set Up the Scheduler

The scheduler manages all your threads:

```cpp
int main()
{
    // Initialize the system
    EurorackUtils::init();
    
    // Create thread instance
    MyFirstThread my_thread;
    
    // Create scheduler and add thread
    SimpleScheduler scheduler;
    scheduler.addThread(&my_thread);
    
    // Main loop
    while (true)
    {
        scheduler.run(); // Execute all threads
    }
    
    return 0;
}
```

### Step 3: Add Multiple Threads

Let's create a more complex example with multiple threads:

```cpp
class LEDBlinkThread : public SimpleThread
{
private:
    bool led_state;
    
public:
    LEDBlinkThread() : led_state(false)
    {
        setInterval(500); // Blink every 500ms
    }
    
    void execute() override
    {
        led_state = !led_state;
        if (led_state) {
            EurorackUtils::LED::on();
        } else {
            EurorackUtils::LED::off();
        }
        printf("LED: %s\n", led_state ? "ON" : "OFF");
    }
};

class StatusThread : public SimpleThread
{
private:
    uint32_t count;
    
public:
    StatusThread() : count(0)
    {
        setInterval(3000); // Report every 3 seconds
    }
    
    void execute() override
    {
        count++;
        printf("Status update #%lu - Uptime: %lu ms\n", 
               count, EurorackUtils::Timing::getMillis());
    }
};
```

### Step 4: Advanced Thread Control

You can enable/disable threads dynamically:

```cpp
class ControlThread : public SimpleThread
{
private:
    LEDBlinkThread* led_thread;
    bool blink_enabled;
    
public:
    ControlThread(LEDBlinkThread* led) : led_thread(led), blink_enabled(true)
    {
        setInterval(5000); // Check every 5 seconds
    }
    
    void execute() override
    {
        blink_enabled = !blink_enabled;
        led_thread->setEnabled(blink_enabled);
        printf("LED blinking: %s\n", blink_enabled ? "ENABLED" : "DISABLED");
    }
};
```

### Step 5: Complete Example

```cpp
#include <stdio.h>
#include "pico/stdlib.h"
#include "framework/simple_threads.h"
#include "framework/eurorack_utils.h"

// [Include the thread classes from above]

int main()
{
    EurorackUtils::init();
    
    printf("Simple Threads Tutorial - Complete Example\n");
    
    // Create thread instances
    LEDBlinkThread led_thread;
    StatusThread status_thread;
    ControlThread control_thread(&led_thread);
    
    // Create scheduler
    SimpleScheduler scheduler;
    scheduler.addThread(&led_thread);
    scheduler.addThread(&status_thread);
    scheduler.addThread(&control_thread);
    
    printf("Started %d threads\n", scheduler.getThreadCount());
    
    // Main execution loop
    while (true)
    {
        scheduler.run();
        tight_loop_contents(); // Pico SDK optimization
    }
    
    return 0;
}
```

---

## Class Reference

### SimpleThread Class

#### Constructor
```cpp
SimpleThread(const char *thread_name = "SimpleThread")
```
- **Parameters**: Optional thread name for debugging
- **Usage**: Always call from derived class constructor

#### Core Methods

##### `virtual void execute() = 0`
**Purpose**: Pure virtual method containing your thread's main logic
**Must Override**: Yes
**Called When**: Thread's execution interval has elapsed

```cpp
void execute() override
{
    // Your thread logic here
}
```

##### `void setInterval(uint32_t ms)`
**Purpose**: Set execution interval in milliseconds
**Parameters**: `ms` - Interval in milliseconds (0 = run every loop)
**Default**: 0 (continuous execution)

```cpp
setInterval(1000); // Execute every 1 second
setInterval(0);    // Execute every main loop iteration
```

##### `bool shouldRun()`
**Purpose**: Check if thread should execute (internal scheduling)
**Returns**: `true` if thread should run now
**Usage**: Called automatically by scheduler

##### `void run()`
**Purpose**: Execute the thread if it should run
**Usage**: Called by scheduler, don't call directly

```cpp
// Scheduler does this automatically:
if (thread->shouldRun()) {
    thread->execute();
}
```

#### Control Methods

##### `void setEnabled(bool enable)`
**Purpose**: Enable or disable thread execution
**Parameters**: `enable` - `true` to enable, `false` to disable

```cpp
my_thread.setEnabled(false); // Disable thread
my_thread.setEnabled(true);  // Re-enable thread
```

##### `bool isEnabled() const`
**Purpose**: Check if thread is currently enabled
**Returns**: `true` if enabled, `false` if disabled

```cpp
if (my_thread.isEnabled()) {
    printf("Thread is running\n");
}
```

##### `const char* getName() const`
**Purpose**: Get the thread's name
**Returns**: Thread name string
**Usage**: Debugging and logging

```cpp
printf("Thread name: %s\n", my_thread.getName());
```

### SimpleScheduler Class

#### Constructor
```cpp
SimpleScheduler()
```
**Purpose**: Initialize empty scheduler
**Capacity**: Up to 16 threads (MAX_THREADS)

#### Methods

##### `bool addThread(SimpleThread *thread)`
**Purpose**: Add a thread to the scheduler
**Parameters**: `thread` - Pointer to SimpleThread instance
**Returns**: `true` if successful, `false` if scheduler is full

```cpp
SimpleScheduler scheduler;
MyThread my_thread;

if (scheduler.addThread(&my_thread)) {
    printf("Thread added successfully\n");
} else {
    printf("Scheduler is full!\n");
}
```

##### `void run()`
**Purpose**: Execute all enabled threads
**Usage**: Call in main loop

```cpp
while (true) {
    scheduler.run(); // Execute all threads
}
```

##### `void clear()`
**Purpose**: Remove all threads from scheduler
**Usage**: Reset scheduler state

```cpp
scheduler.clear(); // Remove all threads
```

##### `int getThreadCount() const`
**Purpose**: Get current number of registered threads
**Returns**: Number of threads (0-16)

```cpp
printf("Active threads: %d\n", scheduler.getThreadCount());
```

---

## Best Practices

### 1. Thread Naming
Always provide meaningful names for debugging:

```cpp
class AudioProcessThread : public SimpleThread
{
public:
    AudioProcessThread() : SimpleThread("AudioProcessor")
    {
        // Thread implementation
    }
};
```

### 2. Appropriate Intervals
Choose intervals based on your requirements:

```cpp
// Fast: Audio processing, critical timing
audio_thread.setInterval(1);     // Every 1ms

// Medium: LED updates, UI
led_thread.setInterval(50);      // Every 50ms

// Slow: Status reports, housekeeping
status_thread.setInterval(5000); // Every 5 seconds
```

### 3. Keep execute() Fast
Each `execute()` call should complete quickly:

```cpp
// ✅ Good - Fast execution
void execute() override
{
    if (button_pressed()) {
        toggle_led();
    }
}

// ❌ Bad - Blocking delays
void execute() override
{
    sleep_ms(100); // Blocks entire system!
}
```

### 4. State Management
Use member variables to maintain state between executions:

```cpp
class SequenceThread : public SimpleThread
{
private:
    int step;           // Current sequence step
    bool direction;     // Forward/backward
    uint32_t pattern;   // Bit pattern
    
public:
    void execute() override
    {
        // Use state variables to maintain sequence position
        if (direction) {
            step++;
        } else {
            step--;
        }
        
        // Update pattern based on step
        pattern = generate_pattern(step);
    }
};
```

### 5. Thread Communication
Use shared variables or callbacks for thread communication:

```cpp
class SensorThread : public SimpleThread
{
private:
    float* shared_value; // Shared with other threads
    
public:
    SensorThread(float* value) : shared_value(value) {}
    
    void execute() override
    {
        *shared_value = read_sensor(); // Update shared value
    }
};

class DisplayThread : public SimpleThread
{
private:
    float* sensor_value; // Same shared variable
    
public:
    DisplayThread(float* value) : sensor_value(value) {}
    
    void execute() override
    {
        printf("Sensor: %.2f\n", *sensor_value);
    }
};
```

---

## Common Patterns

### 1. State Machine Pattern
Perfect for complex behaviors:

```cpp
class SequencerThread : public SimpleThread
{
private:
    enum State { STOPPED, PLAYING, RECORDING };
    State current_state;
    int step;
    
public:
    void execute() override
    {
        switch (current_state)
        {
            case STOPPED:
                if (play_button_pressed()) {
                    current_state = PLAYING;
                    step = 0;
                }
                break;
                
            case PLAYING:
                play_step(step);
                step = (step + 1) % 16; // 16-step sequence
                if (stop_button_pressed()) {
                    current_state = STOPPED;
                }
                break;
                
            case RECORDING:
                record_step(step);
                // Recording logic...
                break;
        }
    }
};
```

### 2. Phase-Based Animation
Great for smooth LED effects:

```cpp
class PulseThread : public SimpleThread
{
private:
    float phase;
    float frequency;
    
public:
    PulseThread() : phase(0.0f), frequency(1.0f)
    {
        setInterval(20); // 50Hz update rate
    }
    
    void execute() override
    {
        phase += frequency * 0.02f; // 20ms interval
        if (phase >= 1.0f) phase -= 1.0f;
        
        // Sine wave breathing effect
        float brightness = (sin(phase * 2 * M_PI) + 1.0f) / 2.0f;
        set_led_brightness(brightness);
    }
};
```

### 3. Event-Driven Threads
Respond to external events:

```cpp
class TriggerThread : public SimpleThread
{
private:
    bool last_trigger_state;
    
public:
    TriggerThread() : last_trigger_state(false)
    {
        setInterval(1); // Fast polling for triggers
    }
    
    void execute() override
    {
        bool current_trigger = read_trigger_input();
        
        // Detect rising edge
        if (current_trigger && !last_trigger_state) {
            handle_trigger_event();
        }
        
        last_trigger_state = current_trigger;
    }
    
private:
    void handle_trigger_event()
    {
        printf("Trigger detected!\n");
        // Generate gate, start envelope, etc.
    }
};
```

---

## Troubleshooting

### Common Issues

#### Thread Not Executing
**Problem**: Your thread's `execute()` method never runs
**Solutions**:
1. Check if thread is enabled: `thread.isEnabled()`
2. Verify thread was added to scheduler: `scheduler.addThread(&thread)`
3. Make sure scheduler.run() is called in main loop
4. Check if interval is too large

```cpp
// Debug thread execution
printf("Thread enabled: %s\n", my_thread.isEnabled() ? "YES" : "NO");
printf("Thread count: %d\n", scheduler.getThreadCount());
```

#### System Appears Frozen
**Problem**: Main loop stops responding
**Causes**:
1. Blocking calls in `execute()` methods
2. Infinite loops in thread code
3. Missing `tight_loop_contents()` in main loop

```cpp
// ❌ Bad - blocks system
void execute() override
{
    while (condition) { } // Infinite loop!
    sleep_ms(100);        // Blocking delay!
}

// ✅ Good - non-blocking
void execute() override
{
    if (condition) {
        do_something_quick();
    }
}
```

#### Timing Issues
**Problem**: Threads don't execute at expected intervals
**Solutions**:
1. Use appropriate intervals for your use case
2. Keep `execute()` methods fast
3. Monitor main loop frequency

```cpp
// Debug timing
class DebugThread : public SimpleThread
{
public:
    void execute() override
    {
        static absolute_time_t last_time = get_absolute_time();
        absolute_time_t current = get_absolute_time();
        int64_t diff = absolute_time_diff_us(last_time, current);
        printf("Thread interval: %lld us\n", diff);
        last_time = current;
    }
};
```

---

## Performance Considerations

### Memory Usage

Each SimpleThread instance uses minimal memory:
- Base class: ~20 bytes
- Your derived class: + your member variables
- Scheduler: ~70 bytes + (16 × 8 bytes for thread pointers)

**Total for typical application**: < 200 bytes

### CPU Usage

SimpleThread overhead is minimal:
- Each `shouldRun()` call: ~10 CPU cycles
- Each `run()` call: ~5 CPU cycles + your `execute()` time
- Scheduler overhead: ~50 CPU cycles per `run()` call

### Optimization Tips

1. **Use appropriate intervals**:
```cpp
// Audio processing - needs fast response
audio_thread.setInterval(1);    // 1ms

// LED updates - medium priority  
led_thread.setInterval(50);     // 50ms

// Housekeeping - low priority
status_thread.setInterval(5000); // 5 seconds
```

2. **Minimize work in execute()**:
```cpp
// ✅ Good - do minimal work
void execute() override
{
    if (needs_update) {
        update_quickly();
        needs_update = false;
    }
}
```

3. **Use static variables for heavy calculations**:
```cpp
void execute() override
{
    static float lookup_table[256] = {0}; // Calculated once
    static bool initialized = false;
    
    if (!initialized) {
        initialize_lookup_table(lookup_table);
        initialized = true;
    }
    
    // Use pre-calculated values
    float result = lookup_table[input_value];
}
```

---

## Examples and Use Cases

### Example 1: Basic LED Blinker

Perfect first project to understand the basics:

```cpp
#include "framework/simple_threads.h"

class BasicBlinker : public SimpleThread
{
private:
    bool led_on;
    
public:
    BasicBlinker() : led_on(false)
    {
        setInterval(1000); // 1 second intervals
    }
    
    void execute() override
    {
        led_on = !led_on;
        EurorackUtils::LED::set(led_on);
        printf("LED: %s\n", led_on ? "ON" : "OFF");
    }
};

int main()
{
    EurorackUtils::init();
    
    BasicBlinker blinker;
    SimpleScheduler scheduler;
    scheduler.addThread(&blinker);
    
    while (true) {
        scheduler.run();
    }
}
```

### Example 2: Multi-Pattern LED Controller

More advanced example with pattern switching:

```cpp
class PatternController : public SimpleThread
{
private:
    enum Pattern { BLINK, PULSE, STROBE };
    Pattern current_pattern;
    uint32_t phase;
    uint32_t pattern_duration;
    
public:
    PatternController() : current_pattern(BLINK), phase(0), pattern_duration(0)
    {
        setInterval(100); // 10Hz update rate
    }
    
    void execute() override
    {
        phase++;
        pattern_duration++;
        
        switch (current_pattern)
        {
            case BLINK:
                execute_blink_pattern();
                break;
            case PULSE:
                execute_pulse_pattern();
                break;
            case STROBE:
                execute_strobe_pattern();
                break;
        }
        
        // Switch patterns every 5 seconds (50 × 100ms)
        if (pattern_duration >= 50) {
            switch_to_next_pattern();
            pattern_duration = 0;
            phase = 0;
        }
    }
    
private:
    void execute_blink_pattern()
    {
        // Simple on/off every 5 cycles
        EurorackUtils::LED::set((phase % 10) < 5);
    }
    
    void execute_pulse_pattern()
    {
        // Sine wave breathing effect
        float brightness = (sin(phase * 0.1f) + 1.0f) / 2.0f;
        // Note: This would require PWM LED control
        EurorackUtils::LED::set(brightness > 0.5f);
    }
    
    void execute_strobe_pattern()
    {
        // Fast strobe: on for 1 cycle, off for 4
        EurorackUtils::LED::set((phase % 5) == 0);
    }
    
    void switch_to_next_pattern()
    {
        current_pattern = (Pattern)((current_pattern + 1) % 3);
        printf("Switched to pattern: %d\n", (int)current_pattern);
    }
};
```

### Example 3: Audio-Rate Thread

High-frequency thread for audio processing simulation:

```cpp
class AudioThread : public SimpleThread
{
private:
    float phase;
    float frequency;
    uint32_t sample_count;
    
public:
    AudioThread() : phase(0.0f), frequency(440.0f), sample_count(0)
    {
        // 48kHz sample rate = ~20.8μs per sample
        // Use 1ms for demonstration (48 samples per execute)
        setInterval(1); 
    }
    
    void execute() override
    {
        // Process 48 samples (simulate 48kHz at 1ms intervals)
        for (int i = 0; i < 48; i++) {
            float sample = sin(phase * 2.0f * M_PI);
            
            // Process audio sample here
            // (In real application: ADC input, DSP, DAC output)
            
            phase += frequency / 48000.0f; // 48kHz sample rate
            if (phase >= 1.0f) phase -= 1.0f;
            
            sample_count++;
        }
        
        // Debug output every second
        if (sample_count % 48000 == 0) {
            printf("Processed %lu samples\n", sample_count);
        }
    }
    
    void setFrequency(float freq) 
    {
        frequency = freq;
    }
};
```

### Example 4: CV Input Monitor

Thread for monitoring control voltage inputs:

```cpp
class CVMonitorThread : public SimpleThread
{
private:
    struct CVInput {
        int adc_channel;
        float last_value;
        float smoothed_value;
        const char* name;
    };
    
    CVInput cv_inputs[4] = {
        {0, 0.0f, 0.0f, "CV1"},
        {1, 0.0f, 0.0f, "CV2"},
        {2, 0.0f, 0.0f, "CV3"},
        {3, 0.0f, 0.0f, "CV4"}
    };
    
public:
    CVMonitorThread()
    {
        setInterval(5); // 200Hz - fast enough for CV
    }
    
    void execute() override
    {
        for (int i = 0; i < 4; i++) {
            // Read ADC (simulated)
            float raw_value = read_adc_channel(cv_inputs[i].adc_channel);
            
            // Simple low-pass filtering
            cv_inputs[i].smoothed_value = 
                cv_inputs[i].smoothed_value * 0.9f + raw_value * 0.1f;
            
            // Detect significant changes
            float change = fabs(cv_inputs[i].smoothed_value - cv_inputs[i].last_value);
            if (change > 0.01f) { // 1% change threshold
                printf("%s: %.3fV (change: %.3f)\n", 
                       cv_inputs[i].name, 
                       cv_inputs[i].smoothed_value,
                       change);
                cv_inputs[i].last_value = cv_inputs[i].smoothed_value;
            }
        }
    }
    
private:
    float read_adc_channel(int channel)
    {
        // Simulate ADC reading (0-3.3V range)
        // In real code: use Pico SDK ADC functions
        return (float)(rand() % 3300) / 1000.0f;
    }
};
```

### Example 5: Complete Eurorack Module

A more complete example combining multiple threads:

```cpp
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "framework/simple_threads.h"
#include "framework/eurorack_utils.h"

// Global shared state
struct ModuleState {
    float cv_inputs[4];
    bool gate_inputs[2];
    float frequency;
    bool led_state;
    uint32_t step_count;
} g_state = {0};

class CVThread : public SimpleThread
{
public:
    CVThread() : SimpleThread("CV_Monitor") 
    { 
        setInterval(2); // 500Hz
    }
    
    void execute() override
    {
        // Read CV inputs (simplified)
        for (int i = 0; i < 4; i++) {
            g_state.cv_inputs[i] = read_cv_input(i);
        }
        
        // Convert CV1 to frequency (1V/octave)
        g_state.frequency = 440.0f * pow(2.0f, g_state.cv_inputs[0]);
    }
};

class GateThread : public SimpleThread
{
public:
    GateThread() : SimpleThread("Gate_Monitor")
    {
        setInterval(1); // 1kHz for fast gate detection
    }
    
    void execute() override
    {
        static bool last_gates[2] = {false, false};
        
        for (int i = 0; i < 2; i++) {
            bool current = read_gate_input(i);
            
            // Detect rising edge
            if (current && !last_gates[i]) {
                handle_gate_trigger(i);
            }
            
            g_state.gate_inputs[i] = current;
            last_gates[i] = current;
        }
    }
    
private:
    void handle_gate_trigger(int gate)
    {
        printf("Gate %d triggered!\n", gate);
        g_state.step_count++;
    }
};

class DisplayThread : public SimpleThread
{
public:
    DisplayThread() : SimpleThread("Display")
    {
        setInterval(200); // 5Hz update
    }
    
    void execute() override
    {
        printf("Freq: %.1fHz | Steps: %lu | CV: %.2fV\n",
               g_state.frequency,
               g_state.step_count,
               g_state.cv_inputs[0]);
    }
};

class LEDThread : public SimpleThread
{
private:
    uint32_t phase;
    
public:
    LEDThread() : SimpleThread("LED_Pattern"), phase(0)
    {
        setInterval(50); // 20Hz
    }
    
    void execute() override
    {
        phase++;
        
        // Blink rate based on frequency
        uint32_t blink_rate = (uint32_t)(20.0f / (g_state.frequency / 440.0f + 1.0f));
        g_state.led_state = (phase % blink_rate) < (blink_rate / 2);
        
        EurorackUtils::LED::set(g_state.led_state);
    }
};

int main()
{
    EurorackUtils::init();
    
    printf("Advanced Eurorack Module Starting...\n");
    
    // Create all threads
    CVThread cv_thread;
    GateThread gate_thread;
    DisplayThread display_thread;
    LEDThread led_thread;
    
    // Set up scheduler
    SimpleScheduler scheduler;
    scheduler.addThread(&cv_thread);
    scheduler.addThread(&gate_thread);
    scheduler.addThread(&display_thread);
    scheduler.addThread(&led_thread);
    
    printf("Initialized %d threads\n", scheduler.getThreadCount());
    
    // Main loop
    uint32_t loop_count = 0;
    while (true) {
        scheduler.run();
        
        // Optional: main loop processing
        tight_loop_contents();
        
        // Occasional housekeeping
        if (++loop_count % 1000000 == 0) {
            printf("Main loop health check: %lu iterations\n", loop_count);
        }
    }
    
    return 0;
}

// Placeholder functions (implement with real hardware)
float read_cv_input(int channel) { return (float)(rand() % 5000) / 1000.0f; }
bool read_gate_input(int gate) { return rand() % 100 < 5; } // 5% chance
```

---

## Summary

The Simple Threads library provides a powerful yet easy-to-use foundation for multi-tasking embedded applications. Key takeaways:

### ✅ **Do This**
- Keep `execute()` methods fast and non-blocking  
- Use appropriate intervals for each thread's requirements
- Provide meaningful names for debugging
- Use member variables to maintain state between executions
- Enable/disable threads dynamically for power management

### ❌ **Avoid This**  
- Blocking delays or infinite loops in `execute()`
- Overly complex logic in a single thread
- Accessing hardware directly from multiple threads simultaneously
- Forgetting to call `scheduler.run()` in main loop
- Creating more than 16 threads (scheduler limit)

### 🎯 **Perfect For**
- LED patterns and visual feedback
- Sensor monitoring and data acquisition  
- User interface handling
- System status and housekeeping tasks
- Audio control and modulation (not sample-level processing)

### 🚫 **Not Ideal For**
- High-frequency audio sample processing (use interrupts)
- Real-time critical operations with strict timing
- Complex inter-thread communication patterns
- Applications requiring preemptive multitasking

The Simple Threads library strikes an excellent balance between simplicity and functionality, making it perfect for most Eurorack module development needs. Start with the basic examples and gradually incorporate more advanced patterns as your project grows!
