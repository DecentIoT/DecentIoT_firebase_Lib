#include "DecentIoT.h"
#include <time.h>

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

void DecentIoTClass::handleFirebaseStreamStatic(AsyncResult &aResult)
{
    if (_instance)
        _instance->handleFirebaseStream(aResult);
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

    // Configure SSL client for ESP8266
    _ssl_client.setInsecure();
    _ssl_client.setBufferSizes(4096, 1024);

    setInstance(this);

    // Set up authentication data
    _app.setCallback(processDataStatic);
    
    // Initialize the app with authentication
    // Note: The exact initialization method may vary based on FirebaseClient version
    // For now, we'll try to use the app directly

    // Bind the RealtimeDatabase to the app
    _app.getApp<RealtimeDatabase>(_database);

    // Set the database URL
    _database.url(_firebaseUrl);

    // Wait for authentication to complete
    unsigned long start = millis();
    while (!_app.ready() && millis() - start < 10000) {
        Serial.print(".");
        _app.loop();
        delay(500);
    }

    if (!_app.ready()) {
        Serial.println("[DecentIoT] Firebase authentication failed!");
        return false;
    }

    _isConnected = true;

    // Force immediate status update
    _statusUpdatePending = true;
    _lastStatusUpdate = 0;
    _lastStatusRetry = 0;

    Serial.println("DecentIoT Initialized Successfully!");
    return true;
}

void DecentIoTClass::run()
{
    if (!_isConnected)
    {
        return;
    }
    
    // Always process Firebase app loop first
    _app.loop();
    
    // Then process status updates
    updateDeviceStatus();
    
    // Then process scheduled tasks
    processScheduledTasks();
    
    // Process any pending database results
    processData(_dbResult);
}

void DecentIoTClass::processScheduledTasks()
{
    unsigned long currentMillis = millis();
    for (auto it = _scheduledTasks.begin(); it != _scheduledTasks.end();)
    {
        if (currentMillis - it->second.lastRun >= it->second.interval)
        {
            it->second.callback();
            it->second.lastRun = currentMillis;
            ++it;
        }
        else
        {
            ++it;
        }
    }
}

void DecentIoTClass::write(const char *pin, bool value)
{
    if (!_app.ready()) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    _database.set<bool>(_async_client, path, value, _dbResult);
}

void DecentIoTClass::write(const char *pin, int value)
{
    if (!_app.ready()) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    _database.set<int>(_async_client, path, value, _dbResult);
}

void DecentIoTClass::write(const char *pin, float value)
{
    if (!_app.ready()) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    _database.set<float>(_async_client, path, value, _dbResult);
}

void DecentIoTClass::write(const char *pin, const char *value)
{
    if (!_app.ready()) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    _database.set<String>(_async_client, path, String(value), _dbResult);
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

void DecentIoTClass::scheduleTask(const char *id, unsigned long interval, TaskCallback callback)
{
    ScheduledTask task;
    task.lastRun = 0;
    task.interval = interval;
    task.callback = callback;
    _scheduledTasks[String(id)] = task;
}

void DecentIoTClass::updateDeviceStatus()
{
    if (!_app.ready()) return;
    
    unsigned long currentMillis = millis();
    
    // Check if it's time for status update
    if (_statusUpdatePending || (currentMillis - _lastStatusUpdate >= _statusUpdateInterval))
    {
        String statusPath = String("/") + _projectId + "/users/" + _userId + "/devices/" + _deviceId + "/status";
        
        // Create status data
        String statusData = "{\"online\": true, \"lastSeen\": " + String(currentMillis) + "}";
        
        // Update status
        _database.set<String>(_async_client, statusPath, statusData, _dbResult);
        
        _lastStatusUpdate = currentMillis;
        _statusUpdatePending = false;
    }
}

void DecentIoTClass::handleFirebaseStream(AsyncResult &aResult)
{
    if (!aResult.isResult()) return;
    
    if (aResult.isEvent())
    {
        // Handle stream events
        if (aResult.eventLog().message().indexOf("put") > -1)
        {
            // Extract path and data from the event
            String path = aResult.eventLog().message();
            String data = aResult.c_str();
            
            // Extract pin name from path
            int lastSlash = path.lastIndexOf('/');
            if (lastSlash > 0)
            {
                String pin = path.substring(lastSlash + 1);
                
                // Find and call the appropriate handler
                for (const auto& handler : _receiveHandlers)
                {
                    if (handler.id == pin)
                    {
                        // Parse the data and call handler
                        if (data == "true" || data == "false")
                        {
                            bool boolValue = (data == "true");
                            DecentIoTValue value;
                            value.type = DecentIoTValue::BOOL;
                            value.boolValue = boolValue;
                            handler.callback(value);
                        }
                        else if (data.toInt() != 0 || data == "0")
                        {
                            int intValue = data.toInt();
                            DecentIoTValue value;
                            value.type = DecentIoTValue::INT;
                            value.intValue = intValue;
                            handler.callback(value);
                        }
                        else if (data.toFloat() != 0.0f || data == "0.0")
                        {
                            float floatValue = data.toFloat();
                            DecentIoTValue value;
                            value.type = DecentIoTValue::FLOAT;
                            value.floatValue = floatValue;
                            handler.callback(value);
                        }
                        else
                        {
                            DecentIoTValue value;
                            value.type = DecentIoTValue::STRING;
                            value.stringValue = data;
                            handler.callback(value);
                        }
                        break;
                    }
                }
            }
        }
    }
    
    if (aResult.isError())
    {
        Serial.printf("[STREAM] Error: %s\n", aResult.error().message().c_str());
    }
}

void DecentIoTClass::processData(AsyncResult &aResult)
{
    if (!aResult.isResult()) return;
    
    if (aResult.isEvent())
    {
        Serial.printf("Event task: %s, msg: %s, code: %d\n", 
                     aResult.uid().c_str(), 
                     aResult.eventLog().message().c_str(), 
                     aResult.eventLog().code());
    }
    
    if (aResult.isDebug())
    {
        Serial.printf("Debug task: %s, msg: %s\n", 
                     aResult.uid().c_str(), 
                     aResult.debug().c_str());
    }
    
    if (aResult.isError())
    {
        Serial.printf("Error task: %s, msg: %s, code: %d\n", 
                     aResult.uid().c_str(), 
                     aResult.error().message().c_str(), 
                     aResult.error().code());
    }
    
    if (aResult.available())
    {
        Serial.printf("task: %s, payload: %s\n", 
                     aResult.uid().c_str(), 
                     aResult.c_str());
    }
}

void DecentIoTClass::dispatchReceiveHandler(const char *id, bool value)
{
    for (const auto& handler : _receiveHandlers)
    {
        if (handler.id == String(id))
        {
            DecentIoTValue val;
            val.type = DecentIoTValue::BOOL;
            val.boolValue = value;
            handler.callback(val);
            break;
        }
    }
}

void DecentIoTClass::dispatchReceiveHandler(const char *id, int value)
{
    for (const auto& handler : _receiveHandlers)
    {
        if (handler.id == String(id))
        {
            DecentIoTValue val;
            val.type = DecentIoTValue::INT;
            val.intValue = value;
            handler.callback(val);
            break;
        }
    }
}

void DecentIoTClass::dispatchReceiveHandler(const char *id, float value)
{
    for (const auto& handler : _receiveHandlers)
    {
        if (handler.id == String(id))
        {
            DecentIoTValue val;
            val.type = DecentIoTValue::FLOAT;
            val.floatValue = value;
            handler.callback(val);
            break;
        }
    }
}

void DecentIoTClass::dispatchReceiveHandler(const char *id, const char *value)
{
    for (const auto& handler : _receiveHandlers)
    {
        if (handler.id == String(id))
        {
            DecentIoTValue val;
            val.type = DecentIoTValue::STRING;
            val.stringValue = String(value);
            handler.callback(val);
            break;
        }
    }
}

