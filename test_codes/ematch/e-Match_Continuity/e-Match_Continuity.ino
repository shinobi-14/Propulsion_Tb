

int sensorPin1 = 34;   //continuity
int sensorValue1 = 0;

int sensorPin2 = 35;   //continuity
int sensorValue2 = 0;

int sensorPin3 = 25;   //trigger
int sensorValue3 = 0;

int sensorPin4 = 26;   //trigger
int sensorValue4 = 0;



void setup() {
  Serial.begin(115200);
}

void loop() {
  continuity();
  trigger();
  delay(100);
}

void continuity(){
  sensorValue1 = analogRead(sensorPin1);
  sensorValue2 = analogRead(sensorPin2);
  Serial.print(sensorValue1);
  Serial.print(",");
  Serial.println(sensorValue2);
}

void trigger(){
  sensorValue3 = digitalRead(sensorPin3);
  sensorValue4 = digitalRead(sensorPin4);
  Serial.print(sensorValue3);
  Serial.print(",");
  Serial.println(sensorValue4);
}
