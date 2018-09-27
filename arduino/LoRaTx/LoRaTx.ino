/* hermit-tx.ino */
/*
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
*/

// Arduino pins for LoRa DRF1278F
// Arduino pin -> DRF1278F pin, Function
// D7 -> LED2 (to show when LoRa module is active)
// D8 -> 1 RESET
// D9 -> 4 DIO2, used for PWM 
// D10 -> 13 NSS
// D11 -> 12 MOSI
// D12 -> 11 MISO 
// D13 -> 10 SCK 

// Arduino pins for uBlox
// D2 to GPS Tx
// D3 to GPS Rx 

// Other pins
// A0 -> main power supply voltage sensing

#include "uBloxLib.h";
#include "SPI.h";
#include <SoftwareSerial.h>;  // remember to increase buffer to 128 in SoftwareSerial.h #define _SS_MAX_RX_BUFF 128 // RX buffer size TODO
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme; // I2C
//Adafruit_BME280 bme(BME_CS); // hardware SPI
//Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // software SPI

unsigned long delayTime;
SoftwareSerial GPS(2,3);	// RX, TX

// Pin assignments
const byte LoRa_NSS = 10;   //LoRa NSS
const byte LoRa_PWM = 9;    //DIO2 used for PWM/Tone generation
const byte LoRa_Reset = 8;  //LoRa RESET
const byte LoRa_Active = 7; // LoRa Activity LED
const byte V_Sense = 0;     // A0 used to sense main power supply


//LoRa Tx/Rx constants
const float LoRa_Freq = 434.700;   // change this to change standard frequency
const byte LoRa_PktSystem = 1;     // system message (main power voltage), 1 byte
const byte LoRa_PktMin = 5;        // minimum gps info (lat, long, fix/HDOP), 9 bytes
const byte LoRa_PktGPS = 10;       // messages containing full location info, 23 bytes

const float uBlox_GoodHDOP = 5.50f;   // define threshold for what constitutes a good HDOP lock for take off

#include "LoRaTX.h";

uBlox_t uBlox;  //defined uBlox structure. Need struct in its own header file to work around Arduino limitations
#include "Utils.h";

String InputString = "";     //data in buffer is copied to this string
String Outputstring = "";
int GpsFix = false;
int GpsHDOP = 0;
byte iCtr = 0;      // used to keep track of when to send full message
char sSerialize[23];  // serialized string
int Smoke = 0;
int Fire = 0;
int Temperature = 0;
int Humidity = 0;
int Altitude = 0;
int Pressure = 0;
int Lat = 0;
int Lon = 0;

void setup()
{
  Serial.begin(9600);
  Serial.println("LoRa Tx");
  Serial.println();

  //BME Init
  bool status;

  //Dummy GPS data
  Lat = 23000;
  Lon = 5000;
  
  // default settings
  // (you can also pass in a Wire library object like &Wire2)
  status = bme.begin();  
  if (!status) {
      Serial.println("Could not find a valid BME280 sensor, check wiring!");
  }
    
  // LoRa init
  pinMode(LoRa_Active, OUTPUT);
  digitalWrite(LoRa_Active, LOW);
  pinMode(LoRa_Reset, OUTPUT);
  digitalWrite(LoRa_Reset, LOW);
  pinMode (LoRa_NSS, OUTPUT);
  digitalWrite(LoRa_NSS, HIGH);
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV2);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  LoRa_ResetDev();			//Reset LoRa
  LoRa_Setup();				//Initialize LoRa
  LoRa_SetFreqF(LoRa_Freq); 

  // GPS init. Use uBlox poll mode
 
}

void GetReadings()
{
  ReadSmoke();
  ReadFire();
  ReadBME();
}

void ReadSmoke()
{
  Smoke = analogRead(A0); 
  
  Serial.println("Smoke:"+ String(Smoke)+".");
  
  sSerialize[0]=Smoke/1000+ 0x30;
  Smoke=Smoke%1000;
  sSerialize[1]=Smoke/100+0x30;
  Smoke=Smoke%100;
  sSerialize[2]=Smoke/10+0x30;
  sSerialize[3]=Smoke%10+0x30;
}

void ReadFire()
{ 
  Fire = analogRead(A1);  
  Serial.println("Fire:"+ String(Fire)+".");
    
  sSerialize[4]=Fire/1000+ 0x30;
  Fire=Fire%1000;
  sSerialize[5]=Fire/100+0x30;
  Fire=Fire%100;
  sSerialize[6]=Fire/10+0x30;
  sSerialize[7]=Fire%10+0x30;
}

void ReadBME()
{ 
  Temperature=(int)bme.readTemperature();
  Pressure=(int)(bme.readPressure() / 100.0F);
  Altitude = (int)bme.readAltitude(SEALEVELPRESSURE_HPA);
  Humidity=(int)bme.readHumidity();
  
  Serial.print("Temperature = ");
  Serial.print(Temperature);
  Serial.println(" *C");
  sSerialize[8]=Temperature/10+0x30;
  sSerialize[9]=Temperature%10+0x30;

  Serial.print("Pressure = ");
  Serial.print(Pressure);
  Serial.println(" hPa");
  sSerialize[10]=Pressure/1000+ 0x30;
  Pressure=Pressure%1000;
  sSerialize[11]=Pressure/100+0x30;
  Pressure=Pressure%100;
  sSerialize[12]=Pressure/10+0x30;
  sSerialize[13]=Pressure%10+0x30;

  Serial.print("Approx. Altitude = ");
  Serial.print(Altitude);
  Serial.println(" m");
  sSerialize[14]=Altitude/100+0x30;
  Altitude=Altitude%100;
  sSerialize[15]=Altitude/10+0x30;
  sSerialize[16]=Altitude%10+0x30;

  Serial.print("Humidity = ");
  Serial.print(Humidity);
  Serial.println(" %");
  sSerialize[17]=Humidity/10+0x30;
  sSerialize[18]=Humidity%10+0x30;

  Serial.println();
}

void loop()
{
  Serial.print("Free RAM ");
  Serial.println(freeRam());
  
  GetReadings();
  LoRa_SetModem(LoRa_BW41_7, LoRa_SF12, LoRa_CR4_5, LoRa_Explicit, LoRa_LowDoptON); //Setup the LoRa modem parameters, lowest speed
  
  Serial.print("Send long message= ");
  Serial.println(iCtr);
  Serial.println(LoRa_SendStr(sSerialize, 19, LoRa_PktGPS, 10, 10));  
  iCtr = 0;
  
  delay(5000);
}
