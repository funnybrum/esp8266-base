#pragma once

#include <ESP8266HTTPClient.h>

#include "Logger.h"
#include "WiFi.h"
#include "WebServerBase.h"

const char INFLUXDB_CONFIG_PAGE[] PROGMEM = R"=====(
<fieldset style='display: inline-block; width: 300px'>
<legend>InfluxDB settings</legend>
InfluxDB integration:<br>
<select name="ifx_enabled">
<option value="true" %s>Enabled</option>
<option value="false" %s>Disabled</option>
</select><br><br>
Address:<br>
<input type="text" name="ifx_address" value="%s"><br>
<small><em>like 'http://192.168.0.1:8086'</em></small><br><br>
Database:<br>
<input type="text" name="ifx_db" value="%s"><br>
<small><em>WiFi network to connect to</em></small><br><br>
Collect interval:<br>
<input type="text" name="ifx_collect" value="%d"><br>
<small><em>in seconds, from 0 to 65535</em></small><br><br>
Push interval:<br>
<input type="text" name="ifx_push" value="%d"><br>
<small><em>in seconds, from 0 to 65535</em></small><br><br>
</fieldset>
)=====";

#ifndef TELEMETRY_BUFFER_SIZE
#define TELEMETRY_BUFFER_SIZE 24 * 1024
#endif

struct InfluxDBCollectorSettings {
    bool enable;
    char address[64];
    char database[16];
    uint16_t pushInterval;
    uint16_t collectInterval;
};

class InfluxDBCollector {
    public:
        InfluxDBCollector(Logger* logger,
                          WiFiManager* wifi,
                          InfluxDBCollectorSettings* settings,
                          NetworkSettings* networkSettings,
                          void (*collectDataCallback)(InfluxDBCollector*),
                          bool (*shouldPushCallback)(void) = NULL,
                          void (*onPushCallback)(void) = NULL) {
            this->logger = logger;
            this->wifi = wifi;
            this->settings = settings;
            this->networkSettings = networkSettings;
            this->collectDataCallback = collectDataCallback;
            this->onPushCallback = onPushCallback;
            this->shouldPushCallback = shouldPushCallback;
        }

        void begin() {
            http = new HTTPClient();
            const char * headerKeys[] = {"date"};
            http->collectHeaders(headerKeys, 1);
        }

        void loop() {
            if (!enabled) {
                return;
            }

            if (remoteTimestamp == 0) {
                // This will be executed only once on startup. Don't shutdown the WiFi, the first
                // push will do that.
                if (!wifi->isConnected()) {
                    wifi->connect();
                } else {
                    ping();
                }
            }

            if (millis() - lastDataPush > settings->pushInterval * 1000 ||
                telemetryDataSize >= 0.80f * TELEMETRY_BUFFER_SIZE ||
                (shouldPushCallback != NULL && shouldPushCallback())) {
                // Time for push. Either the time for that has come or the buffer is getting full.
                if (!wifi->isConnected()) {
                    wifi->connect();
                } else {
                    if (push()) {
                        lastDataPush = millis();
                        // Don't disconnect in the first 30 minutes.
                        if (millis() > 30 * 60 * 1000) {
                            wifi->disconnect();
                        }
                    }
                }
            }

            if (millis() - lastDataCollect > settings->collectInterval * 1000) {
                collectDataCallback(this);
                lastDataCollect += settings->collectInterval * 1000;
            }
        }

        void append(const char* metric, float value, uint8_t precision=0) {
            char format[32];
            int metricSize = 0;
            if (remoteTimestamp > 0) {
                sprintf(format, "%%s,src=%%s value=%%.%df %%ld\n", precision);
                metricSize = snprintf(
                    telemetryData + telemetryDataSize,
                    TELEMETRY_BUFFER_SIZE - telemetryDataSize,
                    format,
                    metric,
                    networkSettings->hostname,
                    value,
                    getTimestamp());
            } else {
                // Same as above, but without a timestamp. In this case the timestamp for the metric will
                // be the current time when they are send to the InfluxDB.
                sprintf(format, "%%s,src=%%s value=%%.%df\n", precision);
                metricSize = snprintf(
                    telemetryData + telemetryDataSize,
                    TELEMETRY_BUFFER_SIZE - telemetryDataSize,
                    format,
                    metric,
                    networkSettings->hostname,
                    value);
            }

            if (telemetryDataSize + metricSize > TELEMETRY_BUFFER_SIZE) {
                // In that case - we don't have the whole metric line in the buffer.
                telemetryData[telemetryDataSize] = '\0';
                logger->log("Telemetry buffer overflow!");
            } else {
                telemetryDataSize += metricSize;
            }
        }

        void stop() {
            if (!enabled) {
                return;
            }

            enabled = false;

            if (telemetryDataSize > 0) {
                // The stop can be invoked only if the settings get changed. In this case the WiFi should
                // be up and running.
                push();
            }
        }

        void start() {
            if (enabled) {
                return;
            }

            enabled = true;

            lastDataCollect = millis() - settings->collectInterval * 1000;
            lastDataPush = millis();
            remoteTimestamp = 0;
        }

        void get_config_page(char* buffer) {
            sprintf_P(
                buffer,
                INFLUXDB_CONFIG_PAGE,
                (settings->enable)?"selected":"",
                (!settings->enable)?"selected":"",
                settings->address,
                settings->database,
                settings->collectInterval,
                settings->pushInterval);
        }

        void parse_config_params(WebServerBase* webServer, bool& save) {
            webServer->process_setting("ifx_enabled", settings->enable, save);
            webServer->process_setting("ifx_address", settings->address, sizeof(settings->address), save);
            webServer->process_setting("ifx_db", settings->database, sizeof(settings->database), save);
            webServer->process_setting("ifx_collect", settings->collectInterval, save);
            webServer->process_setting("ifx_push", settings->pushInterval, save);
        }

    private:
        // Sync the local timestamp based on the date/time response from the InfluxDB server. This
        // is needed in order to append the proper timestamps to the metrics beeing generated.
        void syncTime(const char* dateTime) {
            if (strlen(dateTime) != 29) {
                // Something's wrong. The datetime should be 29 characters.
                logger->log("Failed to parse the InfluxDB date/time: %s", dateTime);
                return;
            }
            // Get the millis when the remote date was received.
            remoteTimestampMillis = millis();

            // Calculate the timestamp from a date/time string as "Sat, 08 Dec 2018 07:38:17 GMT". Based on
            // the "Seconds Since the Epoch" formula as defined by POSIX:2008 section 4.15. The following
            // are the required parameters:
            int16_t tm_sec = atoi(dateTime+23);         // seconds
            int16_t tm_min = atoi(dateTime+20);         // minutes
            int16_t tm_hour = atoi(dateTime+17);        // hours
            int16_t tm_year = atoi(dateTime+12) - 1900; // calendar year minus 1900
            int16_t tm_yday;                            // passed days since January 1 of the current year

            // tm_yday is calculated based on the month, the day and on the year (leap/non-leap).

            // Day in current month
            int16_t day = atoi(dateTime+5);

            // Convert month to number. 1 for January, 12 for December.
            const char* months[12] = {
                "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
            };
            int16_t month = 1;
            while (strncmp(dateTime + 8, months[month-1], 3) != 0 && month < 13) {
                month++;
            }

            // Determine if the year is leap or not.
            bool isLeapYear = false;
            if ((tm_year + 1900) % 4 == 0 )
                isLeapYear = true;
            if ((tm_year + 1900) % 100 == 0 )
                isLeapYear = false;
            if ((tm_year + 1900) % 400 == 100 )
                isLeapYear = true;

            // Calculate the number of passed days before the current month.
            int16_t days_before_month[2][13] = {
                /* Normal years.  */
                {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
                /* Leap years.  */
                {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
            };

            // Calculate the full days that have passed since the year start.
            tm_yday = days_before_month[isLeapYear][month-1] + day-1;

            remoteTimestamp =
                tm_sec + tm_min*60 +
                tm_hour*3600 +
                tm_yday*86400 +
                (tm_year-70)*31536000 +
                ((tm_year-69)/4)*86400 -
                ((tm_year-1)/100)*86400 +
                ((tm_year+299)/400)*86400;
        }

        unsigned long getTimestamp() {
            // The below would be correct even if the millis() have rolled over. The only requirement for
            // this to work is to have the last timestamp retrieved before less than the millis rollover
            // period that is ~50 days.
            unsigned long secondsSinceTimestampRetrieval = (millis() - remoteTimestampMillis) / 1000;

            return remoteTimestamp + secondsSinceTimestampRetrieval;
        }

        // Executed with only purpose to get the current timestamp of the IndluxDB.
        void ping() {
            String url = "";
            url += settings->address;
            url += "/ping";
            http->begin(url);
            int httpCode = http->GET();
            if (httpCode == 204) {
                syncTime(http->header("date").c_str());
            }
            http->end();
        }

        bool push() {
            String url = "";
            url += settings->address;
            url += "/write?precision=s&db=";
            url += settings->database;

            http->begin(url);
            int statusCode = http->POST((uint8_t *)telemetryData, telemetryDataSize-1);  // -1 to remove
                                                                                        // the last '\n'.
            if (statusCode == 204) {
                telemetryDataSize = 0;
                syncTime(http->header("date").c_str());
                if (onPushCallback != NULL) {
                    onPushCallback();
                }
                return true;
            } else {
                logger->log("Push failed with HTTP %d", statusCode);
                wifi->disconnect();
                wifi->connect();
            }

            return false;
        }

        char telemetryData[TELEMETRY_BUFFER_SIZE];
        unsigned int telemetryDataSize = 0;
        unsigned long lastDataCollect;
        unsigned long lastDataPush;
        unsigned long remoteTimestamp;
        unsigned long remoteTimestampMillis;
        bool enabled = false;
        HTTPClient* http = NULL;

        Logger* logger = NULL;
        WiFiManager* wifi = NULL;
        InfluxDBCollectorSettings* settings = NULL;
        NetworkSettings* networkSettings = NULL;
        void (*collectDataCallback)(InfluxDBCollector*) = NULL;
        bool (*shouldPushCallback)(void) = NULL;
        void (*onPushCallback)(void) = NULL;
};
