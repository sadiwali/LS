/* 
 * Title: SD Test
 * Author: Sadi Wali, 
 * Date: 2022-05-29
 * Description: Sample code for reading and writing using the Arduino SD library
 * 
 */

#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <SD.h>

#define SD_CS_PIN 29
#define SD_EJECT_DETECT_PIN 2

class Storage {
  private:
    const String log_file_name = "log.csv"; // file name and directory to save the CSV data log to
    File log_file;                          // the file variable that holds a pointer to the log file
    int CS_PIN;                             // the SPI chip select pin
    int CHECK_PIN;                          // SD card eject detection pin (HIGH when ejected, LOW when inserted)
    bool ERR = false;                       // was there an error with the Storage class?
    
  public:
    Storage(int CS, int CHECK_PIN) {
      /* Given the chip select (CS), and the detection pin (CHECK_PIN), load and open the log file to write to. */
      this->CS_PIN = CS; // the chip select pin
      this->CHECK_PIN = CHECK_PIN; // for checking SD card ejection
      init();
    }

  void init() {
    if (!SD.begin(this->CS_PIN)) {
      // unable to open SD card
      this->ERR = true;
      return;
    }
    // SD started, open log file, or create it if it doesn't exist.
    this->log_file = SD.open(this->log_file_name, FILE_WRITE);
  }

  void close_file(){
    /* Close the open file */
    SD.close(this->log_file);
  }

  bool write_char(char data) {
    /* Write character by character into the log file */
    SD.print(data);
  }
  
  bool errored(){
    return this->ERR;
  }
  
};

Storage s(SD_CS_PIN, SD_EJECT_DETECT_PIN);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("hello");

}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Hi");
  delay(1000);
}
