#ifndef TEENSYDUINO

#define digitalReadFast digitalRead
#define digitalWriteFast digitalWrite
const int led = LED_BUILTIN;
const int lighthouse = D7;

#else

const int led = 13;
const int lighthouse = 4;
//const int lighthouse = 27;

#endif

// after ifndef to catch the digitalFastRead define if needed
#include "Sensor.h"
Sensor<lighthouse> mySensor(led);

// the setup routine runs once when you press reset:
void setup() {
  mySensor.setup();
  delay(500);

  Serial.begin(115200);
  delay(1000);

  Serial.println("x1 y1");
}


// the loop routine runs over and over again forever:
boolean updated;
void loop() {

  updated = mySensor.processPulses();

  if (updated) {
    Serial.print(mySensor.getX());
    Serial.print(", ");
    Serial.print(mySensor.getY());
    Serial.print("\n");
  }

  delayMicroseconds(100);
}
