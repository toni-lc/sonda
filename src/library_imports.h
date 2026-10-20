#if USE_DS
#include <OneWire.h> // DS18B20 sensor library.
#endif

#if USE_DHT
#include "DHT.h" // DHT sensor library
#endif 

#if USE_BMP
#include <SFE_BMP180.h> // BMP180 sensor library
#include <Wire.h> // I2C library, used by the BMP180 sensor
#endif

#if USE_GPS
#include <TinyGPS++.h> // Library for parsing GPS data
#include <SoftwareSerial.h> // Library for communication between the GPS and the Arduino
#endif

#if USE_SD
#include <SD.h> // Library for using the SD card
#include <SPI.h> // Library for communication with the SD card
#endif

#include <util/crc16.h> // Library for calculating checksums