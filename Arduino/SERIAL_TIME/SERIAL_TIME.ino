#include "Adafruit_TinyUSB.h"                       // for serial communication on nrf52840 

enum Instructions {ToggleDataCapture, ManualCapture, ExportData, DeleteAllData, SetCollectionInterval
};
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(7, OUTPUT);
  digitalWrite(7, LOW);
}

void blinkLed(int num) {
  for (int i = 0; i < num; i++) {
    digitalWrite(7, HIGH);
    delay(500);
    digitalWrite(7, LOW);
    delay(250);
  }
}

bool paused = false;
bool time_set = false;

char ser_buffer[32];
int read_index = 0;

void loop() {
  // put your main code here, to run repeatedly:

  if (Serial) {
    if (Serial.available() > 0) {
      char c = (char) Serial.read();
      bool end_of_line = false;
      if (c != '\n') {
        ser_buffer[read_index] = c;
        read_index ++;
      } else {
        end_of_line = true;
        ser_buffer[read_index] = '\0';
      }
      
      if (end_of_line || read_index >= sizeof(ser_buffer)/sizeof(char)) {
        read_index = 0;
        // a line has been read fully
        // parse character array into string
        String instruction = String(ser_buffer);

        if (instruction == "ToggleDataCapture") {
          blinkLed(2);
        } else if (instruction == "ManualCapture") {
          blinkLed(3);
        } else if (instruction == "ExportData") {
          blinkLed(4);
        } else if (instruction == "DeleteAllData") {
          blinkLed(5);  
        } else if (instruction == "SetCollectionInterval") {
          blinkLed(6);
        }

        Serial.print("ECHO: ");
        Serial.println(instruction);
      }
    }
    
  } else {
    blinkLed(1);
  }

}
