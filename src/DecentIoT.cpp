#include "DecentIoT.h"
#include <FirebaseClient.h>
#include <time.h>

// Global instance
DecentIoTClass DecentIoT;
DecentIoTClass &getDecentIoT() { return DecentIoT; }

// Static instance pointer for callback
DecentIoTClass *DecentIoTClass::_instance = nullptr;
void DecentIoTClass::setInstance(DecentIoTClass *instance) { _instance = instance; }
void DecentIoTClass::handleFirebaseStreamStatic(AsyncResult &aResult)
{
    if (_instance)
        _instance->handleFirebaseStream(aResult);
}

DecentIoTClass::DecentIoTClass()
    : _firebaseUrl(nullptr), _firebaseAuth(nullptr), _projectId(nullptr),
      _userId(nullptr), _deviceId(nullptr), _authEmail(nullptr), _authPass(nullptr),
      _isConnected(false),
      _lastRequestTime(0), _requestInterval(50)
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

    // Assign user credentials
    _auth.user.email = _authEmail;
    _auth.user.password = _authPass;

    // Initialize Firebase App
    _app.initializeApp(_async_client, _auth);

    // Set the Firebase Project ID and Database URL
    _app.getApp<AsyncClient>()
        ->setProjectID(_projectId)
        .setDatabaseUrl(_firebaseUrl);

    setInstance(this);

    // Get the Realtime Database object
    RealtimeDatabase rtdb = _app.getDatabase();

    // Set up the stream
    String streamPath = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId;
    rtdb.stream(streamPath.c_str(), handleFirebaseStreamStatic, AsyncClient::Priority::HIGH, "", "");

    // Wait for authentication to complete
    unsigned long start = millis();
    while (!_app.getAuth()->tokenReady() && millis() - start < 10000) {
        Serial.print(".");
        delay(500);
    }

    if (!_app.getAuth()->tokenReady()) {
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
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    RealtimeDatabase rtdb = _app.getDatabase();
    rtdb.set(path.c_str(), value);
}

void DecentIoTClass::write(const char *pin, int value)
{
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    RealtimeDatabase rtdb = _app.getDatabase();
    rtdb.set(path.c_str(), value);
}

void DecentIoTClass::write(const char *pin, float value)
{
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    RealtimeDatabase rtdb = _app.getDatabase();
    rtdb.set(path.c_str(), value);
}

void DecentIoTClass::write(const char *pin, const char *value)
{
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    RealtimeDatabase rtdb = _app.getDatabase();
    rtdb.set(path.c_str(), value);
}

// analog classes
void DecentIoTClass::writeAnalog(const char *pin, int value)
{
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    RealtimeDatabase rtdb = _app.getDatabase();
    if (rtdb.set(path.c_str(), value))
    {
        Serial.printf("[ANALOG] %s set to %d\n", pin, value);
    }
    else
    {
        Serial.printf("[ANALOG] Failed to set %s to %d\n", pin, value);
    }
}

void DecentIoTClass::writePWM(const char *pin, int value)
{
    // Ensure value is in 0-255 range for PWM
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    RealtimeDatabase rtdb = _app.getDatabase();
    if (rtdb.set(path.c_str(), value))
    {
        Serial.printf("[PWM] %s set to %d (0-255)\n", pin, value);
    }
    else
    {
        Serial.printf("[PWM] Failed to set %s to %d\n", pin, value);
    }
}

void DecentIoTClass::writePercent(const char *pin, float value)
{
    // Ensure value is in 0-100 range
    if (value < 0.0) value = 0.0;
    if (value > 100.0) value = 100.0;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    RealtimeDatabase rtdb = _app.getDatabase();
    if (rtdb.set(path.c_str(), value))
    {
        Serial.printf("[PERCENT] %s set to %.1f%%\n", pin, value);
    }
    else
    {
        Serial.printf("[PERCENT] Failed to set %s to %.1f%%\n", pin, value);
    }
}

void DecentIoTClass::writeRange(const char *pin, int value, int min, int max)
{
    // Map the value from min-max range to 0-255 for PWM
    int mappedValue = map(value, min, max, 0, 255);
    
    String path = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/" + pin;
    RealtimeDatabase rtdb = _app.getDatabase();
    if (rtdb.set(path.c_str(), mappedValue))
    {
        Serial.printf("[RANGE] %s mapped from %d (%d-%d) to %d (0-255)\n", pin, value, min, max, mappedValue);
    }
    else
    {
        Serial.printf("[RANGE] Failed to set %s\n", pin);
    }
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

void DecentIoTClass::onSend(const char *pin, SendCallback callback)
{
    _sendHandlers.push_back({pin, callback});
}

void DecentIoTClass::onReceive(const char *pin, ReceiveCallback callback)
{
    // Store the callback for this pin
    _receiveHandlers.push_back({pin, callback});
}

void DecentIoTClass::cancelSend(const char *pin)
{
    // No queue/request system; nothing to cancel. If needed, remove scheduled send tasks here.
}

void DecentIoTClass::handleFirebaseStream(AsyncResult &aResult)
{
    if (aResult.isEvent())
    {
        if (aResult.getEvent().getEventType() == "put")
        {
            String path = aResult.getEvent().getDataPath();
            if (path.startsWith("/"))
            {
                String pin = path.substring(1);
                if (pin.startsWith("P"))
                {
                    Data d = aResult.getEvent().getData();
                    if (d.isBool())
                    {
                        this->dispatchReceiveHandler(pin.c_str(), d.to<bool>());
                    }
                    else if (d.isInt())
                    {
                        this->dispatchReceiveHandler(pin.c_str(), d.to<int>());
                    }
                    else if (d.isFloat())
                    {
                        this->dispatchReceiveHandler(pin.c_str(), d.to<float>());
                    }
                    else if (d.isString())
                    {
                        this->dispatchReceiveHandler(pin.c_str(), d.to<const char *>());
                    }
                }
            }
        }
    }
    else if (aResult.isError())
    {
        Serial.printf("[STREAM] Error: %s\n", aResult.getError().message().c_str());
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


void DecentIoTClass::updateDeviceStatus()
{
    unsigned long currentMillis = millis();

    auto sendStatusUpdate = [this](unsigned long currentMillis) {
        String statusPath = String("/") + _projectId + "/users/" + _userId + "/datastreams/" + _deviceId + "/status";
        RealtimeDatabase rtdb = _app.getDatabase();
        
        Data status_data;
        time_t unixTimestamp = time(nullptr);
        status_data.set("s", 1);
        status_data.set("t", (unsigned long)unixTimestamp);

        if (rtdb.set(statusPath.c_str(), status_data)) {
            Serial.printf("[STATUS] Device status updated successfully: s=1, t=%lu (%s)\n",
                          (unsigned long)unixTimestamp, ctime(&unixTimestamp));
            _lastStatusUpdate = currentMillis;
            _statusUpdatePending = false;
        } else {
            Serial.printf("[STATUS] Failed to update device status.\n");
            _lastStatusRetry = currentMillis;
            _statusUpdatePending = true;
        }
    };

    if (_statusUpdatePending) {
        if (currentMillis - _lastStatusRetry >= _statusRetryInterval) {
            sendStatusUpdate(currentMillis);
        }
    } else if (currentMillis - _lastStatusUpdate >= _statusUpdateInterval) {
        sendStatusUpdate(currentMillis);
    }
}

