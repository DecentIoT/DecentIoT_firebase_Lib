#include "DecentIoT.h"
#include <time.h>
#include <FirebaseClient.h>

// Forward declare your global app pointer for use in the callback
FirebaseApp* g_app_ptr = nullptr;

// Free function for streaming callback
void DecentIoTStreamCallback(AsyncResult& aResult) {
    if (g_app_ptr) g_app_ptr->loop(); // CRITICAL: process async tasks
    Serial.println("[STREAM] DecentIoTStreamCallback called!");
    if (aResult.isEvent()) {
        Serial.println("[STREAM] Stream event received!");
        // You can call your instance handler here if needed
        // For example, if you have a global DecentIoTClass* g_instance;
        // g_instance->processData(aResult);
    }
    if (aResult.isError()) {
        Serial.printf("[STREAM] Error: %s\n", aResult.error().message().c_str());
    }
}

// Global instance
DecentIoTClass DecentIoT;
DecentIoTClass& getDecentIoT() { return DecentIoT; }

// Static instance pointer for callback
DecentIoTClass* DecentIoTClass::_instance = nullptr;

// Static callback function for Firebase
void DecentIoTClass::processDataStatic(AsyncResult& aResult) {
    Serial.println("[DEBUG] processDataStatic CALLED!");
    Serial.printf("[DEBUG] isEvent: %s, isError: %s, isResult: %s\n", 
                  aResult.isEvent() ? "YES" : "NO",
                  aResult.isError() ? "YES" : "NO", 
                  aResult.isResult() ? "YES" : "NO");
    
    if (_instance) {
        Serial.println("[DEBUG] _instance is valid, calling processData");
        _instance->processData(aResult);
    } else {
        Serial.println("[DEBUG] _instance is NULL!");
    }
}

// Auth debug callback
void DecentIoTClass::authDebugPrint(AsyncResult& aResult) {
    if (aResult.isError()) {
        Serial.printf("[AUTH] Error: %s\n", aResult.error().message().c_str());
    }
}

DecentIoTClass::~DecentIoTClass() {
    if (_user_auth) delete _user_auth;
}

DecentIoTClass::DecentIoTClass()
    : _ssl_client(), _stream_ssl_client(),
      _async_client(_ssl_client), _stream_async_client(_stream_ssl_client)
{}

bool DecentIoTClass::begin(const char* firebaseUrl, const char* firebaseAuth,
                           const char* projectId, const char* userId, const char* deviceId,
                           const char* authEmail, const char* authPass) {
    _firebaseUrl = firebaseUrl;
    _firebaseAuth = firebaseAuth;
    _projectId = projectId;
    _userId = userId;
    _deviceId = deviceId;
    _authEmail = authEmail;
    _authPass = authPass;

    // Initialize UserAuth with correct arguments
    if (_user_auth) delete _user_auth;
    _user_auth = new UserAuth(String(_firebaseAuth), String(_authEmail), String(_authPass));
    Serial.printf("[DecentIoT] Initializing with email: %s\n", _authEmail);

    // Configure SSL clients for ESP8266 (separate for regular and stream operations)
    _ssl_client.setInsecure();
    _ssl_client.setBufferSizes(4096, 1024);
    
    _stream_ssl_client.setInsecure();
    _stream_ssl_client.setBufferSizes(4096, 1024);

    // Set instance for callbacks
    _instance = this;

    // Initialize Firebase app (official pattern)
    Serial.println("[DecentIoT] Initializing Firebase app...");
    initializeApp(_async_client, _app, getAuth(*_user_auth), 120000, authDebugPrint);

    // Get RealtimeDatabase from app
    _app.getApp<RealtimeDatabase>(_database);
    _database.url(_firebaseUrl);

    // Wait for authentication
    Serial.println("[DecentIoT] Waiting for authentication...");
    unsigned long start = millis();
    while (!_app.ready() && millis() - start < 10000) {
        _app.loop();
        delay(500);
    }

    if (!_app.ready()) {
        Serial.println("[DecentIoT] Authentication failed!");
        return false;
    }

    // Sync time
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("[DecentIoT] Syncing time...");
    time_t now = 0;
    int retry = 0;
    while (now < 24 * 3600 && retry < 10) {
        delay(500);
        now = time(nullptr);
        retry++;
    }

    // Set up streaming (official FirebaseClient pattern)
    String streamPath = String("/") + _projectId + "/users/" + _userId + "/datastream/" + _deviceId;
    Serial.printf("[DecentIoT] Starting stream on: %s\n", streamPath.c_str());
    Serial.printf("[DEBUG] Full stream path: %s\n", streamPath.c_str());
    
    // Use the official FirebaseClient streaming pattern with SEPARATE clients
    _async_client.setClient(_ssl_client);
    _stream_async_client.setClient(_stream_ssl_client);
    
    // Set SSE filters for event types
    _stream_async_client.setSSEFilters("put,patch,get,keep-alive,cancel,auth_revoked");
    Serial.println("[DEBUG] SSE filters set");
    
    // Set the global app pointer for use in the callback
    g_app_ptr = &_app;

    // Start the stream using the official pattern with SEPARATE stream client
    Serial.printf("[DEBUG] About to start stream on path: %s\n", streamPath.c_str());
    
    _database.get(_stream_async_client, streamPath, DecentIoTStreamCallback, true, "streamTask");
    Serial.println("[DEBUG] Stream start command sent");
    Serial.printf("[DEBUG] processDataStatic function address: %p\n", (void*)processDataStatic);
    Serial.printf("[DEBUG] _instance pointer: %p\n", (void*)_instance);
    Serial.printf("[DEBUG] Current instance pointer: %p\n", (void*)this);
    
    // Wait a moment for stream to establish
    delay(1000);
    
    Serial.println("[DecentIoT] Stream setup completed!");
    Serial.printf("[DEBUG] Stream callback registered: %s\n", processDataStatic ? "YES" : "NO");
    
    // Test the stream by writing a test value
    delay(1000); // Wait a bit for stream to establish
    String testPath = String("/") + _projectId + "/users/" + _userId + "/datastream/" + _deviceId + "/test";
    _database.set<String>(_async_client, testPath, "stream_test", _result);
    Serial.println("[DEBUG] Test write completed - check if stream receives this event");
    
    // Test writing to P0 to see if we receive our own stream event
    delay(2000);
    String p0Path = String("/") + _projectId + "/users/" + _userId + "/datastream/" + _deviceId + "/P0";
    _database.set<bool>(_async_client, p0Path, true, _result);
    Serial.println("[DEBUG] Writing P0=true - check if stream receives this event");
    
    // Add a simple test to verify stream is working
    delay(3000);
    Serial.println("[DEBUG] === STREAM TEST ===");
    Serial.println("[DEBUG] If streaming is working, you should see stream events above");
    Serial.println("[DEBUG] If you don't see any stream events, streaming is NOT working");
    Serial.println("[DEBUG] === END STREAM TEST ===");

    _isConnected = true;
    Serial.println("[DecentIoT] Initialized successfully!");
    return true;
}

void DecentIoTClass::run() {
    if (!_isConnected || !_app.ready())
        return;
    _app.loop();
    
    // Add periodic stream debugging
    static unsigned long lastStreamDebug = 0;
    if (millis() - lastStreamDebug > 5000) { // Every 5 seconds
        Serial.printf("[DEBUG] Run loop - App ready: %s, Connected: %s\n", 
                      _app.ready() ? "YES" : "NO", 
                      _isConnected ? "YES" : "NO");
        lastStreamDebug = millis();
    }

    // Process scheduled tasks
    processScheduledTasks();

    // Update device status
    //updateDeviceStatus();
    
    // Periodic stream status check (every 30 seconds)
    static unsigned long lastStreamCheck = 0;
    if (millis() - lastStreamCheck > 30000) {
        Serial.printf("[DEBUG] Stream status check - App ready: %s, Connected: %s\n", 
                      _app.ready() ? "YES" : "NO", _isConnected ? "YES" : "NO");
        lastStreamCheck = millis();
    }
}

void DecentIoTClass::processScheduledTasks() {
    unsigned long currentMillis = millis();
    for (auto it = _scheduledTasks.begin(); it != _scheduledTasks.end();) {
        if (currentMillis - it->second.lastRun >= it->second.interval) {
            it->second.callback();
            if (it->second.once) {
                it = _scheduledTasks.erase(it);
            } else {
                it->second.lastRun = currentMillis;
                ++it;
            }
        } else {
            ++it;
        }
    }
}

void DecentIoTClass::write(const char* pin, bool value) {
    if (!_app.ready()) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastream/" + _deviceId + "/" + pin;
    _database.set<bool>(_async_client, path, value, _result);
    
    if (_result.isError()) {
        Serial.printf("[WRITE] Failed to write %s: %s\n", pin, _result.error().message().c_str());
    } else {
        Serial.printf("[WRITE] %s = %s\n", pin, value ? "true" : "false");
    }
}

void DecentIoTClass::write(const char* pin, int value) {
    if (!_app.ready()) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastream/" + _deviceId + "/" + pin;
    _database.set<int>(_async_client, path, value, _result);
    
    if (_result.isError()) {
        Serial.printf("[WRITE] Failed to write %s: %s\n", pin, _result.error().message().c_str());
    } else {
        Serial.printf("[WRITE] %s = %d\n", pin, value);
    }
}

void DecentIoTClass::write(const char* pin, float value) {
    if (!_app.ready()) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastream/" + _deviceId + "/" + pin;
    _database.set<float>(_async_client, path, value, _result);
    
    if (_result.isError()) {
        Serial.printf("[WRITE] Failed to write %s: %s\n", pin, _result.error().message().c_str());
    } else {
        Serial.printf("[WRITE] %s = %.2f\n", pin, value);
    }
}

void DecentIoTClass::write(const char* pin, const char* value) {
    if (!_app.ready()) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastream/" + _deviceId + "/" + pin;
    _database.set<String>(_async_client, path, String(value), _result);
    
    if (_result.isError()) {
        Serial.printf("[WRITE] Failed to write %s: %s\n", pin, _result.error().message().c_str());
    } else {
        Serial.printf("[WRITE] %s = %s\n", pin, value);
    }
}

void DecentIoTClass::onReceive(const char* pin, ReceiveCallback callback) {
    ReceiveHandler handler;
    handler.id = String(pin);
    handler.callback = callback;
    _receiveHandlers.push_back(handler);
    
    Serial.printf("[DecentIoT] Registered receive handler for pin: %s (total handlers: %d)\n", pin, _receiveHandlers.size());
    
    // Debug: print all registered handlers
    Serial.print("[DecentIoT] All registered handlers: ");
    for (const auto& h : _receiveHandlers) {
        Serial.print(h.id);
        Serial.print(" ");
    }
    Serial.println();
}

void DecentIoTClass::schedule(const char* id, unsigned long interval, TaskCallback callback) {
    ScheduledTask task;
    task.lastRun = 0;
    task.interval = interval;
    task.callback = callback;
    task.once = false;
    _scheduledTasks[String(id)] = task;
}

void DecentIoTClass::schedule(unsigned long interval, TaskCallback callback) {
    String taskId = "task_" + String(_taskCounter++);
    schedule(taskId.c_str(), interval, callback);
}

void DecentIoTClass::scheduleOnce(unsigned long delay, TaskCallback callback) {
    String taskId = "once_" + String(_taskCounter++);
    ScheduledTask task;
    task.lastRun = millis();
    task.interval = delay;
    task.callback = callback;
    task.once = true;
    _scheduledTasks[taskId] = task;
}

void DecentIoTClass::updateDeviceStatus() {
    if (!_app.ready()) return;

    unsigned long currentMillis = millis();

    if (_statusUpdatePending || (currentMillis - _lastStatusUpdate >= _statusUpdateInterval)) {
        if (currentMillis - _lastStatusRetry >= _statusRetryInterval) {
            String statusPath = String("/") + _projectId + "/users/" + _userId + "/datastream/" + _deviceId + "/status";

            _database.set(_async_client, statusPath + "/s", 1, _result); // 1 = online
            _database.set(_async_client, statusPath + "/t", (int)(time(nullptr)), _result);

            if (!_result.isError()) {
                _lastStatusUpdate = currentMillis;
                _statusUpdatePending = false;
            } else {
                Serial.printf("[STATUS] Update failed: %s\n", _result.error().message().c_str());
                _lastStatusRetry = currentMillis;
                _statusUpdatePending = true;
            }
        }
    }
}

void DecentIoTClass::handleFirebaseStream(AsyncResult& aResult) {
    if (!aResult.isEvent()) return;

    RealtimeDatabaseResult& rtdbResult = aResult.to<RealtimeDatabaseResult>();
    if (!rtdbResult.isStream()) return;

    const char* eventType = rtdbResult.event().c_str();
    if (!eventType || (strcmp(eventType, "put") != 0 && strcmp(eventType, "patch") != 0)) return;

    const char* path = rtdbResult.dataPath().c_str();
    if (!path || *path == '\0') return;

    // Skip leading slash
    if (*path == '/') path++;

    Serial.printf("[STREAM] Event: %s, Path: %s\n", eventType, path);

    // Extract pin name from path (e.g., "P0", "P1", etc.)
    String pinName = String(path);
    
    // Find and call handler
    for (const auto& handler : _receiveHandlers) {
        if (handler.id == pinName) {
            const char* data = rtdbResult.data().c_str();
            if (!data) {
                Serial.printf("[STREAM] No data for %s\n", pinName.c_str());
                continue;
            }

            Serial.printf("[STREAM] Processing %s = %s\n", pinName.c_str(), data);

            DecentIoTValue value;

            // Parse value based on content
            if (strcmp(data, "true") == 0 || strcmp(data, "1") == 0) {
                value.type = DecentIoTValue::BOOL;
                value.boolValue = true;
                Serial.printf("[STREAM] Parsed as BOOL: true\n");
            } else if (strcmp(data, "false") == 0 || strcmp(data, "0") == 0) {
                value.type = DecentIoTValue::BOOL;
                value.boolValue = false;
                Serial.printf("[STREAM] Parsed as BOOL: false\n");
            } else {
                char* endPtr;
                float fVal = strtof(data, &endPtr);

                if (endPtr != data && *endPtr == '\0') {
                    if (fVal == (int)fVal) {
                        value.type = DecentIoTValue::INT;
                        value.intValue = (int)fVal;
                        Serial.printf("[STREAM] Parsed as INT: %d\n", value.intValue);
                    } else {
                        value.type = DecentIoTValue::FLOAT;
                        value.floatValue = fVal;
                        Serial.printf("[STREAM] Parsed as FLOAT: %.2f\n", value.floatValue);
                    }
                } else {
                    value.type = DecentIoTValue::STRING;
                    value.stringValue = data;
                    Serial.printf("[STREAM] Parsed as STRING: %s\n", value.stringValue.c_str());
                }
            }

            Serial.printf("[STREAM] Calling handler for %s\n", pinName.c_str());
            handler.callback(value);
            break;
        }
    }
}

void DecentIoTClass::processData(AsyncResult& aResult) {
    Serial.printf("[DEBUG] processData called, isEvent: %s, isError: %s\n", 
                  aResult.isEvent() ? "true" : "false", 
                  aResult.isError() ? "true" : "false");
    
    // Handle stream events
    if (aResult.isEvent()) {
        Serial.println("[STREAM] Stream event detected!");
        
        // Add more detailed debugging
        RealtimeDatabaseResult& rtdbResult = aResult.to<RealtimeDatabaseResult>();
        Serial.printf("[DEBUG] Event type: %s\n", rtdbResult.event().c_str());
        Serial.printf("[DEBUG] Data path: %s\n", rtdbResult.dataPath().c_str());
        Serial.printf("[DEBUG] Data: %s\n", rtdbResult.data().c_str());
        
        handleFirebaseStream(aResult);
        return;
    }
    
    // Handle errors
    if (aResult.isError()) {
        Serial.printf("[DecentIoT] Error: %s\n", aResult.error().message().c_str());
    }
}
