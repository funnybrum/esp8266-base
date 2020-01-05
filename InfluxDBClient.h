#pragma once

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <ESP8266HTTPClient.h>

#include "Logger.h"
#include "WiFi.h"
#include "WebServerBase.h"
// Compatible with version 6 of the ArduinoJson library.
#include <ArduinoJson.h>

const char INFLUXDB_CLIENT_CONFIG_PAGE[] PROGMEM = R"=====(
<fieldset style='display: inline-block; width: 300px'>
<legend>InfluxDB client settings</legend>
Address:<br>
<input type="text" name="ifxc_address" value="%s"><br>
<small><em>like 'http://192.168.0.1:8086'</em></small><br><br>
Database:<br>
<input type="text" name="ifxc_db" value="%s"><br>
<small><em>Database to query</em></small><br><br>
Value for the 'src' tag:<br>
<input type="text" name="ifxc_src" value="%s"><br>
<small><em>The src to be queried</em></small><br><br>
Query interval:<br>
<input type="text" name="ifxc_qi" value="%d"><br>
<small><em>in seconds, from 0 to 65535</em></small><br><br>
Query window:<br>
<input type="text" name="ifxc_lb" value="%d"><br>
<small><em>Look back minutes, from 0 to 65535</em></small><br><br>

</fieldset>
)=====";

struct InfluxDBClientSettings {
    char address[64];
    char database[16];
    char srcTag[32];
    uint16_t queryInterval;
    uint16_t lookBack;
};

class InfluxDBClient {
    public:
        InfluxDBClient(Logger* _logger,
                       WiFiManager* _wifi,
                       InfluxDBClientSettings* settings,
                       NetworkSettings* networkSettings) {
            this->_logger = _logger;
            this->_wifi = _wifi;
            this->_settings = settings;
            this->_networkSettings = networkSettings;
        }

        void begin() {
            http = new HTTPClient();
            lastQuery = millis() - _settings->queryInterval * 1000;
        }

        void loop() {
            if (millis() - lastQuery > _settings->queryInterval * 1000) {
                // Time for push. Either the time for that has come or the buffer is getting full.
                if (!_wifi->isConnected()) {
                    _wifi->connect();
                } else {
                    if (query()) {
                        lastQuery = millis();
                        // Don't disconnect in the first 10 minutes.
                        if (millis() > 10 * 60 * 1000) {
                            _wifi->disconnect();
                        }
                    }
                }
            }
        }

        void get_config_page(char* buffer) {
            sprintf_P(
                buffer,
                INFLUXDB_CLIENT_CONFIG_PAGE,
                _settings->address,
                _settings->database,
                _settings->srcTag,
                _settings->queryInterval,
                _settings->lookBack);
        }

        void parse_config_params(WebServerBase* webServer, bool& save) {
            webServer->process_setting("ifxc_address", _settings->address, sizeof(_settings->address), save);
            webServer->process_setting("ifxc_db", _settings->database, sizeof(_settings->database), save);
            webServer->process_setting("ifxc_src", _settings->srcTag, sizeof(_settings->srcTag), save);
            webServer->process_setting("ifxc_qi", _settings->queryInterval, save);
            webServer->process_setting("ifxc_lb", _settings->lookBack, save);
        }

        float getQueryResult() {
            return lastQuery;
        }

    // private:
        bool query() {
            String url = "";
            url += _settings->address;
            url += "/query?db=";
            url += _settings->database;
            url += "&q=SELECT%20last%28%22value%22%29%20FROM%20%22humidity%22%20WHERE%20time%20%3E%3D%20now%28%29%20-%20";
            url += _settings->lookBack;
            url += "m%20AND%20%22src%22%3D%27";
            url += _settings->srcTag;
            url += "%27'";

            http->begin(url);
            int statusCode = http->GET();

            bool success = statusCode == 200;
            if (success) {
                DynamicJsonDocument doc(2048);
                deserializeJson(doc, http->getString());
                if (doc["results"][0].containsKey("series")) {
                    lastDataPoint = doc["results"][0]["series"][0]["values"][0][1];
                } else {
                    _logger->log("InfluxDB response with no data.");
                }
            } else {
                _logger->log("Query failed with HTTP %d", statusCode);
                _wifi->disconnect();
                _wifi->connect();
            }

            http->end();
            return success;
        }

        unsigned long lastQuery;
        HTTPClient* http = NULL;

        Logger* _logger = NULL;
        WiFiManager* _wifi = NULL;
        InfluxDBClientSettings* _settings = NULL;
        NetworkSettings* _networkSettings = NULL;

        float lastDataPoint = -1;
};
