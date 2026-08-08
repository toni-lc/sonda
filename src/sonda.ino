// FINAL SONDA CODE.
// This is the code the probe will run during the flight.
// It will measure internal and external temperature, humidity, pressure,
// position, altitude, and time.
// Then it will package these values into a string
// that will be saved to an SD card and transmitted by radio.
// The string is structured as follows:
// External temperature; internal temperature; humidity; BMP temperature; pressure; BMP altitude; latitude; longitude; altitude; time; checksum.
// 
// [X]EXTERNAL TEMPERATURE (DS18B20)
// [X]INTERNAL TEMPERATURE (DHT22)
// [X]HUMIDITY             (DHT22)
// [X]PRESSURE             (BMP180) (+ temperature, + altitude)
// [X]GPS LOCATION         (u-blox MAX-M8C)
//      [X]COORDINATES
//      [X]ALTITUDE
//      [X]DATE AND TIME
// [X]SD STORAGE           (Catalex MicroSD Card Adapter)
// [X]RADIO TRANSMISSION   (Radiometrix NTX2B-FA)
// 
// KNOWN ISSUES:
// -There are no measurements where the decimal part is 1, 3, 6, or 8 (they round up)
// 
// Started on 23-8-2015.
// Finished on 23-9-2015.
// Sources: Parts of the code are taken directly from:
// "DS18x20_Temperature" example from the OneWire library. (external temperature measurement)
// "KitchenSink" example from the TinyGPS++ library. (GPS location)
// -https://ukhas.org.uk/guides:linkingarduinotontx2 (radio transmission)


// LIBRARIES:

#include <OneWire.h> // DS18B20 sensor library.
#include "DHT.h" // DHT sensor library
#include <SFE_BMP180.h> // BMP180 sensor library
#include <Wire.h> // I2C library, used by the BMP180 sensor
#include <TinyGPS++.h> // Library for parsing GPS data
#include <SoftwareSerial.h> // Library for communication between the GPS and the Arduino
#include <util/crc16.h> // Library for calculating checksums
#include <SD.h> // Library for using the SD card
#include <SPI.h> // Library for communication with the SD card

// DEFINITIONS
// We use these flags to include/exclude certain parts of the code, for example sensor reading
// or measurement output. If the flag value is 0 the corresponding part will not execute
// By default all flags are 1 so everything executes.

#ifndef USE_DS
#define USE_DS 1
#endif

#ifndef USE_DHT
#define USE_DHT 1
#endif

#ifndef USE_BMP
#define USE_BMP 1
#endif

#ifndef USE_GPS
#define USE_GPS 1
#endif

#ifndef USE_SD
#define USE_SD 1
#endif

#ifndef USE_RADIO
#define USE_RADIO 1
#endif

// SENSORS:

//DS18B20:

OneWire ds(8); // The DS sensor is connected to digital pin 8

//DHT22:

#define DHTPIN 2 // The DHT sensor is connected to digital pin 2
#define DHTTYPE DHT22 // The DHT sensor is a DHT22

DHT dht(DHTPIN, DHTTYPE); // "dht" object for the DHT sensor: a DHT22 connected to digital pin 2

//BMP180:

SFE_BMP180 pressio; // "pressio" object for the BMP180 sensor
#define ALTITUD 330.0 // Altitude from which pressure measurement starts

// GPS locator:

TinyGPSPlus gps; // "gps" object for the GPS locator

// The GPS communicates with the Arduino board
// through a software serial port (in this case, pins 4 and 3).
SoftwareSerial ss(4, 3); // "ss" object for the software serial port

// SD card:

#define TARGETA 10 // SD card pin (module CS pin) connected to Arduino digital pin 10
File SondaSD; // "SondaSD" object for the SD card file where data will be saved

//Radiometrix NTX2B-FA:

#define RADIOPIN 9 // Transmission pin (transmitter pin 7) connected to Arduino digital pin 9

// VARIABLES:

// float=decimal number; int=integer (2 bytes); long=integer (4 bytes); char=string

// Sensor variables

float tDS = 0; // External temperature (degrees Celsius)
int tDSE = 0;  // External temperature (integer part)
int tDSD = 0;  // External temperature (decimal part)

float tDHT = 0; // Internal temperature from the DHT sensor (degrees Celsius)
int tDHTE = 0;  // Internal temperature (integer part)
int tDHTD = 0;  // Internal temperature (decimal part)
int hDHT = 0; // Humidity (%)
int hDHTE = 0;  // Humidity (integer part)
int hDHTD = 0;  // Humidity (decimal part)

int tBMP = 0; // Internal temperature from the BMP sensor (degrees Celsius)
int pBMP = 0; // Pressure (mbar)
int p0 = 0; // Pressure (relative, at sea level)
int altBMP = 0; // Altitude (calculated by the BMP sensor)

// GPS variables

long latE = 0; // Latitude (degrees, integer part)
long latD = 0; // Latitude (degrees, decimal part)

long lngE = 0; // Longitude (degrees, integer part)
long lngD = 0; // Longitude (degrees, decimal part)

int alt = 0; // Altitude

int hora = 0; // Hours (UTC time!)
int min = 0; // Minutes
int seg = 0; // Seconds

// STRINGS:

// Since the positive or negative sign cannot be added directly to the coordinates,
// it will be stored in separate strings and then added to the final string,
// which will be transmitted by radio and saved to the SD card.

char GPS1[20];
char GPS2[30];
char SONDA [90]; // Final string

void setup() { 
  
  // This section prepares everything the flight computer will do.
  // It only runs once, at startup.

  // OPEN SERIAL PORT COMMUNICATION
  // Serial port communication is only used for pre-flight setup.

  Serial.begin(38400); // Open serial port communication

  // OPEN SD CARD COMMUNICATION

  // Prepare SD card communication

  #if USE_SD
    Serial.println(F("Preparant SD"));
    pinMode(TARGETA,OUTPUT);

    // Start SD card communication.
    // If communication cannot be established now, data cannot be saved during the flight.

    if(!SD.begin(10)){
        Serial.println(F("ERROR SD")); 
    }
    else{
    Serial.println(F("SD OK"));
    }
   #endif

//   // PREPARE RADIO COMMUNICATION

  #if USE_RADIO
    pinMode(RADIOPIN,OUTPUT);
    setPwmFrequency(RADIOPIN, 1);
  #endif
  // START DHT22 SENSOR

  #if USE_DHT
    dht.begin();
  #endif

  // START BMP180 SENSOR
  
  #if USE_BMP
    if (pressio.begin()) { // If the sensor starts,
        Serial.println(F("BMP OK"));
    }
    else{ // If it does not start,
        Serial.println(F("BMP ERROR"));
    }
  #endif

}

void loop() { 
  
  // This section repeats cyclically during the whole run.
  // All actions performed by the flight computer are included here.

  //DS18B20

  // GET EXTERNAL TEMPERATURE.
  // Taken from the "DS18x20_Temperature" example from the OneWire library.
  // This is not original code; it has been left intact.

  #if USE_DS
    byte i;
    byte present = 0;
    byte type_s;
    byte data[12];
    byte addr[8];
    
    if ( !ds.search(addr)) {
        Serial.println("No more addresses.");
        Serial.println();
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
    tDS = (float)raw / 16.0;

    // Convert decimal value into two integers (E=integer part; 2=value minus integer part; D=decimal part)
    // It is approximated to one decimal by truncation.
    // The tDS value is split into tDSE (integer part) and tDSD (decimal part)

    tDSE = tDS;
    float tDS2 = tDS - tDSE;
    tDSD = trunc(tDS2*10);
  #endif

  //DHT
  #if USE_DHT  
  // GET INTERNAL TEMPERATURE.

  tDHT = dht.readTemperature(); // Read internal temperature

  // Convert decimal value into two integers (E=integer part; 2=value minus integer part; D=decimal part)
  // It is approximated to one decimal by truncation.
  // The tInt value is split into tIntE (integer part) and tIntD (decimal part)

  int tDHTE = tDHT;
  float tDHT2 = tDHT - tDHTE;
  int tDHTD = trunc(tDHT2*10);

  // GET HUMIDITY.
     
  hDHT = dht.readHumidity(); // Read the humidity value and name it "h"

  // Convert decimal value into two integers (E=integer part; 2=value minus integer part; D=decimal part)
  // It is approximated to one decimal by truncation.
  // The h value is split into hE (integer part) and hD (decimal part)
  
  hDHTE = hDHT;
  float hDHT2 = hDHT - hDHTE;
  hDHTD = trunc(hDHT2*10);
  #endif

  //BMP180
  #if USE_BMP180
    char status; // Define status variable (returns 1 if there are no problems, 0 if there are)
    double T,P;
    
    // T: define variable T (temperature in degrees C)
    // P: define variable P (absolute pressure in mbar)

    // Measure temperature.
    
    status = pressio.startTemperature();
    if (status != 0) // If temperature can be measured...
    {
        // Wait until temperature measurement is complete
        delay(status);

        // Receive temperature measurement
        // and save it to variable T

        status = pressio.getTemperature(T);
        if (status != 0) // If the measurement can be received...
        {
        // Measure pressure.
        // The "3" indicates the resolution.
        // It can be between 0 and 3. 0 is the lowest, 3 is the highest, but it takes longer.

        status = pressio.startPressure(3);
        if (status != 0) // If pressure can be measured...
        {
            // Wait until pressure measurement is complete.
            delay(status);

            // Receive pressure measurement
            // and save it to variable P.
            // This pressure is calibrated with temperature,
            // so a previous temperature measurement is required.

            status = pressio.getPressure(P,T);
            if (status != 0) // If the measurement can be received...
            {

            // Get relative sea-level pressure
            // from pressure and initial altitude
            
            p0 = pressio.sealevel(P,ALTITUD);

            // Get altitude from the pressure difference.
            // It is not fully reliable because temperature is not constant.
            
            altBMP = pressio.altitude(P,p0);
            }
            else Serial.println(F("error rebent pressio\n"));
        }
        else Serial.println(F("error mesurant pressio\n"));
        }
        else Serial.println(F("error rebent temperatura\n"));
    }
    else Serial.println(F("error mesurant temperatura\n"));

    // Convert temperature and pressure measurements into integers.
    // The temperature measurement complements the DHT measurement.
    // Pressure does not need one-decimal precision; an integer is enough.

    tBMP = T;
    pBMP = P;
  #endif

  //GPS
  #if USE_GPS

    // Start communication with the GPS (through the "secondary" serial port)
    
    ss.begin(9600);

    // Give the GPS three seconds to read the measurements it receives
    // If this step is skipped, most GPS measurements will not be read.
    
        unsigned long start = millis();
    do 
    {
        while (ss.available())
        gps.encode(ss.read());
    } while (millis() - start < 3000);

    // Latitude
    
    latE = gps.location.rawLat().deg;
    latD = gps.location.rawLat().billionths;

    // Longitude

    lngE = gps.location.rawLng().deg;
    lngD = gps.location.rawLng().billionths;

    // Altitude
    
    alt = gps.altitude.meters();

    // Time (UTC)
    
    hora= gps.time.hour();
    min = gps.time.minute();
    seg = gps.time.second();

    // End communication with the GPS.
    // VERY IMPORTANT: DO NOT SKIP THIS STEP,
    // OR THE RADIO WILL NOT TRANSMIT PROPERLY!

    ss.end();
  #endif

  // STRUCTURE STRINGS

  // Add sensor measurements to the SONDA string

  sprintf(SONDA, "%d,%d;%d,%d;%d,%d;%d;%d;%d;", tDSE, tDSD, tDHTE, tDHTD, hDHTE, hDHTD, tBMP, pBMP, altBMP);

  // Add latitude measurement to the GPS1 string.
  // A negative latitude measurement (south) will not show the sign by default.
  // Therefore, it must be added manually.

  if (gps.location.lat() < 0)
  {
    sprintf(GPS1, "-%ld.%ld;", latE, latD);
  }
  else
  {
    sprintf(GPS1, "%ld.%ld;", latE, latD);
  }

  // Add longitude, altitude, and time measurements to the GPS2 string.
  // A negative longitude measurement (west) will not show the sign by default.
  // Therefore, it must be added manually.

  if (gps.location.lng() < 0)
  {
    sprintf(GPS2, "-%ld.%ld;%d;%02d:%02d:%02d", lngE, lngD, alt, hora, min, seg);
  }
  else
  {
    sprintf(GPS2, "%ld.%ld;%d;%02d:%02d:%02d", lngE, lngD, alt, hora, min, seg);
  }

  // Add GPS1 and GPS2 strings to the SONDA string
  
  strcat(SONDA, GPS1);
  strcat(SONDA, GPS2);

  // SAVE SONDA STRING TO THE SD CARD

  #if USE_SD
    SondaSD = SD.open("sonda.txt", FILE_WRITE);
    delay(500);

    if (SondaSD){
        SondaSD.println(SONDA);
        SondaSD.close();
    }
    else{
        Serial.println("error obrint sonda.txt");
    }
  #endif

  // ADD CHECKSUM TO THE SONDA STRING
  
  unsigned int CHECKSUM = gps_CRC16_checksum(SONDA); // Function for calculating the checksum for the SONDA string (defined later)
  char VERIFICACIO[6]; // Define VERIFICACIO as a string
  sprintf(VERIFICACIO, "*%04X\n", CHECKSUM); // Structure the VERIFICACIO string
  strcat(SONDA, VERIFICACIO); // Concatenate SONDA and VERIFICACIO strings (the second string is appended to the first)

  // TRANSMIT SONDA STRING BY RADIO
  
  #if USE_RADIO
    rtty_txstring (SONDA); // Function for transmitting the final probe string by radio (defined later)
  #endif
  // END OF THE "LOOP" FUNCTION. START AGAIN.

    Serial.println(SONDA);
    delay(2000);

}

// RADIO STRING TRANSMISSION FUNCTION.
// This code was taken directly from the code found
// in the "Linking an Arduino to a Radiometrix NTX2B Transmitter" tutorial,
// available at https://ukhas.org.uk/guides:linkingarduinotontx2.
// This is not original code; it has been left intact.

void rtty_txstring (char * string) {
    /* Simple function to sent a char at a time to
    ** rtty_txbyte function.
    ** NB Each char is one byte (8 Bits)
    */
 
    char c;
    c = *string++;
 
    while ( c != '\0') {
        rtty_txbyte (c);
        c = *string++;
    }
}
void rtty_txbyte (char c) {
    /* Simple function to sent each bit of a char to
    ** rtty_txbit function.
    ** NB The bits are sent Least Significant Bit first
    **
    ** All chars should be preceded with a 0 and
    ** proceed with a 1. 0 = Start bit; 1 = Stop bit
    **
    */
 
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
