/*
 * Copyright (C) 2019 nanoLambda, Inc.
 * 
 * Modified June 2022 Sadi Wali
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * A clean and simple example for beginners to start with NSP32.
 *
 * Please open "Serial Monitor" in Arduino IDE, make sure the setting is "115200 baud", and then run the example.
 *
 * [NOTE]: According to your board type, you might need to change the pin number assigned at the beginning of the source code.
 *
 * Typical pin layout used:
 * ---------------------------------------------------------
 *               NSP32      Arduino       Arduino   Arduino   
 *  SPI                     Uno/101       Mega      Nano v3 
 * Signal        Pin          Pin           Pin       Pin     
 * -----------------------------------------------------------
 * Wakeup/Reset  RST          8             49        D8      
 * SPI SSEL      SS           10            53        D10     
 * SPI MOSI      MOSI         11 / ICSP-4   51        D11     
 * SPI MISO      MISO         12 / ICSP-1   50        D12     
 * SPI SCK       SCK          13 / ICSP-3   52        D13     
 * Ready         Ready        2             21        D2
 *
 */

#include <ArduinoAdaptor.h>
#include <NSP32.h>
#include <SPI.h>
#include <SD.h>

// CONSTANTS
#define SD_CS_PIN 22                        // pin connected to SD CS
#define SD_EJECT_DETECT_PIN 5               // pin connected to SD EJECT 
#define LOG_FILENAME "LOG.CSV"              // the log filename on the SD

using namespace NanoLambdaNSP32;

const unsigned int PinRst	= 19;  	          // pin Reset
const unsigned int PinReady	= 23;  	        // pin Ready

ArduinoAdaptor adaptor(PinRst);				      // master MCU adaptor
NSP32 nsp32(&adaptor, NSP32::ChannelSpi);	  // NSP32 (using SPI channel)

// The Storage class
class Storage {
  private:
    String log_file_name;                   // file name and directory to save the CSV data log to
    File log_file;                          // the file variable that holds the log file
    int CS_PIN;                             // the SPI chip select pin
    int CHECK_PIN;                          // SD card eject detection pin (HIGH when ejected, LOW when inserted)
    bool ERR = false;                       // was there an error with the Storage class?

    const int DEBOUNCE_DELAY = 1000;        // the debounce time; increase if the output flickers
    int lastSteadyState = LOW;              // the previous steady state from the input pin
    int lastFlickerableState = LOW;         // the previous flickerable state from the input pin
    int currentState;                       // the current reading from the input pin
    unsigned long lastDebounceTime = 0;     // the last time the output pin was toggled
    
  public:
    Storage(int CS, int check_pin, String filename) {
      /* Given the chip select (CS), and the detection pin (CHECK_PIN), load and open the log file to write to. */
      this->CS_PIN = CS;                    // the chip select pin
      this->CHECK_PIN = check_pin;          // for checking SD card ejection
      this->log_file_name = filename;       // log file name and directory
    }

    void init() {
      pinMode(this->CHECK_PIN, INPUT_PULLUP);
      if (!SD.begin(this->CS_PIN)) {
        // unable to open SD card
        this->ERR = true;
        return;
      }
    }

    void open_file() {
      this->log_file = SD.open(this->log_file_name, FILE_WRITE);
    }
    
    void close_file(){
      this->log_file.close();
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
          return false;
    
        // save the the last steady state
        lastSteadyState = currentState;
      }

      return false;
    }

};

// the SD card object
Storage st(SD_CS_PIN, SD_EJECT_DETECT_PIN, LOG_FILENAME);

void setup() 
{
	// initialize "ready trigger" pin for accepting external interrupt (falling edge trigger)
	pinMode(PinReady, INPUT_PULLUP);      // use pull-up for ready pin
  pinMode(7, OUTPUT);                   // LED output
  pinMode(13, INPUT_PULLUP);            // pushbutton
  
  digitalWrite(7, LOW);                 // write LED
	attachInterrupt(digitalPinToInterrupt(PinReady), PinReadyTriggerISR, FALLING);  // enable interrupt for falling edge
	
	// initialize serial port for "Serial Monitor"
	Serial.begin(115200);
	//while (!Serial);                    // wait for serial if prints inside setup function are important (this will hang the MCU until plugged into serial monitor)
  Serial.println("START");

  // 5 attempts to initialize SD (should pass in first attempt)
  for (int i = 0; i < 5; i++) {
    // initialize SD
    st.init();
    if (!st.is_errored()) {
      break;
    }
  }
  
	// initialize NSP32
	nsp32.Init();

	// =============== standby ===============
	nsp32.Standby(0);

	// =============== wakeup ===============
	nsp32.Wakeup();

}

bool pressed = false;             // for help detecting push button press
bool sd_newly_inserted = false;   // for help detecting new SD insertion

void loop() 
{

  if (!st.detect_eject() && sd_newly_inserted == false) {
    Serial.println("Initializing SD");
    sd_newly_inserted = true;
    st.init();
  }

  if (st.detect_eject()) {
    sd_newly_inserted = false;
  }
  
  if (digitalRead(13) == LOW && pressed == false) {
    pressed = true;

    // Get and extract wavelength data
    nsp32.GetWavelength(0);
  
    WavelengthInfo infoW;
    nsp32.ExtractWavelengthInfo(&infoW);  // now we have all wavelength info in infoW, we can use e.g. "infoW.Wavelength" to access the wavelength data array
  
    // Get and extract spectrum data
    nsp32.AcqSpectrum(0, 64, 3, false);   // integration time = 64; frame avg num = 3; disable AE
    
    // "AcqSpectrum" command takes longer time to execute, the return packet is not immediately available
    // when the acquisition is done, a "ready trigger" will fire, and nsp32.GetReturnPacketSize() will be > 0
    while(nsp32.GetReturnPacketSize() <= 0)
    {
      // TODO: can go to sleep, and wakeup when "ready trigger" interrupt occurs
      delay(100);      // wait 100 ms in between waiting for spectrum data to return
      nsp32.UpdateStatus(); // call UpdateStatus() to check async result
    }
    
    SpectrumInfo infoS;
    nsp32.ExtractSpectrumInfo(&infoS);    // now we have all spectrum info in infoW, we can use e.g. "infoS.Spectrum" to access the spectrum data array
    String line = "";
    // for creating the wavelength column headers
  //  for (int i = 0; i < infoW.NumOfPoints; i++) {
  //    line.concat(340+(i*5));
  //    line.concat(",");
  //  }
  
    // clear the line
    line = "";
    // write the line
    for (int i = 0; i< infoW.NumOfPoints; i++) {
      //Serial.println(sizeof(infoS.Spectrum[i]));
      line.concat(String(infoS.Spectrum[i], 18));     // write with 18 digits (precise)
      //Serial.println(String(infoS.Spectrum[i], 18));
      line.concat(",");
    }
    write_to_sd(line);                    // write the data to the SD card
    
    // wait 10 seconds if running on auto mode
    //delay(1000*10);
  }

  // for help with pushbutton detection
  if (digitalRead(13) == HIGH) {
    pressed = false;
  }
}

void write_to_sd(String line) {
  // turn on the LED to indicate writing
  Serial.println("writing...");
  digitalWrite(7, HIGH);
  st.open_file();
  delay(100);
  st.write_line(line);
  delay(100);
  st.close_file();
  digitalWrite(7, LOW);
  Serial.println("finished...");
  
}

void PinReadyTriggerISR()
{
	// make sure to call this function when receiving a ready trigger from NSP32
	nsp32.OnPinReadyTriggered();
}
