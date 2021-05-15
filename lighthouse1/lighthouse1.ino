#ifndef TEENSYDUINO

#define digitalReadFast digitalRead
const int led = LED_BUILTIN;
const int lighthouse = D7;

#else

const int led = 13;
const int lighthouse = 12;

#endif

const long NUM_TIMINGS = 8;
const long sync_timings[NUM_TIMINGS] = {63, 73, 83, 94, 104, 115, 125, 135};

enum axisType {SWEEP, CREEP};

struct state {
  long sync_pulse_start;
  axisType active_axis; // (of evil?)

  float angle[2];
};

state current_state;



const long BUFFER_LENGTH = 24;

long lasttime = 0;
long rightnow = 0;
long diff = 0;

struct LHData {
  long pulse_start;
  long pulse_length;
};

LHData measured_pulses[BUFFER_LENGTH];
long read_index = 0;
long write_index = 0;

#ifdef ESP8266
void ICACHE_RAM_ATTR myinterrupt() {
#else
void myinterrupt() {
#endif

  rightnow = micros();
  diff = rightnow - lasttime;

  boolean a = digitalReadFast(lighthouse);
  if (a == LOW) {
    measured_pulses[write_index].pulse_start = lasttime;
    measured_pulses[write_index].pulse_length = diff;
    write_index++;
    if (write_index >= BUFFER_LENGTH)
      write_index = 0;
  }
  lasttime = rightnow;

  digitalWrite(led, HIGH);
}

// the setup routine runs once when you press reset:
void setup() {
  pinMode(lighthouse, INPUT);
  pinMode(led, OUTPUT);

  delay(1000);

  Serial.begin(115200);
  attachInterrupt(digitalPinToInterrupt(lighthouse), myinterrupt, CHANGE);

  //Serial.println("come in lighthouse 1");
  //Serial.println("lighthouse 1 reporting heavy traffic");

  memset(&current_state, 0, sizeof(state));
}


// the loop routine runs over and over again forever:
void loop() {
  boolean updated = false;
  
  if (read_index != write_index) {
    boolean identifiedPulse = false, skip, data, axis;
    for (int c = 0; c < NUM_TIMINGS; c++) {
      if (measured_pulses[read_index].pulse_length > sync_timings[c] - 4 && measured_pulses[read_index].pulse_length < sync_timings[c] + 4) {
        identifiedPulse = true;
        skip = (c >> 2) & 1;
        data = (c >> 1) & 1;
        axis = c & 1;
        //        Serial.print("found pulse skip ");
        //        Serial.print(skip);
        //        Serial.print(" data ");
        //        Serial.print(data);
        //        Serial.print(" axis ");
        //        Serial.println(axis);
        //        Serial.println(c);
        break;
      }
    }
    if (identifiedPulse) { // it's a sync
      if (skip == false) { // not skipping, active laser, so we care about the timing for the next beep
        current_state.sync_pulse_start = measured_pulses[read_index].pulse_start;
        if (axis)
          current_state.active_axis = SWEEP;
        else
          current_state.active_axis = CREEP;
      }
    }
    else { // maybe it's a beep (sweep/creep timing)
      if (measured_pulses[read_index].pulse_length < 20) { // a bit bigger because depending on funny angles the beam may be a bit wider? hopefully better closer and at extreme angles
        identifiedPulse = true;

        long relative_time = (measured_pulses[read_index].pulse_start - current_state.sync_pulse_start);
        if (relative_time < 1222 || relative_time > 6777) {
          // do nothing for now, this is likely an impossible reading
        }
        else {
          relative_time = relative_time - 1222; // how far into the acceptable window of the beep are we?
          //relative_time = relative_time + 400; // testing if it's the 2nd lighthouse...
          // the beep is 5555 long, and that 5555 is 120 degrees, so work out the fraction of the 5555, map that to 120, then offset by 0 so 0 degrees is the centre
          current_state.angle[current_state.active_axis] = (relative_time / 5555.0) * 120.0 - 60.0;
          updated = true;
//          Serial.print("axis ");
//          Serial.print(current_state.active_axis);
//          Serial.print(" ");
//          Serial.print(current_state.angle[current_state.active_axis]);
//          Serial.println(" degrees");
        }
      }
    }

    if (!identifiedPulse) {
      //Serial.print("pulse of unspeakable duration ");
      //Serial.println(measured_pulses[read_index].pulse_length);
    }


    read_index++;
    if (read_index >= BUFFER_LENGTH)
      read_index = 0;
  }

  if (updated) {
    Serial.print(current_state.angle[CREEP]);
    Serial.print(", ");
    Serial.print(current_state.angle[SWEEP]);
    Serial.print("\n");
  }

  delayMicroseconds(100);
  digitalWrite(led, LOW);
}
