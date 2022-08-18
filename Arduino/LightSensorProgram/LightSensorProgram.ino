/**
 * Sadi Wali August 2022
 * 
 * This program combines a Sparkfun nRF52840, a microSD card reader, and
 * the NanoLambda NSP32m W1 to create a lightweight portable digital spectrometer that
 * captures spectral data every 60 seconds for a period of up to 7 days continuously on battery.
 * 
 * Pin layout used:
 * ------------------------------------
 *               NSP32      Sparkfun       
 * SPI                      nRF52840     
 * Signal        Pin          Pin        
 * ------------------------------------
 * Wakeup/Reset  RST          28    
 * SPI SSEL      SS           5    
 * SPI MOSI      MOSI         3    
 * SPI MISO      MISO         31  
 * SPI SCK       SCK          30   
 * Ready         Ready        29
 * ------------------------------------
 *               SD Card
 *               Pin
 * ------------------------------------
 * SPI SSEL      CS           4
 * SPI MOSI      DI           3
 * SPI MISO      DO           31
 * SPI SCK       SCK          30
 **/

#include <ArduinoAdaptor.h>                        // for low-level interfacing with the NSP via Arduino
#include "Adafruit_TinyUSB.h"                      // for serial communication on nrf52840 
#include <NSP32.h>                                 // for high-level interfacing with the NSP module
#include <SPI.h>                                   // Arduino SPI 
#include <SD.h>                                    // Arduino SD
#include <bluefruit.h>                             // Bluetooth
#include <TimeLib.h>                              // Date and time

using namespace NanoLambdaNSP32;

// CONSTANTS
#define LOG_FILENAME          "LOG1.CSV"           // the log filename to write to
#define CAPTURE_INTERVAL      5000                 // how frequently to capture data in ms
#define MIN_WAVELENGTH        340                  // the minimum wavelength we are interested in
#define MAX_WAVELENGTH        1010                 // the maximum wavelength we are interested in
#define SENSOR_MIN_WAVELENGTH 340                  // the minimum sensing wavelength (for W1 sensor)
#define SENSOR_MAX_WAVELENGTH 1010                 // the maximum sensing wavelength  (for W1 sensor)
#define WAVELENGTH_STEPSIZE   5                    // the sensor wavelength resolution (5 nm)
#define CAPTURE_PRECISION     18                   // how many digits of precision to write (18 digits)
#define BT_TIMEOUT            1000 * 60            // how long to advertise for synchronizing time in ms

// PINS
#define SD_CS_PIN             4                    // pin connected to SD CS
#define NSP_CS_PIN            5                    // pin connected to NSP CS
#define NSP_RESET             28                   // NSP reset pin
#define NSP_READY             29                   // NSP ready pin
const unsigned int PinRst = NSP_RESET;             // pin Reset
const unsigned int PinSS = NSP_CS_PIN;             // NSP chip select pin
const unsigned int PinReady = NSP_READY;           // pin Ready

// DATE TIME
String myTime = "HHMMSS";                          // the starting time
String myDate = "YYYYMMDD";                        // the starting date
time_t cTime = now();
bool time_set = false;                             // has the time been set?

// BLE
BLEUart bleuart; // uart over ble

// NSP32m
ArduinoAdaptor adaptor(PinRst, PinSS);             // master MCU adaptor
NSP32 nsp32( & adaptor, NSP32::ChannelSpi);        // NSP32 (using SPI channel)

/* The storage class deals with the microSD card, and file management within the card. */
class Storage {
  private:
    String log_file_name;     // file name and directory to save the CSV data log to
    File log_file;            // the file variable that holds the log file
    int CS_PIN;               // the SPI chip select pin
    bool ERR = false;         // was there an error with the Storage class?

  public:
    Storage(int CS, String filename) {
      this -> CS_PIN = CS;                // set the chip select pin for SPI
      this -> log_file_name = filename;   // set the log file name
    }

    /* Attempt to initialize the SD card reader. If unsuccessful, set flag and return */
    void init() {
      if (!SD.begin(this -> CS_PIN)) {
        // unable to open SD card, set error flag
        this -> ERR = true;
        return;
      }
    }

    /* Once the SD card reader is initialized, attempt to open the log file */
    void open_file() {
      // attempt to open SD card for writing
      while (!this->log_file) {
        this->log_file = SD.open(this -> log_file_name, FILE_WRITE);
      }
  
      // file opened, check if new file
      if (this->log_file.size() == 0) {
        // brand new file, add header
        String line = "DATE,TIME,MANUAL,INT_TIME,FRAME_AVG,AE,IS_SATURATED,X,Y,Z,";
  
        // add the wavelengths to the header
        for (int i = MIN_WAVELENGTH; i <= MAX_WAVELENGTH; i += WAVELENGTH_STEPSIZE) {
          line.concat(String(i));
          line.concat(",");
        }
        // finally write the header string to file
        this->write_line(&line);
      }
    }
  
    /* Close the SD file */
    void close_file() {
      this->log_file.close();
    }
  
    /* Write a line to the SD file */
    void write_line(String *line) {
      this->open_file();
      this->log_file.println(*line);
      this->close_file();
    }
  
    /* Read a line given line number from the SD file */
    String read_line(unsigned int line) {
      String line_to_ret = "";
  
      this->log_file.seek(0);
      char cr;
  
      for (unsigned int i = 0; i < (line - 1); i++) {
        cr = this->log_file.read();
        if (cr == '\n') {
          i++;
        }
      }
  
      for (unsigned int i = 0; i < 5000; i++) {
        cr = this->log_file.read();
        line_to_ret += cr;
        if (cr == '\n') {
          break;
        }
      }
      return line_to_ret;
    }
  
    /* Returns false if SD file opened successfully */
    bool is_errored() {
      return this->ERR;
    }
};

// Define the SD card object
Storage st(SD_CS_PIN, LOG_FILENAME);

/*  Returns: the current date and time in the following format 'YYYY/MM/DD HH:MM:SS' */
String get_current_datetime() {
  return "";
}

/* Get a reading from the NSP32 sensor */
void get_reading(SpectrumInfo *info, int int_time = 0, int frame_avg = 3, bool ae = true) {
  // wakeup the sensor if sleeping
  nsp32.Wakeup();
  // get the spectrum data. Note that if ae is true, int_time is ignored
  nsp32.AcqSpectrum(0, int_time, frame_avg, ae);
  // wait until spectrum data completely collected
  while (nsp32.GetReturnPacketSize() <= 0) {
    nsp32.UpdateStatus(); // call UpdateStatus() to check async result
  }
  // data has been collected, extract into info
  nsp32.ExtractSpectrumInfo(info); // now we have all spectrum info in infoW, we can use e.g. "infoS.Spectrum" to access the spectrum data array
}

void take_measurement(bool manual_measurement=false) {
  Serial.println("Recording a data point...");
  SpectrumInfo infoS; // variable for storing the spectrum data
  
  // format each line of CSV file into this line
  String line = "";

  // settings for the reading
  int frame_avg = 3;  // how many frames to average into single reading
  int int_time = 0;   // the sensor integration time (0 because ae is used)
  bool ae = true;     // the auto-exposure flag
  
  // pass in the settings, and take the reading
  get_reading(&infoS, int_time, frame_avg, ae);

  // 1. 
  // write the line
  // first put in the date
  line.concat("0");
  line.concat(",");

  // 2.
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

  // 3.
  // was this recording triggered manually?
  line.concat(String(manual_measurement));
  
  // 4.
  // Then put in the integration time used. Sensor returns this value if ae is used
  line.concat(String(infoS.IntegrationTime));
  line.concat(",");
  
  // 5.
  // Then put in the frame avg
  line.concat(String(frame_avg));
  line.concat(",");

  // 6.
  // Then put in the autoexposure flag
  line.concat(String(ae));
  line.concat(",");

  // 7.
  // Then put in the isSaturated flag. A reading should not be saturated if ae is used.
  line.concat(String(infoS.IsSaturated));
  line.concat(",");

  // 8.
  // Then put in the CIE1931 coords
  line.concat(String(infoS.X));
  line.concat(",");
  line.concat(String(infoS.Y));
  line.concat(",");
  line.concat(String(infoS.Z));
  line.concat(",");

  // 9+.
  /* Then put in the spectrum data. Sensor reads 340-1010 nm in 5nm steps. We are
   only interested in 360-780 in 5nm steps */
  for (int j = ((MIN_WAVELENGTH - SENSOR_MIN_WAVELENGTH) / WAVELENGTH_STEPSIZE); j <= ((MAX_WAVELENGTH - SENSOR_MIN_WAVELENGTH) / WAVELENGTH_STEPSIZE); j++) {
    line.concat(String(infoS.Spectrum[j], CAPTURE_PRECISION)); 
    line.concat(",");
  }

  // Standby seems to clear SpectrumInfo, therefore call it after processing the data
  nsp32.Standby(0);
  
  // write line to Serial for debugging
  Serial.println(line);
  
  // write the data to the SD card
  st.write_line(&line); 
}

void setup_bt() {
  // Initialize Bluetooth:
  Bluefruit.begin();
  // Set power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(-20);
  Bluefruit.setName("NL Sensor");
  bleuart.begin();

  // Start advertising device and bleuart services
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();

   Bluefruit.Advertising.restartOnDisconnect(true);
  // Set advertising interval (in unit of 0.625ms):
  Bluefruit.Advertising.setInterval(32, 244);
  // number of seconds in fast mode:
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(BT_TIMEOUT);  
}


void setup() {
  pinMode(PinReady, INPUT_PULLUP); // use pull-up for ready pin
  pinMode(7, OUTPUT); // onboard LED output
  pinMode(13, INPUT_PULLUP); // pushbutton pull-up input

  digitalWrite(7, HIGH); // turn LED ON
  attachInterrupt(digitalPinToInterrupt(PinReady), PinReadyTriggerISR, FALLING); // enable interrupt for NSP READY
  attachInterrupt(digitalPinToInterrupt(13), PushBtnTriggerISR, FALLING); // enable interrupt for pushbutton
  
  // initialize serial port for "Serial Monitor"
  Serial.begin(115200);
  //while (!Serial); // wait for serial for debugging (this will hang the MCU until plugged into serial monitor) only for debugging
  Serial.println("PROGRAM START. Attempting to initialize SD card...");
  
  // attempt to initialize the SD
  while (true) {
    // initialize SD
    st.init();
    // if the storage object is not errored, continue, otherwise retry
    if (!st.is_errored()) {
      break;
    } else {
      Serial.println("Could not initialize SD card. Retrying...");
    }
  }
  
  Serial.println("SD INITIALIZED. Attempting to initialize NSP32...");

  // initialize NSP32
  nsp32.Init();
  nsp32.Standby(0);
  Serial.println("NSP32 INITIALIZED.");
  
  // start the BT TIME script
  // advertise on bluetooth for 5 minutes to sync time
  // time will be set using a string: DTYYYYMMDDHHMMSS
  setup_bt();

  // time not received after 5 minutes, shut down, try again next power on

  
  // turn off the LED indicating program setup successfully.
  digitalWrite(7, LOW); 
}

// the buffer to write date time into
char bt_buf[16];
int read_index = 0;

void loop() {
  // for sleep offsetting
  unsigned long record_start_ms = millis();

  // if datetime has not been set, loop to set time via BLE
  if (!time_set) {
    // If data has come in via BLE:
    if (bleuart.available()) {
      char c;
      c = (char) bleuart.read();
      bt_buf[read_index] = c; // store the byte into buffer
      read_index++;
      
      // if buffer is full
      if (read_index >= 16) {
        

        
        // if contents are valid
        if (bt_buf[0] == 'D' && bt_buf[1] == 'T') {
          // first convert the buffer to a string
          String dt_string = bt_buf;
          
          int yr = dt_string.substring(2, 6).toInt();
          int mth = dt_string.substring(6, 8).toInt();
          int day = dt_string.substring(8, 10).toInt();

          int hr = dt_string.substring(10, 12).toInt();
          int mn = dt_string.substring(12, 14).toInt();
          int sec = dt_string.substring(14, 16).toInt();

          // set the time 
          
          
        } else {
          // contents are not valid, clear the buffer and keep listening
          Serial.println("Attempted to set invalid datetime.");
        }
        // read new data into buffer
        read_index = 0;
      }
    }
  } else {
    
    // take and record the spectral measurement
    take_measurement(false);
    
    // after the reading is done, calculate how long the data collection took to offset the sleep time
    unsigned long collection_duration = millis() - record_start_ms;
  
    // sleep for some interval before capturing data again
    delay(CAPTURE_INTERVAL - collection_duration);
  }
}

/* NSP32m code */
void PinReadyTriggerISR() {
  // make sure to call this function when receiving a ready trigger from NSP32
  nsp32.OnPinReadyTriggered();
}

/* Pushbutton interrupt */
void PushBtnTriggerISR() {
  take_measurement(true);
}