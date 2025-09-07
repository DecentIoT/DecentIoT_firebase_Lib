#pragma once

#include <Arduino.h>

// Universal value type for callbacks - Clean and efficient
struct DecentIoTValue {
    enum Type { BOOL, INT, FLOAT, STRING } type;
    
    union {
        bool boolValue;
        int intValue;
        float floatValue;
    };
    String stringValue; // String needs to be separate due to C++ union limitations
    
    // Default constructor
    DecentIoTValue() : type(BOOL), boolValue(false), stringValue("") {}
    
    // Type-specific constructors
    DecentIoTValue(bool value) : type(BOOL), boolValue(value), stringValue("") {}
    DecentIoTValue(int value) : type(INT), intValue(value), stringValue("") {}
    DecentIoTValue(float value) : type(FLOAT), floatValue(value), stringValue("") {}
    DecentIoTValue(const String& value) : type(STRING), stringValue(value) {}
    DecentIoTValue(const char* value) : type(STRING), stringValue(value) {}
    
    // Implicit conversion operators - Clean and intuitive
    operator bool() const {
        switch (type) {
            case BOOL: return boolValue;
            case INT: return intValue != 0;
            case FLOAT: return floatValue != 0.0f;
            case STRING: return stringValue == "true" || stringValue == "1";
            default: return false;
        }
    }
    
    operator int() const {
        switch (type) {
            case INT: return intValue;
            case BOOL: return boolValue ? 1 : 0;
            case FLOAT: return static_cast<int>(floatValue);
            case STRING: return stringValue.toInt();
            default: return 0;
        }
    }
    
    operator float() const {
        switch (type) {
            case FLOAT: return floatValue;
            case INT: return static_cast<float>(intValue);
            case BOOL: return boolValue ? 1.0f : 0.0f;
            case STRING: return stringValue.toFloat();
            default: return 0.0f;
        }
    }
    
    operator String() const {
        switch (type) {
            case STRING: return stringValue;
            case BOOL: return boolValue ? "true" : "false";
            case INT: return String(intValue);
            case FLOAT: return String(floatValue);
            default: return "";
        }
    }
    
    operator uint8_t() const {
        switch (type) {
            case BOOL: return boolValue ? HIGH : LOW;
            case INT: return intValue;
            case FLOAT: return static_cast<uint8_t>(floatValue);
            case STRING: return (stringValue == "true" || stringValue == "1") ? HIGH : LOW;
            default: return 0;
        }
    }
    
    // Clear method for reuse
    void clear() {
        type = BOOL;
        boolValue = false;
        stringValue = "";
    }
    
    // Get type as string for debugging
    String getTypeString() const {
        switch (type) {
            case BOOL: return "BOOL";
            case INT: return "INT";
            case FLOAT: return "FLOAT";
            case STRING: return "STRING";
            default: return "UNKNOWN";
        }
    }
};
