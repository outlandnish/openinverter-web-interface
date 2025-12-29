Open Inverter Web Interface
=====================
This is a fork of the [esp32-web-interface](https://github.com/jsphuebner/esp32-web-interface/) for OpenInverter.

**Note:** This fork doesn't support UART based connections. It only works over CAN.

## Notable differences to the original
* The web app has been replaced with a Preact single page web app 
* Lifecycle management of communication to both the ESP32 as well as remote devices on the CAN network
* Responsive UI for different screen viewport sizes
* You can discover and name multiple OpenInverter devices on a network
* Internationalization (please submit translations for your respective language)
* Get values now works in a burst mode and asyncronously (doesn't assume the next received value is for a setting)
* OpenInverter devices that publish a name key for each parameter have a more legible display
* UI to send `canio` control messages
* UI to schedule and send CAN messages

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
Out of the box, this works with:
* ESP32-C3-DevKitM-1
* [Canipulator](https://www.tindie.com/products/fusion/canipulator-automotive-dual-can-esp32-interface/) (ESP32-C6)

The firmware is easily adaptable to other ESP32 hardware as well, including the Xtensa based ESP32-S2/ESP32-S3. Just add the appropriate Platformio environments or use the override file

# Firmware
To compile it follow the [instructions below](#development).

# Development

## PlatformIO
[PlatformIO](https://platformio.org/) is a set of tools, among which [PlatformIO Core (CLI)](https://docs.platformio.org/en/latest/core/index.html) is a command line interface that can be used to build many kind of projects. In particular Arduino-based projects like this one.
(Note: even if PlatformIO provides an IDE, these instructions only target the CLI.)
* [Initial setup](doc/PLATFORMIO_setup.md)
* [Day to day usage](doc/PLATFORMIO_usage.md)
