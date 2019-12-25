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

            uint8_t checksum = EEPROM.read(0);
            for (unsigned int i = 0; i < sizeof(T); i++) {
                *(((uint8_t*)this->getSettings()) + i) = EEPROM.read(i+1);
            }
            EEPROM.end();

            if (checksum == sizeof(T) % 256) {
                _logger->log("Settings loaded successfully.");
            } else {
                _logger->log("Invalid settings checksum.");
                erase();
            }
        }

        void loop() {
        }

        void save() {
            _logger->log("Writing state to EEPROM.");
            writeToEEPROM();
        }

        void erase() {
            _logger->log("Erasing EEPROM.");
            memset(this->getSettings(), 0, sizeof(T));
            initializeSettings();
            writeToEEPROM();
        }

    protected:
        virtual void initializeSettings() = 0;
        virtual T* getSettings() = 0;

    private:
        void writeToEEPROM() {
            EEPROM.begin(sizeof(T)+1);
            EEPROM.write(0, sizeof(T) % 256);
            for (unsigned int i = 0; i < sizeof(T); i++) {
                EEPROM.write(i+1, *(((uint8_t*)this->getSettings()) + i));
            }
            EEPROM.end();
        }

        Logger* _logger = NULL;
};
