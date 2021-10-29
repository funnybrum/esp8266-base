#pragma once

#include "Logger.h"
#include "WebServerBase.h"

#define MAX_HANDLERS 32
#define COMMAND_SEPARATOR ':'
#define TIMEOUT_MILLIS 10000

class RS485ServerBase {
    public:
        typedef std::function<void(const char *)> THandlerFunction;

        RS485ServerBase(Logger* logger,
                        NetworkSettings* networkSettings,
                        uint8_t dePin = D0,
                        uint16_t preDelay = 50,
                        uint16_t postDelay = 50) {
            _networkSettings = networkSettings;
            _logger = logger;
            _dePin = dePin;
            _preDelay = preDelay;
            _postDelay = postDelay;
            _cmdBufferPos = 0;
            _cmdHandlersPos = 0;
        }

        void begin(uint32_t baudRate = 9600, SerialConfig mode = SERIAL_8N1) {
            Serial.begin(baudRate, mode);
            pinMode(_dePin, OUTPUT);
            digitalWrite(_dePin, LOW);
            _transmitting = false;
            _logger->log("RS485 initialized");

            registerHandlers();
        }

        void loop() {
            while (Serial.available() > 0) {
                char nextChar = Serial.read();
                lastCharReceived = millis();

                if (nextChar <= 31) {
                    nextChar = 0;
                }

                if (nextChar >= 127) {
                    continue;
                }

                _cmdBuffer[_cmdBufferPos] = nextChar;
                _cmdBufferPos++;
                if (_cmdBufferPos >= sizeof(_cmdBuffer)) {
                    _logger->log("RS485 buffer overflow detected");
                    _cmdBuffer[sizeof(_cmdBuffer)-1] = 0;
                    nextChar = 0;
                }

                if (nextChar == 0) {
                    processCmdBuffer();
                    _cmdBufferPos = 0;
                }
            }

            if (_cmdBufferPos > 0 && millis() - lastCharReceived > TIMEOUT_MILLIS) {
                _cmdBufferPos = 0;
            }
        }

        void registerHandler(const char* cmd, RS485ServerBase::THandlerFunction fn) {
            if (_cmdHandlersPos >= MAX_HANDLERS) {
                _logger->log("No more handlers can be registerd");
                return;
            }
            _cmdHandlers[_cmdHandlersPos] = new CmdHandler(cmd, fn);
            _cmdHandlersPos++;
        }

        void processCmdBuffer() {
            if (strlen(_cmdBuffer) < 3) {
                // Minimum command is 3 chars - [address][separator][command].
                return;
            }

            uint8_t addressLength = strlen(_networkSettings->hostname);


            bool addressMatch = _cmdBuffer[addressLength] == COMMAND_SEPARATOR &&
                memcmp(_cmdBuffer, _networkSettings->hostname, addressLength) == 0;

            if (addressMatch) {
                for (int i = 0; i < _cmdHandlersPos; i++) {
                    CmdHandler* handler = _cmdHandlers[i];
                    if (handler->canHandle(_cmdBuffer + addressLength + 1)) {
                        handler->handle(_cmdBuffer + addressLength + 1);
                        break;
                    }
                }
            }
        }

        void sendCommand(char* destination, char* cmd) {
            loop();
            beginTransmission();
            Serial.printf("%s:%s", destination, cmd);
            endTransmission();
        } 

    protected:
        void beginTransmission() {
            digitalWrite(_dePin, HIGH);
            delayMicroseconds(_preDelay);
            _transmitting = true;
        }

        void endTransmission() {
            Serial.flush();
            delayMicroseconds(_postDelay);
            digitalWrite(_dePin, LOW);
            _transmitting = false;
        }

        virtual void registerHandlers() = 0;

    private:
        class CmdHandler {
            public:
                CmdHandler(const char* cmd, THandlerFunction fn) {
                    strlcpy(_cmd, cmd, sizeof(_cmd));
                    _cmdLen = strlen(_cmd);
                    _fn = fn;
                }

                bool canHandle(const char* cmd) {
                    if (_cmdLen > strlen(cmd)) {
                        return false;
                    }

                    bool startsWith = memcmp(cmd, _cmd, _cmdLen) == 0;
                    bool terminated = cmd[_cmdLen] == 0 || cmd[_cmdLen] == COMMAND_SEPARATOR;

                    return startsWith && terminated;
                }

                void handle(const char* cmd) {
                    const char* params = "";
                    if (cmd[_cmdLen] == COMMAND_SEPARATOR && cmd[_cmdLen + 1] != 0) {
                        params = cmd + _cmdLen + 1;
                    }

                    _fn(params);
                }
            private:
                THandlerFunction _fn;
                char _cmd[16];
                uint8_t _cmdLen;
        };

        NetworkSettings* _networkSettings;
        Logger* _logger;

        uint8_t _dePin;
        uint16_t _preDelay;
        uint16_t _postDelay;
        bool _transmitting = false;
        
        char _cmdBuffer[256];
        uint16_t _cmdBufferPos;

        CmdHandler* _cmdHandlers[MAX_HANDLERS];
        uint8_t _cmdHandlersPos;

        uint32_t lastCharReceived = 0;
};
