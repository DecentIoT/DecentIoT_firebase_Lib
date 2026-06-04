#include "DecentIoT.h"
#include <time.h>
#include <FirebaseClient.h>



// Global instance
DecentIoTClass DecentIoT;
DecentIoTClass& getDecentIoT() { return DecentIoT; }

// Static instance pointer for callback
DecentIoTClass* DecentIoTClass::_instance = nullptr;

// Static map for stream UID to pin mapping
std::map<String, String> DecentIoTClass::_streamUidToPin;

// Static callback function for Firebase
void DecentIoTClass::processDataStatic(AsyncResult& aResult) {
    if (_instance) {
        _instance->processData(aResult);
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
    // Slightly larger buffers for auth POSTs to googleapis
    _ssl_client.setBufferSizes(2048, 1024);
    
    _stream_ssl_client.setInsecure();
    _stream_ssl_client.setBufferSizes(1024, 512);

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
    unsigned long lastLog = 0;
    while (!_app.ready() && millis() - start < 60000) {
        _app.loop();
        delay(50);
        if (millis() - lastLog > 5000) {
            Serial.println("[DecentIoT] Auth in progress...");
            lastLog = millis();
        }
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

    // Set up streaming clients (individual pin streams will be set up in onReceive)
    _async_client.setClient(_ssl_client);
    _stream_async_client.setClient(_stream_ssl_client);
    
    Serial.println("[DecentIoT] Streaming clients configured - individual pin streams will be set up when onReceive is called");
    



    

    _isConnected = true;
    
    // Start device-level stream (like old library) to see what we receive
    if (!_deviceStreamStarted) {
        startDeviceStream();
    }
    
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
    
    // Update device status
    updateDeviceStatus();

    // Process scheduled tasks
    processScheduledTasks();
    
    // ✅ REMOVED: setupPendingStreams() - we're now manually setting up streams in onReceive()
  
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
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin + "/value";
    _database.set<bool>(_async_client, path, value, _result);
    
    if (_result.isError()) {
        Serial.printf("[WRITE] Failed to write %s: %s\n", pin, _result.error().message().c_str());
    } else {
        Serial.printf("[WRITE] %s = %s\n", pin, value ? "true" : "false");
    }
}

void DecentIoTClass::write(const char* pin, int value) {
    if (!_app.ready()) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin + "/value";
    _database.set<int>(_async_client, path, value, _result);
    
    if (_result.isError()) {
        Serial.printf("[WRITE] Failed to write %s: %s\n", pin, _result.error().message().c_str());
    } else {
        Serial.printf("[WRITE] %s = %d\n", pin, value);
    }
}

void DecentIoTClass::write(const char* pin, float value) {
    if (!_app.ready()) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin + "/value";
    _database.set<float>(_async_client, path, value, _result);
    
    if (_result.isError()) {
        Serial.printf("[WRITE] Failed to write %s: %s\n", pin, _result.error().message().c_str());
    } else {
        Serial.printf("[WRITE] %s = %.2f\n", pin, value);
    }
}

void DecentIoTClass::write(const char* pin, const char* value) {
    if (!_app.ready()) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin + "/value";
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
    
    // Note: Device-level stream is already started in begin()
    // Handlers are just registered here
    
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
            String statusPath = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/status";

            // ✅ Set status directly to Unix timestamp (not as nested object)
            time_t unixTimestamp = time(nullptr);
            _database.set<int>(_async_client, statusPath, (int)unixTimestamp, _result);

            if (!_result.isError()) {
                _lastStatusUpdate = currentMillis;
                _statusUpdatePending = false;
                // Serial.printf("[STATUS] Updated: %d\n", (int)unixTimestamp);
            } else {
                Serial.printf("[STATUS] Update failed: %s\n", _result.error().message().c_str());
                _lastStatusRetry = currentMillis;
                _statusUpdatePending = true;
            }
        }
    }
}

void DecentIoTClass::handleFirebaseStream(AsyncResult& aResult, const char* pinName) {
    // Accept both SSE events and plain results
    if (!aResult.isEvent() && !aResult.isResult()) return;

    RealtimeDatabaseResult& rtdbResult = aResult.to<RealtimeDatabaseResult>();
    if (!rtdbResult.isStream()) return;

    const char* path = rtdbResult.dataPath().c_str();
    if (!path) return;
    
    // Skip leading slash
    if (*path == '/') path++;
    
    // When streaming to /P0 (parent), changes to /P0/value will show path="/value"
    // We need to extract the pin name - but wait, we stream to individual pins!
    // Each stream callback knows which pin it's for via the stream context
    
    // Actually, when we stream to /P0 and /P0/value changes, dataPath will be "/value"
    // But we don't know which pin! We need to track stream->pin mapping
    
    // Better: Use the requestId/streamId from AsyncResult to map back to pin
    // Or store a static map when creating streams
    
    // SIMPLER: Check if path is "/value" - then we need pin from stream context
    // For now, let's try using the request ID or checking the fullPath
    
    const char* data = rtdbResult.data().c_str();
    if (!data) return;
    
    String pinNameStr;
    String valueDataStr; // Store value as String to avoid buffer issues
    
    // When streaming to device level, paths like "/P0/value" come through
    // Extract pin name from path (e.g., "P0/value" -> pin "P0")
    
    // Check if path is exactly "value" (meaning we're at /P0/value for some pin)
    if (path && (strcmp(path, "value") == 0 || strlen(path) == 0)) {
        // This is a direct /value change - need to identify which pin
        // Since we can't get pin from callback context easily, try all handlers
        // Each pin has its own stream, so only one should match at a time
        valueDataStr = String(data);
        
        // Since each pin has its own stream subscription, try all handlers
        // The correct handler will process the value
        bool dispatched = false;
        for (const auto& handler : _receiveHandlers) {
            DecentIoTValue value;
            const char* valueData = valueDataStr.c_str();
            if (strcmp(valueData, "true") == 0 || strcmp(valueData, "1") == 0) {
                value.type = DecentIoTValue::BOOL; value.boolValue = true;
            } else if (strcmp(valueData, "false") == 0 || strcmp(valueData, "0") == 0) {
                value.type = DecentIoTValue::BOOL; value.boolValue = false;
            } else {
                char* endPtr; 
                float fVal = strtof(valueData, &endPtr);
                if (endPtr != valueData && *endPtr == '\0') {
                    if (fVal == (int)fVal) { 
                        value.type = DecentIoTValue::INT; 
                        value.intValue = (int)fVal; 
                    } else { 
                        value.type = DecentIoTValue::FLOAT; 
                        value.floatValue = fVal; 
                    }
                } else { 
                    value.type = DecentIoTValue::STRING; 
                    value.stringValue = valueDataStr; 
                }
            }
            Serial.printf("[STREAM] Dispatching /value to %s: value=%s\n", handler.id.c_str(), valueData);
            handler.callback(value);
            dispatched = true;
            break; // Only dispatch to first handler (workaround - ideally should match correct pin)
        }
        if (!dispatched) {
            Serial.printf("[STREAM] Got /value change but no handlers registered\n");
        }
        return; // Already dispatched
    } else {
        // Path is the pin name itself (e.g., "P0") - whole object changed
        pinNameStr = String(path);
        
        // Check if this looks like a pin name (starts with "P" and has number)
        if (pinNameStr.length() > 0 && pinNameStr.charAt(0) == 'P' && pinNameStr.length() <= 4) {
            // Extract value from JSON payload {"value": ...}
            const char* key = "\"value\"";
            const char* pos = strstr(data, key);
            if (pos) {
                // Found value field in JSON
                pos += strlen(key);
                while (*pos == ' ' || *pos == '\t') pos++;
                if (*pos != ':') return;
                pos++;
                while (*pos == ' ' || *pos == '\t') pos++;
                
                char buf[64]; size_t i = 0;
                if (*pos == '"') { pos++; while (*pos && *pos != '"' && i < sizeof(buf) - 1) buf[i++] = *pos++; }
                else { while (*pos && *pos != ',' && *pos != '}' && *pos != '\n' && i < sizeof(buf) - 1) buf[i++] = *pos++; }
                buf[i] = '\0';
                while (i > 0 && (buf[i-1] == ' ' || buf[i-1] == '\t')) buf[--i] = '\0';
                
                valueDataStr = String(buf); // Store extracted value from JSON
            } else {
                // No value field in JSON - skip
                return;
            }
        } else {
            // Path doesn't look like a pin name, skip
            return;
        }
    }
    
    // Find matching handler and dispatch
    for (const auto& handler : _receiveHandlers) {
        if (handler.id == pinNameStr) {
            DecentIoTValue value;
            const char* valueData = valueDataStr.c_str();
            if (strcmp(valueData, "true") == 0 || strcmp(valueData, "1") == 0) {
                value.type = DecentIoTValue::BOOL; value.boolValue = true;
            } else if (strcmp(valueData, "false") == 0 || strcmp(valueData, "0") == 0) {
                value.type = DecentIoTValue::BOOL; value.boolValue = false;
            } else {
                char* endPtr; 
                float fVal = strtof(valueData, &endPtr);
                if (endPtr != valueData && *endPtr == '\0') {
                    if (fVal == (int)fVal) { 
                        value.type = DecentIoTValue::INT; 
                        value.intValue = (int)fVal; 
                    } else { 
                        value.type = DecentIoTValue::FLOAT; 
                        value.floatValue = fVal; 
                    }
                } else { 
                    value.type = DecentIoTValue::STRING; 
                    value.stringValue = valueDataStr; 
                }
            }
            Serial.printf("[STREAM] Dispatching to %s: value=%s\n", pinNameStr.c_str(), valueData);
            handler.callback(value);
            break;
        }
    }
}

void DecentIoTClass::processData(AsyncResult& aResult) {
    // Treat both event and result as stream messages for SSE
    if (aResult.isEvent() || aResult.isResult()) {
        RealtimeDatabaseResult& rtdbResult = aResult.to<RealtimeDatabaseResult>();
        
        // DEBUG: Print everything we receive to understand the structure
        Serial.printf("[STREAM_DEBUG] Event: %s, Result: %s, Stream: %s\n",
                      aResult.isEvent() ? "YES" : "NO",
                      aResult.isResult() ? "YES" : "NO",
                      rtdbResult.isStream() ? "YES" : "NO");
        
        const char* path = rtdbResult.dataPath().c_str();
        const char* data = rtdbResult.data().c_str();
        const char* event = rtdbResult.event().c_str();
        
        Serial.printf("[STREAM_DEBUG] Path: '%s', Event: '%s', Data: '%s'\n",
                      path ? path : "<null>",
                      event ? event : "<null>",
                      data ? data : "<null>");
        
        handleFirebaseStream(aResult, nullptr);
        return;
    }
    
    // Handle errors
    if (aResult.isError()) {
        Serial.printf("[DecentIoT] Error: %s\n", aResult.error().message().c_str());
    }
}

void DecentIoTClass::setupPendingStreams() {
    static bool streamsSetup = false;
    
    // Only set up streams once when app becomes ready
    if (!streamsSetup && _app.ready()) {
        Serial.println("[STREAM] App is ready, setting up pending streams...");
        
        for (const auto& handler : _receiveHandlers) {
            String pin = handler.id;
            String pinPath = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
            
            Serial.printf("[STREAM] Setting up stream for pin %s at path: %s\n", pin.c_str(), pinPath.c_str());
            
            // Create individual stream client for this pin
            WiFiClientSecure& pinSslClient = _pin_stream_clients[pin];
            AsyncClientClass& pinAsyncClient = _pin_async_clients[pin];
            
            // Configure the pin-specific SSL client
            pinSslClient.setInsecure();
            pinSslClient.setBufferSizes(4096, 1024);
            
            // Set the client for the async client
            pinAsyncClient.setClient(pinSslClient);
            
            // Set SSE filters for this stream
            pinAsyncClient.setSSEFilters("get,put,patch,keep-alive,cancel,auth_revoked");
            
            // Start the stream for this specific pin using processDataStatic
            _database.get(pinAsyncClient, pinPath, processDataStatic, true, String("stream_") + pin);
            
            Serial.printf("[STREAM] Stream started for pin %s\n", pin.c_str());
            
            delay(100); // Small delay between streams
        }
        
        streamsSetup = true;
        Serial.println("[STREAM] All pending streams have been set up!");
    }
}

void DecentIoTClass::startDeviceStream() {
    String devicePath = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId;
    Serial.printf("[STREAM] Starting device-level stream at: %s\n", devicePath.c_str());
    _stream_async_client.setSSEFilters("get,put,patch,keep-alive,cancel,auth_revoked");
    _database.get(_stream_async_client, devicePath, processDataStatic, true, String("stream_device"));
    _deviceStreamStarted = true;
    Serial.println("[STREAM] Device stream started!");
}
