#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <vector>
#include <functional>
#include <map>

// Define required build options for FirebaseClient
#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#define FIREBASE_DISABLE_LARGE_STRING_REALLOC_DEBUG
#include <FirebaseClient.h>

// Universal value type for callbacks
struct DecentIoTValue {
    enum Type { BOOL, INT, FLOAT, STRING } type;
    bool boolValue;
    int intValue;
    float floatValue;
    String stringValue;

    // Implicit conversion operators
    operator bool() const {
        if (type == BOOL) return boolValue;
        if (type == INT) return intValue != 0;
        if (type == FLOAT) return floatValue != 0.0f;
        if (type == STRING) return stringValue == "true" || stringValue == "1";
        return false;
    }
    operator int() const {
        if (type == INT) return intValue;
        if (type == BOOL) return boolValue ? 1 : 0;
        if (type == FLOAT) return static_cast<int>(floatValue);
        if (type == STRING) return stringValue.toInt();
        return 0;
    }
    operator float() const {
        if (type == FLOAT) return floatValue;
        if (type == INT) return static_cast<float>(intValue);
        if (type == BOOL) return boolValue ? 1.0f : 0.0f;
        if (type == STRING) return stringValue.toFloat();
        return 0.0f;
    }
    operator String() const {
        if (type == STRING) return stringValue;
        if (type == BOOL) return boolValue ? "true" : "false";
        if (type == INT) return String(intValue);
        if (type == FLOAT) return String(floatValue);
        return "";
    }
    operator uint8_t() const {
        if (type == BOOL) return boolValue ? HIGH : LOW;
        if (type == INT) return intValue;
        if (type == FLOAT) return static_cast<uint8_t>(floatValue);
        if (type == STRING) return (stringValue == "true" || stringValue == "1") ? HIGH : LOW;
        return 0;
    }
};

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

class DecentIoTClass {
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
    
    // Device status tracking (keeping this as requested)
    unsigned long _lastStatusUpdate = 0;
    const unsigned long _statusUpdateInterval = 30000; // 30 seconds
    unsigned long _lastStatusRetry = 0;
    const unsigned long _statusRetryInterval = 15000; // 15 seconds
    bool _statusUpdatePending = true;
    
    // Firebase objects - single instance approach
    FirebaseApp _app;
    WiFiClientSecure _ssl_client;
    AsyncClientClass _async_client;
    UserAuth _user_auth;
    RealtimeDatabase _database;
    AsyncResult _result;
    
    // Static instance for callbacks
    static DecentIoTClass* _instance;
    
    // Private methods
    void processScheduledTasks();
    void updateDeviceStatus();
    void handleFirebaseStream(AsyncResult& aResult);
    static void processDataStatic(AsyncResult& aResult);
    void processData(AsyncResult& aResult);
    static void authDebugPrint(AsyncResult& aResult);
    
public:
    DecentIoTClass();
    
    // Initialize connection
    bool begin(const char* firebaseUrl, const char* firebaseAuth,
               const char* projectId, const char* userId, const char* deviceId,
               const char* authEmail, const char* authPass);
    
    // Main loop
    void run();
    
    // Write data
    void write(const char* pin, bool value);
    void write(const char* pin, int value);
    void write(const char* pin, float value);
    void write(const char* pin, const char* value);
    
    // Register callbacks
    void onReceive(const char* pin, ReceiveCallback callback);
    
    // Schedule tasks
    void schedule(const char* id, unsigned long interval, TaskCallback callback);
    void schedule(unsigned long interval, TaskCallback callback);
    void scheduleOnce(unsigned long delay, TaskCallback callback);
    
    // Status
    bool isConnected() const { return _isConnected; }
    bool isStreamActive() const { return _isConnected; } // Stream is active when connected. can be removed later
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