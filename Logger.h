#pragma once

#include "Arduino.h"

#ifndef LOG_SIZE
#define LOG_SIZE 1024
#endif

/*
 * Logging tool.
 * 
 * Provide rotating log buffer with configurable maximum size.
 */
class Logger {
    public:
        Logger(bool serial_output=true) {
            this->serial_output = serial_output;
        }

        void begin() {
            buffer[0] = '\0';
        }

        void loop() {
        }

        void log(char* msg) {
            // Check if the log is being duplicated. There is no usecase for identical logs...
            if (pos > strlen(msg) && strncmp(msg, buffer + pos - strlen(msg) - 1, strlen(msg)) == 0) {
                return;
            }

            if (this->serial_output) {
                Serial.println(msg);
            }

            // new_size = pos + strlen(msg) + 1 '\n' + 1 '\0'
            // this will overflow the buffer by new_size - sizeof(buffer)
            int overflowBy = pos + strlen(msg) + 2 - sizeof(buffer);

            if (overflowBy > 0) {
                // shift back overflowBy chars to avoid buffer overflow
                memmove(buffer, buffer + overflowBy, sizeof(buffer) - overflowBy);
                pos -= overflowBy;
            }

            strcpy(buffer + pos, msg);
            pos += strlen(msg);

            // add new line char
            buffer[pos] = '\n';
            pos++;

            // add null terminating character
            buffer[pos] = 0;
        }

        void log(const char *format, ...) {
            va_list arg;
            va_start(arg, format);
            char buffer[128];
            vsnprintf(buffer, sizeof(buffer), format, arg);
            va_end(arg);
            log(buffer);
        }

        const char* getLogs() {
            return buffer;
        }

    private:
        char buffer[LOG_SIZE];
        unsigned int pos = 0;
        bool serial_output;
};
