#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_Master", true);  // true = modo maestro
  Serial.println("Conectando al HC-06...");

  bool conectado = SerialBT.connect("HC-06"); // nombre del módulo

  if (conectado) {
    Serial.println("¡Conectado!");
  } else {
    Serial.println("No se pudo conectar, revisa el nombre/MAC");
  }
}

void loop() {
  if (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }
  if (Serial.available()) {
    SerialBT.write(Serial.read());
  }
}