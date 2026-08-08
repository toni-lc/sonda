// libraries

#include "DHT.h"  // DHT sensor library
#include <OneWire.h>  // needed for DS18B20 sensor
#include <SFE_BMP180.h>  // BMP180 sensor library
#include "LowPower.h"  // power saving library
#include <Wire.h>  // I2C library (for BMP180 sensor)
#include <TinyGPS++.h>  // GPS library
#include <SoftwareSerial.h>  // needed for GPS-Arduino communication
#include <util/crc16.h> // verifies data
#include <SD.h>  // SD card reader library
#include <SPI.h>  // needed to communicate with SD card reader

// sensors

SFE_BMP180 bmp;  // BMP180 sensor
#define ALT 330.0  // ground elevation (in meters)

#define DHTPIN 2  // Arduino digital pin the DHT "out" pin is connected to 
#define DHTTYPE DHT22  // our DHT sensor is a DHT22 type
DHT dht(DHTPIN, DHTTYPE);  // DHT sensor

OneWire ds(8); // Arduino digital pin the DS18B20 "yellow" pin is connected to

TinyGPSPlus gps;  // GPS
// The GPS communicates with the board through a software-emulated serial port
// the RX (TX) serial line of the board must be connected with the TX (RX) line of the GPS
SoftwareSerial ss(4, 3);  // RX - TX serial lines

#define READER 10  // board pin the card reader CS pin is connected to
File SondaSD;  // output file in the SD card

# define RADIOPIN 9  // board pin the transmitter TX pin is connected to

// variables

float T1 = 0;   // DHT22 temperature (ºC)
float T2 = 0;   // DS18B20 temperature (ºC)
double T3 = 0;  // BMP180 temperature (ºC)
double p = 0;   // BMP180 absolute pressure (hPa)
double p0 = 0;  // BMP180 sea-level pressure (hPa)
int alt1 = 0;   // BMP180 calculated altitude (m)
float U = 0;    // DHT22 relative humidity (%)

long latE = 0;  // latitude (deg), integer part
long latD = 0;  // latitude (deg), decimal part
long lngE = 0;  // longitude (deg), integer part
long lngD = 0;  // longitude (deg), decimal part
double lat = 0; // latitude (deg)
double lng = 0; // longitude (deg)
int alt2 = 0;   // GPS calculated altitude (m)
int hour = 0;   // hours (in UTC time)
int min = 0;    // minutes
int sec = 0;    // seconds

// since the positive sign cannot be added directly to the coordinates,
// they will be stored in separate strings and then added to the final string
char latSign;
char lngSign;
char GPS1[20];
char GPS2[30];

// string variables containing float values
char T1c[5];
char T2c[5];
char T3c[5];
char pc[6];
char Uc[5];
char latc[11];
char lngc[11];

char SONDA[90]; // final string

void setup() {  // initializes serial port and components
  
  Serial.begin(38400); //obrir comunicació amb port sèrie
  Serial.println(F("TEST SONDA"));

  // SD card communication
  Serial.println(F("PREPARING SD"));
  pinMode(READER, OUTPUT);

  if (!SD.begin(10)) {
    Serial.println(F("ERROR SD"));  // data will not be stored in the SD card
  }
  else {
    Serial.println(F("SD OK"));
  }

  // radio communication
  pinMode(RADIOPIN, OUTPUT);
  setPwmFrequency(RADIOPIN, 1);

  dht.begin();  // initializes DHT22 sensor
  
  // initializes BMP180 sensor
  if (bmp.begin()) {
    Serial.println(F("BMP OK"));
  }
  else {
    Serial.println(F("BMP ERROR"));
  }

}

void loop() {

  // actions to be done by the flight computer
  
  // DS18B20 measurements
  // extracted from the "DS18x20_Temperature" example in the OneWire library.

  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  
  // searches the address of the DS18B20 sensor
  if ( !ds.search(addr)) {
    ds.reset_search();
    delay(250);
    return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end

  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);         // Read Scratchpad

  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  T2 = (float)raw / 16.0;

  // DHT22 measurements

  T1 = dht.readTemperature();
  U = dht.readHumidity();
  // BMP180 measurements
  
   char status;
  status = bmp.startTemperature();
  if (status != 0)
  {
    delay(status);
    status = bmp.getTemperature(T3);
    if (status != 0)
    {
      status = bmp.startPressure(3);
      if (status != 0)
      {
        delay(status);
        status = bmp.getPressure(p,T3);
        p0 = bmp.sealevel(p, ALT);  // corrects pressure to sea-level 
        alt1 = bmp.altitude(p, p0); 
      }
    }
  }
  // GPS
  ss.begin(9600);  // opens secondary serial port
  unsigned long start = millis();
  do
  {
   while (ss.available())
     gps.encode(ss.read());
  } while (millis() - start < 3000);
  latE = gps.location.rawLat().deg;
  latD = gps.location.rawLat().billionths;
  lngE = gps.location.rawLng().deg;
  lngD = gps.location.rawLng().billionths;

  lat = gps.location.lat();
  lng = gps.location.lng();
  
  alt2 = gps.altitude.meters();
  hour= gps.time.hour();
  min = gps.time.minute();
  sec = gps.time.second();

  ss.end();  // necessary for correct radio transmission (apparently)

  // structures data

  dtostrf(T1, 3, 1, T1c);
  dtostrf(T2, 3, 1, T2c);
  dtostrf(T3, 3, 1, T3c);
  dtostrf(p, 4, 1, pc);
  dtostrf(U, 3, 1, Uc);
  dtostrf(lat, 3, 6, latc);
  dtostrf(lng, 3, 6, lngc);

  sprintf(SONDA, "%s;%s;%s;%s;%s;%u;", T1c, T2c, T3c, pc, Uc, alt1);

  latSign = gps.location.rawLat().negative ? "-" : "+";
  lngSign = gps.location.rawLng().negative ? "-" : "+";

  sprintf(GPS1, "%s;",latc);
  sprintf(GPS2, "%s;%d;%02d:%02d:%02d", lngc, alt2, hour, min, sec);

  strcat(latSign, GPS1);
  strcat(lngSign, GPS2);
  strcat(SONDA, GPS1);
  strcat(SONDA, GPS2);

  // saves data in SD card

  SondaSD = SD.open("res.txt", FILE_WRITE);
  delay(500);

  if (SondaSD) {
  SondaSD.println(SONDA);
    SondaSD.close();
  }
  else {
    Serial.println("ERROR OPENING SD FILE");
  }

  // adds a checksum at the end of the SONDA string
  unsigned int CHECKSUM = gps_CRC16_checksum(SONDA);
  char VERIFICACIO[6];
  sprintf(VERIFICACIO, "*%04X\n", CHECKSUM); 
  strcat(SONDA, VERIFICACIO);

// output

  Serial.print(SONDA);
  rtty_txstring (SONDA); // sends data over radio
  delay(2000);
}

// *****************************************************
// Sends a string via RTTY.
// From https://ukhas.org.uk/guides:linkingarduinotontx2.

void rtty_txstring (char * string) {
/*     Simple function to sent a char at a time to    
 *      ** rtty_txbyte function.    
 *      ** NB Each char is one byte (8 Bits)
 *      */

    char c;
    c = *string++;

    while ( c != '\0') {
        rtty_txbyte (c);
        c = *string++;
    }
  }
  void rtty_txbyte (char c) {
/*     Simple function to sent each bit of a char to    
 *      rtty_txbit function.
 *      NB The bits are sent Least Significant Bit first    
 *      All chars should be preceded with a 0 and    
 *      proceed with a 1. 0 = Start bit; 1 = Stop bit
 *      */

    int i;

    rtty_txbit (0); // Start bit

    // Send bits for for char LSB first

    for (i=0;i<7;i++) { // Change this here 7 or 8 for ASCII-7 / ASCII-8
        if (c & 1) rtty_txbit(1);
        else rtty_txbit(0);

        c = c >> 1;
    }
    rtty_txbit (1); // Stop bit
    rtty_txbit (1); // Stop bit
  }

  void rtty_txbit (int bit) {
    if (bit) {
        // high
        analogWrite(RADIOPIN,110);
    }
    else {
        // low
        analogWrite(RADIOPIN,100);
    }

    // delayMicroseconds(3370); // 300 baud
    delayMicroseconds(10000); // For 50 Baud uncomment this and the line below.
    delayMicroseconds(10150); // You can't do 20150 it just doesn't work as the
    // largest value that will produce an accurate delay is 16383
    // See : http://arduino.cc/en/Reference/DelayMicroseconds
  }

  uint16_t gps_CRC16_checksum (char *string) {
    size_t i;
    uint16_t crc;
    uint8_t c;

    crc = 0xFFFF;

    // Calculate checksum ignoring the first two $s
    for (i = 2; i < strlen(string); i++) {
        c = string[i];
        crc = _crc_xmodem_update (crc, c);
    }

    return crc;
  }

  void setPwmFrequency(int pin, int divisor) {
    byte mode;
    if(pin == 5 || pin == 6 || pin == 9 || pin == 10) {
        switch(divisor) {
            case 1:
                mode = 0x01;
                break;
            case 8:
                mode = 0x02;
                break;
            case 64:
                mode = 0x03;
                break;
            case 256:
                mode = 0x04;
                break;
            case 1024:
                mode = 0x05;
                break;
            default:
                return;
        }

        if(pin == 5 || pin == 6) {
            TCCR0B = TCCR0B & 0b11111000 | mode;
        }
        else {
            TCCR1B = TCCR1B & 0b11111000 | mode;
        }
    }
    else if(pin == 3 || pin == 11) {
        switch(divisor) {
            case 1:
                mode = 0x01;
                break;
            case 8:
                mode = 0x02;
                break;
            case 32:
                mode = 0x03;
                break;
            case 64:
                mode = 0x04;
                break;
            case 128:
                mode = 0x05;
                break;
            case 256:
                mode = 0x06;
                break;
            case 1024:
                mode = 0x7;
                break;
            default:
                return;
        }
        TCCR2B = TCCR2B & 0b11111000 | mode;
    }
  }


String format_string(float x, int w, int d) {
  char s[w + d + 2];
  dtostrf(x, w, d, s);
  s[w] = ',';
  String str(s);
  return str;
}
