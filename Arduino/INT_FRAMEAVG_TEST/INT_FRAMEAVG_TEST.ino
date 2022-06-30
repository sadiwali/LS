/**
 * Sadi Wali June 2022 for Alstan Jakubiec
 * 
 * This program records spectrum data with a range of integration times, with varying frame average,
 * with and without autoexposure enabled to test the data collection capabilities of the NSP32.
 * 
 * Pin layout used:
 * ------------------------------------
 *               NSP32      Sparkfun       
 * SPI                      nRF52840     
 * Signal        Pin          Pin        
 * ------------------------------------
 * Wakeup/Reset  RST          19    
 * SPI SSEL      SS           21    
 * SPI MOSI      MOSI         3    
 * SPI MISO      MISO         31  
 * SPI SCK       SCK          30   
 * Ready         Ready        20
 * ------------------------------------
 *               SD Card
 *               Pin
 * ------------------------------------
 * SPI SSEL      CS           22
 * CARD DETECT   CD           5
 */

#include <ArduinoAdaptor.h>
#include <NSP32.h>
#include <SPI.h>
#include <SD.h>

using namespace NanoLambdaNSP32;


// CONSTANTS
#define SD_CS_PIN 22                      // pin connected to SD CS
#define LOG_FILENAME "INT2.CSV"            // the log filename on the SD
#define NSP_RESET 19                      // NSP reset pin
#define NSP_READY 23                      // NSP ready pin

// PINS
const unsigned int PinRst = NSP_RESET;    // pin Reset
const unsigned int PinReady = NSP_READY;  // pin Ready

// DATE TIME
String myTime = __TIME__;
String myDate = __DATE__;

ArduinoAdaptor adaptor(PinRst);               // master MCU adaptor
NSP32 nsp32( & adaptor, NSP32::ChannelSpi);   // NSP32 (using SPI channel)


/* The storage class deals with the microSD card, and file management within the card. */
class Storage {
  private:
    String log_file_name;     // file name and directory to save the CSV data log to
    File log_file;            // the file variable that holds the log file
    int CS_PIN;               // the SPI chip select pin
    int CHECK_PIN;            // SD card eject detection pin (HIGH when ejected, LOW when inserted)
    bool ERR = false;         // was there an error with the Storage class?

  public:
    Storage(int CS, int check_pin, String filename) {
      /* Given the chip select (CS), and the detection pin (CHECK_PIN), load and open the log file to write to. */
      this -> CS_PIN = CS;                // the chip select pin
      this -> CHECK_PIN = check_pin;      // for checking SD card ejection (not implemented)
      this -> log_file_name = filename;   // log file name and directory
    }

  void init() {
    pinMode(this -> CHECK_PIN, INPUT_PULLUP);
    if (!SD.begin(this -> CS_PIN)) {
      // unable to open SD card
      this -> ERR = true;
      return;
    }
  }

  void open_file() {
    // attempt 5 times to open card, waiting 1 second in between
    for (int i = 0; i < 5; i ++) {
      this -> log_file = SD.open(this -> log_file_name, FILE_WRITE);
      if (this->log_file) {
        break;
      }
      // wait 1 second before attempting to open again
      delay(1000);
    }
    // file not open, errored
    if (!this->log_file) this -> ERR = true;

    // file opened, check if new file
    if (this->log_file.size() == 0) {
      // brand new file, add header
      String line = "DATE,TIME,INT_TIME,FRAME_AVG,AE,IS_SATURATED,X,Y,Z,";
      
      for (int i = 340; i <= 1010; i += 5) {
        line.concat(String(i));
        line.concat(",");
      }
      this->write_line(&line);
    }
  }

  void close_file() {
    this -> log_file.close();
  }

  void write_line(String *line) {
    this -> log_file.println(*line);
  }

  String read_line(unsigned int line) {
    String line_to_ret = "";

    this -> log_file.seek(0);
    char cr;

    for (unsigned int i = 0; i < (line - 1); i++) {
      cr = this -> log_file.read();
      if (cr == '\n') {
        i++;
      }
    }

    for (unsigned int i = 0; i < 5000; i++) {
      cr = this -> log_file.read();
      line_to_ret += cr;
      if (cr == '\n') {
        break;
      }
    }
    return line_to_ret;
  }

  // did the SD class initialize correctly?
  bool is_errored() {
    return this -> ERR;
  }

};

void get_reading(SpectrumInfo *info, int int_time = 64, int frame_avg = 3, bool ae = false) {
  // wakeup the sensor if sleeping
  nsp32.Wakeup();
  nsp32.AcqSpectrum(0, int_time, frame_avg, ae); // sensor_id, integration time, frame avg num, autoexposure
  while (nsp32.GetReturnPacketSize() <= 0) {
    nsp32.UpdateStatus(); // call UpdateStatus() to check async result
  }
  nsp32.ExtractSpectrumInfo(info); // now we have all spectrum info in infoW, we can use e.g. "infoS.Spectrum" to access the spectrum data array

}

// Define the SD card object
Storage st(SD_CS_PIN, 5, LOG_FILENAME);

void setup() {
  // initialize "ready trigger" pin for accepting external interrupt (falling edge trigger)
  pinMode(PinReady, INPUT_PULLUP); // use pull-up for ready pin
  pinMode(7, OUTPUT); // LED output
  //pinMode(13, INPUT_PULLUP); // pushbutton

  digitalWrite(7, HIGH); // write LED ON
  attachInterrupt(digitalPinToInterrupt(PinReady), PinReadyTriggerISR, FALLING); // enable interrupt for falling edge

  // initialize serial port for "Serial Monitor"
  Serial.begin(115200);
  //while (!Serial);                    // wait for serial if prints inside setup function are important (this will hang the MCU until plugged into serial monitor)
  Serial.println("START");

  // attempt to initialize the SD
  while (true) {
    // initialize SD
    st.init();
    if (!st.is_errored()) {
      break;
    }
  }

  // initialize NSP32
  nsp32.Init();
  digitalWrite(7, LOW); // turn off the LED indicating program setup successfully.
}

void loop() {

  // min integration is 1, max is 1200. In ms, it is a little less than double the value passed to the function

  for (int b = 0; b <= 1; b++) {
  for (int a = 1; a <= 3; a++) {
  for (int i = 0; i <= 1200; i += 5) {
    if (i == 0) continue;
    SpectrumInfo infoS; // for storing the data reading into this
    // format each line of CSV file into this line
    String line = "";

    int frame_avg = a;
    bool ae = (b == 0) ? false : true;
    
    // take the measurement
    get_reading(&infoS, i, frame_avg, ae);
    
    // write the line
    // first put in the date
    line.concat("0");
    line.concat(",");

    // then put in the time

    unsigned long ms = millis();
    int seconds = (ms / 1000) % 60 ;
    int minutes = ((ms / (1000*60)) % 60);
    int hours   = ((ms / (1000*60*60)) % 24);
    
    line.concat(String(hours));
    line.concat(":");
    line.concat(String(minutes));
    line.concat(":");
    line.concat(String(seconds));
    line.concat(",");
    
    // Then put in the integration time used
    line.concat(String(infoS.IntegrationTime));
    line.concat(",");

    // Then put in the frame avg
    line.concat(String(frame_avg));
    line.concat(",");

    // Then put in the autoexposure flag
    line.concat(String(ae));
    line.concat(",");
    
    // Then put in the isSaturated flag
    line.concat(String(infoS.IsSaturated));
    line.concat(",");
    
    // Then put in the CIE1931 coords
    line.concat(String(infoS.X));
    line.concat(",");
    line.concat(String(infoS.Y));
    line.concat(",");
    line.concat(String(infoS.Z));
    line.concat(",");
     
    // Then put in the spectrum data
    for (int j = 0; j < infoS.NumOfPoints; j++) {
      line.concat(String(infoS.Spectrum[j], 18)); // write with 18 digits (precise)
      line.concat(",");
    }

    // Standby seems to clear SpectrumInfo, therefore call it after processing the data
    nsp32.Standby(0);
    
    Serial.println(line);
    write_to_sd(&line); // write the data to the SD card
    // sleep for a quarter of a second
    delay(250);

    
  }
  }
  }

  exit(0);
  
}

void update_time() {
  
}

void parse_time() {
  
}



void write_to_sd(String *line) {
  // turn on the LED to indicate writing
  st.open_file();
  st.write_line(line);
  st.close_file();
}

void PinReadyTriggerISR() {
  // make sure to call this function when receiving a ready trigger from NSP32
  nsp32.OnPinReadyTriggered();
}
