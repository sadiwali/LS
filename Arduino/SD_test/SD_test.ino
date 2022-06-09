/* 
 * Title: SD Test
 * Author: Sadi Wali, 
 * Date: 2022-05-29
 * Description: Sample code for reading and writing using the Arduino SD library
 * 
 */

#include <SPI.h>
#include <SD.h>

// CONSTANTS
#define SD_CS_PIN 23                        // pin connected to SD CS
#define SD_EJECT_DETECT_PIN 2               // pin connected to SD EJECT 
#define LOG_FILENAME "LOG.CSV"              // the log filename on the SD

// The Storage class
class Storage {
  private:
    String log_file_name;                   // file name and directory to save the CSV data log to
    File log_file;                          // the file variable that holds the log file
    int CS_PIN;                             // the SPI chip select pin
    int CHECK_PIN;                          // SD card eject detection pin (HIGH when ejected, LOW when inserted)
    bool ERR = false;                       // was there an error with the Storage class?

    const int DEBOUNCE_DELAY = 1000;   // the debounce time; increase if the output flickers
    int lastSteadyState = LOW;       // the previous steady state from the input pin
    int lastFlickerableState = LOW;  // the previous flickerable state from the input pin
    int currentState;                // the current reading from the input pin
    unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
    
  public:
    Storage(int CS, int check_pin, String filename) {
      /* Given the chip select (CS), and the detection pin (CHECK_PIN), load and open the log file to write to. */
      this->CS_PIN = CS; // the chip select pin
      this->CHECK_PIN = check_pin; // for checking SD card ejection
      this->log_file_name = filename; // log file name and directory`
    }

    void init() {
      if (!SD.begin(this->CS_PIN)) {
        // unable to open SD card
        this->ERR = true;
        pinMode(this->CHECK_PIN, INPUT); // input pin for checking ejection
        return;
      }
      // SD started, open log file, or create it if it doesn't exist.
      //open_file();
    }

    void open_file() {
      this->log_file = SD.open(this->log_file_name, FILE_WRITE);
    }

    void write_line(String line) {
      this->log_file.println(line);
    }
    
    String read_line(unsigned int line){

      String line_to_ret = "";

      this->log_file.seek(0);
      char cr;
     
      for(unsigned int i = 0; i < (line -1); i++){
        cr = this->log_file.read();
        if(cr == '\n'){
          i++;
        }
      }
      for(unsigned int i = 0; i < 5000; i++) {
        cr = this->log_file.read();
        line_to_ret += cr;
        if(cr == '\n'){
          break;
        }
      }

      return line_to_ret;
    }
    
    void close_file(){
      /* Close the open file */
      this->log_file.close();
    }
  

    // did the SD class initialize correctly?
    bool is_errored(){
      return this->ERR;
    }
  
    // is the SD card ejected? Returns TRUE if ejected, FALSE if not
    bool detect_eject() {
       currentState = digitalRead(this->CHECK_PIN);

      // If the switch/button changed, due to noise or pressing:
      if (currentState != lastFlickerableState) {
        // reset the debouncing timer
        lastDebounceTime = millis();
        // save the the last flickerable state
        lastFlickerableState = currentState;
      }
    
      if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
        // whatever the reading is at, it's been there for longer than the debounce
        // delay, so take it as the actual current state:
    
        // if the button state has changed:
        if (lastSteadyState == HIGH && currentState == LOW)
          return true;
        else if (lastSteadyState == LOW && currentState == HIGH)
          return false
    
        // save the the last steady state
        lastSteadyState = currentState;
      }
    }

  
};

Storage st(SD_CS_PIN, SD_EJECT_DETECT_PIN, LOG_FILENAME);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial)
  {
    
  }
  Serial.println("hello");
 
  for (int i = 0; i < 5; i++) {
    // initialize SD
    st.init();
    if (!st.is_errored()) {
      break;
    }
  }

  Serial.println("SD initialized");

  // SD card initialized at this point

  // read the first and last line
  Serial.println("---");
  String line = st.read_line(1);
  Serial.println(line);
  Serial.println("---");

}



void loop() {
  // put your main code here, to run repeatedly:
  //Serial.println("Hi");
  //delay(1000);

  if (st.detect_eject()) {
    Serial.println("EJECT");
  }
 
}
