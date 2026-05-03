/*
  DecentIoT Firebase Library
  Copyright 2025 MD Jannatul Nayem
  
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
  
      http://www.apache.org/licenses/LICENSE-2.0
  
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "DecentIoT.h"
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <time.h>

// Global instance
DecentIoTClass DecentIoT;
DecentIoTClass &getDecentIoT() { return DecentIoT; }

// Static instance pointer for callback
DecentIoTClass *DecentIoTClass::_instance = nullptr;
void DecentIoTClass::setInstance(DecentIoTClass *instance) { _instance = instance; }
void DecentIoTClass::handleFirebaseStreamStatic(FirebaseStream data)
{
    if (_instance)
        _instance->handleFirebaseStream(data);
}

DecentIoTClass::DecentIoTClass()
    : _firebaseUrl(nullptr), _firebaseAuth(nullptr), _projectId(nullptr),
      _userId(nullptr), _deviceId(nullptr), _authEmail(nullptr), _authPass(nullptr),
      _isConnected(false),
      _lastRequestTime(0), _requestInterval(50),
      _config(), _auth()
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

    // Set up Firebase config and auth
    _config.api_key = _firebaseAuth;
    _config.database_url = _firebaseUrl;
    _auth.user.email = _authEmail;
    _auth.user.password = _authPass;

    Firebase.begin(&_config, &_auth);
    Firebase.reconnectWiFi(true);

    if (!Firebase.ready())
    {
        Serial.println("[DecentIoT] Firebase not ready!");
        return false;
    }

    // Sync time with NTP server
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("[DecentIoT] Waiting for NTP time sync...");
    time_t now = 0;
    int retry = 0;
    while (now < 24 * 3600 && retry < 10) {
        Serial.print(".");
        delay(500);
        now = time(nullptr);
        retry++;
    }
    Serial.println();
    if (now > 24 * 3600) {
        Serial.printf("[DecentIoT] Time synced: %s", ctime(&now));
    } else {
        Serial.println("[DecentIoT] Failed to sync time");
    }

    setInstance(this);

    String streamPath = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId;
    Firebase.RTDB.setStreamCallback(
        &_fbdo,
        handleFirebaseStreamStatic,
        [](bool timeout)
        {
            if (timeout)
                Serial.println("[DecentIoT] Stream timeout, resuming...");
        });
    if (!Firebase.RTDB.beginStream(&_fbdo, streamPath.c_str()))
    {
        return false;
    }

    _isConnected = true;
    _wasWiFiConnected = true; // Mark WiFi as connected after successful initialization
    
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
    
    unsigned long currentMillis = millis();
    bool wifiCurrentlyConnected = (WiFi.status() == WL_CONNECTED);
    
    // 1. If WiFi is down, can't do anything
    if (!wifiCurrentlyConnected)
    {
        if (_wasWiFiConnected)
        {
            Serial.println("[DecentIoT] WiFi disconnected detected");
            _wasWiFiConnected = false;
        }
        return;
    }
    
    // 2. WiFi is connected - check if we need to handle reconnection
    if (!_wasWiFiConnected)
    {
        Serial.println("[DecentIoT] WiFi reconnected, attempting Firebase reconnection...");
        _wasWiFiConnected = true;
        _lastReconnectAttempt = 0;
        
        delay(2000); // Wait for network stability
        
        if (Firebase.ready())
        {
            Serial.println("[DecentIoT] Firebase reconnected successfully");
            if (reconnectStream())
            {
                Serial.println("[DecentIoT] Stream reconnected successfully");
                
                // Send status FIRST after reconnection (priority!)
                _statusUpdatePending = true;  // Mark status update as needed
                _lastStatusRetry = 0;          // Bypass throttling for immediate update
                updateDeviceStatus();          // Reuse existing status update function
            }
        }
        return;
    }
    
    // 3. Check Firebase connection health
    if (!Firebase.ready())
    {
        if (currentMillis - _lastReconnectAttempt >= _reconnectInterval)
        {
            Serial.println("[DecentIoT] Firebase not ready, attempting reconnection...");
        }
        handleReconnection();
        return;
    }
    
    // 4. Monitor stream health periodically
    if (currentMillis - _lastStreamCheck >= _streamCheckInterval)
    {
        _lastStreamCheck = currentMillis;
        
        if (!Firebase.RTDB.readStream(&_fbdo))
        {
            if (_fbdo.streamTimeout() || !_fbdo.httpConnected())
            {
                if (reconnectStream())
                {
                    Serial.println("[DecentIoT] Stream reconnected");
                }
            }
        }
    }
    
    // 5. Continue normal operations
    updateDeviceStatus();
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
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin + "/value";
    if (Firebase.RTDB.setBool(&_fbdo, path.c_str(), value))
    {
        pollAllReceivePinsOnce();
        return;
    }
}

void DecentIoTClass::write(const char *pin, int value)
{
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin + "/value";
    if (Firebase.RTDB.setInt(&_fbdo, path.c_str(), value))
    {
        pollAllReceivePinsOnce();
        return;
    }
}

void DecentIoTClass::write(const char *pin, float value)
{
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin + "/value";
    if (Firebase.RTDB.setFloat(&_fbdo, path.c_str(), value))
    {
        pollAllReceivePinsOnce();
        return;
    }
}

void DecentIoTClass::write(const char *pin, const char *value)
{
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin + "/value";
    if (Firebase.RTDB.setString(&_fbdo, path.c_str(), value))
    {
        pollAllReceivePinsOnce();
        return;
    }
}

// analog classes
void DecentIoTClass::writeAnalog(const char *pin, int value)
{
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin + "/value";
    if (Firebase.RTDB.setInt(&_fbdo, path.c_str(), value))
    {
        Serial.printf("[ANALOG] %s set to %d\n", pin, value);
        pollAllReceivePinsOnce();
        return;
    }
    Serial.printf("[ANALOG] Failed to set %s to %d\n", pin, value);
}

void DecentIoTClass::writePWM(const char *pin, int value)
{
    // Ensure value is in 0-255 range for PWM
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin + "/value";
    if (Firebase.RTDB.setInt(&_fbdo, path.c_str(), value))
    {
        Serial.printf("[PWM] %s set to %d (0-255)\n", pin, value);
        pollAllReceivePinsOnce();
        return;
    }
    Serial.printf("[PWM] Failed to set %s to %d\n", pin, value);
}

void DecentIoTClass::writePercent(const char *pin, float value)
{
    // Ensure value is in 0-100 range
    if (value < 0.0) value = 0.0;
    if (value > 100.0) value = 100.0;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin + "/value";
    if (Firebase.RTDB.setFloat(&_fbdo, path.c_str(), value))
    {
        Serial.printf("[PERCENT] %s set to %.1f%%\n", pin, value);
        pollAllReceivePinsOnce();
        return;
    }
    Serial.printf("[PERCENT] Failed to set %s to %.1f%%\n", pin, value);
}

void DecentIoTClass::writeRange(const char *pin, int value, int min, int max)
{
    // Map the value from min-max range to 0-255 for PWM
    int mappedValue = map(value, min, max, 0, 255);
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin + "/value";
    if (Firebase.RTDB.setInt(&_fbdo, path.c_str(), mappedValue))
    {
        Serial.printf("[RANGE] %s mapped from %d (%d-%d) to %d (0-255)\n", pin, value, min, max, mappedValue);
        pollAllReceivePinsOnce();
        return;
    }
    Serial.printf("[RANGE] Failed to set %s\n", pin);
}

void DecentIoTClass::schedule(uint32_t interval, TaskCallback callback)
{
    String taskId = String("task_") + String(millis());
    schedule(taskId, interval, callback);
}

void DecentIoTClass::schedule(String taskId, uint32_t interval, TaskCallback callback)
{
    _scheduledTasks[taskId] = {millis(), interval, callback};
}

void DecentIoTClass::scheduleOnce(uint32_t delay, TaskCallback callback)
{
    String taskId = String("once_") + String(millis());
    _scheduledTasks[taskId] = {millis(), delay, callback};
}

void DecentIoTClass::cancel(String taskId)
{
    _scheduledTasks.erase(taskId);
}

void DecentIoTClass::cancelSend(const char *pin)
{
    // Cancel any scheduled send tasks for this pin
    String taskId = String("send_") + pin;
    _scheduledTasks.erase(taskId);
}

void DecentIoTClass::onSend(const char *pin, SendCallback callback)
{
    _sendHandlers.push_back({pin, callback});
}

void DecentIoTClass::onReceive(const char *pin, ReceiveCallback callback)
{
    // Store the callback for this pin
    _receiveHandlers.push_back({pin, callback});
    
    // Set up Firebase stream for this pin
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin + "/value";
    
    if (Firebase.RTDB.beginStream(&_fbdo, path.c_str()))
    {
        Serial.printf("[RECEIVE] Stream started for %s\n", pin);
        
        // Set the stream callback with proper Firebase types
        Firebase.RTDB.setStreamCallback(&_fbdo, handleFirebaseStreamStatic, [](bool timeout) {
            if (timeout)
                Serial.println("[RECEIVE] Stream timeout, resuming...");
        });
    }
    else
    {
        Serial.printf("[RECEIVE] Failed to start stream for %s\n", pin);
    }
}


void DecentIoTClass::handleFirebaseStream(FirebaseStream data)
{
    String path = data.dataPath();
    // Expecting paths like "/P0", "/P0/value", "/P1", etc.
    if (!path.startsWith("/P"))
    {
        return;
    }

    // Normalize pin id from incoming path:
    // "/P0" -> "P0", "/P0/value" -> "P0"
    int secondSlash = path.indexOf('/', 1);
    String pin = secondSlash == -1 ? path.substring(1) : path.substring(1, secondSlash);

    if (data.dataType() == "boolean")
    {
        this->dispatchReceiveHandler(pin.c_str(), data.boolData());
    }
    else if (data.dataType() == "int")
    {
        this->dispatchReceiveHandler(pin.c_str(), data.intData());
    }
    else if (data.dataType() == "float")
    {
        this->dispatchReceiveHandler(pin.c_str(), data.floatData());
    }
    else if (data.dataType() == "string")
    {
        String val = data.stringData();
        this->dispatchReceiveHandler(pin.c_str(), val.c_str());
    }
    else if (data.dataType() == "json")
    {
        // Dashboard/Firebase may emit full pin object (e.g. {"value":1,...}) before direct value events.
        // Parse and dispatch "value" so first switch click is handled immediately.
        FirebaseJson json;
        FirebaseJsonData valueData;
        json.setJsonData(data.jsonString());
        json.get(valueData, "value");

        if (valueData.success)
        {
            if (valueData.type == "boolean")
            {
                this->dispatchReceiveHandler(pin.c_str(), valueData.boolValue);
            }
            else if (valueData.type == "int")
            {
                this->dispatchReceiveHandler(pin.c_str(), valueData.intValue);
            }
            else if (valueData.type == "double" || valueData.type == "float")
            {
                this->dispatchReceiveHandler(pin.c_str(), static_cast<float>(valueData.doubleValue));
            }
            else
            {
                this->dispatchReceiveHandler(pin.c_str(), valueData.stringValue.c_str());
            }
        }
        else
        {
            Serial.printf("[DEBUG] JSON payload for pin %s has no 'value' field\n", pin.c_str());
        }
    }
    else
    {
        Serial.printf("[DEBUG] Unsupported data type %s for pin %s\n", data.dataType().c_str(), pin.c_str());
    }
}

/*/ It parsed JsonVariant if needed. in our current case we may not need it anymore TODO: remove this function later
void DecentIoTClass::dispatchReceiveHandler(const char *id, JsonVariant value)
{
    for (auto &handler : _receiveHandlers)
    {
        if (strcmp(handler.id.c_str(), id) == 0)
        {
            // Try to call with the correct type
            if (value.is<bool>())
            {
                handler.callback(value.as<bool>());
            }
            else if (value.is<int>())
            {
                handler.callback(value.as<int>());
            }
            else if (value.is<float>())
            {
                handler.callback(value.as<float>());
            }
            else if (value.is<const char *>())
            {
                handler.callback(value.as<const char *>());
            }
            return;
        }
    }
}*/


// Dispatch handlers for different data types
// We are passing direct values from firebase, not parsed JSON
void DecentIoTClass::dispatchReceiveHandler(const char *id, bool value)
{
    for (auto &handler : _receiveHandlers)
    {
        if (handler.id == id)
        {
            DecentIoTValue v;
            v.type = DecentIoTValue::BOOL;
            v.boolValue = value;
            handler.callback(v);
            return;
        }
    }
}

void DecentIoTClass::dispatchReceiveHandler(const char *id, int value)
{
    for (auto &handler : _receiveHandlers)
    {
        if (handler.id == id)
        {
            DecentIoTValue v;
            v.type = DecentIoTValue::INT;
            v.intValue = value;
            handler.callback(v);
            return;
        }
    }
}

void DecentIoTClass::dispatchReceiveHandler(const char *id, float value)
{
    for (auto &handler : _receiveHandlers)
    {
        if (handler.id == id)
        {
            DecentIoTValue v;
            v.type = DecentIoTValue::FLOAT;
            v.floatValue = value;
            handler.callback(v);
            return;
        }
    }
}

void DecentIoTClass::dispatchReceiveHandler(const char *id, const char *value)
{
    for (auto &handler : _receiveHandlers)
    {
        if (handler.id == id)
        {
            DecentIoTValue v;
            v.type = DecentIoTValue::STRING;
            v.stringValue = value;
            handler.callback(v);
            return;
        }
    }
}

void DecentIoTClass::debugPrintScheduledTasks()
{
    for (auto &task : _scheduledTasks)
    {
        Serial.printf("[DEBUG] Scheduled task: %s\n", task.first.c_str());
    }
}

void DecentIoTClass::pollAllReceivePinsOnce()
{
    for (const auto &handler : _receiveHandlers)
    {
        String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + handler.id + "/value";
        
        // Simple, predictable order - no assumptions about pin names
        // Let the user's code handle the type conversion, not the library
        if (Firebase.RTDB.getInt(&_pollFbdo, path.c_str())) {
            int val = _pollFbdo.intData();
            DecentIoTValue v;
            v.type = DecentIoTValue::INT;
            v.intValue = val;
            handler.callback(v);
        }
        else if (Firebase.RTDB.getBool(&_pollFbdo, path.c_str())) {
            bool val = _pollFbdo.boolData();
            DecentIoTValue v;
            v.type = DecentIoTValue::BOOL;
            v.boolValue = val;
            handler.callback(v);
        }
        else if (Firebase.RTDB.getString(&_pollFbdo, path.c_str()))
        {
            String val = _pollFbdo.stringData();
            DecentIoTValue v;
            v.type = DecentIoTValue::STRING;
            v.stringValue = val;
            handler.callback(v);
        }
        else if (Firebase.RTDB.getFloat(&_pollFbdo, path.c_str()))
        {
            float val = _pollFbdo.floatData();
            DecentIoTValue v;
            v.type = DecentIoTValue::FLOAT;
            v.floatValue = val;
            handler.callback(v);
        }
        else
        {
            Serial.printf("[POLL] Failed to read %s from Firebase.\n", handler.id.c_str());
        }
    }
}

void DecentIoTClass::updateDeviceStatus()
{
    unsigned long currentMillis = millis();

    // If a status update is pending (e.g., after a failure or on startup)
    if (_statusUpdatePending) {
        // Only retry if enough time has passed since last retry
        if (currentMillis - _lastStatusRetry >= _statusRetryInterval) {
            String statusPath = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/status";
            time_t unixTimestamp = time(nullptr);

            if (Firebase.RTDB.setInt(&_fbdo, statusPath.c_str(), (unsigned long)unixTimestamp)) {
                //Serial.printf("[STATUS] Device status updated successfully: %lu (%s)\n",
                //              (unsigned long)unixTimestamp, ctime(&unixTimestamp));
                _lastStatusUpdate = currentMillis;
                _statusUpdatePending = false;
            } else {
                //Serial.printf("[STATUS] Failed to update device status. Error: %s\n", _fbdo.errorReason().c_str());
                _lastStatusRetry = currentMillis;
                _statusUpdatePending = true;
            }
            pollAllReceivePinsOnce();
        }
    }
    // If not pending, check if it's time for a regular update
    else if (currentMillis - _lastStatusUpdate >= _statusUpdateInterval) {
        String statusPath = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/status";
        time_t unixTimestamp = time(nullptr);

        if (Firebase.RTDB.setInt(&_fbdo, statusPath.c_str(), (unsigned long)unixTimestamp)) {
            //Serial.printf("[STATUS] Device status updated successfully: %lu (%s)\n",
            //              (unsigned long)unixTimestamp, ctime(&unixTimestamp));
            _lastStatusUpdate = currentMillis;
            _statusUpdatePending = false;
        } else {
            //Serial.printf("[STATUS] Failed to update device status. Error: %s\n", _fbdo.errorReason().c_str());
            _lastStatusRetry = currentMillis;
            _statusUpdatePending = true;
        }
        pollAllReceivePinsOnce();
    }
}

void DecentIoTClass::handleReconnection()
{
    unsigned long currentMillis = millis();
    
    // Throttle reconnection attempts
    if (currentMillis - _lastReconnectAttempt < _reconnectInterval)
    {
        return;
    }
    
    _lastReconnectAttempt = currentMillis;
    
    // Try to reconnect (silent unless it succeeds)
    if (Firebase.ready())
    {
        Serial.println("[DecentIoT] Firebase reconnected");
        
        if (reconnectStream())
        {
            _isConnected = true;
        }
    }
}

bool DecentIoTClass::reconnectStream()
{
    // Close existing stream if any
    Firebase.RTDB.endStream(&_fbdo);
    
    // Wait a bit before reconnecting
    delay(100);
    
    // Reconnect the stream
    String streamPath = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId;
    
    if (Firebase.RTDB.beginStream(&_fbdo, streamPath.c_str()))
    {
        // Set the stream callback again
        Firebase.RTDB.setStreamCallback(
            &_fbdo,
            handleFirebaseStreamStatic,
            [](bool timeout)
            {
                if (timeout)
                    Serial.println("[DecentIoT] Stream timeout, resuming...");
            });
        
        return true;
    }
    else
    {
        Serial.printf("[DecentIoT] Stream reconnection failed. Error: %s\n", _fbdo.errorReason().c_str());
        return false;
    }
}

