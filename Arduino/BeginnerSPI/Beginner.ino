/*
 * Copyright (C) 2019 nanoLambda, Inc.
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

#include <SPI.h>


/***********************************************************************************
 * modify this section to fit your need                                            *
 ***********************************************************************************/

	const unsigned int PinRst	= 19;  	// pin Reset
	const unsigned int PinReady	= 23;  	// pin Ready

/***********************************************************************************/

bool isReady = false;

void setup() 
{

	// initialize "ready trigger" pin for accepting external interrupt (falling edge trigger)
	pinMode(PinReady, INPUT_PULLUP);     // use pull-up
  pinMode(PinRst, OUTPUT);
	attachInterrupt(digitalPinToInterrupt(PinReady), PinReadyTriggerISR, FALLING);  // enable interrupt for falling edge
	
	// initialize serial port for "Serial Monitor"
	Serial.begin(115200);
	while (!Serial);
  Serial.println("hello");
  
	// initialize SPI
  
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.begin();

  // RESET NSP32
  Serial.println(isReady);
  resetnl();
  //delay(2000);
  while (!isReady) {}
  isReady = false;
  Serial.println("Reset");
  Serial.println(isReady);
  
  
  // SET the COMMAND BUFFER
  uint8_t m_cmdBuf[5];      // command buffer
  
  m_cmdBuf[0] = 0x03;
  m_cmdBuf[1] = 0xBB;
  m_cmdBuf[2] = 0x01;
  m_cmdBuf[3] = 0x00;
  m_cmdBuf[4] = 0x41;
  //PlaceChecksum(m_cmdBuf, 4);

  uint8_t m_retBuf[5];        // the return buffer
  memset(m_retBuf, 0x01, 5);

  Serial.println("Sending data...");
  //SPI.setClockDivider(SPI_CLOCK_DIV16);  // set SPI baud rate = 2Mbits/s
  SPI.beginTransaction(SPISettings(2000, MSBFIRST, SPI_MODE0));
  pinMode(SS, OUTPUT);
  digitalWrite(SS, LOW);    // SPI SS low
  SPI.transfer(m_cmdBuf, 5);  // send
  digitalWrite(SS, HIGH);   // SPI SS high

  delay(1);
  
  //SPI.setClockDivider(SPI_CLOCK_DIV2);  // set SPI baud rate = 4Mbits/s
  SPI.beginTransaction(SPISettings(4000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS, LOW);    // SPI SS low
  SPI.transfer(m_retBuf, 5);  // receive
  digitalWrite(SS, HIGH);   // SPI SS high
  for (int i =0; i < 5; i++) {
    Serial.println(m_retBuf[i], HEX);
  }


  SPI.end();
}

void resetnl() {
  isReady = false;
  digitalWrite(PinRst, LOW);
  delay(25);
  digitalWrite(PinRst, HIGH);
  delay(50);
}

void loop() 
{
  //Serial.print(".");
  //delay(100);
}

void PinReadyTriggerISR()
{
	// make sure to call this function when receiving a ready trigger from NSP32
  Serial.println("ready");
  isReady = true;
}
