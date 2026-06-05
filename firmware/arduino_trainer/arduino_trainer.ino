void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println("EVENT,BOOT");
  Serial.println("FW,NAME=arduino-trainer-mk0, VERSION=0.1.0");
  Serial.println("BOARD,TYPE=UNO_R3");
  Serial.println("MODE,current=BOOT");
}

void loop() {
  static unsigned long last = 0;
  static unsigned long counter = 0;

  if (millis() - last >= 1000) {
    last = millis();
    counter++;

    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

    Serial.print("TLM,HEALTH,tick=");
    Serial.print(millis());
    Serial.print(",counter=");
    Serial.println(counter);
  }
}
