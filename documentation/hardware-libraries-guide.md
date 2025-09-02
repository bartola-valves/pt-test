# Eurorack Hardware Libraries Documentation
*Complete Guide to Hardware Abstraction for Eurorack Module Development*

## Table of Contents
1. [Introduction](#introduction)
2. [Library Overview](#library-overview)
3. [EurorackUtils Library](#eurorackutils-library)
4. [EurorackHardware Library](#eurorack-hardware-library)
5. [Integration with Threading Libraries](#integration-with-threading-libraries)
6. [Complete Examples](#complete-examples)
7. [Hardware Configuration Guide](#hardware-configuration-guide)
8. [Best Practices](#best-practices)
9. [Troubleshooting](#troubleshooting)

---

## Introduction

The Eurorack Hardware Libraries provide comprehensive hardware abstraction for building synthesizer modules on the Raspberry Pi Pico. These libraries bridge the gap between low-level Pico SDK functions and high-level Eurorack module development, supporting both Simple Threads and Protothreads programming models.

### Library Architecture

```
┌─────────────────────────────────────────────┐
│              Your Module Code               │
├─────────────────────────────────────────────┤
│    Simple Threads    │    Protothreads      │
├─────────────────────────────────────────────┤
│         EurorackHardware.h                  │  ← Event-driven hardware
├─────────────────────────────────────────────┤
│          EurorackUtils.h                    │  ← Basic utilities
├─────────────────────────────────────────────┤
│            Raspberry Pi Pico SDK            │
└─────────────────────────────────────────────┘
```

### Key Features

- **Hardware Abstraction**: High-level interfaces for common Eurorack components
- **Event Integration**: Interrupt-driven programming with automatic event generation
- **Thread Compatibility**: Works seamlessly with both threading libraries
- **Real-time Performance**: Optimized for audio-rate and control-rate processing
- **Comprehensive Coverage**: CV, gates, encoders, buttons, outputs

---

## Library Overview

### EurorackUtils.h - Basic Utilities
- **Purpose**: Fundamental utilities for Eurorack development
- **Approach**: Simple, direct hardware access
- **Best For**: Basic functionality, Simple Threads integration
- **Thread Model**: Timer-based polling

### EurorackHardware.h - Advanced Hardware Classes
- **Purpose**: Object-oriented hardware interfaces with event support
- **Approach**: Interrupt-driven, event-based programming
- **Best For**: Complex interactions, Protothreads integration
- **Thread Model**: Event-driven programming

---

## EurorackUtils Library

### Initialization

```cpp
#include "framework/eurorack_utils.h"

// Initialize the system
EurorackUtils::init();
```

### LED Control

#### Basic LED Operations
```cpp
// Simple LED control
EurorackUtils::LED::on();        // Turn LED on
EurorackUtils::LED::off();       // Turn LED off
EurorackUtils::LED::toggle();    // Toggle LED state
bool state = EurorackUtils::LED::getState(); // Get current state
```

#### With Simple Threads
```cpp
class BlinkThread : public SimpleThread
{
private:
    bool led_state;
    
public:
    BlinkThread() : led_state(false)
    {
        setInterval(500); // 500ms
    }
    
    void execute() override
    {
        EurorackUtils::LED::toggle();
        printf("LED: %s\n", EurorackUtils::LED::getState() ? "ON" : "OFF");
    }
};
```

#### With Protothreads
```cpp
class PTBlinkThread : public PTThread
{
private:
    uint32_t timer_start;
    
public:
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            EurorackUtils::LED::on();
            timer_start = time_us_32();
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 500000);
            
            EurorackUtils::LED::off();
            timer_start = time_us_32();
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - timer_start) >= 500000);
        }
        
        PT_THREAD_END(this);
    }
};
```

### CV (Control Voltage) Operations

#### Setup and Basic Reading
```cpp
// Initialize CV input on GPIO 26 (ADC0)
EurorackUtils::CV::initInput(26);

// Read raw ADC value (0-4095)
uint16_t raw = EurorackUtils::CV::readRaw(0);

// Read as 0-3.3V
float voltage = EurorackUtils::CV::readVoltage(0);

// Read as Eurorack voltage (-5V to +5V)
float cv_voltage = EurorackUtils::CV::readEurorackVoltage(0);
```

#### Conversion Functions
```cpp
// Convert between different representations
uint16_t adc_value = 2048;
float eurorack_v = EurorackUtils::CV::adcToEurorackVoltage(adc_value);

float input_voltage = 2.5f;
uint16_t dac_value = EurorackUtils::CV::eurorackVoltageToDAC(input_voltage);
```

#### CV Monitoring with Simple Threads
```cpp
class CVMonitorThread : public SimpleThread
{
private:
    float last_cv_value;
    float cv_threshold;
    
public:
    CVMonitorThread() : last_cv_value(0.0f), cv_threshold(0.1f)
    {
        setInterval(10); // 10ms = 100Hz
        EurorackUtils::CV::initInput(26); // Initialize ADC0
    }
    
    void execute() override
    {
        float current_cv = EurorackUtils::CV::readEurorackVoltage(0);
        
        // Detect significant changes
        if (abs(current_cv - last_cv_value) > cv_threshold) {
            printf("CV changed: %.2fV (was %.2fV)\n", current_cv, last_cv_value);
            last_cv_value = current_cv;
            
            // Process CV change
            handleCVChange(current_cv);
        }
    }
    
private:
    void handleCVChange(float voltage)
    {
        // Convert CV to frequency (1V/octave)
        float frequency = 440.0f * pow(2.0f, voltage);
        printf("Frequency: %.1f Hz\n", frequency);
    }
};
```

### Gate Operations

#### Basic Gate I/O
```cpp
// Initialize gate pins
EurorackUtils::Gate::initInput(15);   // Gate input
EurorackUtils::Gate::initOutput(16);  // Gate output

// Read gate input
bool gate_state = EurorackUtils::Gate::read(15);

// Control gate output
EurorackUtils::Gate::write(16, true);   // Gate high
EurorackUtils::Gate::write(16, false);  // Gate low
EurorackUtils::Gate::toggle(16);        // Toggle gate
```

#### Gate Sequencer with Simple Threads
```cpp
class GateSequencerThread : public SimpleThread
{
private:
    bool pattern[16];
    int current_step;
    uint32_t bpm;
    
public:
    GateSequencerThread(uint32_t beats_per_minute) 
        : current_step(0), bpm(beats_per_minute)
    {
        // Set interval for 16th notes
        uint32_t interval = (60000) / (bpm * 4); // ms per 16th note
        setInterval(interval);
        
        // Initialize hardware
        EurorackUtils::Gate::initOutput(16);
        
        // Create a simple pattern
        for (int i = 0; i < 16; i++) {
            pattern[i] = (i % 4 == 0) || (i == 6) || (i == 14);
        }
    }
    
    void execute() override
    {
        // Play current step
        bool should_gate = pattern[current_step];
        EurorackUtils::Gate::write(16, should_gate);
        
        printf("Step %d: %s\n", current_step, should_gate ? "GATE" : "-");
        
        // Advance to next step
        current_step = (current_step + 1) % 16;
    }
    
    void setPattern(int step, bool active) {
        if (step >= 0 && step < 16) {
            pattern[step] = active;
        }
    }
};
```

### Timing Utilities

#### Time Functions
```cpp
uint64_t micros = EurorackUtils::Timing::getMicros();
uint32_t millis = EurorackUtils::Timing::getMillis();

// Non-blocking delay check
static uint32_t last_time = 0;
if (EurorackUtils::Timing::delayElapsed(last_time, 1000)) {
    printf("One second elapsed\n");
}
```

#### Precise Timing Thread
```cpp
class PrecisionTimingThread : public SimpleThread
{
private:
    uint32_t last_microsecond_time;
    uint32_t microsecond_counter;
    
public:
    PrecisionTimingThread()
    {
        setInterval(0); // Run every loop iteration
        last_microsecond_time = EurorackUtils::Timing::getMicros();
    }
    
    void execute() override
    {
        uint32_t current_time = EurorackUtils::Timing::getMicros();
        
        // Check for 1000µs (1ms) intervals
        if (current_time - last_microsecond_time >= 1000) {
            microsecond_counter++;
            last_microsecond_time = current_time;
            
            // Do precise 1kHz processing
            processPreciseTiming();
        }
    }
    
private:
    void processPreciseTiming()
    {
        // High-frequency processing (1kHz)
        if (microsecond_counter % 1000 == 0) {
            printf("Precision timing: %lu ms\n", microsecond_counter);
        }
    }
};
```

### Math Utilities

#### Value Mapping and Constraining
```cpp
// Map CV input (0-3.3V) to MIDI note (0-127)
float cv_voltage = EurorackUtils::CV::readVoltage(0);
int midi_note = EurorackUtils::Math::map(cv_voltage, 0.0f, 3.3f, 0.0f, 127.0f);

// Constrain frequency to audible range
float frequency = 1000.0f * some_factor;
frequency = EurorackUtils::Math::constrain(frequency, 20.0f, 20000.0f);

// Clamp integer values
int step = EurorackUtils::Math::clamp(calculated_step, 0, 15);
```

---

## EurorackHardware Library

The EurorackHardware library provides object-oriented, interrupt-driven hardware interfaces designed for event-based programming with Protothreads.

### PTEncoder - Rotary Encoder with Button

#### Basic Setup
```cpp
#include "framework/eurorack_hardware.h"

// Create encoder: pin A, pin B, button pin (optional)
PTEncoder encoder(10, 11, 12);

// In your thread or main loop
encoder.setEventQueue(scheduler.getEventQueue());
```

#### With Protothreads
```cpp
class EncoderControlThread : public PTThread
{
private:
    PTEncoder* encoder;
    int32_t last_position;
    
public:
    EncoderControlThread(PTEncoder* enc) : encoder(enc), last_position(0) {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        printf("Encoder thread: Waiting for events...\n");
        
        while (true) {
            PTEvent event;
            PT_WAIT_EVENT(this, event);
            
            switch (event.type) {
                case PTEventType::ENCODER_TURN:
                    {
                        int32_t new_position = encoder->getPosition();
                        int32_t delta = new_position - last_position;
                        last_position = new_position;
                        
                        printf("Encoder turned: position=%ld, delta=%ld\n", 
                               new_position, delta);
                        handleEncoderChange(delta);
                    }
                    break;
                    
                case PTEventType::BUTTON_PRESS:
                    printf("Encoder button pressed!\n");
                    handleEncoderPress();
                    break;
                    
                case PTEventType::BUTTON_RELEASE:
                    printf("Encoder button released\n");
                    break;
                    
                default:
                    // Ignore other events
                    break;
            }
        }
        
        PT_THREAD_END(this);
    }
    
private:
    void handleEncoderChange(int32_t delta)
    {
        // Adjust some parameter based on encoder movement
        static float parameter = 0.5f; // 0.0 to 1.0
        parameter += delta * 0.05f; // 5% per step
        parameter = EurorackUtils::Math::constrain(parameter, 0.0f, 1.0f);
        
        printf("Parameter: %.2f\n", parameter);
    }
    
    void handleEncoderPress()
    {
        // Reset parameter or change mode
        encoder->setPosition(0); // Reset encoder position
    }
};
```

#### With Simple Threads (Polling Mode)
```cpp
class SimpleEncoderThread : public SimpleThread
{
private:
    PTEncoder* encoder;
    int32_t last_position;
    bool last_button_state;
    
public:
    SimpleEncoderThread(PTEncoder* enc) 
        : encoder(enc), last_position(0), last_button_state(false)
    {
        setInterval(10); // Poll every 10ms
    }
    
    void execute() override
    {
        // Check encoder position
        int32_t current_position = encoder->getPosition();
        if (current_position != last_position) {
            int32_t delta = current_position - last_position;
            last_position = current_position;
            printf("Encoder: %ld (delta: %ld)\n", current_position, delta);
        }
        
        // Check button state
        bool current_button = encoder->getButtonState();
        if (current_button != last_button_state) {
            last_button_state = current_button;
            printf("Button: %s\n", current_button ? "PRESSED" : "RELEASED");
        }
    }
};
```

### PTButton - Debounced Button Input

#### Basic Setup
```cpp
// Create button: pin, active_low (default true), debounce_time_us
PTButton button(14, true, 50000); // 50ms debounce

button.setEventQueue(scheduler.getEventQueue());
```

#### Multi-Button Interface
```cpp
class ButtonInterfaceThread : public PTThread
{
private:
    PTButton* buttons[4];
    const char* button_names[4] = {"PLAY", "STOP", "REC", "MODE"};
    
public:
    ButtonInterfaceThread(PTButton* btn_array[4])
    {
        for (int i = 0; i < 4; i++) {
            buttons[i] = btn_array[i];
        }
    }
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            PTEvent event;
            PT_WAIT_EVENT(this, event);
            
            if (event.type == PTEventType::BUTTON_PRESS) {
                uint8_t button_id = event.data;
                if (button_id < 4) {
                    printf("%s button pressed\n", button_names[button_id]);
                    handleButtonPress(button_id);
                }
            } else if (event.type == PTEventType::BUTTON_RELEASE) {
                uint8_t button_id = event.data;
                if (button_id < 4) {
                    uint32_t press_duration = buttons[button_id]->getPressTime();
                    printf("%s button released (held %lu ms)\n", 
                           button_names[button_id], 
                           (time_us_32() - press_duration) / 1000);
                }
            }
        }
        
        PT_THREAD_END(this);
    }
    
private:
    void handleButtonPress(uint8_t button_id)
    {
        switch (button_id) {
            case 0: // PLAY
                startPlayback();
                break;
            case 1: // STOP
                stopPlayback();
                break;
            case 2: // REC
                toggleRecording();
                break;
            case 3: // MODE
                switchMode();
                break;
        }
    }
    
    void startPlayback() { printf("Starting playback...\n"); }
    void stopPlayback() { printf("Stopping playback...\n"); }
    void toggleRecording() { printf("Toggle recording...\n"); }
    void switchMode() { printf("Switching mode...\n"); }
};
```

### PTGateInput - Gate/Trigger Input

#### Basic Setup
```cpp
// Create gate input: pin, active_high (default true)
PTGateInput gate_in(15, true);
gate_in.setEventQueue(scheduler.getEventQueue());
```

#### Trigger Processor Thread
```cpp
class TriggerProcessorThread : public PTThread
{
private:
    PTGateInput* gate_input;
    uint32_t trigger_count;
    
public:
    TriggerProcessorThread(PTGateInput* gate) 
        : gate_input(gate), trigger_count(0) {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        printf("Trigger processor: Ready for gates\n");
        
        while (true) {
            PTEvent event;
            PT_WAIT_EVENT(this, event);
            
            switch (event.type) {
                case PTEventType::GATE_RISING:
                    {
                        trigger_count++;
                        uint32_t timestamp = gate_input->getLastEdgeTime();
                        printf("Trigger #%lu at %lu µs\n", trigger_count, timestamp);
                        
                        // Process trigger
                        processTrigger();
                    }
                    break;
                    
                case PTEventType::GATE_FALLING:
                    {
                        uint32_t gate_duration = gate_input->getGateDuration();
                        printf("Gate ended, duration: %lu µs\n", gate_duration);
                        
                        // Classify as trigger (<10ms) or gate (>10ms)
                        if (gate_duration < 10000) {
                            printf("Classification: TRIGGER\n");
                        } else {
                            printf("Classification: GATE\n");
                        }
                    }
                    break;
                    
                default:
                    break;
            }
        }
        
        PT_THREAD_END(this);
    }
    
private:
    void processTrigger()
    {
        // Example: advance sequencer, trigger envelope, etc.
        EurorackUtils::LED::toggle(); // Visual feedback
    }
};
```

### PTCVInput - ADC-Based CV Input

#### Basic Setup
```cpp
// Create CV input: ADC pin (26-29), change threshold (default 50)
PTCVInput cv_input(26, 100); // GPIO26 (ADC0), threshold = 100 ADC steps
cv_input.setEventQueue(scheduler.getEventQueue());
```

#### CV Monitor Thread
```cpp
class CVAnalyzerThread : public PTThread
{
private:
    PTCVInput* cv_inputs[3]; // 3 CV inputs
    const char* cv_names[3] = {"PITCH", "FILTER", "AMP"};
    
public:
    CVAnalyzerThread(PTCVInput* cv_array[3])
    {
        for (int i = 0; i < 3; i++) {
            cv_inputs[i] = cv_array[i];
        }
    }
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        // Polling mode for CV inputs (they don't auto-generate events)
        static uint32_t last_poll_time = 0;
        
        while (true) {
            uint32_t current_time = time_us_32();
            
            // Poll CV inputs every 5ms (200Hz)
            if (current_time - last_poll_time >= 5000) {
                last_poll_time = current_time;
                
                for (int i = 0; i < 3; i++) {
                    cv_inputs[i]->update(); // Check for changes
                }
            }
            
            // Wait for CV change events
            PTEvent event;
            PT_WAIT_EVENT(this, event);
            
            if (event.type == PTEventType::CV_CHANGE) {
                uint8_t adc_channel = event.data;
                if (adc_channel < 3) {
                    float voltage = cv_inputs[adc_channel]->getVoltage();
                    uint16_t raw = cv_inputs[adc_channel]->getValue();
                    
                    printf("%s CV: %.3fV (raw: %u)\n", 
                           cv_names[adc_channel], voltage, raw);
                           
                    processCVChange(adc_channel, voltage);
                }
            }
        }
        
        PT_THREAD_END(this);
    }
    
private:
    void processCVChange(uint8_t channel, float voltage)
    {
        switch (channel) {
            case 0: // PITCH
                {
                    float frequency = 440.0f * pow(2.0f, voltage);
                    printf("  → Frequency: %.1f Hz\n", frequency);
                }
                break;
                
            case 1: // FILTER
                {
                    float cutoff = EurorackUtils::Math::map(voltage, -5.0f, 5.0f, 100.0f, 10000.0f);
                    printf("  → Filter cutoff: %.0f Hz\n", cutoff);
                }
                break;
                
            case 2: // AMP
                {
                    float amplitude = EurorackUtils::Math::map(voltage, 0.0f, 5.0f, 0.0f, 1.0f);
                    amplitude = EurorackUtils::Math::constrain(amplitude, 0.0f, 1.0f);
                    printf("  → Amplitude: %.2f\n", amplitude);
                }
                break;
        }
    }
};
```

### PTCVOutput and PTGateOutput

#### CV and Gate Output Setup
```cpp
// Create outputs
PTCVOutput cv_out(2);    // GPIO2 for CV output (PWM)
PTGateOutput gate_out(3, true, 10000); // GPIO3, active high, 10ms duration

// Set CV voltage (-5V to +5V range)
cv_out.setVoltage(2.5f);  // 2.5V output

// Trigger gate
gate_out.trigger();  // 10ms pulse

// Manual gate control
gate_out.setHigh();  // Gate on
gate_out.setLow();   // Gate off
```

#### Envelope Generator Thread
```cpp
class EnvelopeGeneratorThread : public PTThread
{
private:
    PTCVOutput* cv_output;
    PTGateOutput* gate_output;
    
    enum Phase { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    Phase current_phase;
    uint32_t phase_start_time;
    float current_level;
    
    // ADSR parameters (in milliseconds)
    uint32_t attack_time = 50;   // 50ms
    uint32_t decay_time = 100;   // 100ms
    float sustain_level = 0.6f;  // 60%
    uint32_t release_time = 150; // 150ms
    
public:
    EnvelopeGeneratorThread(PTCVOutput* cv_out, PTGateOutput* gate_out)
        : cv_output(cv_out), gate_output(gate_out), 
          current_phase(IDLE), current_level(0.0f) {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            switch (current_phase) {
                case IDLE:
                    {
                        // Wait for trigger
                        PTEvent trigger;
                        PT_WAIT_EVENT_TYPE(this, trigger, PTEventType::GATE_RISING);
                        
                        printf("Envelope: Triggered!\n");
                        current_phase = ATTACK;
                        phase_start_time = time_us_32();
                        gate_output->setHigh(); // Start gate
                    }
                    break;
                    
                case ATTACK:
                    {
                        uint32_t elapsed = time_us_32() - phase_start_time;
                        float progress = (float)elapsed / (attack_time * 1000);
                        
                        if (progress >= 1.0f) {
                            // Attack complete
                            current_level = 1.0f;
                            current_phase = DECAY;
                            phase_start_time = time_us_32();
                            printf("Envelope: Attack → Decay\n");
                        } else {
                            // Linear attack
                            current_level = progress;
                        }
                        
                        cv_output->setVoltage(current_level * 5.0f); // 0-5V
                        
                        // Small delay for smooth envelope
                        uint32_t delay_start = time_us_32();
                        PT_THREAD_WAIT_UNTIL(this, (time_us_32() - delay_start) >= 1000); // 1ms
                    }
                    break;
                    
                case DECAY:
                    {
                        uint32_t elapsed = time_us_32() - phase_start_time;
                        float progress = (float)elapsed / (decay_time * 1000);
                        
                        if (progress >= 1.0f) {
                            // Decay complete
                            current_level = sustain_level;
                            current_phase = SUSTAIN;
                            phase_start_time = time_us_32();
                            printf("Envelope: Decay → Sustain\n");
                        } else {
                            // Linear decay
                            current_level = 1.0f - progress * (1.0f - sustain_level);
                        }
                        
                        cv_output->setVoltage(current_level * 5.0f);
                        
                        uint32_t delay_start = time_us_32();
                        PT_THREAD_WAIT_UNTIL(this, (time_us_32() - delay_start) >= 1000);
                    }
                    break;
                    
                case SUSTAIN:
                    {
                        // Wait for gate release or timeout
                        uint32_t sustain_start = time_us_32();
                        
                        PT_THREAD_WAIT_UNTIL(this, 
                            (time_us_32() - sustain_start) >= 500000); // 500ms max sustain
                        
                        current_phase = RELEASE;
                        phase_start_time = time_us_32();
                        gate_output->setLow(); // End gate
                        printf("Envelope: Sustain → Release\n");
                    }
                    break;
                    
                case RELEASE:
                    {
                        uint32_t elapsed = time_us_32() - phase_start_time;
                        float progress = (float)elapsed / (release_time * 1000);
                        
                        if (progress >= 1.0f) {
                            // Release complete
                            current_level = 0.0f;
                            current_phase = IDLE;
                            printf("Envelope: Release → Idle\n");
                        } else {
                            // Linear release
                            current_level = sustain_level * (1.0f - progress);
                        }
                        
                        cv_output->setVoltage(current_level * 5.0f);
                        
                        uint32_t delay_start = time_us_32();
                        PT_THREAD_WAIT_UNTIL(this, (time_us_32() - delay_start) >= 1000);
                    }
                    break;
            }
        }
        
        PT_THREAD_END(this);
    }
    
    // Parameter setters
    void setAttack(uint32_t ms) { attack_time = ms; }
    void setDecay(uint32_t ms) { decay_time = ms; }
    void setSustain(float level) { sustain_level = EurorackUtils::Math::constrain(level, 0.0f, 1.0f); }
    void setRelease(uint32_t ms) { release_time = ms; }
};
```

---

## Integration with Threading Libraries

### Choosing the Right Approach

| Use Case | Simple Threads | Protothreads | Library Choice |
|----------|----------------|-------------|----------------|
| **Basic LED patterns** | ✅ EurorackUtils | ✅ EurorackUtils | Either |
| **CV monitoring** | ✅ EurorackUtils polling | ✅ EurorackHardware events | Protothreads preferred |
| **Button interfaces** | ✅ EurorackUtils polling | ✅ EurorackHardware events | Protothreads preferred |
| **Complex sequencing** | ⚠️ Becomes complex | ✅ EurorackHardware events | Protothreads recommended |
| **Real-time response** | ❌ Timer limited | ✅ EurorackHardware interrupts | Protothreads only |

### Hybrid Approach Example

You can use both libraries in the same project:

```cpp
#include "framework/simple_threads.h"
#include "framework/pt_thread.h"
#include "framework/eurorack_utils.h"
#include "framework/eurorack_hardware.h"

// Simple timer-based tasks use Simple Threads + EurorackUtils
class StatusDisplayThread : public SimpleThread
{
public:
    StatusDisplayThread() { setInterval(5000); } // 5 seconds
    
    void execute() override
    {
        printf("Status: LED=%s, Uptime=%lu ms\n", 
               EurorackUtils::LED::getState() ? "ON" : "OFF",
               EurorackUtils::Timing::getMillis());
    }
};

// Event-driven tasks use Protothreads + EurorackHardware
class UserInterfaceThread : public PTThread
{
private:
    PTEncoder* encoder;
    PTButton* button;
    
public:
    UserInterfaceThread(PTEncoder* enc, PTButton* btn) 
        : encoder(enc), button(btn) {}
    
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            PTEvent event;
            PT_WAIT_EVENT(this, event);
            
            // Handle UI events immediately
            if (event.type == PTEventType::ENCODER_TURN) {
                handleEncoderTurn(encoder->getPosition());
            } else if (event.type == PTEventType::BUTTON_PRESS) {
                handleButtonPress();
            }
        }
        
        PT_THREAD_END(this);
    }
};

int main()
{
    EurorackUtils::init();
    
    // Create hardware objects
    PTEncoder encoder(10, 11, 12);
    PTButton button(14);
    
    // Create threads
    StatusDisplayThread status_thread;
    UserInterfaceThread ui_thread(&encoder, &button);
    
    // Create schedulers
    SimpleScheduler simple_scheduler;
    PTScheduler pt_scheduler;
    
    // Add threads
    simple_scheduler.addThread(&status_thread);
    pt_scheduler.addThread(&ui_thread);
    
    // Connect hardware to PT event system
    encoder.setEventQueue(pt_scheduler.getEventQueue());
    button.setEventQueue(pt_scheduler.getEventQueue());
    
    // Run both schedulers in main loop
    while (true) {
        simple_scheduler.run();     // Timer-based tasks
        pt_scheduler.runOnce();     // Event-driven tasks
        tight_loop_contents();
    }
}
```

---

## Complete Examples

### Example 1: Simple CV-to-MIDI Converter

```cpp
#include "framework/simple_threads.h"
#include "framework/eurorack_utils.h"

class CVToMidiThread : public SimpleThread
{
private:
    int last_midi_note;
    float cv_threshold;
    
public:
    CVToMidiThread() : last_midi_note(-1), cv_threshold(0.08f) // ~1 semitone
    {
        setInterval(50); // 20Hz - fast enough for CV tracking
        EurorackUtils::CV::initInput(26); // ADC0
    }
    
    void execute() override
    {
        float cv_voltage = EurorackUtils::CV::readEurorackVoltage(0);
        
        // Convert 1V/octave to MIDI note (C4 = 60 at 0V)
        int midi_note = 60 + (int)(cv_voltage * 12);
        midi_note = EurorackUtils::Math::clamp(midi_note, 0, 127);
        
        // Only send if note changed significantly
        if (abs(midi_note - last_midi_note) >= 1) {
            printf("CV: %.3fV → MIDI Note: %d\n", cv_voltage, midi_note);
            sendMidiNote(midi_note);
            last_midi_note = midi_note;
        }
    }
    
private:
    void sendMidiNote(int note)
    {
        // Implementation would send MIDI over UART/USB
        printf("MIDI: Note On %d\n", note);
    }
};

int main()
{
    EurorackUtils::init();
    
    CVToMidiThread cv_midi;
    SimpleScheduler scheduler;
    scheduler.addThread(&cv_midi);
    
    printf("CV-to-MIDI Converter Started\n");
    scheduler.run();
}
```

### Example 2: Advanced Eurorack Sequencer

```cpp
#include "framework/pt_thread.h"
#include "framework/eurorack_hardware.h"

class EurorackSequencerModule
{
private:
    PTEncoder encoder;
    PTButton play_button, stop_button, rec_button;
    PTGateInput clock_in;
    PTCVInput cv_input;
    PTCVOutput cv_output;
    PTGateOutput gate_output;
    
    PTScheduler scheduler;
    
    // Sequencer threads
    class ClockThread : public PTThread
    {
    private:
        EurorackSequencerModule* module;
        
    public:
        ClockThread(EurorackSequencerModule* mod) : module(mod) {}
        
        int run() override
        {
            PT_THREAD_BEGIN(this);
            
            while (true) {
                PTEvent event;
                PT_WAIT_EVENT_TYPE(this, event, PTEventType::GATE_RISING);
                
                printf("Clock received\n");
                // Advance sequencer
                module->advanceSequencer();
            }
            
            PT_THREAD_END(this);
        }
    };
    
    class UIThread : public PTThread
    {
    private:
        EurorackSequencerModule* module;
        
    public:
        UIThread(EurorackSequencerModule* mod) : module(mod) {}
        
        int run() override
        {
            PT_THREAD_BEGIN(this);
            
            while (true) {
                PTEvent event;
                PT_WAIT_EVENT(this, event);
                
                switch (event.type) {
                    case PTEventType::BUTTON_PRESS:
                        module->handleButton(event.data);
                        break;
                    case PTEventType::ENCODER_TURN:
                        module->handleEncoder();
                        break;
                }
            }
            
            PT_THREAD_END(this);
        }
    };
    
    ClockThread clock_thread;
    UIThread ui_thread;
    
    // Sequencer state
    float sequence[16];
    int current_step;
    int sequence_length;
    bool playing;
    bool recording;
    
public:
    EurorackSequencerModule() 
        : encoder(10, 11, 12),           // Encoder pins
          play_button(13), stop_button(14), rec_button(15),  // Button pins
          clock_in(16),                  // Clock input
          cv_input(26),                  // CV input (ADC0)
          cv_output(2),                  // CV output
          gate_output(3),                // Gate output
          clock_thread(this),
          ui_thread(this),
          current_step(0), sequence_length(16),
          playing(false), recording(false)
    {
        // Initialize sequence
        for (int i = 0; i < 16; i++) {
            sequence[i] = 0.0f;
        }
        
        // Connect hardware to event system
        encoder.setEventQueue(scheduler.getEventQueue());
        play_button.setEventQueue(scheduler.getEventQueue());
        stop_button.setEventQueue(scheduler.getEventQueue());
        rec_button.setEventQueue(scheduler.getEventQueue());
        clock_in.setEventQueue(scheduler.getEventQueue());
        
        // Add threads
        scheduler.addThread(&clock_thread);
        scheduler.addThread(&ui_thread);
    }
    
    void run()
    {
        printf("Eurorack Sequencer Module Started\n");
        scheduler.run();
    }
    
    void advanceSequencer()
    {
        if (!playing) return;
        
        // Output current step
        float cv_voltage = sequence[current_step];
        cv_output.setVoltage(cv_voltage);
        
        // Trigger gate if step has voltage
        if (cv_voltage != 0.0f) {
            gate_output.trigger();
            printf("Step %d: %.3fV\n", current_step, cv_voltage);
        } else {
            printf("Step %d: (silence)\n", current_step);
        }
        
        // Advance to next step
        current_step = (current_step + 1) % sequence_length;
    }
    
    void handleButton(uint8_t button_id)
    {
        switch (button_id) {
            case 0: // Play button
                playing = !playing;
                printf("Sequencer: %s\n", playing ? "PLAYING" : "STOPPED");
                break;
                
            case 1: // Stop button
                playing = false;
                current_step = 0;
                printf("Sequencer: STOPPED and RESET\n");
                break;
                
            case 2: // Record button
                recording = !recording;
                printf("Sequencer: %s\n", recording ? "RECORDING" : "PLAYBACK");
                break;
        }
    }
    
    void handleEncoder()
    {
        int32_t position = encoder.getPosition();
        
        if (recording) {
            // Record CV input to current step
            cv_input.update();
            float cv_voltage = cv_input.getVoltage();
            sequence[current_step] = cv_voltage;
            
            printf("Recorded step %d: %.3fV\n", current_step, cv_voltage);
        } else {
            // Navigate through sequence
            current_step = abs(position) % sequence_length;
            printf("Viewing step %d: %.3fV\n", current_step, sequence[current_step]);
        }
    }
};

int main()
{
    EurorackUtils::init();
    
    EurorackSequencerModule sequencer;
    sequencer.run();
    
    return 0;
}
```

---

## Hardware Configuration Guide

### Pin Assignments for Typical Eurorack Module

```cpp
// Analog Inputs (ADC)
#define CV_INPUT_1    26  // ADC0 - Primary CV input
#define CV_INPUT_2    27  // ADC1 - Secondary CV input
#define CV_INPUT_3    28  // ADC2 - Modulation input

// Digital Inputs
#define GATE_INPUT_1  16  // Gate/trigger input
#define GATE_INPUT_2  17  // Clock input
#define BUTTON_1      18  // Play/stop button
#define BUTTON_2      19  // Mode button
#define ENCODER_A     20  // Encoder channel A
#define ENCODER_B     21  // Encoder channel B
#define ENCODER_BTN   22  // Encoder button

// Digital Outputs  
#define GATE_OUTPUT_1  2  // Primary gate output
#define GATE_OUTPUT_2  3  // Secondary gate output
#define CV_OUTPUT_1    4  // PWM CV output
#define CV_OUTPUT_2    5  // PWM CV output
#define LED_STATUS    25  // Status LED (or use PICO_DEFAULT_LED_PIN)

// Hardware initialization function
void initializeHardware()
{
    EurorackUtils::init();
    
    // Initialize CV inputs
    EurorackUtils::CV::initInput(CV_INPUT_1);
    EurorackUtils::CV::initInput(CV_INPUT_2);
    EurorackUtils::CV::initInput(CV_INPUT_3);
    
    // Initialize gate I/O
    EurorackUtils::Gate::initInput(GATE_INPUT_1);
    EurorackUtils::Gate::initInput(GATE_INPUT_2);
    EurorackUtils::Gate::initOutput(GATE_OUTPUT_1);
    EurorackUtils::Gate::initOutput(GATE_OUTPUT_2);
    
    printf("Hardware initialized\n");
}
```

### Voltage Scaling and Protection

```cpp
// Example voltage scaling for different CV ranges
namespace ModuleCV
{
    // Scale 0-3.3V input to -5V to +5V
    inline float readBipolarCV(uint8_t adc_channel)
    {
        return EurorackUtils::CV::readEurorackVoltage(adc_channel);
    }
    
    // Scale 0-3.3V input to 0V to +10V (unipolar)
    inline float readUnipolarCV(uint8_t adc_channel)
    {
        float voltage_3v3 = EurorackUtils::CV::readVoltage(adc_channel);
        return voltage_3v3 * (10.0f / 3.3f); // Scale to 0-10V
    }
    
    // Convert to 1V/octave frequency
    inline float cvToFrequency(float cv_voltage, float base_freq = 440.0f)
    {
        return base_freq * pow(2.0f, cv_voltage);
    }
    
    // Quantize CV to semitones
    inline float quantizeToSemitones(float cv_voltage)
    {
        return round(cv_voltage * 12.0f) / 12.0f;
    }
}
```

---

## Best Practices

### 1. Thread Model Selection

**Use Simple Threads + EurorackUtils when:**
- Learning embedded programming
- Building simple, timer-based modules
- Prototyping basic functionality
- Not requiring real-time response

**Use Protothreads + EurorackHardware when:**
- Building production modules
- Needing immediate response to user input
- Implementing complex state machines
- Handling multiple concurrent events

### 2. Hardware Interface Design

```cpp
// ✅ Good - Encapsulate hardware in classes
class ModuleFrontPanel
{
private:
    PTEncoder frequency_encoder;
    PTEncoder filter_encoder;
    PTButton play_button;
    PTButton mode_button;
    PTCVInput pitch_cv;
    PTCVInput filter_cv;
    
public:
    ModuleFrontPanel() 
        : frequency_encoder(10, 11, 12),
          filter_encoder(13, 14, 15),
          play_button(16), mode_button(17),
          pitch_cv(26), filter_cv(27) {}
    
    void connectToScheduler(PTScheduler* scheduler) {
        frequency_encoder.setEventQueue(scheduler->getEventQueue());
        filter_encoder.setEventQueue(scheduler->getEventQueue());
        play_button.setEventQueue(scheduler->getEventQueue());
        mode_button.setEventQueue(scheduler->getEventQueue());
    }
};

// ❌ Bad - Global hardware objects scattered throughout code
extern PTEncoder g_encoder1;
extern PTButton g_button1;
// Makes code hard to maintain and debug
```

### 3. Error Handling and Validation

```cpp
class SafeCVInput
{
private:
    PTCVInput cv_input;
    float min_voltage, max_voltage;
    float last_valid_reading;
    
public:
    SafeCVInput(uint pin, float min_v = -5.0f, float max_v = 5.0f) 
        : cv_input(pin), min_voltage(min_v), max_voltage(max_v), last_valid_reading(0.0f) {}
    
    float readSafeVoltage()
    {
        cv_input.update();
        float voltage = cv_input.getVoltage();
        
        // Validate reading
        if (voltage >= min_voltage && voltage <= max_voltage) {
            last_valid_reading = voltage;
            return voltage;
        } else {
            printf("Warning: CV reading out of range: %.3fV\n", voltage);
            return last_valid_reading; // Return last valid reading
        }
    }
};
```

### 4. Performance Monitoring

```cpp
class PerformanceMonitor
{
private:
    uint32_t loop_count;
    uint32_t last_report_time;
    uint32_t max_loop_time;
    
public:
    void startLoop() {
        loop_start = time_us_32();
    }
    
    void endLoop() {
        uint32_t loop_time = time_us_32() - loop_start;
        if (loop_time > max_loop_time) {
            max_loop_time = loop_time;
        }
        
        loop_count++;
        
        if (time_us_32() - last_report_time >= 5000000) { // 5 seconds
            printf("Performance: %lu loops, max time: %lu µs\n", 
                   loop_count, max_loop_time);
            last_report_time = time_us_32();
            loop_count = 0;
            max_loop_time = 0;
        }
    }
    
private:
    uint32_t loop_start;
};
```

---

## Troubleshooting

### Common Issues

#### 1. Events Not Being Received

**Problem**: PTThread waiting for events that never arrive

**Debug Steps:**
```cpp
// Check event queue status
printf("Event queue size: %zu\n", scheduler.getEventQueue()->size());

// Verify hardware connection
printf("Encoder position: %ld\n", encoder.getPosition());
printf("Button state: %s\n", button.isPressed() ? "PRESSED" : "RELEASED");

// Test event generation manually
scheduler.postEvent(PTEventType::USER_EVENT, 123);
```

**Solutions:**
- Ensure `setEventQueue()` was called on hardware objects
- Check if interrupts are properly enabled
- Verify pin assignments match hardware

#### 2. Unstable CV Readings

**Problem**: CV inputs giving noisy or unstable readings

**Solutions:**
```cpp
// Add software filtering
class FilteredCVInput
{
private:
    PTCVInput cv_input;
    float filter_alpha; // 0.0 to 1.0
    float filtered_value;
    
public:
    FilteredCVInput(uint pin, float alpha = 0.1f) 
        : cv_input(pin), filter_alpha(alpha), filtered_value(0.0f) {}
    
    float getFilteredVoltage()
    {
        cv_input.update();
        float raw_voltage = cv_input.getVoltage();
        
        // Simple low-pass filter
        filtered_value = filtered_value * (1.0f - filter_alpha) + raw_voltage * filter_alpha;
        
        return filtered_value;
    }
};
```

#### 3. Timing Issues

**Problem**: Threads not executing at expected rates

**Debug Code:**
```cpp
class TimingDebugThread : public PTThread
{
private:
    uint32_t last_execution_time;
    uint32_t execution_count;
    
public:
    int run() override
    {
        PT_THREAD_BEGIN(this);
        
        while (true) {
            uint32_t current_time = time_us_32();
            
            if (last_execution_time > 0) {
                uint32_t interval = current_time - last_execution_time;
                printf("Thread interval: %lu µs\n", interval);
            }
            
            last_execution_time = current_time;
            execution_count++;
            
            // Wait some time
            uint32_t wait_start = time_us_32();
            PT_THREAD_WAIT_UNTIL(this, (time_us_32() - wait_start) >= 10000); // 10ms
        }
        
        PT_THREAD_END(this);
    }
};
```

---

## Summary

The Eurorack Hardware Libraries provide a complete foundation for building professional synthesizer modules:

### ✅ **EurorackUtils.h - Choose When:**
- Learning embedded programming
- Building simple, timer-based modules  
- Using Simple Threads library
- Needing direct hardware control
- Prototyping and experimentation

### ✅ **EurorackHardware.h - Choose When:**
- Building production-quality modules
- Requiring immediate response to inputs
- Using Protothreads library
- Implementing complex user interfaces
- Need interrupt-driven programming

### 🎯 **Integration Guidelines**
- **Simple Projects**: EurorackUtils + Simple Threads
- **Advanced Projects**: EurorackHardware + Protothreads  
- **Hybrid Projects**: Use both libraries for different subsystems
- **Migration Path**: Start with EurorackUtils, migrate to EurorackHardware as complexity grows

### 🚀 **Production Ready Features**
- Hardware abstraction for all common Eurorack components
- Interrupt-driven event system for real-time response
- Comprehensive voltage scaling and conversion functions
- Built-in debouncing and signal conditioning
- Thread-safe event queues for concurrent programming

These libraries transform the Raspberry Pi Pico into a powerful platform for Eurorack module development, providing both beginner-friendly utilities and advanced event-driven programming capabilities.
