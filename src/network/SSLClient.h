#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "../utils/Logger.h"

// Clean SSL client wrapper - No FirebaseClient dependencies!
class DecentSSLClient {
public:
    DecentSSLClient() : _client(), _isConnected(false) {}
    
    // Initialize the SSL client
    bool begin() {
        _client.setInsecure(); // Skip certificate verification for now
        _client.setBufferSizes(4096, 1024); // Optimized for ESP8266
        
        Logger::network("SSL client initialized");
        return true;
    }
    
    // Connect to host
    bool connect(const char* host, int port) {
        Logger::networkf("Connecting to %s:%d", host, port);
        
        if (_client.connect(host, port)) {
            _isConnected = true;
            Logger::network("SSL connection established");
            return true;
        } else {
            Logger::error("NETWORK", "SSL connection failed");
            _isConnected = false;
            return false;
        }
    }
    
    // Disconnect
    void disconnect() {
        if (_isConnected) {
            _client.stop();
            _isConnected = false;
            Logger::network("SSL connection closed");
        }
    }
    
    // Check if connected
    bool connected() { return _isConnected && _client.connected(); }
    
    // Get the underlying client for HTTP operations
    WiFiClientSecure& getClient() { return _client; }
    const WiFiClientSecure& getClient() const { return _client; }
    
    // Available data
    int available() { return _client.available(); }
    
    // Read data
    int read() { return _client.read(); }
    int read(uint8_t* buffer, size_t size) { return _client.read(buffer, size); }
    
    // Read string until delimiter
    String readStringUntil(char delimiter) {
        String result = "";
        while (_client.available()) {
            char c = _client.read();
            if (c == delimiter) break;
            result += c;
        }
        return result;
    }
    
    // Write data
    size_t write(uint8_t byte) { return _client.write(&byte, 1); }
    size_t write(const uint8_t* buffer, size_t size) { return _client.write(buffer, size); }
    size_t write(const char* str) { return _client.write((const uint8_t*)str, strlen(str)); }
    size_t write(const String& str) { return _client.write((const uint8_t*)str.c_str(), str.length()); }
    
    // Flush
    void flush() { _client.flush(); }
    
    // Stop
    void stop() { 
        _client.stop(); 
        _isConnected = false;
    }

private:
    WiFiClientSecure _client;
    bool _isConnected;
};
