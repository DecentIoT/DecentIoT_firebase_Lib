#pragma once

#include <Arduino.h>
#include <vector>
#include <functional>
#include <map>
#include "DecentIoTValue.h"

// Platform-specific includes
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiClientSecure.h>
#endif

// Our custom components
#include "../network/SSLClient.h"
#include "../firebase/FirebaseConnection.h"
#include "../utils/Logger.h"

// Forward declarations
class StreamManager;
struct StreamEvent;

// Callback types
using ReceiveCallback = std::function<void(const DecentIoTValue& value)>;
using SendCallback = std::function<void()>;
using TaskCallback = std::function<void()>;

// Handler structures
struct ReceiveHandler {
    String id;
    ReceiveCallback callback;
};

struct ScheduledTask {
    unsigned long lastRun;
    unsigned long interval;
    TaskCallback callback;
    bool once = false;
};

// Main DecentIoT class - Clean and focused!
class DecentIoTClass {
public:
    DecentIoTClass();
    ~DecentIoTClass();
    
    // Initialize connection
    bool begin(const char* firebaseUrl, const char* firebaseAuth,
               const char* projectId, const char* userId, const char* deviceId,
               const char* authEmail, const char* authPass);
    
    // Main loop
    void run();
    
    // Write data to Firebase
    void write(const char* pin, bool value);
    void write(const char* pin, int value);
    void write(const char* pin, float value);
    void write(const char* pin, const char* value);
    
    // Write percentage value (0.0 to 100.0)
    void writePercent(const char* pin, float percent);
    
    // Register receive callbacks
    void onReceive(const char* pin, ReceiveCallback callback);
    
    // Schedule tasks
    void schedule(const char* id, unsigned long interval, TaskCallback callback);
    void schedule(unsigned long interval, TaskCallback callback);
    void scheduleOnce(unsigned long delay, TaskCallback callback);
    
    // Status
    bool isConnected() const { return _isConnected; }
    bool isStreamActive() const { return _streamManager != nullptr && _isConnected; }
    
    // Get instance for callbacks
    static DecentIoTClass* getInstance() { return _instance; }

private:
    // Firebase credentials
    const char* _firebaseUrl;
    const char* _firebaseAuth;
    const char* _projectId;
    const char* _userId;
    const char* _deviceId;
    const char* _authEmail;
    const char* _authPass;
    
    // Connection status
    bool _isConnected = false;
    
    // Handlers and tasks
    std::vector<ReceiveHandler> _receiveHandlers;
    std::map<String, ScheduledTask> _scheduledTasks;
    unsigned int _taskCounter = 0;
    
    // Firebase connection
    FirebaseConnection _firebaseConnection;
    
    // Stream manager for real-time updates
    StreamManager* _streamManager = nullptr;
    
    // Static instance for callbacks
    static DecentIoTClass* _instance;
    
    // Private methods
    void processScheduledTasks();
    void updateDeviceStatus();
    bool authenticateWithFirebase();
    DecentIoTValue _parseStreamEventToValue(const StreamEvent& event);
    
    // Device status tracking
    unsigned long _lastStatusUpdate = 0;
    const unsigned long _statusUpdateInterval = 30000; // 30 seconds
};

// Global instance
extern DecentIoTClass DecentIoT;
DecentIoTClass& getDecentIoT();

// Helper classes for static registration
class DecentIoTReceiveRegistrar {
public:
    DecentIoTReceiveRegistrar(const char* pin, ReceiveCallback cb) {
        getDecentIoT().onReceive(pin, cb);
    }
};

class DecentIoTSendRegistrar {
public:
    DecentIoTSendRegistrar(const char* pin, SendCallback cb, uint32_t interval = 0) {
        if (interval > 0) {
            String taskId = String("send_") + pin;
            getDecentIoT().schedule(taskId.c_str(), interval, cb);
        }
    }
};

// User-friendly macros
#define DECENTIOT_RECEIVE(PIN_NAME) \
    void DECENTIOT_RECEIVE_HANDLER_##PIN_NAME(const DecentIoTValue& value); \
    static DecentIoTReceiveRegistrar _decentiot_receive_registrar_##PIN_NAME(#PIN_NAME, DECENTIOT_RECEIVE_HANDLER_##PIN_NAME); \
    void DECENTIOT_RECEIVE_HANDLER_##PIN_NAME(const DecentIoTValue& value)

#define DECENTIOT_SEND(PIN_NAME, ...) \
    void DECENTIOT_SEND_HANDLER_##PIN_NAME(); \
    static DecentIoTSendRegistrar _decentiot_send_registrar_##PIN_NAME(#PIN_NAME, DECENTIOT_SEND_HANDLER_##PIN_NAME, ##__VA_ARGS__); \
    void DECENTIOT_SEND_HANDLER_##PIN_NAME()
// Pin definitions
#define P0 "P0"
#define P1 "P1"
#define P2 "P2"
#define P3 "P3"
#define P4 "P4"
#define P5 "P5"
#define P6 "P6"
#define P7 "P7"
#define P8 "P8"
#define P9 "P9"
#define P10 "P10"
