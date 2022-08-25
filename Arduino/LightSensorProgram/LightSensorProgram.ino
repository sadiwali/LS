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

#include <ArduinoAdaptor.h>                         // for low-level interfacing with the NSP via Arduino
#include "Adafruit_TinyUSB.h"                       // for serial communication on nrf52840 
#include <NSP32.h>                                  // for high-level interfacing with the NSP module
#include <SPI.h>                                    // Arduino SPI 
#include <SD.h>                                     // Arduino SD
#ifdef BLUETOOTH
#include <bluefruit.h>                              // Bluetooth
#endif
#include <TimeLib.h>                                // Date and time

using namespace NanoLambdaNSP32;

// DEBUG
#define NO_SAVE               true                  // do not save measurements (for debugging SD)
#define BLUETOOTH             false

// CONSTANTS
#define LOG_FILENAME          "LOG1"                // the log filename to write to
#define METADATA_SUFFIX       "_metadata"           // the suffix to indicate metadata file
#define LOG_FILE_EXT          ".CSV"                // the log file extension
#define DEF_CAPTURE_INTERVAL  5000                  // how frequently to capture data in ms
#define MIN_WAVELENGTH        340                   // the minimum wavelength we are interested in
#define MAX_WAVELENGTH        1010                  // the maximum wavelength we are interested in
#define SENSOR_MIN_WAVELENGTH 340                   // the minimum sensing wavelength (for W1 sensor)
#define SENSOR_MAX_WAVELENGTH 1010                  // the maximum sensing wavelength  (for W1 sensor)
#define WAVELENGTH_STEPSIZE   5                     // the sensor wavelength resolution (5 nm for W1 sensor)
#define CAPTURE_PRECISION     18                    // how many digits of precision to write (18 digits)
#define BT_TIMEOUT            1000 * 60             // how long to advertise for synchronizing time in ms

// PINS
#define SD_CS_PIN             4                     // pin connected to SD CS
#define NSP_CS_PIN            5                     // pin connected to NSP CS
#define NSP_RESET             28                    // NSP reset pin
#define NSP_READY             29                    // NSP ready pin
const unsigned int PinRst = NSP_RESET;              // pin Reset
const unsigned int PinSS = NSP_CS_PIN;              // NSP chip select pin
const unsigned int PinReady = NSP_READY;            // pin Ready

// VARIABLES
// DATE TIME
String myTime = "HHMMSS";                           // the starting time
String myDate = "YYYYMMDD";                         // the starting date
int logging_interval = DEF_CAPTURE_INTERVAL;        // the data logging interval
time_t cTime = now();
bool time_set = false;                              // has the time been set?
unsigned long data_counter = 10;                     // how many data points have been collected this session?
bool paused = false;                                // is data capture paused?


// NSP32m
ArduinoAdaptor adaptor(PinRst, PinSS);              // master MCU adaptor
NSP32 nsp32( & adaptor, NSP32::ChannelSpi);         // NSP32 (using SPI channel)

/* The storage class deals with the microSD card, and file management within the card. */
class Storage {
  private:
    String log_file_name;                           // file name and directory to save the CSV data log to
    File log_file;                                  // the file variable that holds the log file
    int CS_PIN;                                     // the SPI chip select pin
    bool ERR = false;                               // was there an error with the Storage class?

  public:
  /* Initialize the class with chip select pin and file name */
    Storage(int CS, String filename) {
      // set the chip select pin for SPI, and the log file name
      this -> CS_PIN = CS;                          
      this -> log_file_name = filename + LOG_FILE_EXT;
    }

    /* Attempt to initialize the SD card reader. If unsuccessful, set error flag */
    void init() {
      // SD is not opened if NO_SAVE is true
      if (!NO_SAVE && !SD.begin(this -> CS_PIN) ) {
        // unable to open SD card, set error flag
        this -> ERR = true;
        return;
      }
      
      // open the metadata file and read data counter if it exists
      String metadata_file_name = log_file_name + METADATA_SUFFIX + LOG_FILE_EXT;
      if (SD.exists(metadata_file_name)) {
        File metadata_file = SD.open(metadata_file_name, FILE_READ);
        
        if (!metadata_file) {
          this->ERR = true;
          return;
        }
        
        String line = metadata_file.readStringUntil('\n');
        // read the integer into data_counter
        data_counter = line.toInt();
        metadata_file.close();
      }
    }

    /* Once the SD card reader is initialized, attempt to open the log file */
    void open_file() {
      // attempt to open SD card for writing
      while (!this->log_file) {
        this->log_file = SD.open(this->log_file_name, FILE_WRITE);
      }
  
      // file opened, check if new file
      if (this->log_file.size() == 0) {
        // brand new file, add headers
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

    void delete_file(String filename = LOG_FILENAME) {
      SD.remove(filename + LOG_FILE_EXT);
      SD.remove(filename + METADATA_SUFFIX + LOG_FILE_EXT);
    }
  
    /* Open the file, Write a line, then close the file. */
    void write_line(String *line) {
      data_counter++;
      if (NO_SAVE) return; // skip if no save flag is set
      this->open_file();
      this->log_file.println(*line);
      
      this->close_file();

      // open the metadata file and write counter to it
      String metadata_file_name = log_file_name + METADATA_SUFFIX + LOG_FILE_EXT;
      if (SD.exists(metadata_file_name)) {
        // delete the file before updating
        SD.remove(metadata_file_name);
      }
      // create it brand new
      File metadata_file = SD.open(metadata_file_name, FILE_WRITE);
      
      if (!metadata_file) {
        this->ERR = true;
        return;
      }
      
      metadata_file.println(data_counter);
      metadata_file.close();
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
  
    /* If SD is errored, return true, else return false. */
    bool is_errored() {
      return this->ERR;
    }
};

// Define the SD card object
Storage st(SD_CS_PIN, LOG_FILENAME);

/* Returns: the current date and time in the following format 'YYYY/MM/DD HH:MM:SS' */
String get_current_datetime() {
  return "";
}

/* Get a reading from the NSP32 sensor */
void get_reading(SpectrumInfo *info, int int_time= 0, int frame_avg = 3, bool ae = true) {
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

/* Take a manual or automatic measurement. */
String take_measurement(bool manual_measurement=false) {
  digitalWrite(7, HIGH); // turn on LED
  SpectrumInfo infoS; // variable for storing the spectrum data
  
  // format each line of CSV file into this line
  String line = "";

  // settings for the reading
  int frame_avg = 3;  // how many frames to average into single reading
  int int_time = 0;   // the sensor integration time (0 because ae is used)
  bool ae = true;     // the auto-exposure flag
  
  // pass in the settings, and take the reading from the NSP sensor
  get_reading(&infoS, int_time, frame_avg, ae);
  
  // 1. Which date was this data point collected on?
  line.concat(String(day()));
  line.concat("/");
  line.concat(String(month()));
  line.concat("/");
  line.concat(String(year()));
  line.concat(",");

  // 2. When was this data point collected?
//  unsigned long ms = millis();
//  int seconds = (ms / 1000) % 60 ;
//  int minutes = ((ms / (1000*60)) % 60);
//  int hours   = ((ms / (1000*60*60)) % 24);
  
  line.concat(String(hour()));
  line.concat(":");
  line.concat(String(minute()));
  line.concat(":");
  line.concat(String(second()));
  line.concat(",");

  // 3. Was this recording triggered manually?
  line.concat(String(manual_measurement));
  
  // 4. What integration time was used?
  line.concat(String(infoS.IntegrationTime));
  line.concat(",");
  
  // 5. What frame average number was used?
  line.concat(String(frame_avg));
  line.concat(",");

  // 6. Was auto-exposure used?
  line.concat(String(ae));
  line.concat(",");

  // 7. Was the reading saturated? A reading should not be saturated if ae is used.
  line.concat(String(infoS.IsSaturated));
  line.concat(",");

  // 8. Then put in the CIE1931 coords
  line.concat(String(infoS.X));
  line.concat(",");
  line.concat(String(infoS.Y));
  line.concat(",");
  line.concat(String(infoS.Z));
  line.concat(",");

  // 9. Then put in the spectrum data. Sensor reads 340-1010 nm (inclusive) in 5nm increments. */
  for (int j = ((MIN_WAVELENGTH - SENSOR_MIN_WAVELENGTH) / WAVELENGTH_STEPSIZE); j <= ((MAX_WAVELENGTH - SENSOR_MIN_WAVELENGTH) / WAVELENGTH_STEPSIZE); j++) {
    line.concat(String(infoS.Spectrum[j], CAPTURE_PRECISION)); 
    line.concat(",");
  }

  // Standby seems to clear SpectrumInfo, therefore call it after processing the data
  nsp32.Standby(0);
    
  // write the data to the SD card
  st.write_line(&line); 

  digitalWrite(7, LOW); // turn off LED
  return line;
}

///* Setup BLE */
//void setup_bt() {
//  // Initialize Bluetooth:
//  Bluefruit.begin();
//  // Set power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
//  Bluefruit.setTxPower(-20);
//  Bluefruit.setName("NL Sensor");
//  bleuart.begin();
//
//  // Start advertising device and bleuart services
//  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
//  Bluefruit.Advertising.addTxPower();
//  Bluefruit.Advertising.addService(bleuart);
//  Bluefruit.ScanResponse.addName();
//
//   Bluefruit.Advertising.restartOnDisconnect(true);
//  // Set advertising interval (in unit of 0.625ms):
//  Bluefruit.Advertising.setInterval(32, 244);
//  // number of seconds in fast mode:
//  Bluefruit.Advertising.setFastTimeout(30);
//  Bluefruit.Advertising.start(BT_TIMEOUT);  
//}

/* Arduino setup function. */
void setup() {
  pinMode(PinReady, INPUT_PULLUP); // use pull-up for ready pin
  pinMode(7, OUTPUT); // onboard LED output
  pinMode(13, INPUT_PULLUP); // pushbutton pull-up input

  digitalWrite(7, HIGH); // turn LED ON
  attachInterrupt(digitalPinToInterrupt(PinReady), PinReadyTriggerISR, FALLING); // enable interrupt for NSP READY
  attachInterrupt(digitalPinToInterrupt(13), PushBtnTriggerISR, FALLING); // enable interrupt for pushbutton
  attachInterrupt(digitalPinToInterrupt(PIN_SERIAL_RX), SerialPlugIn, FALLING); // enable interrupt for serial
  // initialize serial port for "Serial Monitor"
  Serial.begin(115200);
  //while (!Serial); // wait for serial for debugging (this will hang the MCU until plugged into serial monitor) only for debugging
  
  // attempt to initialize the SD
  while (true) {
    // initialize SD
    st.init();
    // if the storage object is not errored, continue, otherwise retry
    if (!st.is_errored()) {
      break;
    } else {
    }
  }
  
  // initialize NSP32
  nsp32.Init();
  nsp32.Standby(0);

  // turn off the LED indicating program setup successfully.
  digitalWrite(7, LOW); 
}

// the serial buffer and pointer
char ser_buffer[32];
int read_index = 0;

/* Arduino loop function */
void loop() {
  if (Serial) {
    // cable plugged in
    digitalWrite(7, HIGH);
    if (Serial.available() > 0) {
      // data available
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
        // end of line reached, or buffer has been filled, read the buffer
        read_index = 0;

        if (ser_buffer[0] == '0' && ser_buffer[1] == '0') {
          // 00: Toggle data capture while plugged in
          Serial.println("OK. ToggleDataCapture");
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '1') {
          // 01: collect a data point manually
          String manual_data = take_measurement(true);
          Serial.println("DATA");
          Serial.println(manual_data);
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '2') {
          // 02: export all data line by line

          // add the data export header
          Serial.println("DATA");
          // how many lines to expect
          Serial.println("0");
          // print all lines of code
          
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '3') {
          // 03: Delete the data logging file
          st.delete_file();
          Serial.println("OK. Log file deleted");
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '4') {
          // 04: Set new collection interval
          // create string with serial buffer
          String instruction = String(ser_buffer);
          int new_logging_interval = instruction.substring(instruction.indexOf("_")+1).toInt();
          logging_interval = new_logging_interval;
          Serial.println("OK. SetCollectionInterval");
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '5') {
          // 05: Set date and time
          String instruction = String(ser_buffer);
          
          int y = instruction.substring(2, 6).toInt();
          int mth = instruction.substring(6, 8).toInt();
          int d = instruction.substring(8, 10).toInt();

          int h = instruction.substring(10, 12).toInt();
          int m = instruction.substring(12, 14).toInt();
          int s = instruction.substring(14, 16).toInt();
          
          setTime(h,m,s,d,mth,y);
          Serial.println("OK. Date set");
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '6') {
          // 06: list number of entries
          Serial.println("DATA");
          Serial.println(data_counter);
        } else {
          String instruction = String(ser_buffer);
          Serial.print("Err. ");
          Serial.println(instruction);
        }
      }
    }

    // if the correct interval has passed, capture a data point unless paused.
    
  
  } else {
    // cable not plugged in
    digitalWrite(7, LOW);
    
    // for sleep offsetting
    unsigned long record_start_ms = millis();

    // take and record the spectral measurement
    take_measurement(false);
    
    // after the reading is done, calculate how long the data collection took to offset the sleep time
    unsigned long collection_duration = millis() - record_start_ms;
  
    // sleep for some interval before capturing data again
    delay(logging_interval - collection_duration);

  }  
}

void blinkLed(int num) {
  for (int i = 0; i < num; i++) {
    digitalWrite(7, HIGH);
    delay(250);
    digitalWrite(7, LOW);
    delay(250);
  }
}

/* NSP32m ready interrupt */
void PinReadyTriggerISR() {
  // make sure to call this function when receiving a ready trigger from NSP32
  nsp32.OnPinReadyTriggered();
}

/* Pushbutton interrupt. When the pushbutton is pressed, take a manual measurement. */
void PushBtnTriggerISR() {
  //take_measurement(true);
  blinkLed(2);
}

void SerialPlugIn() {
  Serial.println("Hello");
}
