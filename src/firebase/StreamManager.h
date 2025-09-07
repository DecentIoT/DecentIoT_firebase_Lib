#pragma once

#include <Arduino.h>
#include <vector>
#include <functional>
#include "../network/SSLClient.h"
#include "../utils/Logger.h"
#include "../core/DecentIoTValue.h"

// Forward declaration
class DecentIoTClass;

// Stream event structure
struct StreamEvent {
    String eventType;    // "put", "patch", "delete"
    String path;         // Data path
    String data;         // Event data
    String timestamp;    // Event timestamp
};

// Stream callback type
using StreamCallback = std::function<void(const StreamEvent& event)>;

// Individual pin stream
struct PinStream {
    String pinId;
    String streamPath;
    DecentSSLClient sslClient;
    StreamCallback callback;
    bool isActive;
    unsigned long lastHeartbeat;
    
    PinStream() : pinId(""), streamPath(""), isActive(false), lastHeartbeat(0) {}
    PinStream(const String& id, const String& path, StreamCallback cb) 
        : pinId(id), streamPath(path), callback(cb), isActive(false), lastHeartbeat(0) {}
};

// Clean Stream Manager - Handles Firebase real-time streaming
class StreamManager {
public:
    StreamManager(DecentIoTClass* parent);
    ~StreamManager();
    
    // Initialize stream manager
    bool begin(const char* firebaseUrl, const char* authToken);
    
    // Add a pin stream
    bool addPinStream(const char* pinId, const char* streamPath, StreamCallback callback);
    
    // Remove a pin stream
    void removePinStream(const char* pinId);
    
    // Start all streams
    bool startAllStreams();
    
    // Stop all streams
    void stopAllStreams();
    
    // Process incoming stream data
    void processStreams();
    
    // Check if streams are active
    bool hasActiveStreams() const { return _activeStreams > 0; }
    
    // Get active stream count
    int getActiveStreamCount() const { return _activeStreams; }

private:
    DecentIoTClass* _parent;
    String _firebaseUrl;
    String _authToken;
    String _host;
    int _port;
    
    std::vector<PinStream> _pinStreams;
    int _activeStreams;
    
    // Stream management
    bool _startPinStream(PinStream& stream);
    void _stopPinStream(PinStream& stream);
    bool _processPinStream(PinStream& stream);
    
    // SSE parsing
    bool _parseSSEEvent(const String& line, StreamEvent& event);
    DecentIoTValue _parseValue(const String& data);
    
    // Heartbeat management
    void _checkHeartbeats();
    static const unsigned long HEARTBEAT_INTERVAL = 30000; // 30 seconds
    static const unsigned long RECONNECT_INTERVAL = 5000;  // 5 seconds
};
