# Threading Libraries Comparison

## Overview

This document compares the two threading libraries available in the Eurorack framework:

- **`framework/simple_threads.h`** - Simple timer-based cooperative threading
- **`framework/pt_thread.h`** - Full protothread implementation with event system

Understanding the differences will help you choose the right library for your project.

---

## Core Architecture Differences

### Simple Threads (`framework/simple_threads.h`)

```cpp
class SimpleThread {
    virtual void execute() = 0;  // Called at regular intervals
    void setInterval(uint32_t ms); // Set execution frequency
};
```

**Key characteristics:**
- **Timer-based execution**: Uses absolute time intervals to determine when threads should run
- **No state preservation**: Each thread runs from start to finish in one execution
- **Cooperative but simple**: Threads yield control by completing their `execute()` method
- **Minimal memory footprint**: Very lightweight implementation

### Protothreads (`framework/pt_thread.h`)

```cpp
class PTThread {
    virtual int run() = 0;  // Main thread function with state preservation
    PTEventQueue *event_queue; // Access to global event system
};
```

**Key characteristics:**
- **True protothreads**: Uses state machines with `PT_BEGIN`, `PT_WAIT_UNTIL`, `PT_END` macros
- **State preservation**: Threads can pause mid-execution and resume exactly where they left off
- **Advanced cooperative**: Threads can yield at any point and maintain local variables
- **Event-driven**: Built-in support for inter-thread communication

---

## Feature Comparison Matrix

| Feature | Simple Threads | Protothreads |
|---------|---------------|--------------|
| **Complexity** | Simple timer-based | Full protothread implementation |
| **State Management** | None - runs to completion | Preserves execution state between calls |
| **Memory Usage** | Minimal (~50 bytes per thread) | Higher (~200+ bytes per thread) |
| **Blocking Operations** | Not supported | Supported via `PT_WAIT_UNTIL` |
| **Event System** | None | Full event queue with interrupt support |
| **Thread Communication** | Manual/external | Built-in event-driven messaging |
| **Scheduling** | Simple round-robin | Priority-based with state management |
| **Hardware Abstraction** | None | Encoders, Buttons, CV I/O, Gates |
| **Learning Curve** | Easy | Moderate to Advanced |
| **Best For** | Simple periodic tasks | Complex state machines |

---

## Major Gaps in Simple Threads

### 1. No Event System

**Protothreads have:**
```cpp
PTEventQueue global_event_queue;
PTEvent switch_event(PATTERN_SWITCH_EVENT, 1);
event_queue->push(switch_event);

// In another thread:
PT_WAIT_EVENT_TYPE(this, event, PATTERN_SWITCH_EVENT);
```

**Simple Threads require:**
```cpp
// Manual coordination through global variables
volatile bool pattern_switch_requested = false;

// Thread 1 sets flag:
pattern_switch_requested = true;

// Thread 2 polls flag:
if (pattern_switch_requested) {
    // Handle switch
    pattern_switch_requested = false;
}
```

### 2. No Blocking/Waiting Capabilities

**Protothreads can:**
```cpp
int run() override {
    PT_BEGIN(this);
    
    while(true) {
        // Non-blocking wait for condition
        PT_WAIT_UNTIL(this, button_pressed());
        
        // This line runs ONLY after button is pressed
        handle_button_press();
        
        // Wait for button release
        PT_WAIT_UNTIL(this, !button_pressed());
    }
    
    PT_END(this);
}
```

**Simple Threads must:**
```cpp
enum State { WAITING_PRESS, WAITING_RELEASE };
State state = WAITING_PRESS;

void execute() override {
    switch(state) {
        case WAITING_PRESS:
            if (button_pressed()) {
                handle_button_press();
                state = WAITING_RELEASE;
            }
            break;
        case WAITING_RELEASE:
            if (!button_pressed()) {
                state = WAITING_PRESS;
            }
            break;
    }
}
```

### 3. No State Preservation

**Protothreads maintain context:**
```cpp
int run() override {
    PT_BEGIN(this);
    
    static int counter = 0;  // Preserved between calls
    
    for (int i = 0; i < 10; i++) {
        gpio_put(LED_PIN, 1);
        PT_TIMER_DELAY(this, 100);  // Yields here, resumes next time
        gpio_put(LED_PIN, 0);
        PT_TIMER_DELAY(this, 100);  // Yields here too
        counter++;
    }
    
    PT_END(this);
}
```

**Simple Threads need manual state tracking:**
```cpp
enum BlinkState { LED_ON, LED_OFF, SEQUENCE_PAUSE };
BlinkState state = LED_ON;
uint32_t timer = 0;
int blink_count = 0;
int sequence_count = 0;

void execute() override {
    uint32_t now = time_us_32();
    
    switch(state) {
        case LED_ON:
            if ((now - timer) >= 100000) {
                gpio_put(LED_PIN, 1);
                state = LED_OFF;
                timer = now;
            }
            break;
        case LED_OFF:
            if ((now - timer) >= 100000) {
                gpio_put(LED_PIN, 0);
                blink_count++;
                if (blink_count >= 10) {
                    state = SEQUENCE_PAUSE;
                    sequence_count++;
                    blink_count = 0;
                } else {
                    state = LED_ON;
                }
                timer = now;
            }
            break;
        case SEQUENCE_PAUSE:
            // Handle pause logic...
            break;
    }
}
```

### 4. No Hardware Abstraction

**Protothreads include:**
```cpp
// Built-in hardware classes
PTEncoder encoder(PIN_A, PIN_B, PIN_BTN);
PTButton button(PIN);
PTCVInput cv_in(ADC_CHANNEL);
PTCVOutput cv_out(PWM_PIN);
PTGateInput gate_in(PIN);
PTGateOutput gate_out(PIN);

// Easy to use in threads
PT_WAIT_UNTIL(this, encoder.hasChanged());
float cv_value = cv_in.read();
```

**Simple Threads require:**
```cpp
// Manual hardware handling
void execute() override {
    // Raw GPIO/ADC operations
    bool button_state = !gpio_get(BUTTON_PIN);  // Active low
    uint16_t adc_raw = adc_read();
    float cv_voltage = (adc_raw * 3.3f / 4096.0f);
    
    // Manual debouncing, scaling, etc.
}
```

---

## When to Use Which Library

### Use Simple Threads When:

✅ **Simple periodic tasks**
- LED blinking at regular intervals
- Sensor polling every few milliseconds
- Status updates or heartbeat signals

✅ **Learning cooperative multitasking**
- Understanding basic threading concepts
- Educational projects
- Simple proof-of-concept code

✅ **Memory constraints**
- Very limited RAM available
- Need minimal code overhead
- Simple embedded projects

✅ **No inter-thread communication needed**
- Independent tasks
- No coordination between threads
- Simple automation

### Use Protothreads When:

✅ **Complex state machines**
- Multi-step sequences
- User interface handling
- Protocol implementations

✅ **Event-driven programming**
- Responding to user input
- Hardware interrupts
- Inter-thread coordination

✅ **Need to wait for conditions**
- Button press/release sequences
- Timer-based operations
- Sensor threshold monitoring

✅ **Hardware-intensive applications**
- CV/Gate processing
- Encoder/button interfaces
- Audio sample processing

✅ **Professional applications**
- Commercial products
- Complex Eurorack modules
- Multi-threaded audio processing

---

## Code Examples

### Simple LED Blink Comparison

#### Simple Threads Version
```cpp
#include "framework/simple_threads.h"

class BlinkThread : public SimpleThread {
public:
    BlinkThread() : SimpleThread("Blink") {
        setInterval(500);  // 500ms intervals
    }
    
    void execute() override {
        gpio_put(LED_PIN, !gpio_get(LED_PIN));  // Toggle LED
    }
};

// Usage
BlinkThread blink_thread;
SimpleScheduler scheduler;
scheduler.addThread(&blink_thread);

while (true) {
    scheduler.run();
    tight_loop_contents();
}
```

#### Protothreads Version
```cpp
#include "framework/pt_thread.h"

class BlinkThread : public PTThread {
public:
    BlinkThread() : PTThread("Blink") {}
    
    int run() override {
        PT_BEGIN(this);
        
        while (true) {
            gpio_put(LED_PIN, 1);
            PT_TIMER_DELAY(this, 500);
            gpio_put(LED_PIN, 0);
            PT_TIMER_DELAY(this, 500);
        }
        
        PT_END(this);
    }
};

// Usage
BlinkThread blink_thread;
PTScheduler scheduler;
scheduler.addThread(&blink_thread);

while (true) {
    scheduler.runOnce();
    tight_loop_contents();
}
```

### Complex Pattern with Communication

#### Simple Threads Version (More Complex)
```cpp
class FastBlinkThread : public SimpleThread {
private:
    enum State { INIT, BLINK_ON, BLINK_OFF, SEQUENCE_PAUSE, WAIT_FOR_RESUME };
    State state = INIT;
    uint32_t timer = 0;
    uint32_t blink_count = 0;
    uint32_t sequence_count = 0;
    int current_blink = 0;
    
public:
    FastBlinkThread() : SimpleThread("FastBlink") {
        setInterval(10);  // Check every 10ms
    }
    
    void execute() override {
        uint32_t now = time_us_32();
        
        switch(state) {
            case INIT:
                printf("FastBlink: Starting\n");
                state = BLINK_ON;
                current_blink = 0;
                timer = now;
                break;
                
            case BLINK_ON:
                if ((now - timer) >= 100000) { // 100ms
                    gpio_put(LED_PIN, 1);
                    state = BLINK_OFF;
                    timer = now;
                }
                break;
                
            case BLINK_OFF:
                if ((now - timer) >= 100000) { // 100ms
                    gpio_put(LED_PIN, 0);
                    blink_count++;
                    current_blink++;
                    
                    if (current_blink >= 6) {
                        state = SEQUENCE_PAUSE;
                        timer = now;
                    } else {
                        state = BLINK_ON;
                        timer = now;
                    }
                }
                break;
                
            case SEQUENCE_PAUSE:
                if ((now - timer) >= 1000000) { // 1 second
                    current_blink = 0;
                    sequence_count++;
                    
                    if (sequence_count >= 3) {
                        printf("Switching to slow pattern\n");
                        // Manual coordination needed here
                        g_switch_to_slow = true;
                        sequence_count = 0;
                        state = WAIT_FOR_RESUME;
                    } else {
                        state = BLINK_ON;
                        timer = now;
                    }
                }
                break;
                
            case WAIT_FOR_RESUME:
                if (g_resume_fast_pattern) {
                    g_resume_fast_pattern = false;
                    printf("Resuming fast pattern\n");
                    state = BLINK_ON;
                    timer = now;
                }
                break;
        }
    }
};

// Global coordination variables (not ideal)
volatile bool g_switch_to_slow = false;
volatile bool g_resume_fast_pattern = false;
```

#### Protothreads Version (Much Cleaner)
```cpp
class FastBlinkThread : public PTThread {
private:
    uint32_t blink_count = 0;
    uint32_t sequence_count = 0;
    
public:
    FastBlinkThread() : PTThread("FastBlink") {}
    
    int run() override {
        PT_BEGIN(this);
        
        printf("FastBlink: Starting\n");
        
        while (true) {
            // Blink sequence: 6 fast blinks
            for (int i = 0; i < 6; i++) {
                gpio_put(LED_PIN, 1);
                PT_TIMER_DELAY(this, 100);
                gpio_put(LED_PIN, 0);
                PT_TIMER_DELAY(this, 100);
                blink_count++;
            }
            
            // Pause between sequences
            PT_TIMER_DELAY(this, 1000);
            sequence_count++;
            
            // Switch to slow after 3 sequences
            if (sequence_count >= 3) {
                printf("Switching to slow pattern\n");
                PTEvent switch_event(PATTERN_SWITCH_EVENT, 1);
                event_queue->push(switch_event);
                sequence_count = 0;
                
                // Wait for resume signal
                PT_WAIT_EVENT_TYPE(this, event, PATTERN_SWITCH_EVENT);
                if (event.data == 0) {
                    printf("Resuming fast pattern\n");
                }
            }
        }
        
        PT_END(this);
    }
};
```

---

## Performance Considerations

### Memory Usage

| Component | Simple Threads | Protothreads |
|-----------|---------------|--------------|
| Base thread object | ~40 bytes | ~120 bytes |
| Scheduler | ~100 bytes | ~500 bytes |
| Event system | N/A | ~200 bytes |
| Per-thread state | Varies | ~50 bytes |

### CPU Overhead

- **Simple Threads**: ~5-10 CPU cycles per thread per scheduler run
- **Protothreads**: ~20-50 CPU cycles per thread per scheduler run (includes state management)

### Real-time Performance

- **Simple Threads**: More predictable timing, lower jitter
- **Protothreads**: Slightly higher jitter due to state management, but still suitable for audio applications

---

## Migration Guide

### From Simple Threads to Protothreads

1. **Change base class**:
   ```cpp
   // Before
   class MyThread : public SimpleThread
   
   // After  
   class MyThread : public PTThread
   ```

2. **Replace execute() with run()**:
   ```cpp
   // Before
   void execute() override { /* code */ }
   
   // After
   int run() override {
       PT_BEGIN(this);
       /* code */
       PT_END(this);
   }
   ```

3. **Replace manual state machines with PT_WAIT_UNTIL**:
   ```cpp
   // Before (state machine)
   if (state == WAITING && condition) {
       state = NEXT_STATE;
   }
   
   // After (protothread)
   PT_WAIT_UNTIL(this, condition);
   ```

4. **Replace global variables with events**:
   ```cpp
   // Before
   volatile bool flag = false;
   
   // After
   PTEvent event(MY_EVENT_TYPE, data);
   event_queue->push(event);
   ```

---

## Best Practices

### For Simple Threads
- Keep `execute()` methods short and fast
- Use member variables to track state between calls
- Set appropriate intervals to balance responsiveness and CPU usage
- Avoid blocking operations entirely

### For Protothreads
- Always use `PT_BEGIN` and `PT_END` macros
- Don't use local variables across `PT_WAIT_UNTIL` calls (use static or member variables)
- Handle events promptly to avoid queue overflow
- Use meaningful event types and data fields

---

## Conclusion

Both libraries have their place in embedded development:

- **Choose Simple Threads** for straightforward, periodic tasks where you need maximum efficiency and don't require complex coordination between threads.

- **Choose Protothreads** for sophisticated applications that need event-driven programming, complex state management, or extensive inter-thread communication.

For most Eurorack module development, the full protothreads implementation (`framework/pt_thread.h`) is recommended as it provides the flexibility and features needed for professional-quality audio applications.

---

## Further Reading

- [Protothreads Library Documentation](http://dunkels.com/adam/pt/)
- [Cooperative Multitasking in Embedded Systems](https://www.embedded.com/cooperative-multitasking-for-embedded-systems/)
- [Raspberry Pi Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
