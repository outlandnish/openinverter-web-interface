Open Inverter Web Interface
=====================
This is a fork of the `esp32-web-interface` for the Huebner inverter. It has been modified to work on the ESP32-C3-DevKitM-1 with a CAN interface and supports multiple Open Inverter devices.

[![Build status](../../actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)

# Table of Contents
<details>
 <summary>Click to open TOC</summary>
<!-- MarkdownTOC autolink="true" levels="1,2,3,4,5,6" bracket="round" style="unordered" indent="    " autoanchor="false" markdown_preview="github" -->

- [esp32-web-interface](#esp32-web-interface)
- [Table of Contents](#table-of-contents)
- [About](#about)
- [Usage](#usage)
  - [Wifi network](#wifi-network)
  - [Reaching the board](#reaching-the-board)
- [Hardware](#hardware)
- [Firmware](#firmware)
- [Flashing / Upgrading](#flashing--upgrading)
  - [Wirelessly](#wirelessly)
  - [Wired](#wired)
- [Documentations](#documentations)
- [Development](#development)
  - [Arduino](#arduino)
  - [PlatformIO](#platformio)

<!-- /MarkdownTOC -->
</details>

# About
This repository hosts the source code for the Web Interface for the Huebner inverter, and derivated projects:
* [OpenInverter Sine (and FOC) firmware](https://github.com/jsphuebner/stm32-sine)
* [Vehicle Control Unit for Electric Vehicle Conversion Projects](https://github.com/damienmaguire/Stm32-vcu)
* [OpenInverter buck or boost mode charger firmware](https://github.com/jsphuebner/stm32-charger)
* [OpenInverter non-grid connected inverter](https://github.com/jsphuebner/stm32-island)
* [BMS project firmware](https://github.com/jsphuebner/bms-software)
* ...

It is written with the Arduino development environment and libraries.

# Usage
To use the web interface 2 things are needed :
* You need to have a computer on the same WiFi network as the board,
* You need to 'browse' the web interface page.

## Wifi network
During normal operation / debugging, the board will look for a `wifi.txt` file with the following:
* line 1 - WiFi SSID
* line 2 - WiFi Password

If connection fails or the file doesn't exist, the board will start up in station mode and broadcast its own network. In station mode, the board will be accessible at http://192.168.4.1

## mDNS
The board announces itself to the world using mDNS protocol (aka Bonjour, or Rendezvous, or Zeroconf), so you may be able to reach the board using a local name of `inverter.local`.
So first try to reach it on http://inverter.local/

# Hardware
The web interface has been modified to run on an ESP32C3-DevKitM-1. It uses the NeoPixel for communicating status

# Firmware
Tompile it follow the [instructions below](#development).

# Flashing / Upgrading
## Wirelessly
Create a file platformio-local-override.ini and put in it
```
[env]
upload_protocol = espota
upload_port = 192.168.4.1
upload_flags = 
```
Install platformio on your system and flash with commands
```
pio run --target upload
pio run --target uploadfs
```

## Wired
If your board is new and unprogrammed, or if you want to fully re-program it, you'll need to have a wired connection between your computer and the board.
You'll either need a ESP32 board with an on board USB to serial converter or a 3.3v capable USB / Serial adapter
* the following connections:  

Pin#  | ESP32 Board Function | USB / Serial adapter
----- | ---------------------- | --------------------
1     | +3.3v input            | (Some adapters provide a +3.3v output, you can use it)
2     | GND                    | GND
3     | RXD input              | TXD output
4     | TXD output             | RXD input

Then you would use any of the the [development tool below](#development) ; or the `esptool.py` tool to upload either a binary firmware file, or a binary filesystem file.

# Documentations
* [Openinverter Web Interface Protocol](PROTOCOL.md)

# Development
You can choose between the following tools:

## Arduino
[Arduino IDE](https://www.arduino.cc/en/software) is an easy-to-use desktop IDE, which provides a quick and integrated way to develop and update your board.
* [Initial setup](doc/ARDUINO_IDE_setup.md)
* [Day to day usage](doc/ARDUINO_IDE_usage.md)
* [Initial setup with Arduino IDE version 2](doc/arduino2_with_CAN_installation_notes.md)

## PlatformIO
[PlatformIO](https://platformio.org/) is a set of tools, among which [PlatformIO Core (CLI)](https://docs.platformio.org/en/latest/core/index.html) is a command line interface that can be used to build many kind of projects. In particular Arduino-based projects like this one.
(Note: even if PlatformIO provides an IDE, these instructions only target the CLI.)
* [Initial setup](doc/PLATFORMIO_setup.md)
* [Day to day usage](doc/PLATFORMIO_usage.md)
