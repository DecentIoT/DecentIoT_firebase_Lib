#include "DecentIoT.h"
#include "../firebase/StreamManager.h"
#include <time.h>

// Global instance
DecentIoTClass DecentIoT;
DecentIoTClass& getDecentIoT() { return DecentIoT; }

// Static instance pointer for callback
DecentIoTClass* DecentIoTClass::_instance = nullptr;

DecentIoTClass::DecentIoTClass() 
    : _firebaseUrl(nullptr), _firebaseAuth(nullptr), _projectId(nullptr), 
      _userId(nullptr), _deviceId(nullptr), _authEmail(nullptr), _authPass(nullptr),
      _isConnected(false), _lastStatusUpdate(0) {
    _instance = this;
}

DecentIoTClass::~DecentIoTClass() {
    if (_streamManager) {
        delete _streamManager;
        _streamManager = nullptr;
    }
}

bool DecentIoTClass::begin(const char* firebaseUrl, const char* firebaseAuth,
                           const char* projectId, const char* userId, const char* deviceId,
                           const char* authEmail, const char* authPass) {
    
    // Add safety checks
    if (!firebaseUrl || !firebaseAuth || !projectId || !userId || !deviceId) {
        Serial.println("[ERROR] Invalid parameters passed to DecentIoT.begin()");
        return false;
    }
    
    Serial.println("[INFO] Starting DecentIoT initialization with custom implementation...");
    
    _firebaseUrl = firebaseUrl;
    _firebaseAuth = firebaseAuth;
    _projectId = projectId;
    _userId = userId;
    _deviceId = deviceId;
    _authEmail = authEmail;
    _authPass = authPass;

    Serial.printf("[INFO] Project: %s, User: %s, Device: %s\n", projectId, userId, deviceId);

    // Initialize Firebase connection with safety check
    Serial.println("[INFO] Initializing Firebase connection...");
    if (!_firebaseConnection.begin(firebaseUrl, firebaseAuth)) {
        Serial.println("[ERROR] Failed to initialize Firebase connection");
        return false;
    }
    Serial.println("[INFO] Firebase connection initialized");

    // Skip Stream Manager for now to avoid memory issues
    Serial.println("[INFO] Skipping Stream Manager initialization for stability");
    _streamManager = nullptr;

    // Skip time sync for now to avoid issues
    Serial.println("[INFO] Skipping time sync for stability");

    _isConnected = true;
    Serial.println("[INFO] DecentIoT library initialized successfully!");
    return true;
}

void DecentIoTClass::run() {
    if (!_isConnected) return;

    // Process scheduled tasks
    processScheduledTasks();
    
    // Update device status
    updateDeviceStatus();
    
    // Process streams if available
    if (_streamManager) {
        _streamManager->processStreams();
    }
}

void DecentIoTClass::write(const char* pin, bool value) {
    if (!_isConnected) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/dataStream/" + _deviceId + "/" + pin;
    String data = value ? "true" : "false";
    String response;
    
    if (_firebaseConnection.put(path.c_str(), data, response)) {
        Serial.printf("[INFO] Write successful: %s = %s\n", pin, data.c_str());
        Serial.printf("[DEBUG] Response: %s\n", response.c_str());
    } else {
        Serial.printf("[ERROR] Write failed: %s = %s\n", pin, data.c_str());
        Serial.printf("[DEBUG] Response: %s\n", response.c_str());
    }
}

void DecentIoTClass::write(const char* pin, int value) {
    if (!_isConnected) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/dataStream/" + _deviceId + "/" + pin;
    String data = String(value);
    String response;
    
    if (_firebaseConnection.put(path.c_str(), data, response)) {
        Serial.printf("[INFO] Write successful: %s = %d\n", pin, value);
        Serial.printf("[DEBUG] Response: %s\n", response.c_str());
    } else {
        Serial.printf("[ERROR] Write failed: %s = %d\n", pin, value);
        Serial.printf("[DEBUG] Response: %s\n", response.c_str());
    }
}

void DecentIoTClass::write(const char* pin, float value) {
    if (!_isConnected) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/dataStream/" + _deviceId + "/" + pin;
    String data = String(value, 2);
    String response;
    
    if (_firebaseConnection.put(path.c_str(), data, response)) {
        Serial.printf("[INFO] Write successful: %s = %.2f\n", pin, value);
        Serial.printf("[DEBUG] Response: %s\n", response.c_str());
    } else {
        Serial.printf("[ERROR] Write failed: %s = %.2f\n", pin, value);
        Serial.printf("[DEBUG] Response: %s\n", response.c_str());
    }
}

void DecentIoTClass::write(const char* pin, const char* value) {
    if (!_isConnected) return;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/dataStream/" + _deviceId + "/" + pin;
    String data = String(value);
    String response;
    
    if (_firebaseConnection.put(path.c_str(), data, response)) {
        Serial.printf("[INFO] Write successful: %s = %s\n", pin, value);
        Serial.printf("[DEBUG] Response: %s\n", response.c_str());
    } else {
        Serial.printf("[ERROR] Write failed: %s = %s\n", pin, value);
        Serial.printf("[DEBUG] Response: %s\n", response.c_str());
    }
}

void DecentIoTClass::writePercent(const char* pin, float percent) {
    if (!_isConnected) return;
    
    // Ensure percent is between 0.0 and 100.0
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;
    
    String path = String("/") + _projectId + "/users/" + _userId + "/dataStream/" + _deviceId + "/" + pin;
    String data = String(percent, 2);
    String response;
    
    if (_firebaseConnection.put(path.c_str(), data, response)) {
        Serial.printf("[INFO] Write percent successful: %s = %.2f%%\n", pin, percent);
        Serial.printf("[DEBUG] Response: %s\n", response.c_str());
    } else {
        Serial.printf("[ERROR] Write percent failed: %s = %.2f%%\n", pin, percent);
        Serial.printf("[DEBUG] Response: %s\n", response.c_str());
    }
}

void DecentIoTClass::onReceive(const char* pin, ReceiveCallback callback) {
    ReceiveHandler handler;
    handler.id = String(pin);
    handler.callback = callback;
    _receiveHandlers.push_back(handler);
    
    Serial.printf("[INFO] Registered receive handler for pin: %s (total handlers: %d)\n", 
                  pin, _receiveHandlers.size());
    
    // Set up stream for this pin if stream manager is ready
    if (_streamManager) {
        String streamPath = String("/") + _projectId + "/users/" + _userId + "/dataStream/" + _deviceId + "/" + pin;
        
        // Create a callback that converts StreamEvent to DecentIoTValue and calls the user's callback
        auto streamCallback = [this, callback](const StreamEvent& event) {
            DecentIoTValue value = _parseStreamEventToValue(event);
            callback(value);
        };
        
        if (_streamManager->addPinStream(pin, streamPath.c_str(), streamCallback)) {
            Serial.printf("[INFO] Stream set up for pin: %s\n", pin);
        } else {
            Serial.printf("[ERROR] Failed to set up stream for pin: %s\n", pin);
        }
    }
}

void DecentIoTClass::schedule(const char* id, unsigned long interval, TaskCallback callback) {
    ScheduledTask task;
    task.lastRun = 0;
    task.interval = interval;
    task.callback = callback;
    task.once = false;
    _scheduledTasks[String(id)] = task;
    
    Logger::debugf("DECENTIOT", "Scheduled task: %s (interval: %lu ms)", id, interval);
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
    
    Logger::debugf("DECENTIOT", "Scheduled one-time task: %s (delay: %lu ms)", taskId.c_str(), delay);
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

void DecentIoTClass::updateDeviceStatus() {
    if (!_isConnected) return;

    unsigned long currentMillis = millis();
    if (currentMillis - _lastStatusUpdate >= _statusUpdateInterval) {
        String statusPath = String("/") + _projectId + "/users/" + _userId + "/dataStream/" + _deviceId + "/status";
        String statusData = "{\"s\":1,\"t\":" + String(time(nullptr)) + "}";
        String response;
        
        if (_firebaseConnection.put(statusPath.c_str(), statusData, response)) {
            _lastStatusUpdate = currentMillis;
            Logger::debug("DECENTIOT", "Device status updated");
        } else {
            Logger::warn("DECENTIOT", "Failed to update device status");
        }
    }
}

bool DecentIoTClass::authenticateWithFirebase() {
    // For now, we'll use the auth token directly
    // In the future, we can implement proper Firebase Auth
    Logger::info("DECENTIOT", "Using direct auth token (no authentication required)");
    return true;
}

DecentIoTValue DecentIoTClass::_parseStreamEventToValue(const StreamEvent& event) {
    DecentIoTValue value;
    
    // Parse the data based on content
    if (event.data == "true" || event.data == "1") {
        value = DecentIoTValue(true);
    } else if (event.data == "false" || event.data == "0") {
        value = DecentIoTValue(false);
    } else {
        // Try to parse as number
        char* endPtr;
        float fVal = strtof(event.data.c_str(), &endPtr);
        
        if (endPtr != event.data.c_str() && *endPtr == '\0') {
            if (fVal == (int)fVal) {
                value = DecentIoTValue((int)fVal);
            } else {
                value = DecentIoTValue(fVal);
            }
        } else {
            value = DecentIoTValue(event.data);
        }
    }
    
    Logger::debugf("DECENTIOT", "Parsed stream event: %s = %s (%s)", 
                   event.path.c_str(), event.data.c_str(), value.getTypeString().c_str());
    
    return value;
}
