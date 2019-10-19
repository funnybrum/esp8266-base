#pragma once

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

struct NetworkSettings {
    char hostname[64];
    char ssid[32];
    char password[32];
};

#include "Logger.h"
#include "WebServerBase.h"

const char NETWORK_CONFIG_PAGE[] PROGMEM = R"=====(
<fieldset style='display: inline-block; width: 300px'>
<legend>Network settings</legend>
Hostname:<br>
<input type="text" name="hostname" value="%s"><br>
<small><em>from 4 to 63 characters lenght, can contain chars, digits and '-'</em></small><br><br>
SSID:<br>
<input type="text" name="ssid" value="%s"><br>
<small><em>WiFi network to connect to</em></small><br><br>
Password:<br>
<input type="password" name="password"><br>
<small><em>WiFi network password</em></small><br>
</fieldset>
)=====";

enum WiFiState {
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
    AP
};

class WiFiManager {
    public:
        WiFiManager(Logger* logger, NetworkSettings* settings) {
            this->logger = logger;
            this->settings = settings;
        }

        void begin() {
            WiFi.persistent(false);
            WiFi.mode(WIFI_STA);
            state = DISCONNECTED;
            lastStateSetAt = millis();
        }

        void loop() {
            switch(state) {
                case CONNECTED:
                    // Do nothing.
                    break;
                case CONNECTING:
                    if (WiFi.status() == WL_CONNECTED) {
                        logger->log("Connected in %.1f seconds", (millis() - lastStateSetAt)/1000.0f);
                        logger->log("IP address is %s", WiFi.localIP().toString().c_str());
                        state = CONNECTED;
                        lastStateSetAt = millis();
                    } else if (millis() - lastStateSetAt > 30000) {
                        logger->log("Failed to connect in 30 seconds, switching to AP mode");

                        // For debug purposes - switch to access mode.
                        WiFi.softAPConfig(
                            IPAddress(192, 168, 0, 1),
                            IPAddress(192, 168, 0, 1),
                            IPAddress(255, 255, 255, 0)); 
                        WiFi.softAP(settings->hostname);
                        state = AP;
                        lastStateSetAt = millis();
                    }
                    break;
                case DISCONNECTED:
                    // Do nothing.
                    break;
                case AP:
                    // If there is network configured - don't stay in AP mode for more than 5
                    // minutes. Try to reconnect to the configured network.
                    if (strlen(settings->ssid) > 1 &&
                        millis() - lastStateSetAt > 5 * 60 * 1000) {
                        disconnect();
                        delay(1000);
                        connect();
                    }
            }
        }

        void connect() {
            if (state != DISCONNECTED) {
                return;
            }
            WiFi.mode(WIFI_STA);
            _connect();
        }

        void disconnect() {
            if (state == DISCONNECTED) {
                return;
            }
            WiFi.mode(WIFI_OFF);
            state = DISCONNECTED;
            lastStateSetAt = millis();
        }

        bool isConnected() {
            return WiFi.status() == WL_CONNECTED && state == CONNECTED;
        }

        bool isInAPMode() {
            return state == AP;
        }

        void get_config_page(char* buffer) {
            sprintf_P(
                buffer,
                NETWORK_CONFIG_PAGE,
                settings->hostname,
                settings->ssid);
        }

        void parse_config_params(WebServerBase* webServer, bool& save) {
            webServer->process_setting("hostname", settings->hostname, sizeof(settings->hostname), save);
            webServer->process_setting("ssid", settings->ssid, sizeof(settings->ssid), save);
            webServer->process_setting("password", settings->password, sizeof(settings->password), save);
        }

    private:
        void _connect() {
            WiFi.disconnect();

            int networks = WiFi.scanNetworks();
            String strongestSSID = String("");
            int strongestSignalStrength = -1000;

            for (int i = 0; i < networks; i++) {
                int signalStrength = WiFi.RSSI(i);
                String ssid = WiFi.SSID(i);
                if (ssid.equals(settings->ssid)) {
                    if (strongestSignalStrength < signalStrength) {
                        logger->log("Found %s (%ddBm)", ssid.c_str(), signalStrength);
                        strongestSignalStrength = signalStrength;
                        strongestSSID = WiFi.SSID(i);
                    }
                }
            }

            logger->log("Hostname is %s", settings->hostname);

            if (strongestSSID.compareTo("") != 0) {
                WiFi.hostname(settings->hostname);
                WiFi.begin(strongestSSID.c_str(), settings->password);

                lastStateSetAt = millis();
                state = CONNECTING;
            } else {
                logger->log("SSID \"%s\" not found, switching to AP mode", settings->ssid);

                // For debug purposes - switch to access mode.
                WiFi.softAPConfig(
                    IPAddress(192, 168, 0, 1),
                    IPAddress(192, 168, 0, 1),
                    IPAddress(255, 255, 255, 0)); 
                WiFi.softAP(settings->hostname);

                lastStateSetAt = millis();
                state = AP;
            }

            WiFi.scanDelete();
        }

        WiFiState state;
        unsigned long lastStateSetAt;

        Logger* logger = NULL;
        NetworkSettings* settings = NULL;
};
