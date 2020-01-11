#pragma once

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <ESP8266HTTPClient.h>

#include "Logger.h"
#include "WiFi.h"
#include "WebServerBase.h"
// Compatible with version 6 of the ArduinoJson library.
#include <ArduinoJson.h>

#define JSON_DOC_CAPACITY 3*JSON_ARRAY_SIZE(1) + 2*JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + 120

const char INFLUXDB_CLIENT_CONFIG_PAGE[] PROGMEM = R"=====(
<fieldset style='display: inline-block; width: 300px'>
<legend>InfluxDB client settings</legend>
Address:<br>
<input type="text" name="ifxc_address" value="%s"><br>
<small><em>like 'http://192.168.0.1:8086'</em></small><br><br>
Database:<br>
<input type="text" name="ifxc_db" value="%s"><br>
<small><em>Database to query</em></small><br><br>
Metric:<br>
<input type="text" name="ifxc_metric" value="%s"><br>
<small><em>i.e. humidity</em></small><br><br>
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
    char metric[16];
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
            doc = new DynamicJsonDocument(JSON_DOC_CAPACITY);
            lastQuery = millis() - _settings->queryInterval * 1000;
        }

        void loop() {
            if (_settings->queryInterval < 0 || strlen(_settings->database) < 0) {
                // Not configured.
                return;
            }
            if (millis() - lastQuery > _settings->queryInterval * 1000) {
                // Time for pull. Either the time for that has come or the buffer is getting full.
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
                _settings->metric,
                _settings->srcTag,
                _settings->queryInterval,
                _settings->lookBack);
        }

        void parse_config_params(WebServerBase* webServer, bool& save) {
            webServer->process_setting("ifxc_address", _settings->address, sizeof(_settings->address), save);
            webServer->process_setting("ifxc_db", _settings->database, sizeof(_settings->database), save);
            webServer->process_setting("ifxc_metric", _settings->metric, sizeof(_settings->metric), save);
            webServer->process_setting("ifxc_src", _settings->srcTag, sizeof(_settings->srcTag), save);
            webServer->process_setting("ifxc_qi", _settings->queryInterval, save);
            webServer->process_setting("ifxc_lb", _settings->lookBack, save);
        }

        float getQueryResult() {
            return lastDataPoint;
        }

        bool isDataAvailable() {
            return dataAvailable;
        }

        void purgeData() {
            dataAvailable = false;
        }

    private:
        bool query() {
            if (strlen(_settings->address) < 5 ||
                strlen(_settings->database) == 0 ||
                strlen(_settings->metric) == 0 ||
                strlen(_settings->srcTag) == 0) {
                _logger->log("InfluxDB integration is not configure.");
                return false;
            }
            String url = "";
            url += _settings->address;
            url += "/query?db=";
            url += _settings->database;
            url += "&q=SELECT+last%28%22value%22%29+FROM+%22";
            url += _settings->metric;
            url += "%22+WHERE+time+%3E%3D+now%28%29+-+";
            url += _settings->lookBack;
            url += "m+AND+%22src%22%3D%27";
            url += _settings->srcTag;
            url += "%27";

            http->begin(url);
            int statusCode = http->GET();

            bool success = statusCode == 200;
            if (success) {
                deserializeJson(*doc, http->getString());
                if ((*doc)["results"][0].containsKey("series")) {
                    lastDataPoint = (*doc)["results"][0]["series"][0]["values"][0][1];
                    dataAvailable = true;
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
        DynamicJsonDocument *doc;

        Logger* _logger = NULL;
        WiFiManager* _wifi = NULL;
        InfluxDBClientSettings* _settings = NULL;
        NetworkSettings* _networkSettings = NULL;

        bool dataAvailable = false;
        float lastDataPoint = -1;
};
