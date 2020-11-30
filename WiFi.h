#pragma once

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

struct NetworkSettings {
    char hostname[64];
    char ssid[32];
    char password[32];
    uint8_t wifi_channel;  // Managed by the WiFiManager, used for quick reconnect
    uint8_t bssid[6];   // Managed by the WiFiManager, used for quick reconnect
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

enum _WiFiState {
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
    AP
};

#define WIFI_CONNECT_TIMEOUT 15000  // 15 seconds

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
                        logger->log("Connected in %.1f seconds, IP address is %s",
                                    (millis() - lastStateSetAt)/1000.0f,
                                    WiFi.localIP().toString().c_str());

                        // Store these for quicker connect next time.
                        memcpy(settings->bssid, WiFi.BSSID(), 6);
                        settings->wifi_channel = WiFi.channel();

                        state = CONNECTED;
                        lastStateSetAt = millis();
                    } else if (millis() - lastStateSetAt > WIFI_CONNECT_TIMEOUT) {
                        logger->log("Connection failed, going in AP mode");

                        // For setup and debug purposes.
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

        void parse_config_params(WebServerBase* webServer) {
            webServer->process_setting("hostname", settings->hostname, sizeof(settings->hostname));
            webServer->process_setting("ssid", settings->ssid, sizeof(settings->ssid));
            webServer->process_setting("password", settings->password, sizeof(settings->password));
        }

    private:
        void _connect() {
            WiFi.disconnect();

            logger->log("Hostname is %s", settings->hostname);
            WiFi.hostname(settings->hostname);

            lastStateSetAt = millis();
            state = CONNECTING;

            // Even with invalid bssid and wifi channel the below will succeed with correct ssid/password.
            WiFi.begin(settings->ssid,
                       settings->password,
                       settings->wifi_channel,
                       settings->bssid,
                       true);
        }

        _WiFiState state;
        unsigned long lastStateSetAt;

        Logger* logger = NULL;
        NetworkSettings* settings = NULL;
};
