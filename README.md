# Eurorack Protothreads Framework

A C++ framework for developing Eurorack modules using the Raspberry Pi Pico SDK and the protothreads library.

## Overview

This framework provides:
- **Protothreads Integration**: Non-preemptive cooperative threading for the Pico
- **C++ Wrapper Classes**: Easy-to-use object-oriented interface
- **Eurorack Utilities**: Helper functions for CV, gates, timing, and LED control
- **Scheduler System**: Manages multiple concurrent threads

## Features

### Protothreads
- Lightweight cooperative threading
- No stack overhead per thread
- Automatic state management
- Compatible with Pico SDK timing functions

### Eurorack Utilities
- **CV Input/Output**: ADC reading with voltage scaling
- **Gate/Trigger I/O**: Digital input/output with proper initialization
- **LED Control**: Onboard LED management
- **Timing**: Non-blocking delay functions
- **Math**: Mapping and constraint utilities

## Project Structure

```
framework/
├── protothreads/
│   ├── pt.h          # Core protothreads library
│   └── lc.h          # Local continuations implementation
├── pt_thread.h       # C++ wrapper classes
└── eurorack_utils.h  # Eurorack-specific utilities
```

## Usage Example

The current demo (`pt-test.cpp`) shows three concurrent threads:

1. **FastBlinkThread**: Creates rapid blink sequences
2. **SlowPulseThread**: Creates slow breathing LED effect
3. **StatusThread**: Reports system status every 5 seconds

### Creating a Thread

```cpp
class MyThread : public PTThread {
public:
    MyThread() {
        setInterval(100); // 100ms interval
    }
    
    char run() override {
        PT_CLASS_BEGIN();
        
        while (true) {
            // Your thread logic here
            EurorackUtils::LED::toggle();
            PT_CLASS_WAIT_UNTIL(intervalElapsed());
        }
        
        PT_CLASS_END();
    }
};
```

### Using the Scheduler

```cpp
int main() {
    EurorackUtils::init();
    
    MyThread thread1;
    MyThread thread2;
    
    PTScheduler scheduler;
    scheduler.addThread(&thread1);
    scheduler.addThread(&thread2);
    
    while (true) {
        scheduler.run();
    }
}
```

## Available Macros

Within your thread's `run()` method, you can use:

- `PT_CLASS_BEGIN()` / `PT_CLASS_END()`: Thread boundaries
- `PT_CLASS_WAIT_UNTIL(condition)`: Wait until condition is true
- `PT_CLASS_WAIT_WHILE(condition)`: Wait while condition is true
- `PT_CLASS_YIELD()`: Yield control to other threads
- `PT_CLASS_EXIT()`: Exit the thread
- `PT_CLASS_RESTART()`: Restart the thread

## Building

1. Ensure Pico SDK is properly installed and configured
2. Use the provided CMakeLists.txt
3. Build with: `ninja -C build`

## Hardware Connections

- **Onboard LED**: GPIO 25 (PICO_DEFAULT_LED_PIN)
- **ADC Inputs**: GPIO 26, 27, 28 (ADC0, ADC1, ADC2)
- **Digital I/O**: Any available GPIO pins

## Next Steps

This framework can be extended for specific Eurorack modules:
- Audio processing loops
- CV input scaling and processing
- Gate/trigger detection and generation
- SPI/I2C communication for DACs/displays
- Envelope generators, LFOs, sequencers, etc.

## License

Based on the protothreads library by Adam Dunkels (Swedish Institute of Computer Science).
Framework extensions for Eurorack use.
