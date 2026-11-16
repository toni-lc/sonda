#if USE_DS
#include <OneWire.h> // DS18B20 sensor library.
#endif

#if USE_DHT
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#endif 

#if USE_BMP
#include <Adafruit_BMP085.h>
#endif

#if USE_GPS
#include <TinyGPS++.h> // Library for parsing GPS data
#include <SoftwareSerial.h> // Library for communication between the GPS and the Arduino
#endif

#include <util/crc16.h> // Library for calculating checksums