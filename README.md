# Eurorack Multi-Threading Framework

A comprehensive C++ framework for developing Eurorack modules using the Raspberry Pi Pico SDK, featuring both simplified cooperative threading and full protothreads implementation with hardware abstraction.

## Overview

This framework provides multiple approaches for Eurorack module development:

### 🚀 **SimpleThreads System** (Recommended for beginners)
- **Easy-to-use cooperative threading**: Simple interval-based execution
- **C++ Object-Oriented**: Clean class-based interface
- **Thread management**: Enable/disable, naming, scheduling
- **Perfect for**: Basic sequencers, LFOs, simple UI handling

### ⚡ **Full Protothreads Framework** (Advanced)
- **True protothreads integration**: Non-preemptive cooperative threading
- **Event-driven programming**: Interrupt-based event system
- **Hardware abstraction**: Complete encoder, button, CV/Gate I/O classes
- **Perfect for**: Complex modules with real-time requirements

### 🎛️ **Comprehensive Eurorack Hardware Support**
- **CV Input/Output**: 12-bit ADC with Eurorack voltage scaling
- **Gate/Trigger I/O**: Interrupt-driven edge detection
- **Encoder Support**: Quadrature encoders with button integration
- **Button Interfaces**: Debounced input with press/release events
- **LED Control**: Status indicators and pattern generation
- **Event System**: Inter-thread communication and synchronization

## Framework Architecture

```
framework/
├── simple_threads.h       # SimpleThread cooperative system (easy)
├── pt_thread.h           # Full protothreads C++ wrapper (advanced)
├── eurorack_hardware.h   # Hardware abstraction classes
├── eurorack_utils.h      # Utility functions and math
└── protothreads/
    ├── pt.h              # Core protothreads library  
    ├── pt-sem.h          # Semaphores and mutexes
    └── lc.h              # Local continuations
```

## Example Projects

1. **`pt-test-working.cpp`** - SimpleThreads demo with LED patterns
2. **`pt-test-full.cpp`** - Full protothreads with event system  
3. **`pt-test-eurorack.cpp`** - Complete Eurorack module example

## Quick Start Guide

### Option 1: SimpleThreads (Beginner-Friendly)

```cpp
#include "framework/simple_threads.h"
#include "framework/eurorack_utils.h"

class MyEurorackThread : public SimpleThread {
public:
    MyEurorackThread() : SimpleThread("MyModule") {
        setInterval(100); // Execute every 100ms
    }
    
    void execute() override {
        // Your module logic here
        EurorackUtils::LED::toggle();
        
        // Read CV input
        float cv = EurorackUtils::CV::readEurorackVoltage(0);
        printf("CV Input: %.2fV\n", cv);
    }
};

int main() {
    EurorackUtils::init();
    
    MyEurorackThread my_thread;
    SimpleScheduler scheduler;
    scheduler.addThread(&my_thread);
    
    while (true) {
        scheduler.run();
        tight_loop_contents();
    }
}
```

### Option 2: Full Protothreads (Advanced)

```cpp
#include "framework/pt_thread.h"
#include "framework/eurorack_hardware.h"

class AdvancedEurorackThread : public PTThread {
private:
    PTEvent event;
    PTEncoder encoder;
    PTCVInput cv_input;
    
public:
    AdvancedEurorackThread() : PTThread("Advanced") {
        encoder.init();
        cv_input.init();
    }
    
    int run() override {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            // Wait for encoder events
            PT_WAIT_EVENT_TYPE(this, event, PTEventType::ENCODER_TURN);
            
            // Process encoder changes
            int32_t position = encoder.getPosition();
            printf("Encoder: %ld\n", position);
            
            PT_THREAD_YIELD(this);
        }
        
        PT_THREAD_END(this);
    }
};
```

## Hardware Abstraction Classes

### Encoder Support
```cpp
PTEncoder encoder(PIN_A, PIN_B, PIN_BUTTON);
encoder.init();
encoder.setEventQueue(&event_queue);

// In thread:
int32_t position = encoder.getPosition();
bool button_pressed = encoder.getButtonState();
```

### Button Interface
```cpp
PTButton button(PIN_BUTTON);
button.init();
button.setEventQueue(&event_queue);

// Events automatically generated on press/release
```

### CV Input/Output
```cpp
PTCVInput cv_in(26);  // ADC0
PTCVOutput cv_out(20); // PWM pin

cv_in.init();
cv_out.init();

// Read/write Eurorack voltages (-5V to +5V)
float voltage = cv_in.readEurorackVoltage();
cv_out.setEurorackVoltage(voltage);
```

### Gate I/O
```cpp
PTGateInput gate_in(7);
PTGateOutput gate_out(8);

gate_in.init();
gate_out.init();

// Event-driven gate detection
gate_out.trigger(1000); // 1ms trigger
```

## Framework Features

### SimpleThreads System
- ✅ **Interval-based execution**: Set execution intervals in milliseconds
- ✅ **Thread naming**: Named threads for debugging and identification  
- ✅ **Enable/disable control**: Dynamic thread activation
- ✅ **Thread counting**: Scheduler reports active thread count
- ✅ **Cooperative scheduling**: Non-blocking execution model
- ✅ **Easy C++ interface**: Virtual `execute()` method override

### Protothreads Framework  
- ⚡ **True protothreads**: Full Adam Dunkels implementation
- 🔄 **Event-driven**: Interrupt-based event queue system
- 🎛️ **Hardware integration**: Direct encoder, button, CV/Gate classes
- 📡 **Inter-thread communication**: Event passing between threads
- ⏱️ **Precise timing**: Microsecond-level timing control
- 🔒 **Synchronization**: Semaphores and mutexes available

### Eurorack-Specific Features
- 🎚️ **CV Processing**: -5V to +5V voltage range scaling
- ⚡ **Gate/Trigger**: Rising/falling edge detection
- 🔘 **Rotary Encoders**: Quadrature encoding with button support
- 🔲 **Button Debouncing**: Hardware debounced button inputs
- 💡 **LED Control**: Status indicators and pattern generation
- 🔢 **Math Utilities**: Mapping, clamping, interpolation functions

## Event System

The framework includes a comprehensive event system for real-time interaction:

```cpp
enum class PTEventType {
    ENCODER_TURN,      // Encoder position changed
    BUTTON_PRESS,      // Button pressed down
    BUTTON_RELEASE,    // Button released
    GATE_RISING,       // Gate input rising edge
    GATE_FALLING,      // Gate input falling edge  
    TIMER_TICK,        // Timer/tempo events
    ADC_READY,         // CV input ready
    SEQUENCE_STEP,     // Sequencer step advance
    CV_CHANGE,         // Significant CV change detected
    USER_EVENT         // Custom application events
};

// Usage in threads:
PT_WAIT_EVENT_TYPE(this, event, PTEventType::BUTTON_PRESS);
```

## Utility Functions

### CV Voltage Conversion
```cpp
// Convert between ADC values and Eurorack voltages
float voltage = EurorackUtils::CV::adcToEurorackVoltage(adc_value);
uint16_t dac_value = EurorackUtils::CV::eurorackVoltageToDAC(voltage);

// Direct hardware reading
float cv = EurorackUtils::CV::readEurorackVoltage(0); // ADC channel 0
```

### Mathematical Functions
```cpp
// Constrain values to range
float constrained = EurorackUtils::Math::clamp(value, -5.0f, 5.0f);

// Map ranges
float mapped = EurorackUtils::Math::map(input, 0, 4095, -5.0f, 5.0f);

// Interpolation
float lerped = EurorackUtils::Math::lerp(a, b, t);
```

### Timing and Delays
```cpp
// Non-blocking delays in threads
PT_THREAD_WAIT_UNTIL(this, 
    EurorackUtils::Timing::getMillis() - start_time > 1000);

// Current system time
uint32_t now = EurorackUtils::Timing::getMicros();
```

## Building and Configuration

### Prerequisites
- Raspberry Pi Pico SDK 2.2.0 or later
- CMake 3.13 or later  
- ARM GCC toolchain
- Ninja build system

### Build Instructions

1. **Configure the build system:**
   ```bash
   mkdir build
   cd build
   cmake ..
   ```

2. **Build the project:**
   ```bash
   ninja
   ```

3. **Flash to Pico:**
   ```bash
   # Method 1: Direct flash via SWD/picoprobe
   ninja flash
   
   # Method 2: Copy .uf2 file to mounted Pico
   cp pt-test.uf2 /path/to/RPI-RP2/
   ```

### Switching Between Examples

Edit `CMakeLists.txt` to change the target source file:

```cmake
add_executable(pt-test
    pt-test-working.cpp     # SimpleThreads demo
    # pt-test-full.cpp      # Full protothreads demo  
    # pt-test-eurorack.cpp  # Complete Eurorack example
)
```

### Hardware Configuration

#### Pin Assignments (Default)
```cpp
// Encoders
#define ENCODER1_A_PIN    2
#define ENCODER1_B_PIN    3  
#define ENCODER1_BTN_PIN  4

// Buttons  
#define BUTTON1_PIN       5
#define BUTTON2_PIN       6

// LEDs
#define LED1_PIN          25  // Onboard LED
#define LED2_PIN          15  // External LEDs
#define LED3_PIN          16

// CV I/O
#define CV_IN1_PIN        26  // ADC0 
#define CV_IN2_PIN        27  // ADC1
#define CV_OUT1_PIN       20  // PWM
#define CV_OUT2_PIN       21  // PWM

// Gate I/O  
#define GATE_IN_PIN       7
#define GATE_OUT_PIN      8
```

#### Hardware Requirements
- **CV Inputs**: Require external conditioning circuits (op-amp based)
- **CV Outputs**: PWM-based, requires RC filtering and buffering
- **Gate I/O**: 3.3V logic levels, may need level shifting for ±12V
- **Encoders**: Standard quadrature encoders with optional switch
- **Buttons**: Active-low with internal pullup (configurable)

## Protothreads Macros Reference

### For Advanced Protothreads Users

#### Thread Control
```cpp
PT_THREAD_BEGIN(this);         // Start of thread function
PT_THREAD_END(this);           // End of thread function
PT_THREAD_YIELD(this);         // Yield to other threads
PT_THREAD_EXIT(this);          // Exit thread permanently
PT_THREAD_RESTART(this);       // Restart thread from beginning
```

#### Conditional Waiting
```cpp
PT_THREAD_WAIT_UNTIL(this, condition);  // Wait until condition true
PT_THREAD_WAIT_WHILE(this, condition);  // Wait while condition true
PT_YIELD_UNTIL(this, condition);        // Yield until condition
```

#### Event Handling
```cpp
PT_WAIT_EVENT(this, event);                    // Wait for any event
PT_WAIT_EVENT_TYPE(this, event, EVENT_TYPE);   // Wait for specific event
```

#### Semaphores (Advanced Synchronization)
```cpp
#include "protothreads/pt-sem.h"

struct pt_sem my_semaphore;
PT_SEM_INIT(&my_semaphore, 1);    // Initialize with count = 1
PT_SEM_WAIT(pt, &my_semaphore);   // Wait for semaphore
PT_SEM_SIGNAL(pt, &my_semaphore); // Signal semaphore
```

## Example Applications

### 1. Simple LED Sequencer (SimpleThreads)
```cpp
class SequencerThread : public SimpleThread {
private:
    uint8_t step = 0;
    uint8_t pattern[8] = {1,0,1,1,0,1,0,0};
    
public:
    SequencerThread() : SimpleThread("Sequencer") {
        setInterval(250); // 250ms per step
    }
    
    void execute() override {
        EurorackUtils::LED::write(pattern[step]);
        step = (step + 1) % 8;
    }
};
```

### 2. CV-Controlled Oscillator (Protothreads)
```cpp
class CVOscillatorThread : public PTThread {
private:
    PTCVInput cv_input;
    PTCVOutput cv_output;
    float frequency = 1.0f;
    float phase = 0.0f;
    
public:
    int run() override {
        PT_THREAD_BEGIN(this);
        
        cv_input.init(26);  // ADC0 for frequency CV
        cv_output.init(20); // PWM for audio output
        
        while (true) {
            // Read frequency control voltage
            float cv = cv_input.readEurorackVoltage();
            frequency = 440.0f * powf(2.0f, cv); // 1V/octave
            
            // Generate sawtooth wave
            phase += frequency * 10.0f / 1000000.0f; // 10us timing
            if (phase >= 1.0f) phase -= 1.0f;
            
            // Output wave (-5V to +5V range)
            cv_output.setEurorackVoltage((phase * 2.0f) - 1.0f);
            
            PT_THREAD_WAIT_UNTIL(this, timer_elapsed_us(10));
        }
        
        PT_THREAD_END(this);
    }
};
```

### 3. Interactive Parameter Control
```cpp
class UIThread : public PTThread {
private:
    PTEncoder encoder;
    PTButton button;
    PTEvent event;
    float parameter_value = 0.0f;
    
public:
    int run() override {
        PT_THREAD_BEGIN(this);
        
        encoder.init(2, 3, 4);  // A, B, Button pins
        button.init(5);
        encoder.setEventQueue(event_queue);
        button.setEventQueue(event_queue);
        
        while (true) {
            PT_WAIT_EVENT(this, event);
            
            switch (event.type) {
                case PTEventType::ENCODER_TURN:
                    parameter_value += (event.data > 0) ? 0.1f : -0.1f;
                    parameter_value = clamp(parameter_value, 0.0f, 5.0f);
                    printf("Parameter: %.2fV\n", parameter_value);
                    break;
                    
                case PTEventType::BUTTON_PRESS:
                    parameter_value = 2.5f; // Reset to center
                    printf("Parameter reset\n");
                    break;
            }
        }
        
        PT_THREAD_END(this);
    }
};
```

## Advanced Topics

### Custom Event Types
```cpp
// Extend the event system with your own events
enum class MyEventType : uint32_t {
    CUSTOM_EVENT_1 = static_cast<uint32_t>(PTEventType::USER_EVENT) + 1,
    CUSTOM_EVENT_2,
    TEMPO_SYNC,
    MIDI_NOTE_ON
};

// Generate custom events
PTEvent custom_event(static_cast<PTEventType>(MyEventType::TEMPO_SYNC), bpm);
event_queue->push(custom_event);
```

### Multi-Core Usage
```cpp
// Use core1 for audio processing
void core1_entry() {
    // High-priority audio thread
    AudioProcessingThread audio_thread;
    
    while (true) {
        audio_thread.execute();
        tight_loop_contents();
    }
}

int main() {
    // Start core1 for audio
    multicore_launch_core1(core1_entry);
    
    // Core0 handles UI and control
    UIThread ui_thread;
    SequencerThread seq_thread;
    
    PTScheduler scheduler;
    scheduler.addThread(&ui_thread);  
    scheduler.addThread(&seq_thread);
    
    while (true) {
        scheduler.run();
    }
}
```

### Performance Tips

1. **Thread Timing**: Use appropriate intervals for your needs
   - UI updates: 50-100ms intervals  
   - Audio processing: 10-100μs intervals
   - LED updates: 10-50ms intervals

2. **Memory Usage**: SimpleThreads use less RAM than full protothreads

3. **Interrupt Priority**: Gate/trigger inputs use high-priority interrupts

4. **CPU Usage**: Yield frequently in tight loops to maintain responsiveness

## Troubleshooting

### Common Issues

**Build Errors:**
- Ensure Pico SDK path is correctly set
- Check that all header files are included
- Verify CMakeLists.txt target file exists

**Runtime Issues:**  
- Check hardware pin assignments
- Verify voltage levels for CV I/O
- Add debug printf() statements for troubleshooting

**Thread Performance:**
- Reduce thread intervals if system becomes unresponsive
- Use PT_THREAD_YIELD() in long-running loops
- Monitor stack usage with debug builds

### Debug Output
```cpp
// Enable detailed debug output
#define DEBUG_THREADS 1

#ifdef DEBUG_THREADS
    printf("[%s] Thread executing at %lu\n", getName(), time_us_32());
#endif
```
