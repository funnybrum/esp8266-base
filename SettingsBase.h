#pragma once

#include "Logger.h"
#include <EEPROM.h>

template <class T> class SettingsBase {
    public:
        SettingsBase(Logger* logger) {
            this->_logger = logger;
        }

        void begin() {
            EEPROM.begin(sizeof(T)+1);

            _checksum = EEPROM.read(0);
            for (unsigned int i = 0; i < sizeof(T); i++) {
                *(((uint8_t*)this->getSettings()) + i) = EEPROM.read(i+1);
            }
            EEPROM.end();

            if (_checksum == calculateChecksum()) {
                _logger->log("Settings loaded successfully.");
            } else {
                _logger->log("Invalid settings checksum.");
                memset(this->getSettings(), 0, sizeof(T));
                initializeSettings();
                _checksum = calculateChecksum();
            }
        }

        void loop() {
            if (_checksum != calculateChecksum()) {
                _logger->log("Writing settings to EEPROM.");
                writeToEEPROM();
            }
        }

    protected:
        virtual void initializeSettings() = 0;
        virtual T* getSettings() = 0;

    private:
        uint8_t calculateChecksum() {
            uint16_t checksum = 13;
            for (unsigned int i = 0; i < sizeof(T); i++) {
                checksum += *(((uint8_t*)this->getSettings()) + i);
            }
            return checksum % 256;
        }
        void writeToEEPROM() {
            EEPROM.begin(sizeof(T)+1);
            _checksum = calculateChecksum();
            EEPROM.write(0, _checksum);
            for (unsigned int i = 0; i < sizeof(T); i++) {
                EEPROM.write(i+1, *(((uint8_t*)this->getSettings()) + i));
            }
            EEPROM.end();
        }

        Logger* _logger = NULL;
        uint8_t _checksum;
};
