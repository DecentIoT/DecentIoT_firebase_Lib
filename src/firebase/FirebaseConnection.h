#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../network/SSLClient.h"

// Clean Firebase REST API client - No FirebaseClient dependencies!
class FirebaseConnection {
public:
    FirebaseConnection() : _sslClient(), _host(""), _port(443), _authToken("") {}
    
    // Initialize connection
    bool begin(const char* host, const char* authToken) {
        _host = String(host);
        _authToken = String(authToken);
        
        // Extract host from URL if needed
        if (_host.startsWith("https://")) {
            _host = _host.substring(8);
        }
        
        Serial.printf("[INFO] Firebase connection initialized for host: %s\n", _host.c_str());
        return _sslClient.begin();
    }
    
    // Basic GET request
    bool get(const char* path, String& response) {
        if (!_sslClient.connected() && !_sslClient.connect(_host.c_str(), _port)) {
            Serial.println("[ERROR] Failed to connect for GET request");
            return false;
        }
        
        String request = String("GET ") + path + " HTTP/1.1\r\n";
        request += "Host: " + _host + "\r\n";
        request += "Authorization: Bearer " + _authToken + "\r\n";
        request += "Accept: application/json\r\n";
        request += "Connection: close\r\n\r\n";
        
        Serial.printf("[DEBUG] Sending GET request to: %s\n", path);
        
        if (_sslClient.write(request.c_str()) != request.length()) {
            Serial.println("[ERROR] Failed to send GET request");
            return false;
        }
        
        return _readResponse(response);
    }
    
    // Basic PUT request
    bool put(const char* path, const String& data, String& response) {
        if (!_sslClient.connected() && !_sslClient.connect(_host.c_str(), _port)) {
            Serial.println("[ERROR] Failed to connect for PUT request");
            return false;
        }
        
        String request = String("PUT ") + path + " HTTP/1.1\r\n";
        request += "Host: " + _host + "\r\n";
        request += "Authorization: Bearer " + _authToken + "\r\n";
        request += "Content-Type: application/json\r\n";
        request += "Content-Length: " + String(data.length()) + "\r\n";
        request += "Connection: close\r\n\r\n";
        request += data;
        
        Serial.printf("[DEBUG] Sending PUT request to: %s with data: %s\n", path, data.c_str());
        
        if (_sslClient.write(request.c_str()) != request.length()) {
            Serial.println("[ERROR] Failed to send PUT request");
            return false;
        }
        
        return _readResponse(response);
    }
    
    // Check if connected
    bool connected() { return _sslClient.connected(); }
    
    // Disconnect
    void disconnect() { _sslClient.disconnect(); }

private:
    DecentSSLClient _sslClient;
    String _host;
    int _port;
    String _authToken;
    
    // Read HTTP response
    bool _readResponse(String& response) {
        response = "";
        unsigned long timeout = millis() + 10000; // 10 second timeout
        
        while (millis() < timeout) {
            if (_sslClient.available()) {
                String line = _sslClient.readStringUntil('\n');
                if (line.length() == 0 || line == "\r") {
                    // Headers ended, read body
                    while (_sslClient.available()) {
                        response += (char)_sslClient.read();
                    }
                    break;
                }
            }
            delay(1);
        }
        
        if (response.length() > 0) {
            Serial.printf("[DEBUG] Response received: %s\n", response.c_str());
            return true;
        } else {
            Serial.println("[ERROR] No response received or timeout");
            return false;
        }
    }
};
