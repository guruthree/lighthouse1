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
Sensor<27> mySensor2(32);

// the setup routine runs once when you press reset:
void setup() {
  mySensor.setup();
  mySensor2.setup();
  delay(500);

  Serial.begin(115200);
  delay(1000);

  Serial.println("x1 y1 x2 y2");
}


// the loop routine runs over and over again forever:
boolean updated;
void loop() {

  updated = mySensor.processPulses() | mySensor2.processPulses();

  if (updated) {
    Serial.print(mySensor.getX());
    Serial.print(", ");
    Serial.print(mySensor.getY());
    Serial.print(", ");
    Serial.print(mySensor2.getX());
    Serial.print(", ");
    Serial.print(mySensor2.getY());
    Serial.print("\n");
  }

  delayMicroseconds(1);
}
