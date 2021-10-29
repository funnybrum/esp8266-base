# esp8266-base

A set of basic tools that are needed for ESP8266 based projects (at least for the one that I'm creating). The provided tools are:

## Logger

The Logger class is a logger that uses in-memory buffer for storing the data. This is useful for debugging projects that doesn't have serial connectivity.

## Settings

Common class used to save and load settings from the EEPROM. Pass in the structure of the settings and the provided implementation will take care for saving and loading. The settings are guarded by checksum and are loaded only if it is correct.

## SystemCheck

A kind of watchdog, but very software one. If enabled - it will monitor the ESP8266 for WiFi connectivity. If there is no such in 2 minutes - the microcontroller will get restarted. It provides additional watchdog for the REST API calls. If REST API has not been invoked for 10 minutes - the microcontroller will get restarted.

## WebServerBase

Web server base with build-in OTA update mechanism. Integrated with the logger and the system check tools.

## RS485ServerBase

Similar to an web server, but support RS485 communication. Commands are received over the wire.

## WiFiManager

The WiFi Manager will take care for the WiFi connectivity. Integrated with the other tools. Settings are stored in the EEPROM and  details like SSID and password can be kept between restarts. If the configured WiFi SSID/password are invalid - the microcontroller will switch to AP mode. The user can connect to it and open 192.168.0.1 to configure the correct SSID/password.

## InfluxDBCollector

A tool to automate the data publishing to InfluxDB. Requires DB that is not password protected. Designed with one main goal - to reduce the WiFi polution. Data is collected in in-memory buffer and pushed once the buffer is full or the time for a push has come.

Several parameters can be configured, but the main one are - push interval, collect interval and InfluxDB address. If all of them are valid - the microcontroller will keep the WiFi off while data is being collected on regular intervals. Once the time for push has come - WiFi will be turned on, data will be pushed to the InfluxDB and the WiFi will be turned off again.

# Usage

Clone the project in the lib/common folder and just use the provided classes.