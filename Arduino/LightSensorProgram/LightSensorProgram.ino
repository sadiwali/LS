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

#include "Adafruit_TinyUSB.h"                       // for serial communication on nrf52840 
#include <SPI.h>                                    // Arduino SPI 
#include <SD.h>                                     // Arduino SD
#include <TimeLib.h>                                // Date and time
#include <ArduinoAdaptor.h>                         // for low-level interfacing with the NSP via Arduino
#include <NSP32.h>                                  // for high-level interfacing with the NSP module
#include "DataStorage.h"
using namespace NanoLambdaNSP32;


const unsigned int PinRst = NSP_RESET;              // pin Reset
const unsigned int PinSS = NSP_CS_PIN;              // NSP chip select pin
const unsigned int PinReady = NSP_READY;            // pin Ready

// VARIABLES
int logging_interval = DEF_CAPTURE_INTERVAL;        // the data logging interval
unsigned long data_counter = 0;                     // how many data points have been collected this session?
bool paused = false;                                // is data capture paused?
char ser_buffer[16];                                // the serial buffer
int read_index = 0;                                 // the serial buffer read index

// OBJECTS
ArduinoAdaptor adaptor(PinRst, PinSS);              // master MCU adaptor
NSP32 nsp32( & adaptor, NSP32::ChannelSpi);         // NSP32 (using SPI channel)
Storage st(SD_CS_PIN, LOG_FILENAME);                // the data storage object


/* Get a reading from the NSP32 sensor */
void read_sensor(SpectrumInfo *info, int int_time= 0, int frame_avg = 3, bool ae = true) {
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
  read_sensor(&infoS, int_time, frame_avg, ae);
  
  // 1. Which date was this data point collected on?
  line.concat(String(day()));
  line.concat("/");
  line.concat(String(month()));
  line.concat("/");
  line.concat(String(year()));
  line.concat(",");

  // 2. When was this data point collected?  
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

  // 9. Then put in the spectrum data. Sensor reads 340-1010 nm (inclusive) in 5nm increments.
  for (int j = ((MIN_WAVELENGTH - SENSOR_MIN_WAVELENGTH) / WAVELENGTH_STEPSIZE); j <= ((MAX_WAVELENGTH - SENSOR_MIN_WAVELENGTH) / WAVELENGTH_STEPSIZE); j++) {
    line.concat(String(infoS.Spectrum[j] * CALIBRATION_FACTOR, CAPTURE_PRECISION)); 
    line.concat(",");
  }

  // Standby seems to clear SpectrumInfo, therefore call it after processing the data
  nsp32.Standby(0);

  // write the data to the SD card
  st.write_line(&line); 

  // increment data counter
  data_counter++;
  
  // turn off LED
  digitalWrite(7, LOW); 
  
  return line;
}

/* Infinite loop LED to indicate an error. */
void error_state() {
  while (true) {
    digitalWrite(7, HIGH);
    delay(250);
    digitalWrite(7, LOW);
    delay(250);
  }
}

/* Arduino setup function. */
void setup() {
  pinMode(PinReady, INPUT_PULLUP); // use pull-up for ready pin
  pinMode(7, OUTPUT); // onboard LED output
  digitalWrite(7, HIGH); // turn LED ON
  attachInterrupt(digitalPinToInterrupt(PinReady), PinReadyTriggerISR, FALLING); // enable interrupt for NSP READY
  // initialize serial port
  Serial.begin(115200);

  // attempt to initialize the SD
  while (true) {
    // initialize SD
    st.init();
    // if the storage object is not errored, continue, otherwise retry
    if (!st.is_errored()) {
      break;
    } else {
      // the SD card could not be initialized
    }
  }

  // initialize NSP32
  nsp32.Init();
  nsp32.Standby(0);
}

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
        // if character is not EOL
        ser_buffer[read_index] = c;
        read_index ++;
      } else {
        end_of_line = true;
        // mark the end of buffer for String conversion
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
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '7') {
          // 07: Hello
          Serial.println("Hello");
        } else {
          String instruction = String(ser_buffer);
          Serial.print("Err. ");
          Serial.println(instruction);
        }
      }
    }  
  
  } else {
    // cable not plugged in

    // if time has not been set, do nothing
    if (timeStatus() == timeNotSet) {
      return;
    }
    
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

/* NSP32m ready interrupt */
void PinReadyTriggerISR() {
  // make sure to call this function when receiving a ready trigger from NSP32
  nsp32.OnPinReadyTriggered();
}
