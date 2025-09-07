#pragma once

#include <Arduino.h>

// Clean logging system - No more FirebaseClient spam!
class Logger {
public:
    enum Level {
        NONE = 0,
        ERROR = 1,
        WARN = 2,
        INFO = 3,
        DEBUG = 4,
        VERBOSE = 5
    };
    
    static void setLevel(Level level) { _level = level; }
    static Level getLevel() { return _level; }
    
    // Error logging - Always shown
    static void error(const char* tag, const char* message) {
        if (_level >= ERROR) {
            Serial.printf("[ERROR][%s] %s\n", tag, message);
        }
    }
    
    static void errorf(const char* tag, const char* format, ...) {
        if (_level >= ERROR) {
            Serial.printf("[ERROR][%s] ", tag);
            va_list args;
            va_start(args, format);
            Serial.printf(format, args);
            va_end(args);
            Serial.println();
        }
    }
    
    // Warning logging
    static void warn(const char* tag, const char* message) {
        if (_level >= WARN) {
            Serial.printf("[WARN][%s] %s\n", tag, message);
        }
    }
    
    static void warnf(const char* tag, const char* format, ...) {
        if (_level >= WARN) {
            Serial.printf("[WARN][%s] ", tag);
            va_list args;
            va_start(args, format);
            Serial.printf(format, args);
            va_end(args);
            Serial.println();
        }
    }
    
    // Info logging
    static void info(const char* tag, const char* message) {
        if (_level >= INFO) {
            Serial.printf("[INFO][%s] %s\n", tag, message);
        }
    }
    
    static void infof(const char* tag, const char* format, ...) {
        if (_level >= INFO) {
            Serial.printf("[INFO][%s] ", tag);
            va_list args;
            va_start(args, format);
            Serial.printf(format, args);
            va_end(args);
            Serial.println();
        }
    }
    
    // Debug logging
    static void debug(const char* tag, const char* message) {
        if (_level >= DEBUG) {
            Serial.printf("[DEBUG][%s] %s\n", tag, message);
        }
    }
    
    static void debugf(const char* tag, const char* format, ...) {
        if (_level >= DEBUG) {
            Serial.printf("[DEBUG][%s] ", tag);
            va_list args;
            va_start(args, format);
            Serial.printf(format, args);
            va_end(args);
            Serial.println();
        }
    }
    
    // Verbose logging
    static void verbose(const char* tag, const char* message) {
        if (_level >= VERBOSE) {
            Serial.printf("[VERBOSE][%s] %s\n", tag, message);
        }
    }
    
    // Convenience methods for common tags
    static void firebase(const char* message) { info("FIREBASE", message); }
    static void firebasef(const char* format, ...) {
        Serial.printf("[INFO][FIREBASE] ");
        va_list args;
        va_start(args, format);
        Serial.printf(format, args);
        va_end(args);
        Serial.println();
    }
    
    static void stream(const char* message) { info("STREAM", message); }
    static void streamf(const char* format, ...) {
        Serial.printf("[INFO][STREAM] ");
        va_list args;
        va_start(args, format);
        Serial.printf(format, args);
        va_end(args);
        Serial.println();
    }
    
    static void network(const char* message) { info("NETWORK", message); }
    static void networkf(const char* format, ...) {
        Serial.printf("[INFO][NETWORK] ");
        va_list args;
        va_start(args, format);
        Serial.printf(format, args);
        va_end(args);
        Serial.println();
    }

private:
    static Level _level;
};

// Default to INFO level - Clean but informative
#define LOG_LEVEL Logger::INFO
