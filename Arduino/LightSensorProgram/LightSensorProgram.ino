/**
 * Sadi Wali August 2022
 * Last modified: September 07 2022
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
 * 
 * 
 * 
 * Error codes:
 * ------------------------------------
 * 
 * Error         Number of
 *               flashes
 * ------------------------------------
 * SD card       1
 * Internal FS   2
 * 
 * 
 **/

#include "Adafruit_TinyUSB.h"                       // for serial communication on nrf52840 
#include <SPI.h>                                    // Arduino SPI 
#include <TimeLib.h>                                // Date and time
#include <ArduinoAdaptor.h>                         // for low-level interfacing with the NSP via Arduino
#include <NSP32.h>                                  // for high-level interfacing with the NSP module
#include "DataStorage.h"                            // Data storage module
#include <Adafruit_LittleFS.h>                      // persistent data storage       
#include <InternalFileSystem.h>                     // persistent data storage

using namespace NanoLambdaNSP32;
using namespace Adafruit_LittleFS_Namespace;

const unsigned int PinRst = NSP_RESET;              // pin Reset
const unsigned int PinSS = NSP_CS_PIN;              // NSP chip select pin
const unsigned int PinReady = NSP_READY;            // pin Ready

// VARIABLES
int logging_interval = DEF_CAPTURE_INTERVAL;        // the data logging interval
bool paused = false;                                // is data capture paused?
char ser_buffer[16];                                // the serial buffer
int read_index = 0;                                 // the serial buffer read index
String device_name = DEV_NAME_PREFIX;               // the device name for easier identification
int int_time = 500;                                 // default integration time for sensor
int mod_int_time = int_time;                    
int frame_avg = DEF_FRAME_AVG;                      // how many frames to average
int mod_frame_avg = frame_avg;
bool ae = true;                                     // use autoexposure?
bool mod_ae = ae;
unsigned int data_counter = 0;                      // how many data points collected and stored

// OBJECTS
ArduinoAdaptor adaptor(PinRst, PinSS);              // master MCU adaptor
NSP32 nsp32( & adaptor, NSP32::ChannelSpi);         // NSP32 (using SPI channel)
Storage st(SD_CS_PIN, LOG_FILENAME);                // the data storage object
Adafruit_LittleFS_Namespace::File file(InternalFS); // the persistent file stored in persistent flash

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
  nsp32.ExtractSpectrumInfo(info); 
}

/* Check  */
int validate_reading(SpectrumInfo infoS) {
  if (infoS.IntegrationTime == 1 || infoS.Y <= MIN_ACCEPTABLE_Y) {
    // Too dark
    return -1;
  }

  if (infoS.IsSaturated == 1) {
    return 1;
  }

  return 0; 
}

/* Take a manual or automatic measurement. */
String take_measurement(bool manual_measurement=false) {
  digitalWrite(7, HIGH); // turn on LED
  SpectrumInfo infoS; // variable for storing the spectrum data
  
  bool dark_flag = true;
  bool too_dark = false;

  read_sensor(&infoS, mod_int_time, mod_frame_avg, mod_ae);

  while (true) {
    
    if (mod_int_time >= 800) {
      // raising integration time did not improve reading
      too_dark = true;
      break;
    }
    
    // pass in the settings, and take the reading from the NSP sensor
    read_sensor(&infoS, mod_int_time, mod_frame_avg, mod_ae);

    // validate the results
    int validation_res = validate_reading(infoS);
    
    if (validation_res == 1) {
      // too bright, nothing to do
      break;
    } else if (validation_res == -1) {
      // too dark, try upping integration time until not saturated
      
      // turn off auto-exposure, and set frame average to half to speed up finding the right integration time
      mod_ae = false;
      mod_frame_avg = frame_avg / 2;
      
      if (dark_flag) {
        dark_flag = false;
        mod_int_time = 500;
        continue;
      }

      mod_int_time += 50;
      
    } else {
      // no issues, break out of loop
      break;
    }
  }

  // format each line of CSV file into this line
  String line = format_line(infoS, manual_measurement, too_dark);

  // Standby seems to clear SpectrumInfo, therefore call it after processing the data
  nsp32.Standby(0);

  // reset the ae, int_time, and frame_avg
  mod_ae = ae;
  mod_int_time = int_time;
  mod_frame_avg = frame_avg;

  // write the data to the SD card
  bool write_res = st.write_line(&line);
  if (!write_res) {
    error_state(3);
  }
  
  data_counter++;
  
  update_memory();
  
  // turn off LED
  digitalWrite(7, LOW); 
  
  return line;
}

String format_line(SpectrumInfo infoS, bool manual_measurement, bool too_dark) {
  String line = "";
  // 1. Which date was this data point collected on?
  line += String(day());
  line += "/";
  line += String(month());
  line += "/";
  line += String(year());
  line += ",";

  // 2. When was this data point collected?  
  line += String(hour());
  line += ":";
  line += String(minute());
  line += ":";
  line += String(second());
  line += ",";

  // 3. Was this recording triggered manually?
  line += String(manual_measurement);
  line += ",";
  
  // 4. What integration time was used?
  line += String(infoS.IntegrationTime);
  line += ",";
  
  // 5. What frame average number was used?
  line += String(mod_frame_avg);
  line += ",";

  // 6. Was auto-exposure used?
  line += String(mod_ae);
  line += ",";

  // 7. Was the reading saturated? A reading should not be saturated if ae is used.
  line += String(infoS.IsSaturated);
  line += ",";

  // 8. Was the lighting too dark when this reading took place? This reading should not be used.
  line += String(too_dark);
  line += ",";

  // 9. Then put in the CIE1931 values
  line += String(infoS.X);
  line += ",";
  line += String(infoS.Y);
  line += ",";
  line += String(infoS.Z);
  line += ",";

  // 10. Then put in the spectrum data. Sensor reads 340-1010 nm (inclusive) in 5 nm increments
  for (int j = ((MIN_WAVELENGTH - SENSOR_MIN_WAVELENGTH) / WAVELENGTH_STEPSIZE);
       j <= ((MAX_WAVELENGTH - SENSOR_MIN_WAVELENGTH) / WAVELENGTH_STEPSIZE);
       j++) {
    line += String(infoS.Spectrum[j] * CALIBRATION_FACTOR, CAPTURE_PRECISION); 
    line += ",";
  }
  return line;
}

/* Update the persistent storage */
void update_memory() {
  // overwrite the file
  String filename = "/" + String(METADATA_FILENAME) + String(FILE_EXT);
  InternalFS.remove(filename.c_str());
  if (file.open(filename.c_str(), FILE_O_WRITE)) {
    String writeline = device_name + "," + String(logging_interval) + "," + String(data_counter);
    file.write(writeline.c_str(), strlen(writeline.c_str()));
    file.close();
  } else {
    // could not create file
    error_state(2);
  }
}

/* Infinite loop LED to indicate an error. */
void error_state(int code) {
  while (true) {
    for (int i = 0; i < code; i ++) {
      digitalWrite(7, HIGH);
      delay(250);
      digitalWrite(7, LOW);
      delay(250);
    }
    delay(500);
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
  //while (!Serial) delay(10);
  
  // initialize the persistent storage
  // structure looks like: "device_name,logging_interval,data_counter"  
  InternalFS.begin();
//  delay(1000);
//  InternalFS.format();
//  Serial.println("FORMAT COMPLETE");
//  return;
  String filename = "/" + String(METADATA_FILENAME) + String(FILE_EXT);
  file.open(filename.c_str(), FILE_O_READ);

  if (file) {
    // file exits, read the line
    uint32_t readlen;
    char buffer[64] = {0};
    readlen = file.read(buffer, sizeof(buffer));
    buffer[readlen] = '\0';
    String readline = String(buffer);
    
    // parse the line
    int delimiters[2];
    int d_count = 0;
    
    for (int i = 0; i < readlen; i++) {
      // loop through the character array
      if (buffer[i] == ',') {
        delimiters[d_count] = i;
        d_count++;
      }
    }
    
    device_name = readline.substring(0, delimiters[0]);
    logging_interval = readline.substring(delimiters[0] + 1, delimiters[1]).toInt();
    data_counter = readline.substring(delimiters[1] + 1).toInt();
    
    file.close();
  } else {
    update_memory();
  }

  // attempt to initialize the SD
  
  st.init();
  // if the SD storage object is errored, go into error state
  if (st.is_errored()) {
    error_state(1);
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
    
    // TODO: print the status of the sensor, is it reading? paused? etc.
    
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
          // TODO implement
          Serial.println("OK");
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '1') {
          // 01: collect a data point manually
          String manual_data = take_measurement(true);
          Serial.println("DATA");
          Serial.println(manual_data);
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '2') {
          // 02: export all data line by line
          // add the data export header
          Serial.println("DATA");

          st.open_file();

          for (int i = 0; i < data_counter + 1; i++) {
            String line = st.read_line(i,5000);
            Serial.println(line);
          }
          st.close_file();
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '3') {
          // 03: Delete the data logging file
          st.delete_file();
          InternalFS.format();
          Serial.println("OK");
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '4') {
          // 04: Set new collection interval
          // create string with serial buffer
          String instruction = String(ser_buffer);
          int new_logging_interval = instruction.substring(instruction.indexOf("_")+1).toInt();
          logging_interval = new_logging_interval;
          update_memory();
          Serial.println("OK");
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
          Serial.println("OK");
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '6') {
          // 06: list number of entries
          Serial.println("DATA");
          Serial.println(data_counter);
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '7') {
          // 07: Hello
          Serial.println("Hello");
          // send name
          Serial.println(device_name);
          // send interval
          Serial.println(logging_interval);
          // send status
          Serial.println(paused);
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '8') {
          // 08: Set device name
          String instruction = String(ser_buffer);
          device_name = DEV_NAME_PREFIX + instruction.substring(2);
          update_memory();
          Serial.println("OK");
        } else if (ser_buffer[0] == '0' && ser_buffer[1] == '9') {
          // 09: Get device information
          Serial.println("DATA");
          String to_ret = "device_name: " + device_name + " data_points: " + 
                          String(data_counter) + " uptime: " + String(millis()/3600000, 8) + "h Logging interval: " + 
                          String(logging_interval) + "ms";
          Serial.println(to_ret);
        } else {
          Serial.println("Err '" + String(ser_buffer) + "'");

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
