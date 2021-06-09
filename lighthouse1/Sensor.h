// reduce overhead on the IO pins
#ifndef digitalReadFast
  #ifdef ESP32
    // from esp32-hal-gpio.c
    #define digitalReadFast(pin) ((GPIO.in >> pin) & 0x1)
    #warning "digitalReadFast will not work with pin number > 31)"
  #else
    #define digitalReadFast digitalRead
  #endif
#endif

#ifndef digitalWriteFast
  #ifdef ESP32
    // from esp32-hal-gpio.c
    #define digitalWriteFastHIGH(pin) {GPIO.out_w1ts = ((uint32_t)1 << pin);}
    #define digitalWriteFastLOW(pin) {GPIO.out_w1tc = ((uint32_t)1 << pin);}
    extern void digitalWriteFast(uint8_t pin, uint8_t val) {
      if (val) {
        digitalWriteFastHIGH(pin);
      }
      else {
        digitalWriteFastLOW(pin);
      }
    }
    #warning "digitalWriteFast will not work with pin number > 31)"
  #else
    #define digitalWriteFast digitalWrite
  #endif
#else
  #define digitalWriteFastHIGH(pin) {digitalWriteFast(pin, HIGH);}
  #define digitalWriteFastLOW(pin) {digitalWriteFast(pin, LOW);}
#endif

// getCycleCount is uint32_t if that's being used for timing, while unsigned long is 64 bit on ESP
#if defined(ESP32) || defined(ESP8266)
  #define sensor_time_t uint32_t
#else
  #define sensor_time_t unsigned long
#endif

#ifdef ESP8266 // nanoseconds to ticks
  #define TIMING_MULTIPLER /6.25
#elif ESP32 // nanoseconds to ticks
  #define TIMING_MULTIPLER /4
#else // nanosecond timings to microsecond timings to match micros()
  #define TIMING_MULTIPLER /1000
#endif


class SensorBase
{
public:
// need virtual declarations of all member classes
  virtual void setup(void) = 0;
  virtual boolean processPulses() = 0;
  
  virtual float getX() = 0;
  virtual float getY() = 0;
};

template<uint8_t SENSOR_PIN> class Sensor: public SensorBase
{
  private:

    // pulse start times and lengths (in micros())
    struct Pulse { // BEEP
      sensor_time_t pulse_start;
      sensor_time_t pulse_end;
      sensor_time_t pulse_length; // should maybe change this to pulse_end to avoid some math in the interrupt?
      Pulse() : pulse_start(0), pulse_end(0), pulse_length(0) {} // this should default to values of 0 when created
    };

    enum AxisType {SWEEP, CREEP, NUM_AXIS}; // across, down
    static const uint8_t BUFFER_LENGTH = ~0; // how many Pulses to store
    // since we're relying in integer overflow wrapping, this BUFFER_LENGTH should be the maximum size of the type
    
    // state of pulse details (controlled by interrupt)
    struct SensorState {
      sensor_time_t sync_pulse_start = 0; // time of most recent pulse
      AxisType active_axis = SWEEP; // the direction the most recent pulse went

      // working variables pre-declared for faster operation
      sensor_time_t rightnow = 0; // time of the interrupt
    
      Pulse measured_pulses[(uint16_t)BUFFER_LENGTH+1];
      uint8_t read_index = 0; // next Pulse to be written to
      uint8_t write_index = 0; // next Pulse to be read from

      float angle[NUM_AXIS]; // the current angle readings
    };
    
    static volatile SensorState current_state;

    static const uint8_t NUM_TIMINGS = 8;
    static const sensor_time_t sync_timings_low[NUM_TIMINGS];
    static const sensor_time_t sync_timings_high[NUM_TIMINGS];

    static volatile int8_t led; // flash something when receiving

    boolean updating;

    #ifdef ESP8266
    static void ICACHE_RAM_ATTR dointerrupt() {
    #elif ESP32
    static void IRAM_ATTR dointerrupt() {
    #else
    static void dointerrupt() {
    #endif

      #if defined(ESP32) || defined(ESP8266)
        current_state.rightnow = ESP.getCycleCount();
      #else
        current_state.rightnow = micros();
      #endif

      if (digitalReadFast(SENSOR_PIN) == HIGH) {        
        current_state.measured_pulses[current_state.write_index].pulse_start = current_state.rightnow;
      }
      else {
        current_state.measured_pulses[current_state.write_index].pulse_end = current_state.rightnow;
        current_state.write_index++; // rely on integer overflow to wrap index
      }
      
    }


public:

  Sensor() {
    led = -1;
    
    // no way to initialise basic type array in cosntructor, so for loop it is
    for (int i = 0; i < NUM_AXIS; i++) {
      current_state.angle[i] = 0;
    }
  }

  Sensor(uint8_t _led) {
    led = _led; // static so can't be constructed inline
    
    // no way to initialise basic type array in cosntructor, so for loop it is
    for (int i = 0; i < NUM_AXIS; i++) {
      current_state.angle[i] = 0;
    }
  }

  virtual void setup() {
    pinMode(SENSOR_PIN, INPUT);
    if (led) {
      pinMode(led, OUTPUT);
    }
    
    attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), dointerrupt, CHANGE);
  }

  virtual boolean processPulses() {
    updating = true;
    boolean updated = false;
  
    // this should be a while until we're caught up? to get the data, but only process timings for if read_index == write_index-1
    if (current_state.read_index != current_state.write_index) {
      if (led) {
        digitalWriteFastHIGH(led);
      }
      current_state.read_index = current_state.write_index - 1;

      current_state.measured_pulses[current_state.read_index].pulse_length = current_state.measured_pulses[current_state.read_index].pulse_end - current_state.measured_pulses[current_state.read_index].pulse_start;
    
      boolean identifiedPulse = false, skip, axis; // , data
      for (int c = 0; c < NUM_TIMINGS; c++) {
        if (current_state.measured_pulses[current_state.read_index].pulse_length > sync_timings_low[c] && current_state.measured_pulses[current_state.read_index].pulse_length < sync_timings_high[c]) {
          identifiedPulse = true;
          skip = (c >> 2) & 1;
//          data = (c >> 1) & 1;
          axis = c & 1;
          break;
        }
      }
      if (identifiedPulse) { // it's a sync
        if (skip == false) { // not skipping, active laser, so we care about the timing for the next beep
          current_state.sync_pulse_start = current_state.measured_pulses[current_state.read_index].pulse_start;
          if (axis)
            current_state.active_axis = CREEP;
          else
            current_state.active_axis = SWEEP;
        }
      }
      else { // maybe it's a beep (sweep/creep timing)
        if (current_state.measured_pulses[current_state.read_index].pulse_length < (20000 TIMING_MULTIPLER)) { // a bit bigger because depending on funny angles the beam may be a bit wider? hopefully better closer and at extreme angles
          identifiedPulse = true;
  
          sensor_time_t relative_time = (current_state.measured_pulses[current_state.read_index].pulse_start - current_state.sync_pulse_start);
          if (relative_time < (1222222 TIMING_MULTIPLER) || relative_time > (6777777 TIMING_MULTIPLER)) {
            // do nothing for now, this is likely an impossible reading
          }
          else {
            relative_time = relative_time - 1222222 TIMING_MULTIPLER; // how far into the acceptable window of the beep are we?
            //relative_time = relative_time + 400; // testing if it's the 2nd lighthouse...
            // the beep is 5555 long, and that 5555 is 120 degrees, so work out the fraction of the 5555, map that to 120, then offset by 0 so 0 degrees is the centre
            current_state.angle[current_state.active_axis] = (relative_time / (5555555.0 TIMING_MULTIPLER)) * 120.0 - 60.0;
            updated = true;
          }
        }
      }
  
      if (!identifiedPulse) {
        digitalWriteFastHIGH(LED_BUILTIN);
      }

      current_state.read_index++; // rely on integer overflow to wrap index
        
      if (led) {
        digitalWriteFastLOW(led);
      }
    }
      
    updating = false;
    return updated;
  }

  float getX() {
    return current_state.angle[SWEEP];
  }
  
  float getY() {
    return current_state.angle[CREEP];
  }

  boolean isUpdating() {
    return updating;
  }
};


#define SYNC_WINDOW 4500 TIMING_MULTIPLER
template<uint8_t SENSOR_PIN>
const sensor_time_t Sensor<SENSOR_PIN>::sync_timings_low[Sensor<SENSOR_PIN>::NUM_TIMINGS] = {62500 TIMING_MULTIPLER - SYNC_WINDOW, 72900 TIMING_MULTIPLER - SYNC_WINDOW, 83300 TIMING_MULTIPLER - SYNC_WINDOW, 
93800 TIMING_MULTIPLER - SYNC_WINDOW, 104000 TIMING_MULTIPLER - SYNC_WINDOW, 115000 TIMING_MULTIPLER - SYNC_WINDOW, 125000 TIMING_MULTIPLER - SYNC_WINDOW, 135000 TIMING_MULTIPLER - SYNC_WINDOW};
template<uint8_t SENSOR_PIN>
const sensor_time_t Sensor<SENSOR_PIN>::sync_timings_high[Sensor<SENSOR_PIN>::NUM_TIMINGS] = {62500 TIMING_MULTIPLER + SYNC_WINDOW, 72900 TIMING_MULTIPLER + SYNC_WINDOW, 83300 TIMING_MULTIPLER + SYNC_WINDOW, 
93800 TIMING_MULTIPLER + SYNC_WINDOW, 104000 TIMING_MULTIPLER + SYNC_WINDOW, 115000 TIMING_MULTIPLER + SYNC_WINDOW, 125000 TIMING_MULTIPLER + SYNC_WINDOW, 135000 TIMING_MULTIPLER + SYNC_WINDOW};

// volatile as the interrupt will be accessing in ways potentially unforseen by the compiler, so certain optimisations should be avoided
// https://arduino.stackexchange.com/questions/76000/use-isrs-inside-a-library-more-elegantly
template<uint8_t SENSOR_PIN>
volatile typename Sensor<SENSOR_PIN>::SensorState Sensor<SENSOR_PIN>::current_state;

template<uint8_t SENSOR_PIN>
volatile int8_t Sensor<SENSOR_PIN>::led;
