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
      unsigned long pulse_start;
      unsigned long pulse_length; // should maybe change this to pulse_end to avoid some math in the interrupt?
      Pulse() : pulse_start(0), pulse_length(0) {} // this should default to values of 0 when created
    };

    enum AxisType {SWEEP, CREEP, NUM_AXIS}; // across, down
    static const uint8_t BUFFER_LENGTH = 8; // how many Pulses to store
    
    // state of pulse details (controlled by interrupt)
    struct SensorState {
      unsigned long sync_pulse_start = 0; // time of most recent pulse
      AxisType active_axis = SWEEP; // the direction the most recent pulse went

      // working variables pre-declared for faster operation
      unsigned long lasttime = 0; // the last time a pulse was receieved
      unsigned long rightnow = 0; // time of the interrupt
    
      Pulse measured_pulses[BUFFER_LENGTH];
      uint8_t read_index = 0; // next Pulse to be written to
      uint8_t write_index = 0; // next Pulse to be read from

      float angle[NUM_AXIS]; // the current angle readings
    };
    
    static volatile SensorState current_state;

    static const uint8_t NUM_TIMINGS = 8;
    static const unsigned long sync_timings_low[NUM_TIMINGS];
    static const unsigned long sync_timings_high[NUM_TIMINGS];

    static volatile uint8_t led; // flash something when receiving

    boolean updating;

    #ifdef ESP8266
    static void ICACHE_RAM_ATTR dointerrupt() {
    #elif ESP32
    static void IRAM_ATTR dointerrupt() {
    #else
    static void dointerrupt() {
    #endif


      #ifdef ESP8266
        current_state.rightnow = ((unsigned long)(ESP.getCycleCount())) * 6.25; // 6.25 ticks per ns (may be faster to just use micros?)
      #elif ESP32
        current_state.rightnow = ((unsigned long)(ESP.getCycleCount())) << 2; // 4 ticks per ns
      #else
        current_state.rightnow = micros()*1000; // ns timings
      #endif
    
      if (digitalReadFast(SENSOR_PIN) == LOW) {        
        current_state.measured_pulses[current_state.write_index].pulse_start = current_state.lasttime;
        current_state.measured_pulses[current_state.write_index].pulse_length = current_state.rightnow - current_state.lasttime;
        current_state.write_index++;
        if (current_state.write_index >= BUFFER_LENGTH)
          current_state.write_index = 0;
      }
      current_state.lasttime = current_state.rightnow;
    
      //digitalWriteFastHIGH(led);
    }

public:
  Sensor(uint8_t _led) {
    led = _led; // static so can't be constructed inline
    
    // no way to initialise basic type array in cosntructor, so for loop it is
    for (int i = 0; i < NUM_AXIS; i++) {
      current_state.angle[i] = 0;
    }
  }

  virtual void setup() {
    pinMode(SENSOR_PIN, INPUT);
    pinMode(led, OUTPUT);
    
    attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), dointerrupt, CHANGE);
  }

  virtual boolean processPulses() {
    updating = true;
    boolean updated = false;
  
    // this should be a while until we're caught up? to get the data, but only process timings for if read_index == write_index-1
    if (current_state.read_index != current_state.write_index) {
      digitalWriteFastHIGH(led);
      current_state.read_index = current_state.write_index - 1;
    
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
        if (current_state.measured_pulses[current_state.read_index].pulse_length < 20000) { // a bit bigger because depending on funny angles the beam may be a bit wider? hopefully better closer and at extreme angles
          identifiedPulse = true;
  
          unsigned long relative_time = (current_state.measured_pulses[current_state.read_index].pulse_start - current_state.sync_pulse_start);
          if (relative_time < 1222222 || relative_time > 6777777) {
            // do nothing for now, this is likely an impossible reading
          }
          else {
            relative_time = relative_time - 1222222; // how far into the acceptable window of the beep are we?
            //relative_time = relative_time + 400; // testing if it's the 2nd lighthouse...
            // the beep is 5555 long, and that 5555 is 120 degrees, so work out the fraction of the 5555, map that to 120, then offset by 0 so 0 degrees is the centre
            current_state.angle[current_state.active_axis] = (relative_time / 5555555.0) * 120.0 - 60.0;
            updated = true;
          }
        }
      }
  
//      if (!identifiedPulse) {
//      }

      current_state.read_index++;
      if (current_state.read_index >= BUFFER_LENGTH)
        current_state.read_index = 0;
        
      digitalWriteFastLOW(led);
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

#define SYNC_WINDOW 4500
template<uint8_t SENSOR_PIN>
const unsigned long Sensor<SENSOR_PIN>::sync_timings_low[Sensor<SENSOR_PIN>::NUM_TIMINGS] = {62500-SYNC_WINDOW, 72900-SYNC_WINDOW, 83300-SYNC_WINDOW, 93800-SYNC_WINDOW, 104000-SYNC_WINDOW, 115000-SYNC_WINDOW, 125000-SYNC_WINDOW, 135000-SYNC_WINDOW};
template<uint8_t SENSOR_PIN>
const unsigned long Sensor<SENSOR_PIN>::sync_timings_high[Sensor<SENSOR_PIN>::NUM_TIMINGS] = {62500+SYNC_WINDOW, 72900+SYNC_WINDOW, 83300+SYNC_WINDOW, 93800+SYNC_WINDOW, 104000+SYNC_WINDOW, 115000+SYNC_WINDOW, 125000+SYNC_WINDOW, 135000+SYNC_WINDOW};

// volatile as the interrupt will be accessing in ways potentially unforseen by the compiler, so certain optimisations should be avoided
// https://arduino.stackexchange.com/questions/76000/use-isrs-inside-a-library-more-elegantly
template<uint8_t SENSOR_PIN>
volatile typename Sensor<SENSOR_PIN>::SensorState Sensor<SENSOR_PIN>::current_state;

template<uint8_t SENSOR_PIN>
volatile uint8_t Sensor<SENSOR_PIN>::led;
