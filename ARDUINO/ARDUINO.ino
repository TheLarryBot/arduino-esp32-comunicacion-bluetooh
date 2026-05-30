#include <SoftwareSerial.h>

SoftwareSerial BT(2, 3); // RX, TX

void setup() {
  Serial.begin(9600);
  BT.begin(9600);          // velocidad por defecto del HC-06
  Serial.println("HC-06 listo");
}

void loop() {
  // Lo que llega del ESP32 lo muestro en el monitor serie
  if (BT.available()) {
    Serial.write(BT.read());
  }
  // Lo que escribo en el monitor serie lo envío al ESP32
  if (Serial.available()) {
    BT.write(Serial.read());
  }
}