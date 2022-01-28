#pragma once

#include "Logger.h"
#include <EEPROM.h>

template <class T_EEPROM, class T_RTC> class SettingsBase {
    public:
        SettingsBase(Logger* logger) {
            _logger = logger;
        }

        void begin() {
            if (readEEPROM()) {
                _logger->log("Settings loaded successfully");
            } else {
                _logger->log("Invalid settings checksum");
                memset(getSettings(), 0, sizeof(T_EEPROM));
                initializeSettings();
                _checksum = calculateEEPROMChecksum();
            }

            if (getRTCSettings() != NULL) {
                if (readRTC()) {
                    _logger->log("RTC settings loaded successfully");
                } else {
                    _logger->log("Invalid RTC settings checksum");
                    memset(getRTCSettings(), 0, sizeof(T_RTC));
                    _rtcChecksum = calculateRTCChecksum();
                }
            }
        }

        void loop() {
            if (_checksum != calculateEEPROMChecksum()) {
                _logger->log("Writing settings to EEPROM");
                writeEEPROM();
            }

            if (getRTCSettings() != NULL && _rtcChecksum != calculateRTCChecksum()) {
                writeRTC();
                _logger->log("Writing settings to RTC");
            }            
        }

        uint32_t getRTCCheckSum() {
            return calculateRTCChecksum();
        }

    protected:
        virtual void initializeSettings() = 0;
        virtual T_EEPROM* getSettings() = 0;
        virtual T_RTC* getRTCSettings() = 0;

    private:
        uint32_t crc32(const void *data, uint16_t size) {
            uint16_t i, j;
            uint32_t byte, crc, mask;

            crc = 0xFFFFFFFF;
            for (i = 0; i < size; i++) {
                byte = *(((uint8_t*)data) + i);            // Get next byte.
                crc = crc ^ byte;
                for (j = 0; j < 8; j++) {    // Do eight times.
                    mask = -(crc & 1);
                    crc = (crc >> 1) ^ (0xEDB88320 & mask);
                }
            }
            return crc;
        }

        uint32_t calculateRTCChecksum() {
            return crc32(getRTCSettings(), sizeof(T_RTC));
        }

        uint32_t calculateEEPROMChecksum() {
            return crc32(getSettings(), sizeof(T_EEPROM));
        }

        bool readEEPROM() {
            EEPROM.begin(sizeof(T_EEPROM)+4);

            // Read the checksum
            _checksum = (EEPROM.read(0) << 24) |
                        (EEPROM.read(1) << 16) |
                        (EEPROM.read(2) << 8) |
                        EEPROM.read(3);
            
            // Read the data
            for (unsigned int i = 0; i < sizeof(T_EEPROM); i++) {
                *((uint8_t*)getSettings() + i) = EEPROM.read(i+4);
            }
            EEPROM.end();

            return _checksum == calculateEEPROMChecksum();
        }

        void writeEEPROM() {
                _checksum = calculateEEPROMChecksum();
                EEPROM.begin(sizeof(T_EEPROM)+4);

                // Write the checksum
                EEPROM.write(0, (_checksum >> 24) & 0xFF);
                EEPROM.write(1, (_checksum >> 16) & 0xFF);
                EEPROM.write(2, (_checksum >> 8) & 0xFF);
                EEPROM.write(3, _checksum & 0xFF);

                // Write the data
                for (unsigned int i = 0; i < sizeof(T_EEPROM); i++) {
                    EEPROM.write(i+4, *(((uint8_t*)getSettings()) + i));
                }
                EEPROM.end();
        }

        bool readRTC() {
            ESP.rtcUserMemoryRead(0, &_rtcChecksum, 4);
            ESP.rtcUserMemoryRead(1, (uint32_t*)getRTCSettings(), sizeof(T_RTC));
            return _rtcChecksum == calculateRTCChecksum();
        }

        void writeRTC() {
            _rtcChecksum = calculateRTCChecksum();
            ESP.rtcUserMemoryWrite(0, &_rtcChecksum, 4);
            bool res = ESP.rtcUserMemoryWrite(1, (uint32_t*)getRTCSettings(), sizeof(T_RTC));
            if (!res) 
                _logger->log("Failed to write RTC settings");
        }

        Logger* _logger = NULL;
        uint32_t _checksum;
        uint32_t _rtcChecksum;
};
