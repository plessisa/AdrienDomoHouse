#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "Transmiter.h"
#include <DHT.h>

int ScrVer = 4;

// This are related to the sensor DHT22
#define DHTPIN 2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// This are related to the IR sensor
int irPin = 4;
int irMotiondetected = 0;

// This are frequency captor
int cycle = 0;

void setup() {
  Serial.begin(9600);
  Serial.print("DHT22 Humidity and Temperature Digital Transmiter v");
  Serial.print(ScrVer);
  Serial.print(", libAdrien v");
  Serial.println (LibVer);
  delay(1000);          // wait for sensor initialization
  dht.begin();
  RadioStart(ChannelTemp1);
  pinMode(irPin, INPUT);
}

void loop() {
  // 1 cycle is 1 sec
  if (cycle == 10) {
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float flHumi = dht.readHumidity();
    // Read temperature as Celsius
    float flTemp = dht.readTemperature();
    
    // Check if any reads failed and exit early (to try again).
    if (isnan(flHumi) || isnan(flTemp)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    } else {
      // Transmit the data
      RadioTransmit(HUMI, 1, flHumi);
      delay(500); 
      RadioTransmit(TEMP, 1, flTemp);
      delay(500); 
    }
    RadioTransmit(MOVE, 1, irMotiondetected);
    irMotiondetected = 0;
    delay(500);
    cycle = 0; 
  }
  if (irMotiondetected == 0) {
    irMotiondetected = digitalRead(irPin);
  }
  cycle++; 
  delay(1000); 
}
