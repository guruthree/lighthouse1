int led = 13;
int lighthouse = 12;

long lasttime = 0;
long rightnow = 0;
long diff = 0;

void myinterrupt() {
  rightnow = micros();
  diff = rightnow - lasttime;

  boolean a = digitalReadFast(lighthouse);
  if (a == LOW) {
    diff = -diff;
  }
  
  Serial.println(diff);
  
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
  
  Serial.println("come in lighthouse 1");
  Serial.println("lighthouse 1 reporting heavy traffic");

}

// the loop routine runs over and over again forever:
void loop() {
  delayMicroseconds(100);
  digitalWrite(led, LOW);
}
