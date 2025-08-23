#include "DecentIoT.h"
#include <time.h>
#include <FirebaseClient.h>

// Global instance
DecentIoTClass DecentIoT;
DecentIoTClass &getDecentIoT() { return DecentIoT; }

// Static instance pointer for callback
DecentIoTClass *DecentIoTClass::_instance = nullptr;
void DecentIoTClass::setInstance(DecentIoTClass *instance) { _instance = instance; }

// Static callback function for FirebaseApp
void DecentIoTClass::processDataStatic(AsyncResult &aResult)
{
    if (_instance)
        _instance->processData(aResult);
}

// Auth debug callback function (required by FirebaseClient)
void DecentIoTClass::authDebugPrint(AsyncResult &aResult)
{
    if (_instance)
        _instance->authDebugPrintImpl(aResult);
}

void DecentIoTClass::authDebugPrintImpl(AsyncResult &aResult)
{
    if (aResult.isEvent())
    {
        Serial.printf("[AUTH] Event: %s, msg: %s, code: %d\n", 
                     aResult.uid().c_str(), 
                     aResult.eventLog().message().c_str(), 
                     aResult.eventLog().code());
    }

    if (aResult.isDebug())
    {
        Serial.printf("[AUTH] Debug: %s, msg: %s\n", 
                     aResult.uid().c_str(), 
                     aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        Serial.printf("[AUTH] Error: %s, msg: %s, code: %d\n", 
                     aResult.uid().c_str(), 
                     aResult.error().message().c_str(), 
                     aResult.error().code());
    }
}

DecentIoTClass::DecentIoTClass()
    : _firebaseUrl(nullptr), _firebaseAuth(nullptr), _projectId(nullptr),
      _userId(nullptr), _deviceId(nullptr), _authEmail(nullptr), _authPass(nullptr),
      _isConnected(false),
      _lastRequestTime(0), _requestInterval(50),
      _async_client(_ssl_client),
      _user_auth("", "", "") // Initialize with empty strings, will be set in begin()
{
}

bool DecentIoTClass::begin(const char *firebaseUrl, const char *firebaseAuth,
                           const char *projectId, const char *userId, const char *deviceId,
                           const char *authEmail, const char *authPass)
{
    _firebaseUrl = firebaseUrl;
    _firebaseAuth = firebaseAuth;
    _projectId = projectId;
    _userId = userId;
    _deviceId = deviceId;
    _authEmail = authEmail;
    _authPass = authPass;

    // Initialize UserAuth with the correct constructor
    _user_auth = UserAuth(String(_firebaseAuth), String(_authEmail), String(_authPass));
    Serial.printf("[DecentIoT] UserAuth initialized with email: %s\n", _authEmail);

    // Configure SSL client for ESP8266
    _ssl_client.setInsecure();
    _ssl_client.setBufferSizes(4096, 1024);

    // Set up the async client with the SSL client
    _async_client.setClient(_ssl_client);

    setInstance(this);

    // Set up the callback for async results
    _app.setCallback(processDataStatic);

    // ---- OFFICIAL AUTHENTICATION FLOW ----
    // Use initializeApp and getAuth as in the official examples
    Serial.println("[DecentIoT] Calling initializeApp (official convention)...");
    initializeApp(_async_client, _app, getAuth(_user_auth), 120000, authDebugPrint);

    // Bind the RealtimeDatabase to the app
    _app.getApp<RealtimeDatabase>(_database);

    // Set the database URL
    _database.url(_firebaseUrl);
    Serial.printf("[DecentIoT] Database URL set to: %s\n", _firebaseUrl);

    // Wait for authentication to complete
    Serial.println("[DecentIoT] Waiting for authentication...");
    unsigned long start = millis();
    while (!_app.ready() && millis() - start < 10000)
    {
        _app.loop();
        delay(500);
    }

    if (!_app.ready())
    {
        Serial.println("[DecentIoT] Firebase authentication failed!");
        return false;
    }

    // Sync time with NTP server first
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("[DecentIoT] Waiting for NTP time sync...");
    time_t now = 0;
    int retry = 0;
    while (now < 24 * 3600 && retry < 10)
    {
        Serial.print(".");
        delay(500);
        now = time(nullptr);
        retry++;
    }
    Serial.println();

    if (now > 24 * 3600)
    {
        Serial.printf("[DecentIoT] Time synced: %s", ctime(&now));
    }
    else
    {
        Serial.println("[DecentIoT] Failed to sync time");
    }

    // Set up streaming for the entire device's datastream
    // This will capture all changes to any pin under the device
    String streamPath = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId;

    // Start the stream with auto-reconnect enabled and proper callback
    _database.get(_async_client, streamPath, _streamResult, true);

    Serial.printf("[DecentIoT] Started data stream on path: %s\n", streamPath.c_str());
    Serial.println("[DecentIoT] Stream will capture all pin changes automatically");

    _isConnected = true;
    _statusUpdatePending = true;
    _lastStatusUpdate = 0;
    _lastStatusRetry = 0;

    Serial.println("DecentIoT Initialized Successfully!");
    return true;
}

void DecentIoTClass::run()
{
    if (!_isConnected || !_app.ready())
        return;

    // Process Firebase main loop
    _app.loop();

    // Process RTDB operations in order:
    // 1. Handle any stream events first (for real-time updates)
    handleFirebaseStream(_streamResult);

    // 2. Process any write operation results
    processData(_dbResult);

    // 3. Update device status (if needed)
    updateDeviceStatus();

    // 4. Process scheduled tasks
    processScheduledTasks();
}

void DecentIoTClass::processScheduledTasks()
{
    unsigned long currentMillis = millis();
    for (auto it = _scheduledTasks.begin(); it != _scheduledTasks.end();)
    {
        if (currentMillis - it->second.lastRun >= it->second.interval)
        {
            it->second.callback();
            if (it->second.once)
            {
                it = _scheduledTasks.erase(it); // Erase and get next iterator
            }
            else
            {
                it->second.lastRun = currentMillis;
                ++it;
            }
        }
        else
        {
            ++it;
        }
    }
}

void DecentIoTClass::write(const char *pin, bool value)
{
    if (!_app.ready())
        return;

    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    _database.set<bool>(_async_client, path, value, _dbResult);
    
    if (_dbResult.isError()) {
        Serial.printf("[WRITE] Failed to write %s: %s\n", pin, _dbResult.error().message().c_str());
    } else {
        Serial.printf("[WRITE] %s = %s\n", pin, value ? "true" : "false");
    }
}

void DecentIoTClass::write(const char *pin, int value)
{
    if (!_app.ready())
        return;

    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    _database.set<int>(_async_client, path, value, _dbResult);
    
    if (_dbResult.isError()) {
        Serial.printf("[WRITE] Failed to write %s: %s\n", pin, _dbResult.error().message().c_str());
    } else {
        Serial.printf("[WRITE] %s = %d\n", pin, value);
    }
}

void DecentIoTClass::write(const char *pin, float value)
{
    if (!_app.ready())
        return;

    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    _database.set<float>(_async_client, path, value, _dbResult);
    
    if (_dbResult.isError()) {
        Serial.printf("[WRITE] Failed to write %s: %s\n", pin, _dbResult.error().message().c_str());
    } else {
        Serial.printf("[WRITE] %s = %.2f\n", pin, value);
    }
}

void DecentIoTClass::write(const char *pin, const char *value)
{
    if (!_app.ready())
        return;

    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    _database.set<String>(_async_client, path, String(value), _dbResult);
    
    if (_dbResult.isError()) {
        Serial.printf("[WRITE] Failed to write %s: %s\n", pin, _dbResult.error().message().c_str());
    } else {
        Serial.printf("[WRITE] %s = %s\n", pin, value);
    }
}

void DecentIoTClass::writeAnalog(const char *pin, int value)
{
    // For ESP8266, analogWrite range is 0-1023 for PWM
    write(pin, value);
}

void DecentIoTClass::writePWM(const char *pin, int value)
{
    // Standard PWM range is 0-255
    write(pin, constrain(value, 0, 255));
}

void DecentIoTClass::writePercent(const char *pin, float value)
{
    int pwmValue = map(constrain(value, 0.0, 100.0), 0, 100, 0, 255);
    write(pin, pwmValue);
}

void DecentIoTClass::writeRange(const char *pin, int value, int min, int max)
{
    int pwmValue = map(value, min, max, 0, 255);
    write(pin, constrain(pwmValue, 0, 255));
}

void DecentIoTClass::onReceive(const char *pin, ReceiveCallback callback)
{
    ReceiveHandler handler;
    handler.id = String(pin);
    handler.callback = callback;
    _receiveHandlers.push_back(handler);
}

void DecentIoTClass::onSend(const char *pin, SendCallback callback)
{
    SendHandler handler;
    handler.id = String(pin);
    handler.callback = callback;
    _sendHandlers.push_back(handler);
}

void DecentIoTClass::schedule(const char *id, unsigned long interval, TaskCallback callback)
{
    ScheduledTask task;
    task.lastRun = 0; // Set to 0 to run immediately on the first check
    task.interval = interval;
    task.callback = callback;
    task.once = false;
    _scheduledTasks[String(id)] = task;
}

void DecentIoTClass::schedule(unsigned long interval, TaskCallback callback)
{
    String taskId = "task_" + String(_taskCounter++);
    schedule(taskId.c_str(), interval, callback);
}

void DecentIoTClass::scheduleOnce(unsigned long delay, TaskCallback callback)
{
    String taskId = "once_" + String(_taskCounter++);
    ScheduledTask task;
    task.lastRun = millis(); // Start counting delay from now
    task.interval = delay;
    task.callback = callback;
    task.once = true;
    _scheduledTasks[taskId] = task;
}

void DecentIoTClass::cancelSend(const char *pin)
{
    String taskId = String("send_") + pin;
    _scheduledTasks.erase(taskId);
}

void DecentIoTClass::updateDeviceStatus()
{
    if (!_app.ready())
        return;

    unsigned long currentMillis = millis();

    // If a status update is pending or it's time for regular update
    if (_statusUpdatePending || (currentMillis - _lastStatusUpdate >= _statusUpdateInterval))
    {
        if (currentMillis - _lastStatusRetry >= _statusRetryInterval)
        {
            String statusPath = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/status";

            // Create status data similar to OpenIoT (using s for status and t for timestamp)
            _database.set(_async_client, statusPath + "/s", 1, _dbResult); // 1 = online
            _database.set(_async_client, statusPath + "/t", (int)(time(nullptr)), _dbResult);

            if (!_dbResult.isError())
            {
                _lastStatusUpdate = currentMillis;
                _statusUpdatePending = false;
            }
            else
            {
                Serial.printf("[STATUS] Update failed: %s\n", _dbResult.error().message().c_str());
                _lastStatusRetry = currentMillis;
                _statusUpdatePending = true;
            }
        }
    }
}

void DecentIoTClass::handleFirebaseStream(AsyncResult &aResult)
{
    if (!aResult.isEvent())
        return;

    RealtimeDatabaseResult &rtdbResult = aResult.to<RealtimeDatabaseResult>();

    if (!rtdbResult.isStream())
        return;

    const char *eventType = rtdbResult.event().c_str();
    if (!eventType || (strcmp(eventType, "put") != 0 && strcmp(eventType, "patch") != 0)) // Check for "put" or "patch"
        return;

    const char *path = rtdbResult.dataPath().c_str();
    if (!path || *path == '\0')
        return;

    // Skip leading slash
    if (*path == '/')
        path++;

    Serial.printf("[STREAM] Event: %s, Path: %s\n", eventType, path);

    // Only process if we have a handler for this pin
    for (const auto &handler : _receiveHandlers)
    {
        if (handler.id == path)
        {
            const char *data = rtdbResult.data().c_str();
            if (!data)
                continue;

            Serial.printf("[STREAM] Processing %s = %s\n", path, data);

            DecentIoTValue value;

            // Quick boolean check
            if (strcmp(data, "true") == 0 || strcmp(data, "1") == 0)
            {
                value.type = DecentIoTValue::BOOL;
                value.boolValue = true;
            }
            else if (strcmp(data, "false") == 0 || strcmp(data, "0") == 0)
            {
                value.type = DecentIoTValue::BOOL;
                value.boolValue = false;
            }
            // Number check
            else
            {
                char *endPtr;
                float fVal = strtof(data, &endPtr);

                if (endPtr != data) // Conversion succeeded
                {
                    if (*endPtr == '\0') // No extra characters - it's a clean number
                    {
                        if (fVal == (int)fVal)
                        {
                            value.type = DecentIoTValue::INT;
                            value.intValue = (int)fVal;
                        }
                        else
                        {
                            value.type = DecentIoTValue::FLOAT;
                            value.floatValue = fVal;
                        }
                    }
                    else
                    {
                        value.type = DecentIoTValue::STRING;
                        value.stringValue = data;
                    }
                }
                else
                {
                    value.type = DecentIoTValue::STRING;
                    value.stringValue = data;
                }
            }

            Serial.printf("[STREAM] Calling handler for %s with type %d\n", path, value.type);
            handler.callback(value);
            break;
        }
    }
}

void DecentIoTClass::processData(AsyncResult &aResult)
{
    // Handle any errors from database operations
    if (aResult.isError())
    {
        Serial.printf("[DecentIoT] Error: %s, code: %d\n", aResult.error().message().c_str(), aResult.error().code());
        
        // Check if it's a connection error
        if (aResult.error().code() == 401 || aResult.error().code() == 403) {
            Serial.println("[DecentIoT] Authentication error - check credentials");
        } else if (aResult.error().code() == 404) {
            Serial.println("[DecentIoT] Path not found - check database structure");
        } else if (aResult.error().code() >= 500) {
            Serial.println("[DecentIoT] Server error - Firebase may be down");
        }
    }
}

// These are the dispatcher methods that create the appropriate DecentIoTValue
void DecentIoTClass::dispatchReceiveHandler(const char *id, bool value)
{
    DecentIoTValue v;
    v.type = DecentIoTValue::BOOL;
    v.boolValue = value;

    for (auto &handler : _receiveHandlers)
    {
        if (handler.id == id)
        {
            handler.callback(v);
            break;
        }
    }
}

void DecentIoTClass::dispatchReceiveHandler(const char *id, int value)
{
    DecentIoTValue v;
    v.type = DecentIoTValue::INT;
    v.intValue = value;

    for (auto &handler : _receiveHandlers)
    {
        if (handler.id == id)
        {
            handler.callback(v);
            break;
        }
    }
}

void DecentIoTClass::dispatchReceiveHandler(const char *id, float value)
{
    DecentIoTValue v;
    v.type = DecentIoTValue::FLOAT;
    v.floatValue = value;

    for (auto &handler : _receiveHandlers)
    {
        if (handler.id == id)
        {
            handler.callback(v);
            break;
        }
    }
}

void DecentIoTClass::dispatchReceiveHandler(const char *id, const char *value)
{
    DecentIoTValue v;
    v.type = DecentIoTValue::STRING;
    v.stringValue = value;

    for (auto &handler : _receiveHandlers)
    {
        if (handler.id == id)
        {
            handler.callback(v);
            break;
        }
    }
}
