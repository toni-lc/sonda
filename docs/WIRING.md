# Wiring

This document describes the wiring used by the original `sonda.ino` firmware **in an Arduino Uno (or compatible) board**.

## Pin Table

| Component | Component pin | Arduino Uno pin |
| --- | --- | --- |
| DS18B20 | Red wire | 5V |
| DS18B20 | Yellow wire | Digital pin `8` |
| DS18B20 | Black wire | GND |
| DHT22 | `+` | 3.3V |
| DHT22 | `out` | Digital pin `2` |
| DHT22 | `-` | GND |
| BMP180 | `VIN` | 3.3V |
| BMP180 | `GND` | GND |
| BMP180 | `SCL` | Analog pin `5` |
| BMP180 | `SDA` | Analog pin `4` |
| u-blox MAX-M8C | `VCC` | 3.3V |
| u-blox MAX-M8C | `GND` | GND |
| u-blox MAX-M8C | `TXD` | Digital pin `4` |
| microSD module | `CS` | Digital pin `10` |
| microSD module | `SCK` | Digital pin `13` |
| microSD module | `MOSI` | Digital pin `11` |
| microSD module | `MISO` | Digital pin `12` |
| microSD module | `VCC` | 5V |
| microSD module | `GND` | GND |
| Radiometrix NTX2B | Pin `4` | 3.3V |
| Radiometrix NTX2B | Pin `5` | 3.3V |
| Radiometrix NTX2B | Pin `6` | GND |
| Radiometrix NTX2B | Pin `7` | Digital pin `9` |

## Pull-Up Resistors

- DS18B20: the yellow data wire is also connected to 5V through a 4.7 kΩ pull-up resistor.
- DHT22: the `out` pin is also connected to 5V through a 10 kΩ pull-up resistor.