#include "StreamManager.h"
#include "../core/DecentIoT.h"

StreamManager::StreamManager(DecentIoTClass* parent) 
    : _parent(parent), _activeStreams(0) {
}

StreamManager::~StreamManager() {
    stopAllStreams();
}

bool StreamManager::begin(const char* firebaseUrl, const char* authToken) {
    _firebaseUrl = String(firebaseUrl);
    _authToken = String(authToken);
    
    // Extract host from URL
    if (_firebaseUrl.startsWith("https://")) {
        _host = _firebaseUrl.substring(8);
    } else {
        _host = _firebaseUrl;
    }
    
    // Remove path from host
    int slashPos = _host.indexOf('/');
    if (slashPos > 0) {
        _host = _host.substring(0, slashPos);
    }
    
    _port = 443; // Default HTTPS port
    
    Logger::infof("STREAM", "Stream Manager initialized for host: %s", _host.c_str());
    return true;
}

bool StreamManager::addPinStream(const char* pinId, const char* streamPath, StreamCallback callback) {
    // Check if stream already exists
    for (auto& stream : _pinStreams) {
        if (stream.pinId == pinId) {
            Logger::warnf("STREAM", "Stream for pin %s already exists, updating callback", pinId);
            stream.callback = callback;
            return true;
        }
    }
    
    // Add new stream
    PinStream newStream(pinId, streamPath, callback);
    _pinStreams.push_back(newStream);
    
    Logger::infof("STREAM", "Added pin stream: %s at path: %s", pinId, streamPath);
    return true;
}

void StreamManager::removePinStream(const char* pinId) {
    for (auto it = _pinStreams.begin(); it != _pinStreams.end(); ++it) {
        if (it->pinId == pinId) {
            if (it->isActive) {
                _stopPinStream(*it);
            }
            _pinStreams.erase(it);
            Logger::infof("STREAM", "Removed pin stream: %s", pinId);
            break;
        }
    }
}

bool StreamManager::startAllStreams() {
    Logger::info("STREAM", "Starting all pin streams...");
    
    bool allStarted = true;
    for (auto& stream : _pinStreams) {
        if (!_startPinStream(stream)) {
            allStarted = false;
        }
    }
    
    if (allStarted) {
        Logger::info("STREAM", "All pin streams started successfully");
    } else {
        Logger::warn("STREAM", "Some pin streams failed to start");
    }
    
    return allStarted;
}

void StreamManager::stopAllStreams() {
    Logger::info("STREAM", "Stopping all pin streams...");
    
    for (auto& stream : _pinStreams) {
        if (stream.isActive) {
            _stopPinStream(stream);
        }
    }
    
    _activeStreams = 0;
    Logger::info("STREAM", "All pin streams stopped");
}

void StreamManager::processStreams() {
    // For now, just check heartbeats
    _checkHeartbeats();
    
    // Process each active stream
    for (auto& stream : _pinStreams) {
        if (stream.isActive) {
            _processPinStream(stream);
        }
    }
}

bool StreamManager::_startPinStream(PinStream& stream) {
    if (stream.isActive) return true;
    
    Logger::infof("STREAM", "Starting stream for pin: %s", stream.pinId.c_str());
    
    // Initialize SSL client for this stream
    if (!stream.sslClient.begin()) {
        Logger::errorf("STREAM", "Failed to initialize SSL client for pin: %s", stream.pinId.c_str());
        return false;
    }
    
    // Connect to Firebase
    if (!stream.sslClient.connect(_host.c_str(), _port)) {
        Logger::errorf("STREAM", "Failed to connect to Firebase for pin: %s", stream.pinId.c_str());
        return false;
    }
    
    // Send SSE request
    String request = String("GET ") + stream.streamPath + " HTTP/1.1\r\n";
    request += "Host: " + _host + "\r\n";
    request += "Authorization: Bearer " + _authToken + "\r\n";
    request += "Accept: text/event-stream\r\n";
    request += "Cache-Control: no-cache\r\n";
    request += "Connection: keep-alive\r\n\r\n";
    
    if (stream.sslClient.write(request.c_str()) != request.length()) {
        Logger::errorf("STREAM", "Failed to send SSE request for pin: %s", stream.pinId.c_str());
        stream.sslClient.disconnect();
        return false;
    }
    
    stream.isActive = true;
    stream.lastHeartbeat = millis();
    _activeStreams++;
    
    Logger::infof("STREAM", "Stream started successfully for pin: %s", stream.pinId.c_str());
    return true;
}

void StreamManager::_stopPinStream(PinStream& stream) {
    if (!stream.isActive) return;
    
    Logger::infof("STREAM", "Stopping stream for pin: %s", stream.pinId.c_str());
    
    stream.sslClient.disconnect();
    stream.isActive = false;
    _activeStreams--;
}

bool StreamManager::_processPinStream(PinStream& stream) {
    if (!stream.sslClient.connected()) {
        Logger::warnf("STREAM", "Stream disconnected for pin: %s, attempting reconnect", stream.pinId.c_str());
        _stopPinStream(stream);
        
        // Try to reconnect after delay
        if (millis() - stream.lastHeartbeat > RECONNECT_INTERVAL) {
            return _startPinStream(stream);
        }
        return false;
    }
    
    // Process incoming data
    while (stream.sslClient.available()) {
        String line = stream.sslClient.readStringUntil('\n');
        line.trim();
        
        if (line.length() > 0) {
            StreamEvent event;
            if (_parseSSEEvent(line, event)) {
                // Call the callback
                if (stream.callback) {
                    stream.callback(event);
                }
            }
        }
    }
    
    return true;
}

bool StreamManager::_parseSSEEvent(const String& line, StreamEvent& event) {
    // Basic SSE parsing - in a real implementation, this would be more robust
    if (line.startsWith("data:")) {
        event.data = line.substring(5);
        event.eventType = "data";
        event.path = "";
        event.timestamp = String(millis());
        return true;
    }
    
    return false;
}

DecentIoTValue StreamManager::_parseValue(const String& data) {
    DecentIoTValue value;
    
    // Parse the data based on content
    if (data == "true" || data == "1") {
        value = DecentIoTValue(true);
    } else if (data == "false" || data == "0") {
        value = DecentIoTValue(false);
    } else {
        // Try to parse as number
        char* endPtr;
        float fVal = strtof(data.c_str(), &endPtr);
        
        if (endPtr != data.c_str() && *endPtr == '\0') {
            if (fVal == (int)fVal) {
                value = DecentIoTValue((int)fVal);
            } else {
                value = DecentIoTValue(fVal);
            }
        } else {
            value = DecentIoTValue(data);
        }
    }
    
    return value;
}

void StreamManager::_checkHeartbeats() {
    unsigned long currentMillis = millis();
    
    for (auto& stream : _pinStreams) {
        if (stream.isActive && (currentMillis - stream.lastHeartbeat > HEARTBEAT_INTERVAL)) {
            Logger::warnf("STREAM", "Heartbeat timeout for pin: %s, reconnecting", stream.pinId.c_str());
            _stopPinStream(stream);
            
            // Try to reconnect
            if (currentMillis - stream.lastHeartbeat > RECONNECT_INTERVAL) {
                _startPinStream(stream);
            }
        }
    }
}
