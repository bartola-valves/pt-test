# Protothreads (pt_thread) Library Tutorial
*Advanced Event-Driven Programming for Eurorack Development*

## Table of Contents
1. [Introduction](#introduction)
2. [Protothreads vs Simple Threads](#protothreads-vs-simple-threads)
3. [Getting Started](#getting-started)
4. [Core Concepts](#core-concepts)
5. [Step-by-Step Tutorial](#step-by-step-tutorial)
6. [Class Reference](#class-reference)
7. [Event System Deep Dive](#event-system-deep-dive)
8. [Advanced Patterns](#advanced-patterns)
9. [Best Practices](#best-practices)
10. [Performance and Limitations](#performance-and-limitations)
11. [Migration Guide](#migration-guide)
12. [Complete Examples](#complete-examples)

---

## Introduction

The Protothreads library (`framework/pt_thread.h`) provides an advanced, event-driven cooperative threading system for Raspberry Pi Pico embedded applications. Unlike Simple Threads, protothreads use continuation-based execution, allowing threads to suspend and resume execution at specific points with minimal memory overhead.

### Why Choose Protothreads?

- **Memory Efficient**: Each thread uses only 2-3 bytes for state storage
- **Event-Driven**: Built-in event queue system for interrupt handling
- **True Continuations**: Suspend/resume execution at any point in your code
- **Complex State Machines**: Perfect for intricate control flow
- **Hardware Integration**: Direct support for interrupts, ADC, PWM, timers

### When to Use Protothreads vs Simple Threads

| Feature | Simple Threads | Protothreads |
|---------|----------------|-------------|
| **Learning Curve** | Easy | Moderate to Advanced |
| **Memory Usage** | ~20 bytes/thread | ~3 bytes/thread |
| **State Management** | Manual with variables | Automatic with continuations |
| **Event Handling** | Manual polling | Built-in event queue |
| **Complex Logic** | Can become unwieldy | Natural fit |
| **Real-time Response** | Timer-based only | Event + timer driven |
| **Best For** | Simple periodic tasks | Complex reactive systems |

---

## Protothreads vs Simple Threads

### Execution Model Comparison

**Simple Threads (from Simple Threads Tutorial):**
```cpp
class SimpleBlinkThread : public SimpleThread {
    void execute() override {
        // Entire logic runs each time interval expires
        led_state = !led_state;
        EurorackUtils::LED::set(led_state);
    }
};
```

**Protothreads:**
```cpp
class PTBlinkThread : public PTThread {
    int run() override {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            gpio_put(LED_PIN, 1);
            PT_THREAD_WAIT_UNTIL(this, timer_expired(100)); // Suspend here
            
            gpio_put(LED_PIN, 0);
            PT_THREAD_WAIT_UNTIL(this, timer_expired(100)); // Resume here
        }
        
        PT_THREAD_END(this);
    }
};
```

### Key Differences

1. **State Preservation**: Protothreads automatically save execution state at suspension points
2. **Flow Control**: Natural `while` loops and `if` statements work across suspensions
3. **Event Integration**: Built-in event waiting with `PT_WAIT_EVENT()`
4. **Memory Efficiency**: Minimal per-thread overhead

---

## Getting Started

### Prerequisites

- Completed Simple Threads Tutorial (recommended)
- Understanding of C++ and embedded programming
- Raspberry Pi Pico SDK configured
- Familiarity with state machines and event-driven programming

### Including the Library

```cpp
#include "framework/pt_thread.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
```

### Required Setup

```cpp
// In main()
stdio_init_all();

// Initialize any hardware you'll use
gpio_init(LED_PIN);
gpio_set_dir(LED_PIN, GPIO_OUT);
```

---

## Core Concepts

### 1. Protothreads Macros

Every protothread function must use these macros:

```cpp
int run() override {
    PT_THREAD_BEGIN(this);    // Mark thread start
    
    // Your thread logic here
    // Can use PT_THREAD_WAIT_UNTIL(), PT_THREAD_YIELD(), etc.
    
    PT_THREAD_END(this);      // Mark thread end
}
```

### 2. Continuation Points

Protothreads can suspend execution and resume later:

```cpp
PT_THREAD_WAIT_UNTIL(this, condition);  // Suspend until condition is true
PT_THREAD_YIELD(this);                  // Suspend for one scheduler cycle
PT_THREAD_WAIT_WHILE(this, condition);  // Suspend while condition is true
```

### 3. Event-Driven Programming

Unlike Simple Threads' timer-based approach, protothreads excel at event handling:

```cpp
PT_WAIT_EVENT(this, event);              // Wait for any event
PT_WAIT_EVENT_TYPE(this, event, BUTTON_PRESS); // Wait for specific event
```

### 4. Thread Lifecycle

1. **Initialization**: `PT_THREAD_BEGIN()` sets up continuation state
2. **Execution**: Code runs until a suspension point
3. **Suspension**: Thread yields control, state is preserved
4. **Resumption**: Execution continues from suspension point
5. **Completion**: `PT_THREAD_END()` or `PT_THREAD_EXIT()`

---

## Step-by-Step Tutorial

### Step 1: Your First Protothread

Let's start with a simple blinking LED:

```cpp
#include "framework/pt_thread.h"
#include "hardware/gpio.h"

#define LED_PIN PICO_DEFAULT_LED_PIN

class MyFirstPTThread : public PTThread
{
private:
    uint32_t timer_start;
    
public:
    MyFirstPTThread() : PTThread("FirstPT") {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            // Turn LED on
            gpio_put(LED_PIN, 1);
            printf("LED ON\n");
            
            // Wait 500ms
            timer_start = time_us_32();
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 500000);
            
            // Turn LED off  
            gpio_put(LED_PIN, 0);
            printf("LED OFF\n");
            
            // Wait another 500ms
            timer_start = time_us_32();
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 500000);
        }
        
        PT_THREAD_END(this);
    }
};
```

**Key Points:**
- `PT_THREAD_BEGIN(this)` and `PT_THREAD_END(this)` are required
- `PT_THREAD_WAIT_UNTIL()` suspends execution until condition is true
- Thread resumes exactly where it left off
- Natural `while(true)` loop works across suspensions

### Step 2: Setting Up the Scheduler

```cpp
int main()
{
    stdio_init_all();
    
    // Initialize LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    sleep_ms(1000); // Wait for serial
    
    printf("Starting protothread example\n");
    
    // Create thread
    MyFirstPTThread my_thread;
    
    // Create scheduler
    PTScheduler scheduler;
    scheduler.addThread(&my_thread);
    
    // Run scheduler (blocks forever)
    scheduler.run();
    
    return 0;
}
```

### Step 3: Adding Event Handling

Let's create a thread that responds to events:

```cpp
class EventDrivenThread : public PTThread
{
private:
    uint32_t event_count;
    
public:
    EventDrivenThread() : PTThread("EventDriven"), event_count(0) {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        printf("EventDrivenThread: Waiting for events...\n");
        
        while (true) {
            PTEvent event;
            PT_WAIT_EVENT(this, event); // Wait for any event
            
            event_count++;
            printf("Received event #%lu: Type=%d, Data=%lu\n", 
                   event_count, (int)event.type, event.data);
                   
            // Process different event types
            switch (event.type) {
                case PTEventType::BUTTON_PRESS:
                    printf("Button pressed! Data: %lu\n", event.data);
                    break;
                    
                case PTEventType::TIMER_TICK:
                    printf("Timer tick received\n");
                    break;
                    
                case PTEventType::USER_EVENT:
                    printf("User event: %lu\n", event.data);
                    break;
                    
                default:
                    printf("Unknown event type\n");
                    break;
            }
        }
        
        PT_THREAD_END(this);
    }
};
```

### Step 4: Inter-Thread Communication

Create threads that communicate via events:

```cpp
class ProducerThread : public PTThread
{
private:
    uint32_t timer_start;
    uint32_t message_count;
    
public:
    ProducerThread() : PTThread("Producer"), message_count(0) {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            // Wait 2 seconds
            timer_start = time_us_32();
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 2000000);
            
            // Send event to other threads
            message_count++;
            if (event_queue) {
                PTEvent msg(PTEventType::USER_EVENT, message_count);
                event_queue->push(msg);
                printf("Producer: Sent message #%lu\n", message_count);
            }
        }
        
        PT_THREAD_END(this);
    }
};

class ConsumerThread : public PTThread
{
public:
    ConsumerThread() : PTThread("Consumer") {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            PTEvent event;
            PT_WAIT_EVENT_TYPE(this, event, PTEventType::USER_EVENT);
            
            printf("Consumer: Processed message #%lu\n", event.data);
            
            // Toggle LED based on message
            gpio_put(LED_PIN, event.data % 2);
        }
        
        PT_THREAD_END(this);
    }
};
```

### Step 5: Complex State Machine

Here's a more advanced example with multiple states:

```cpp
class SequencerThread : public PTThread
{
private:
    enum State { STOPPED, PLAYING, RECORDING, PAUSED };
    State state;
    int step;
    uint32_t timer_start;
    bool pattern[16];
    
public:
    SequencerThread() : PTThread("Sequencer"), state(STOPPED), step(0)
    {
        // Initialize empty pattern
        for (int i = 0; i < 16; i++) {
            pattern[i] = (i % 4 == 0); // Every 4th step active
        }
    }
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            switch (state) {
                case STOPPED:
                    printf("Sequencer: Stopped, waiting for start event\n");
                    PTEvent start_event;
                    PT_WAIT_EVENT_TYPE(this, start_event, PTEventType::BUTTON_PRESS);
                    state = PLAYING;
                    step = 0;
                    break;
                    
                case PLAYING:
                    // Play current step
                    if (pattern[step]) {
                        gpio_put(LED_PIN, 1);
                        printf("Step %d: ON\n", step);
                    } else {
                        gpio_put(LED_PIN, 0);
                        printf("Step %d: off\n", step);
                    }
                    
                    // Wait for step duration (250ms)
                    timer_start = time_us_32();
                    PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 250000);
                    
                    // Advance to next step
                    step = (step + 1) % 16;
                    
                    // Check for stop event (non-blocking)
                    PTEvent stop_event;
                    if (event_queue && event_queue->pop(stop_event)) {
                        if (stop_event.type == PTEventType::BUTTON_RELEASE) {
                            state = STOPPED;
                            gpio_put(LED_PIN, 0);
                        }
                    }
                    break;
                    
                // Add RECORDING and PAUSED states as needed...
            }
        }
        
        PT_THREAD_END(this);
    }
};
```

---

## Class Reference

### PTThread Class

#### Constructor
```cpp
PTThread(const char *thread_name = "PTThread")
```
**Purpose**: Initialize protothread with optional name
**Parameters**: `thread_name` - Thread identifier for debugging

#### Core Virtual Method

##### `virtual int run() = 0`
**Purpose**: Main thread execution function - must be implemented
**Returns**: Protothread status (`PT_WAITING`, `PT_YIELDED`, `PT_ENDED`, `PT_EXITED`)
**Usage**: Contains your thread logic with PT macros

```cpp
int run() override {
    PT_THREAD_BEGIN(this);
    // Your logic here
    PT_THREAD_END(this);
}
```

#### Control Methods

##### `void init()`
**Purpose**: Restart the thread from the beginning
**Usage**: Reset thread state and continue execution

```cpp
my_thread.init(); // Restart thread
```

##### `void stop()`
**Purpose**: Stop thread execution
**Usage**: Thread will no longer be scheduled

```cpp
my_thread.stop(); // Stop thread
```

##### `bool isActive() const`
**Purpose**: Check if thread is currently active
**Returns**: `true` if thread is running

```cpp
if (my_thread.isActive()) {
    printf("Thread is running\n");
}
```

#### Information Methods

##### `const char* getName() const`
**Purpose**: Get thread name for debugging
**Returns**: Thread name string

##### `uint32_t getRunCount() const`
**Purpose**: Get number of times thread has been executed
**Returns**: Execution count

##### `uint32_t getLastRunTime() const`
**Purpose**: Get timestamp of last execution
**Returns**: Microsecond timestamp

```cpp
printf("Thread %s: %lu runs, last at %lu\n", 
       thread.getName(), 
       thread.getRunCount(), 
       thread.getLastRunTime());
```

#### Event Integration

##### `void setEventQueue(PTEventQueue* queue)`
**Purpose**: Connect thread to event queue (done automatically by scheduler)
**Internal Use**: Called by PTScheduler

### PTScheduler Class

#### Constructor
```cpp
PTScheduler()
```
**Purpose**: Initialize empty scheduler
**Capacity**: Up to 16 threads (configurable)

#### Thread Management

##### `bool addThread(PTThread* thread)`
**Purpose**: Add thread to scheduler and connect to event queue
**Parameters**: `thread` - Pointer to PTThread instance
**Returns**: `true` if successful, `false` if scheduler full

```cpp
PTScheduler scheduler;
MyThread my_thread;

if (scheduler.addThread(&my_thread)) {
    printf("Thread added successfully\n");
}
```

##### `bool removeThread(PTThread* thread)`
**Purpose**: Remove thread from scheduler
**Parameters**: `thread` - Thread to remove
**Returns**: `true` if found and removed

```cpp
scheduler.removeThread(&my_thread);
```

#### Execution Control

##### `void runOnce()`
**Purpose**: Execute one scheduler cycle (all active threads once)
**Usage**: Manual control over scheduler timing

```cpp
while (running) {
    scheduler.runOnce();
    // Do other work...
    tight_loop_contents();
}
```

##### `void run()`
**Purpose**: Run scheduler indefinitely (blocks until stopped)
**Usage**: Simple main loop

```cpp
scheduler.run(); // Runs forever
```

##### `void stop()`
**Purpose**: Stop the scheduler
**Usage**: Break out of `run()` method

#### Event System

##### `PTEventQueue* getEventQueue()`
**Purpose**: Access global event queue
**Returns**: Pointer to event queue

##### `bool postEvent(PTEventType type, uint32_t data = 0)`
**Purpose**: Post event to global queue
**Returns**: `true` if event queued successfully

```cpp
// Post a button press event
scheduler.postEvent(PTEventType::BUTTON_PRESS, button_id);

// Post a timer tick
scheduler.postEvent(PTEventType::TIMER_TICK);

// Post custom user event
scheduler.postEvent(PTEventType::USER_EVENT, custom_data);
```

#### Statistics

##### `size_t getThreadCount() const`
**Purpose**: Get current number of active threads
**Returns**: Thread count (0-16)

##### `uint32_t getSchedulerTicks() const`
**Purpose**: Get total scheduler cycles executed
**Returns**: Cycle count

### PTEventQueue Class

#### Methods

##### `bool push(const PTEvent& event)`
**Purpose**: Add event to queue (interrupt-safe)
**Returns**: `true` if space available

##### `bool pop(PTEvent& event)`
**Purpose**: Remove event from queue (interrupt-safe)
**Returns**: `true` if event was available

##### `bool isEmpty() const`
**Purpose**: Check if queue is empty
**Returns**: `true` if no events pending

##### `size_t size() const`
**Purpose**: Get current queue size
**Returns**: Number of pending events

##### `void clear()`
**Purpose**: Remove all events from queue

### PTEvent Structure

```cpp
struct PTEvent {
    PTEventType type;    // Event type
    uint32_t data;       // Event-specific data
    uint32_t timestamp;  // When event occurred
    bool processed;      // Processing flag
};
```

#### Event Types

```cpp
enum class PTEventType {
    NONE,            // No event
    ENCODER_TURN,    // Rotary encoder movement
    BUTTON_PRESS,    // Button pressed
    BUTTON_RELEASE,  // Button released
    GATE_RISING,     // Gate input rising edge
    GATE_FALLING,    // Gate input falling edge
    TIMER_TICK,      // Timer interrupt
    ADC_READY,       // ADC conversion complete
    SCREEN_REFRESH,  // Display update needed
    SEQUENCE_STEP,   // Sequencer step
    CV_CHANGE,       // Control voltage changed
    USER_EVENT       // Custom application events
};
```

---

## Event System Deep Dive

### Event-Driven vs Timer-Driven

**Simple Threads (Timer-Driven):**
```cpp
class SimpleThread : public SimpleThread {
    void execute() override {
        // Called every N milliseconds
        if (button_changed()) {
            handle_button();
        }
        if (cv_changed()) {
            handle_cv();
        }
    }
};
```

**Protothreads (Event-Driven):**
```cpp
class PTThread : public PTThread {
    int run() override {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            PTEvent event;
            PT_WAIT_EVENT(this, event); // Sleep until event occurs
            
            switch (event.type) {
                case PTEventType::BUTTON_PRESS:
                    handle_button_press(event.data);
                    break;
                case PTEventType::CV_CHANGE:
                    handle_cv_change(event.data);
                    break;
            }
        }
        
        PT_THREAD_END(this);
    }
};
```

### Interrupt Integration

Connect hardware interrupts to the event system:

```cpp
// GPIO interrupt handler
void gpio_interrupt_handler(uint gpio, uint32_t events)
{
    // Get scheduler from global scope
    extern PTScheduler* g_scheduler;
    
    if (events & GPIO_IRQ_EDGE_RISE) {
        g_scheduler->postEvent(PTEventType::BUTTON_PRESS, gpio);
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        g_scheduler->postEvent(PTEventType::BUTTON_RELEASE, gpio);
    }
}

// Setup in main()
void setup_interrupts(PTScheduler* scheduler)
{
    // Store scheduler globally for interrupt access
    g_scheduler = scheduler;
    
    // Enable GPIO interrupt
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, 
                                      GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, 
                                      true, 
                                      gpio_interrupt_handler);
}
```

### Event Patterns

#### 1. Event Filtering
```cpp
int run() override {
    PT_THREAD_BEGIN(this);
    
    while (true) {
        PTEvent event;
        PT_WAIT_EVENT(this, event);
        
        // Only process button events
        if (event.type == PTEventType::BUTTON_PRESS) {
            handle_button(event.data);
        }
        // Ignore all other events
    }
    
    PT_THREAD_END(this);
}
```

#### 2. Event Timeout
```cpp
int run() override {
    PT_THREAD_BEGIN(this);
    
    while (true) {
        uint32_t timeout_start = time_us_32();
        PTEvent event;
        
        // Wait for event OR timeout
        PT_THREAD_WAIT_UNTIL(this, 
            (event_queue && event_queue->pop(event)) ||
            (time_us_32() - timeout_start) > 5000000); // 5 second timeout
            
        if (event.type != PTEventType::NONE) {
            printf("Got event: %d\n", (int)event.type);
        } else {
            printf("Timeout occurred\n");
        }
    }
    
    PT_THREAD_END(this);
}
```

#### 3. Event Buffering
```cpp
class BufferedEventThread : public PTThread
{
private:
    static const size_t BUFFER_SIZE = 10;
    PTEvent event_buffer[BUFFER_SIZE];
    size_t buffer_count;
    
public:
    int run() override {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            // Collect multiple events
            buffer_count = 0;
            
            while (buffer_count < BUFFER_SIZE) {
                PTEvent event;
                PT_WAIT_EVENT(this, event);
                event_buffer[buffer_count++] = event;
                
                // Stop collecting if no more events immediately available
                if (event_queue->isEmpty()) break;
            }
            
            // Process batch of events
            process_event_batch(event_buffer, buffer_count);
        }
        
        PT_THREAD_END(this);
    }
};
```

---

## Advanced Patterns

### 1. Hierarchical State Machines

Combine protothreads with complex state management:

```cpp
class HierarchicalSMThread : public PTThread
{
private:
    enum MainState { IDLE, ACTIVE, ERROR };
    enum ActiveSubState { RECORDING, PLAYING, OVERDUBBING };
    
    MainState main_state;
    ActiveSubState active_substate;
    uint32_t timer_start;
    
public:
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        main_state = IDLE;
        
        while (true) {
            switch (main_state) {
                case IDLE:
                    PT_THREAD_WAIT_UNTIL(this, handle_idle_state());
                    break;
                    
                case ACTIVE:
                    PT_THREAD_WAIT_UNTIL(this, handle_active_state());
                    break;
                    
                case ERROR:
                    PT_THREAD_WAIT_UNTIL(this, handle_error_state());
                    break;
            }
        }
        
        PT_THREAD_END(this);
    }
    
private:
    bool handle_idle_state()
    {
        PTEvent event;
        if (event_queue && event_queue->pop(event)) {
            if (event.type == PTEventType::BUTTON_PRESS) {
                main_state = ACTIVE;
                active_substate = RECORDING;
                return true; // State changed
            }
        }
        return false; // Stay in current state
    }
    
    bool handle_active_state()
    {
        switch (active_substate) {
            case RECORDING:
                return handle_recording_substate();
            case PLAYING:
                return handle_playing_substate();
            case OVERDUBBING:
                return handle_overdubbing_substate();
        }
        return false;
    }
    
    // Implement substates...
};
```

### 2. Producer-Consumer with Backpressure

```cpp
class ProducerThread : public PTThread
{
private:
    uint32_t production_rate_us;
    uint32_t timer_start;
    
public:
    ProducerThread(uint32_t rate_us) : production_rate_us(rate_us) {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            // Wait for production interval
            timer_start = time_us_32();
            PT_THREAD_WAIT_UNTIL(this, 
                (time_us_32() - timer_start) >= production_rate_us);
            
            // Check if consumer can handle more events
            if (event_queue->size() < event_queue->MAX_EVENTS - 2) {
                PTEvent data_event(PTEventType::USER_EVENT, generate_data());
                event_queue->push(data_event);
            } else {
                printf("Producer: Backpressure detected, skipping\n");
            }
        }
        
        PT_THREAD_END(this);
    }
    
private:
    uint32_t generate_data() {
        return time_us_32() % 1000; // Sample data
    }
};

class ConsumerThread : public PTThread
{
private:
    uint32_t processing_time_us;
    
public:
    ConsumerThread(uint32_t proc_time_us) : processing_time_us(proc_time_us) {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            PTEvent event;
            PT_WAIT_EVENT_TYPE(this, event, PTEventType::USER_EVENT);
            
            // Simulate processing time
            uint32_t start = time_us_32();
            PT_THREAD_WAIT_UNTIL(this, 
                (time_us_32() - start) >= processing_time_us);
            
            printf("Consumer: Processed data %lu\n", event.data);
        }
        
        PT_THREAD_END(this);
    }
};
```

### 3. Periodic Tasks with Event Interruption

```cpp
class PeriodicTaskThread : public PTThread
{
private:
    uint32_t period_us;
    uint32_t timer_start;
    bool task_interrupted;
    
public:
    PeriodicTaskThread(uint32_t period) : period_us(period), task_interrupted(false) {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            timer_start = time_us_32();
            task_interrupted = false;
            
            // Start periodic task
            printf("Starting periodic task...\n");
            
            // Wait for period OR interruption event
            PT_THREAD_WAIT_UNTIL(this, 
                (time_us_32() - timer_start) >= period_us || 
                check_interruption());
            
            if (task_interrupted) {
                printf("Task interrupted by event\n");
                handle_interruption();
            } else {
                printf("Periodic task completed normally\n");
                perform_periodic_work();
            }
        }
        
        PT_THREAD_END(this);
    }
    
private:
    bool check_interruption()
    {
        PTEvent event;
        if (event_queue && event_queue->pop(event)) {
            if (event.type == PTEventType::BUTTON_PRESS) {
                task_interrupted = true;
                return true;
            }
        }
        return false;
    }
    
    void handle_interruption() {
        printf("Handling interruption...\n");
    }
    
    void perform_periodic_work() {
        printf("Performing scheduled work...\n");
    }
};
```

---

## Best Practices

### 1. Protothread Structure

**✅ Always use proper macros:**
```cpp
int run() override {
    PT_THREAD_BEGIN(this);    // Required at start
    
    // Your logic here
    
    PT_THREAD_END(this);      // Required at end
}
```

**❌ Never forget the macros:**
```cpp
int run() override {
    // Missing PT_THREAD_BEGIN(this)
    while (true) {
        PT_THREAD_WAIT_UNTIL(this, condition); // Will not work!
    }
    // Missing PT_THREAD_END(this)
}
```

### 2. Local Variables

**⚠️ Limitation**: Local variables don't survive suspension:

```cpp
// ❌ Bad - local variable lost across suspension
int run() override {
    PT_THREAD_BEGIN(this);
    
    int local_var = 42;                    // Lost after suspension
    PT_THREAD_WAIT_UNTIL(this, condition);
    printf("%d\n", local_var);             // Undefined behavior!
    
    PT_THREAD_END(this);
}

// ✅ Good - use member variables
class MyThread : public PTThread {
private:
    int member_var;  // Survives suspension
    
public:
    int run() override {
        PT_THREAD_BEGIN(this);
        
        member_var = 42;                    // Safe
        PT_THREAD_WAIT_UNTIL(this, condition);
        printf("%d\n", member_var);        // Works correctly
        
        PT_THREAD_END(this);
    }
};
```

### 3. Switch Statements

**⚠️ Limitation**: Switch statements conflict with protothread macros:

```cpp
// ❌ Bad - switch with PT macros inside
int run() override {
    PT_THREAD_BEGIN(this);
    
    switch (state) {
        case STATE_A:
            PT_THREAD_WAIT_UNTIL(this, condition); // Compiler error!
            break;
    }
    
    PT_THREAD_END(this);
}

// ✅ Good - use separate functions or if/else
int run() override {
    PT_THREAD_BEGIN(this);
    
    if (state == STATE_A) {
        PT_THREAD_WAIT_UNTIL(this, condition_a);
        handle_state_a();
    } else if (state == STATE_B) {
        PT_THREAD_WAIT_UNTIL(this, condition_b);
        handle_state_b();
    }
    
    PT_THREAD_END(this);
}
```

### 4. Event Handling Patterns

**✅ Always check event validity:**
```cpp
PTEvent event;
PT_WAIT_EVENT(this, event);

if (event.type != PTEventType::NONE) {
    // Process event
    handle_event(event);
}
```

**✅ Use event-specific waits when possible:**
```cpp
// Better than waiting for any event and filtering
PT_WAIT_EVENT_TYPE(this, event, PTEventType::BUTTON_PRESS);
```

### 5. Memory Management

**✅ Minimal memory usage:**
```cpp
class EfficientThread : public PTThread {
private:
    // Use bit fields for flags
    struct {
        uint8_t state : 3;        // 3 bits for up to 8 states
        uint8_t enabled : 1;      // 1 bit flag
        uint8_t direction : 1;    // 1 bit flag
        uint8_t reserved : 3;     // 3 bits unused
    } flags;
    
    uint16_t counter;            // Use smallest necessary type
    
public:
    int run() override {
        PT_THREAD_BEGIN(this);
        // Efficient implementation
        PT_THREAD_END(this);
    }
};
```

---

## Performance and Limitations

### Memory Usage

| Component | Memory per Instance |
|-----------|-------------------|
| PTThread | 16-20 bytes |
| PTScheduler | ~80 bytes + (16 × 8 bytes) |
| PTEventQueue | ~140 bytes |
| PTEvent | 16 bytes |

**Total typical application**: 200-400 bytes

### CPU Performance

- **Thread switching**: ~20-30 CPU cycles
- **Event posting**: ~50-100 cycles (interrupt-safe)
- **Event waiting**: ~10 cycles (when no event)
- **Scheduler overhead**: ~100 cycles per `runOnce()`

### Limitations

1. **Local Variables**: Don't survive suspension points
2. **Switch Statements**: Can't contain PT macros
3. **Function Calls**: Functions with PT macros can't be called from threads
4. **Stack Usage**: Each suspension point uses ~2 bytes
5. **Nested Calls**: Limited support for nested protothread calls

### Comparison with Simple Threads

| Metric | Simple Threads | Protothreads |
|--------|----------------|-------------|
| **Memory/Thread** | ~20 bytes | ~3 bytes |
| **CPU Overhead** | ~50 cycles/thread | ~30 cycles/thread |
| **Event Response** | Timer-based (delayed) | Immediate |
| **Code Complexity** | Simple | Moderate |
| **State Management** | Manual | Automatic |

---

## Migration Guide

### From Simple Threads to Protothreads

#### 1. Basic Timer-Based Task

**Simple Threads:**
```cpp
class SimpleTimerThread : public SimpleThread {
private:
    uint32_t count;
    
public:
    SimpleTimerThread() : count(0) {
        setInterval(1000); // 1 second
    }
    
    void execute() override {
        count++;
        printf("Count: %lu\n", count);
    }
};
```

**Protothreads:**
```cpp
class PTTimerThread : public PTThread {
private:
    uint32_t count;
    uint32_t timer_start;
    
public:
    PTTimerThread() : count(0) {}
    
    int run() override {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            timer_start = time_us_32();
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 1000000);
            
            count++;
            printf("Count: %lu\n", count);
        }
        
        PT_THREAD_END(this);
    }
};
```

#### 2. State Machine

**Simple Threads:**
```cpp
class SimpleStateMachine : public SimpleThread {
private:
    enum State { A, B, C };
    State state;
    uint32_t counter;
    
public:
    void execute() override {
        switch (state) {
            case A:
                if (++counter > 10) { state = B; counter = 0; }
                break;
            case B:
                if (++counter > 5) { state = C; counter = 0; }
                break;
            case C:
                if (++counter > 3) { state = A; counter = 0; }
                break;
        }
    }
};
```

**Protothreads:**
```cpp
class PTStateMachine : public PTThread {
private:
    uint32_t timer_start;
    
public:
    int run() override {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            // State A - wait 1 second
            printf("State A\n");
            timer_start = time_us_32();
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 1000000);
            
            // State B - wait 0.5 seconds
            printf("State B\n");
            timer_start = time_us_32();
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 500000);
            
            // State C - wait 0.3 seconds
            printf("State C\n");
            timer_start = time_us_32();
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 300000);
        }
        
        PT_THREAD_END(this);
    }
};
```

#### 3. Migration Checklist

- [ ] Replace `SimpleThread` inheritance with `PTThread`
- [ ] Move `execute()` logic to `run()` with PT macros
- [ ] Convert timer-based waits to `PT_THREAD_WAIT_UNTIL()`
- [ ] Move local variables to member variables
- [ ] Replace polling with event-driven patterns
- [ ] Update scheduler from `SimpleScheduler` to `PTScheduler`

---

## Complete Examples

### Example 1: Basic Event-Driven LED Controller

```cpp
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "framework/pt_thread.h"

#define LED_PIN PICO_DEFAULT_LED_PIN
#define BUTTON_PIN 15

PTScheduler* g_scheduler = nullptr; // Global for interrupt access

// Button interrupt handler
void button_interrupt(uint gpio, uint32_t events) {
    if (g_scheduler) {
        if (events & GPIO_IRQ_EDGE_RISE) {
            g_scheduler->postEvent(PTEventType::BUTTON_PRESS, gpio);
        } else if (events & GPIO_IRQ_EDGE_FALL) {
            g_scheduler->postEvent(PTEventType::BUTTON_RELEASE, gpio);
        }
    }
}

class LEDControllerThread : public PTThread
{
private:
    bool led_state;
    
public:
    LEDControllerThread() : PTThread("LEDController"), led_state(false) {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        printf("LED Controller: Waiting for button events\n");
        
        while (true) {
            PTEvent event;
            PT_WAIT_EVENT_TYPE(this, event, PTEventType::BUTTON_PRESS);
            
            // Toggle LED on button press
            led_state = !led_state;
            gpio_put(LED_PIN, led_state);
            
            printf("Button pressed! LED: %s\n", led_state ? "ON" : "OFF");
        }
        
        PT_THREAD_END(this);
    }
};

int main()
{
    stdio_init_all();
    sleep_ms(1000);
    
    // Initialize hardware
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    
    // Create scheduler and thread
    PTScheduler scheduler;
    g_scheduler = &scheduler;
    
    LEDControllerThread led_controller;
    scheduler.addThread(&led_controller);
    
    // Setup button interrupt
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, 
                                      GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, 
                                      true, 
                                      button_interrupt);
    
    printf("Event-driven LED controller started\n");
    
    // Run scheduler
    scheduler.run();
    
    return 0;
}
```

### Example 2: Multi-Thread Audio Sequencer

```cpp
class ClockGeneratorThread : public PTThread
{
private:
    uint32_t timer_start;
    uint32_t bpm;
    uint32_t step_count;
    
public:
    ClockGeneratorThread(uint32_t beats_per_minute) 
        : PTThread("ClockGen"), bpm(beats_per_minute), step_count(0) {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        uint32_t step_duration_us = 60000000 / (bpm * 4); // 16th notes
        
        while (true) {
            timer_start = time_us_32();
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= step_duration_us);
            
            step_count++;
            
            // Post clock event to sequencer
            if (event_queue) {
                event_queue->push(PTEvent(PTEventType::SEQUENCE_STEP, step_count));
            }
        }
        
        PT_THREAD_END(this);
    }
    
    void setBPM(uint32_t new_bpm) { bpm = new_bpm; }
};

class SequencerThread : public PTThread
{
private:
    bool pattern[16];
    int current_step;
    uint32_t pattern_length;
    
public:
    SequencerThread() : PTThread("Sequencer"), current_step(0), pattern_length(16)
    {
        // Initialize with a basic pattern
        for (int i = 0; i < 16; i++) {
            pattern[i] = (i % 4 == 0) || (i == 6) || (i == 14);
        }
    }
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        printf("Sequencer: Waiting for clock events\n");
        
        while (true) {
            PTEvent clock_event;
            PT_WAIT_EVENT_TYPE(this, clock_event, PTEventType::SEQUENCE_STEP);
            
            // Play current step
            if (pattern[current_step]) {
                gpio_put(LED_PIN, 1);
                printf("Step %d: TRIGGER\n", current_step);
                
                // Post trigger event for other threads
                if (event_queue) {
                    event_queue->push(PTEvent(PTEventType::GATE_RISING, current_step));
                }
            } else {
                gpio_put(LED_PIN, 0);
                printf("Step %d: -\n", current_step);
            }
            
            // Advance to next step
            current_step = (current_step + 1) % pattern_length;
        }
        
        PT_THREAD_END(this);
    }
    
    void setPattern(int step, bool active) {
        if (step >= 0 && step < 16) {
            pattern[step] = active;
        }
    }
};

class EnvelopeThread : public PTThread
{
private:
    enum Phase { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    Phase phase;
    uint32_t timer_start;
    float current_level;
    
public:
    EnvelopeThread() : PTThread("Envelope"), phase(IDLE), current_level(0.0f) {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            switch (phase) {
                case IDLE:
                    {
                        PTEvent trigger;
                        PT_WAIT_EVENT_TYPE(this, trigger, PTEventType::GATE_RISING);
                        
                        printf("Envelope: Gate received, starting attack\n");
                        phase = ATTACK;
                        timer_start = time_us_32();
                    }
                    break;
                    
                case ATTACK:
                    // Attack phase - 50ms
                    PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 50000);
                    
                    current_level = 1.0f;
                    phase = DECAY;
                    timer_start = time_us_32();
                    printf("Envelope: Attack complete\n");
                    break;
                    
                case DECAY:
                    // Decay phase - 100ms
                    PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 100000);
                    
                    current_level = 0.6f; // Sustain level
                    phase = SUSTAIN;
                    timer_start = time_us_32();
                    printf("Envelope: Decay complete\n");
                    break;
                    
                case SUSTAIN:
                    // Sustain - wait 200ms then release
                    PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 200000);
                    
                    phase = RELEASE;
                    timer_start = time_us_32();
                    printf("Envelope: Starting release\n");
                    break;
                    
                case RELEASE:
                    // Release phase - 150ms
                    PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 150000);
                    
                    current_level = 0.0f;
                    phase = IDLE;
                    printf("Envelope: Release complete\n");
                    break;
            }
        }
        
        PT_THREAD_END(this);
    }
    
    float getLevel() const { return current_level; }
};

int main()
{
    stdio_init_all();
    sleep_ms(1000);
    
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    printf("Multi-Thread Audio Sequencer\n");
    
    // Create threads
    ClockGeneratorThread clock(120); // 120 BPM
    SequencerThread sequencer;
    EnvelopeThread envelope;
    
    // Create scheduler
    PTScheduler scheduler;
    scheduler.addThread(&clock);
    scheduler.addThread(&sequencer);
    scheduler.addThread(&envelope);
    
    printf("Started %zu threads\n", scheduler.getThreadCount());
    
    // Run the sequencer
    scheduler.run();
    
    return 0;
}
```

---

## Summary

The Protothreads library provides a powerful foundation for event-driven embedded programming. Key takeaways:

### ✅ **Choose Protothreads When You Need**
- Event-driven programming with hardware interrupts
- Complex state machines with natural flow control
- Minimal memory usage (< 5 bytes per thread)
- Real-time response to external events
- Advanced inter-thread communication

### ✅ **Use Simple Threads When You Need**
- Simple timer-based periodic tasks
- Learning cooperative threading concepts
- Straightforward program flow
- Easy debugging and understanding
- Quick prototyping

### 🎯 **Protothreads Excel At**
- Audio synthesizer modules with trigger/gate inputs
- User interface handling with immediate button response
- Complex multi-stage processes (envelopes, sequencers)
- Interrupt-driven sensor monitoring
- Real-time control systems

### ⚠️ **Remember the Limitations**
- Local variables don't survive suspension points
- Switch statements can't contain PT macros
- Steeper learning curve than Simple Threads
- Debugging can be more complex

### 🚀 **Best Practice**
Start with Simple Threads for learning and basic tasks, then migrate to Protothreads when you need event-driven behavior, complex state management, or minimal memory usage. The two libraries can even coexist in the same project for different types of tasks!

The Protothreads library transforms embedded programming from polling-based to event-driven, enabling responsive, efficient, and elegant solutions for complex Eurorack module development.
