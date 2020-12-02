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

    protected:
        virtual void initializeSettings() = 0;
        virtual T_EEPROM* getSettings() = 0;
        virtual T_RTC* getRTCSettings() = 0;

    private:
        uint8_t calculateChecksum(void* addr, uint16_t size) {
            uint16_t checksum = 13;
            for (unsigned int i = 0; i < size; i++) {
                checksum += *(((uint8_t*)addr) + i);
            }
            checksum += size;
            return checksum % 256;
        }

        uint8_t calculateRTCChecksum() {
            return calculateChecksum(getRTCSettings(), sizeof(T_RTC));
        }

        uint8_t calculateEEPROMChecksum() {
            return calculateChecksum(getSettings(), sizeof(T_EEPROM));
        }

        bool readEEPROM() {
            EEPROM.begin(sizeof(T_EEPROM)+1);
            _checksum = EEPROM.read(0);
            for (unsigned int i = 0; i < sizeof(T_EEPROM); i++) {
                *((uint8_t*)getSettings() + i) = EEPROM.read(i+1);
            }
            EEPROM.end();

            return _checksum == calculateEEPROMChecksum();
        }

        void writeEEPROM() {
                _checksum = calculateEEPROMChecksum();
                EEPROM.begin(sizeof(T_EEPROM)+1);
                EEPROM.write(0, _checksum);
                for (unsigned int i = 0; i < sizeof(T_EEPROM); i++) {
                    EEPROM.write(i+1, *(((uint8_t*)getSettings()) + i));
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
        uint8_t _checksum;
        uint32_t _rtcChecksum;
};
