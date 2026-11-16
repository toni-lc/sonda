# sonda

Flight computer for a weather balloon / [high altitude balloon](https://ukhas.org.uk/doku.php?id=general:beginners_guide_to_high_altitude_ballooning) using an Arduino Uno.
The flight computer measures temperature (both inside and outside the payload), humidity, pressure, gets the position, altitude, and time with GPS, then formats the data for serial output and optional RTTY radio transmission.

Originally built as a school project in 2015. The original version logged data to an SD card; the current firmware does not.

<img src="docs/assets/payload.JPG" alt="Hardware setup for the flight computer in a protoboard." width=600>
<img src="docs/assets/fldigi.png" alt="Screenshot of fldigi program showing the transmitted data from the flight computer." width=600>

## Hardware

Target board:

- Arduino Uno-compatible board -- tested on the Uno R3 (ATmega328P).
- Serial monitor speed: 38400 baud

If using an ATmega328P + ESP8266 board, upload the ATmega firmware with the board switch/jumper mode set to `USB+MCU`.

Sensors and modules:

- [DS18B20](https://randomnerdtutorials.com/guide-for-ds18b20-temperature-sensor-with-arduino/) external temperature sensor
- [DHT22](https://www.instructables.com/How-to-use-DHT-22-sensor-Arduino-Tutorial/) internal temperature and humidity sensor
- [BMP180](https://randomnerdtutorials.com/guide-for-bmp180-barometric-sensor-with-arduino/) internal temperature and pressure sensor
- [u-blox GPS module](https://store.uputronics.com/products/ublox-max-m8c-pico-breakout-with-chip-antenna)
- [Radiometrix NTX2B-FA](https://ukhas.org.uk/doku.php?id=guides:radio_modules:ntx2) radio transmitter on 434.075 MHz

See [WIRING.md](docs/WIRING.md) for more details on the specific wiring needed to reproduce the code.

<img src="docs/assets/ds.JPG" alt="DS18B20 sensor" width=400>
<img src="docs/assets/other_sensors.JPG" alt="Original setup with radio transmitter, GPS module and sensors." width=400>

## Layout

```text
platformio.ini   PlatformIO build environments
src/sonda.ino    Main firmware
LICENSE          Project license
```

## Build, setup and upload

Install PlatformIO, then open this repository folder directly in VS Code / PlatformIO.

Build and upload an environment

```powershell
pio run -e atmega328p_sensors_ext
pio run -e atmega328p_sensors_ext -t upload
```

PlatformIO usually finds the port automatically, otherwise it can be set when uploading:

```powershell
pio device list
pio run -e atmega328p_sensors_ext -t upload --upload-port [PORT]
```

Open the serial monitor at 38400 baud:

```powershell
pio device monitor -b 38400
```

The port can also be set manually,

```powershell
pio device monitor -p [PORT] -b 38400
```

## Libraries

PlatformIO installs the dependencies declared in `platformio.ini`:

| Library | Used for |
| --- | --- |
| `OneWire` | DS18B20 external temperature sensor |
| `DHT sensor library` | DHT22 temperature and humidity sensor |
| `Adafruit BMP085 Library` | BMP180 pressure and temperature sensor |
| `TinyGPSPlus` | GPS NMEA parsing |


## Test environments

The firmware uses compile-time flags so subsystems can be tested independently without repeatedly commenting and uncommenting code.

Available environments:

| Environment | Purpose |
| --- | --- |
| `atmega328p_test` | Initializes only; does not read sensors and stops after 10 iterations |
| `atmega328p_dht` | Tests DHT22 temperature and humidity |
| `atmega328p_sensors` | Tests DHT22 and BMP180 |
| `atmega328p_sensors_ext` | Tests DS18B20 external temperature |
| `atmega328p_sensors_gps` | Tests GPS |
| `atmega328p_sensors_all` | Reads all sensors without radio transmission |
| `atmega328p_full` | Full firmware: sensors, GPS, and radio |

The flags are defined in `platformio.ini`:

```ini
-DUSE_DS=1
-DUSE_DHT=1
-DUSE_BMP=1
-DUSE_GPS=1
-DUSE_RADIO=1
```

Set a flag to `0` to exclude that subsystem from the build.

## Output

The main telemetry string is structured as:

```text
ds_temp;dht_temp;humidity;bmp_temp;pressure_pa;latitude;longitude;gps_altitude_m;time_utc
```

Example shape:

```text
27.7;24.3;60.0;22.0;101325;+41.123456789;+2.123456789;350;12:34:56
```

Decimal values use a point. Missing readings are represented by `NA`.

To get the radio transmitted data it is necessary to use a radio receiver for 430-440 MHz and supporting SSB (single-sideband) such as the [Yupiteru MVT-7100](https://www.rigpix.com/yupiteru/mvt7100.htm); this data can then be decoded with [fldigi](https://sourceforge.net/p/fldigi/fldigi/ci/master/tree/).

## License

This project is currently licensed under GPLv3. Third-party libraries keep their own licenses and copyright notices.
