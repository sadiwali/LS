#include "Adafruit_TinyUSB.h"                      // for serial communication on nrf52840 

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial);
  pinMode(7, OUTPUT);
}

char message[16];
int read_index = 0;
void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available() > 0) {
    char incoming_byte = Serial.read();
    Serial.print(incoming_byte);
    message[read_index] = incoming_byte;
    read_index++;

    if (read_index >= 16) {
      // buffer is full
      String msg = message;
      Serial.println(msg);
      if (msg == "abcdefghijklmn") {
        digitalWrite(7, HIGH);
      } else {
        digitalWrite(7, LOW);
      }
    }
  }
  
}
