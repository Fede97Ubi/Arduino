#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(9600);
  delay(1000);
  Serial.println("Scansione I2C...");
  for (byte address = 1; address < 127; address++) {
    Serial.print("test ");
    Serial.println(address, HEX);
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("Trovato dispositivo a 0x");
      Serial.println(address, HEX);
    }
  }
  Serial.println("Fine scansione.");
}

void loop() {}
