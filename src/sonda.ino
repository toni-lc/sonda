// FINAL SONDA CODE.
// This is the code the probe will run during the flight.
// It will measure internal and external temperature, humidity, pressure,
// position, altitude, and time.
// Then it will package these values into a string
// that will be transmitted by radio.
// The string is structured as follows:
// External temperature; internal temperature; humidity; BMP temperature; pressure; BMP altitude; latitude; longitude; altitude; time; checksum.
// 
// [X]EXTERNAL TEMPERATURE (DS18B20)
// [X]INTERNAL TEMPERATURE (DHT22)
// [X]HUMIDITY             (DHT22)
// [X]PRESSURE (in Pa)     (BMP180) (+ temperature, + altitude)
// [X]GPS LOCATION         (u-blox MAX-M8C)
//      [X]COORDINATES
//      [X]ALTITUDE
//      [X]DATE AND TIME
// [X]RADIO TRANSMISSION   (Radiometrix NTX2B-FA)
//
// Sources: Parts of the code are taken directly from:
// "DS18x20_Temperature" example from the OneWire library. (external temperature measurement)
// "KitchenSink" example from the TinyGPS++ library. (GPS location)
// -https://ukhas.org.uk/guides:linkingarduinotontx2 (radio transmission)

// DEFINITIONS
// We use these flags to include/exclude certain parts of the code, for example sensor reading
// or measurement output. If the flag value is 0 the corresponding part will not execute
// By default all flags are 1 so everything executes.

#include <Arduino.h>

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

#ifndef USE_RADIO
#define USE_RADIO 1
#endif

#ifndef TEST_RUN
#define TEST_RUN 0
#endif

#include <library_imports.h> // libraries

// SENSORS:

//DS18B20:

#if USE_DS
#define DS_DATA_PIN 8 // connected to the DS18B20 data wire 
OneWire ds(DS_DATA_PIN);
byte type_s = 0; // for DS18S20 change to 1
float tDS = 0;
#endif

//DHT22:

#if USE_DHT
#define DHTPIN 2 // connected to the out pin in the DHT sensor
#define DHTTYPE DHT22 // The DHT sensor is a DHT22
DHT_Unified dht(DHTPIN, DHTTYPE);
float tDHT = 0; 
float hDHT = 0; 
#endif

//BMP180:

#if USE_BMP
Adafruit_BMP085 BMP;
float tBMP;
int32_t pBMP;
#endif

// GPS locator:

#if USE_GPS
TinyGPSPlus gps; // "gps" object for the GPS locator

// The GPS communicates with the Arduino board
// through a software serial port (in this case, pins 4 and 3).
#define GPS_TX_PIN 4 // connected to the TXD pin in the GPS
#define GPS_RX_PIN 3 // connected to the RXD pin in the GPS
SoftwareSerial ss(GPS_TX_PIN, GPS_RX_PIN);

RawDegrees raw_lat; // latitude (degrees)
RawDegrees raw_lng; // longitude (degrees)
int32_t altitude;   // altitude (meters)
TinyGPSTime time;   // time (UTC)

#endif

//Radiometrix NTX2B-FA:

#if USE_RADIO
#define RADIOPIN 9 // Transmission pin (transmitter pin 7) connected to Arduino digital pin 9
#endif

// other variables
int iter = 0; // measurement iteration
char buf[15]; // buffer to store each measure before adding to the final string
char SEP[] = ";"; // separator between variables
char ERR[] = "NA"; // error value
char res[90]; // Final string

void setup() { 
  
  // This section prepares everything the flight computer will do.
  // It only runs once, at startup.

  // OPEN SERIAL PORT COMMUNICATION
  // Serial port communication is only used for pre-flight setup.

  Serial.begin(38400); // Open serial port communication

//   // PREPARE RADIO COMMUNICATION

  #if USE_RADIO
    pinMode(RADIOPIN,OUTPUT);
    setPwmFrequency(RADIOPIN, 1);
  #endif
  // START DHT22 SENSOR

  #if USE_DHT
    dht.begin();
    Serial.println(F("DHT OK"));
  #endif

  // START BMP180 SENSOR
  
  #if USE_BMP
    if (BMP.begin()) { // If the sensor starts,
        Serial.println(F("BMP OK"));
    }
    else{ // If it does not start,
        Serial.println(F("BMP ERROR"));
    }
  #endif

}

void loop() { 

    iter++;

    #if TEST_RUN
    if (iter > 10) {
        // enters infinite loop, does not measure again
        // this is the closest we have to "stopping the program"
        while (true) {
            delay(1000);
        }
    }
    #endif
  
  // This section repeats cyclically during the whole run.
  // All actions performed by the flight computer are included here.

  //DS18B20

  // GET EXTERNAL TEMPERATURE.
  // Taken from the "DS18x20_Temperature" example from the OneWire library.
  // This is not original code; it has been left intact.

  // TODO: fix! the address search should be in the setup? otherwise there are no measurements
  // if the DS18B20 is not found!!
  #if USE_DS
    byte i;
    byte present = 1;
    byte data[12];
    byte addr[8];
    
    if ( !ds.search(addr)) {
        Serial.println("No more addresses.");
        Serial.println();
        ds.reset_search();
        delay(250);
        present = 0;
    }

    if (present){
    
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
    ds.reset_search();

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
    
    // convert float to string and append to final string
    // sprintf in Arduino does not support floats or doubles,
    // it's either dtostrf or splitting each variable into integer and decimal part 
    dtostrf(tDS, 2, 1, buf);
}
else {
    strcpy(buf, ERR);
}
    strcat(res, buf);
    strcat(res, SEP);

  #endif

  //DHT
  #if USE_DHT  
  sensors_event_t event;
  // temperature
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    strcpy(buf, ERR);
  }
  else {
    tDHT = event.temperature;
    dtostrf(tDHT, 2, 1, buf);
  }
  strcat(res, buf);
  strcat(res, SEP);

  // humidity
  dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
    strcpy(buf, ERR);
  }
  else {
    tDHT = event.relative_humidity;
    dtostrf(tDHT, 2, 1, buf);
  }
  strcat(res, buf);
  strcat(res, SEP);
  #endif

  //BMP180
  #if USE_BMP
    tBMP = BMP.readTemperature();
    dtostrf(tBMP, 2, 1, buf);
    strcat(res, buf);
    strcat(res, SEP);

    pBMP = BMP.readPressure();
    snprintf(buf, sizeof(buf), "%ld", (long)pBMP);
    strcat(res, buf);
    strcat(res, SEP);
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

    if (!gps.location.isValid()) {
        snprintf(buf, sizeof(buf), "%s%s%s%s", ERR, SEP, ERR, SEP);
        strcat(res, buf);
    } else {
    // latitude
    raw_lat = gps.location.rawLat();
    if (raw_lat.negative){
        strcat(res, "-");
    }
    else {
        strcat(res, "+");
    }
    sprintf(buf, "%u.%09lu",
            (unsigned int)raw_lat.deg,
            (unsigned long)raw_lat.billionths);
    strcat(res, buf);
    strcat(res, SEP);

    // longitude
    raw_lng = gps.location.rawLng();
    if (raw_lng.negative){
        strcat(res, "-");
    }
    else {
        strcat(res, "+");
    }
    sprintf(buf, "%u.%09lu",
            (unsigned int)raw_lng.deg,
            (unsigned long)raw_lng.billionths);
    strcat(res, buf);
    strcat(res, SEP);
    }

    // Altitude
    
    if (gps.altitude.isValid()){
        altitude = gps.altitude.value() / 100;
        snprintf(buf, sizeof(buf), "%ld", (long)altitude);
    }
    else {
        strcpy(buf, ERR);
    }
    strcat(res, buf);
    strcat(res, SEP);

    // Time (UTC)
    time = gps.time;
    if (!time.isValid()){
        strcpy(buf, ERR);
        strcat(buf, SEP);
        strcat(res, buf);
    } else {
    sprintf(buf, "%02d:%02d:%02d", time.hour(), time.minute(), time.second());
    strcat(res, buf);
    }

    // End communication with the GPS.
    // VERY IMPORTANT: DO NOT SKIP THIS STEP,
    // OR THE RADIO WILL NOT TRANSMIT PROPERLY!

    ss.end();

  #endif

  // ADD CHECKSUM TO THE SONDA STRING
  sprintf(buf, "*%04X\n", gps_CRC16_checksum(res));
  strcat(res, buf);

  // TRANSMIT SONDA STRING BY RADIO
  
  #if USE_RADIO
    rtty_txstring (RADIOPIN, res); // Function for transmitting the final probe string by radio (defined later)
  #endif
  // END OF THE "LOOP" FUNCTION. START AGAIN.
  
    Serial.print("Iteration ");
    Serial.print(iter);
    Serial.print(" : ");
    Serial.print(res);

    // clean final string
    res[0] = '\0';
    delay(2000);

}

// RADIO STRING TRANSMISSION FUNCTION.
// This code was taken directly from the code found
// in the "Linking an Arduino to a Radiometrix NTX2B Transmitter" tutorial,
// available at https://ukhas.org.uk/guides:linkingarduinotontx2.
// This is not original code; it has been left intact.

void rtty_txstring (int pin, char * string) {
    /* Simple function to sent a char at a time to
    ** rtty_txbyte function.
    ** NB Each char is one byte (8 Bits)
    */
 
    char c;
    c = *string++;
 
    while ( c != '\0') {
        rtty_txbyte (pin, c);
        c = *string++;
    }
}
void rtty_txbyte (int pin, char c) {
    /* Simple function to sent each bit of a char to
    ** rtty_txbit function.
    ** NB The bits are sent Least Significant Bit first
    **
    ** All chars should be preceded with a 0 and
    ** proceed with a 1. 0 = Start bit; 1 = Stop bit
    **
    */
 
    int i;
 
    rtty_txbit (pin, 0); // Start bit
 
    // Send bits for for char LSB first
 
    for (i=0;i<7;i++) { // Change this here 7 or 8 for ASCII-7 / ASCII-8
        if (c & 1) rtty_txbit(pin, 1);
        else rtty_txbit(pin, 0);
 
        c = c >> 1;
    }
    rtty_txbit (pin, 1); // Stop bit
    rtty_txbit (pin, 1); // Stop bit
}

void rtty_txbit (int pin, int bit) {
    if (bit) {
        // high
        analogWrite(pin,110);
    }
    else {
        // low
        analogWrite(pin,100);
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
