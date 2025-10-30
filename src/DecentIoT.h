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

#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <vector>
#include <functional>
#include <map>
#include <queue>
#include <Firebase_ESP_Client.h>

// Callback types
// Universal callback that can handle any type
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
using ReceiveCallback = std::function<void(const DecentIoTValue& value)>;

typedef std::function<void()> SendCallback;
typedef std::function<void()> TaskCallback;

// Handler structures
struct ReceiveHandler
{
    String id;
    ReceiveCallback callback;
};

struct SendHandler
{
    String id;
    SendCallback callback;
};

// Scheduled task structure
struct ScheduledTask
{
    unsigned long lastRun;
    unsigned long interval;
    TaskCallback callback;
};

class DecentIoTClass
{
private:
    // Initialize members in declaration order to avoid warnings
    const char *_firebaseUrl;
    const char *_firebaseAuth;
    const char *_projectId;
    const char *_userId;
    const char *_deviceId;
    const char *_authEmail;
    const char *_authPass;
    bool _isConnected = false;
    std::vector<ReceiveHandler> _receiveHandlers;
    std::vector<SendHandler> _sendHandlers;
    std::map<String, ScheduledTask> _scheduledTasks;
    unsigned long _lastRequestTime = 0;
    const unsigned long _requestInterval = 50; // 50ms between requests
    unsigned long _lastStatusUpdate = 0;
    const unsigned long _statusUpdateInterval = 30000; // 30 seconds between status updates
    unsigned long _lastStatusRetry = 0;
    const unsigned long _statusRetryInterval = 15000; // 15 seconds retry interval
    bool _statusUpdatePending = true; // Force immediate status update
    unsigned long _lastReconnectAttempt = 0;
    const unsigned long _reconnectInterval = 5000; // 5 seconds between reconnection attempts
    unsigned long _lastStreamCheck = 0;
    const unsigned long _streamCheckInterval = 10000; // Check stream health every 10 seconds
    bool _wasWiFiConnected = false; // Track WiFi state to detect reconnections
    FirebaseConfig _config;
    FirebaseAuth _auth;
    FirebaseData _fbdo;
    FirebaseData _pollFbdo;
    void processScheduledTasks();
    bool isNumericOrBoolean(const char *s);
    void handleFirebaseStream(FirebaseStream data);
    static void handleFirebaseStreamStatic(FirebaseStream data);
    static DecentIoTClass *_instance;
    static void setInstance(DecentIoTClass *instance);
    //void dispatchReceiveHandler(const char *id, JsonVariant value);
    void dispatchReceiveHandler(const char *id, bool value);
    void dispatchReceiveHandler(const char *id, int value);
    void dispatchReceiveHandler(const char *id, float value);
    void dispatchReceiveHandler(const char *id, const char *value);
    void cancel(String taskId);
    void debugPrintScheduledTasks();
    void pollAllReceivePinsOnce();
    void updateDeviceStatus();
    void handleReconnection();
    bool reconnectStream();

public:
    DecentIoTClass();
    bool begin(const char *firebaseUrl, const char *firebaseAuth, const char *projectId, const char *userId, const char *deviceId, const char *authEmail, const char *authPass);
    bool isConnected() { return _isConnected; }
    void onReceive(const char *pin, ReceiveCallback callback);
    void onSend(const char *pin, SendCallback callback);
    void run();
    void cancelSend(const char *pin);
    void write(const char *pin, bool value);
    void write(const char *pin, int value);
    void write(const char *pin, float value);
    void write(const char *pin, const char *value);
    //newly analog control functions
    void writeAnalog(const char *pin, int value);        // 0-255 range for PWM
    void writePWM(const char *pin, int value);           // PWM control (0-255)
    void writePercent(const char *pin, float value);     // 0-100% range
    void writeRange(const char *pin, int value, int min, int max); // Custom range mapping

    void schedule(uint32_t interval, TaskCallback callback);
    void schedule(String taskId, uint32_t interval, TaskCallback callback);
    void scheduleOnce(uint32_t delay, TaskCallback callback);
};

extern DecentIoTClass DecentIoT;
DecentIoTClass &getDecentIoT();

// Helper class to register receive handlers at static initialization time
class DecentIoTReceiveRegistrar
{
public:
    DecentIoTReceiveRegistrar(const char *pin, ReceiveCallback cb)
    {
        getDecentIoT().onReceive(pin, cb);
    }
};

// Helper class to register send handlers at static initialization time
class DecentIoTSendRegistrar
{
public:
    DecentIoTSendRegistrar(const char *pin, SendCallback cb, uint32_t interval = 0)
    {
        if (interval > 0)
        {
            // If interval is provided, create a scheduled task
            String taskId = String("send_") + pin;
            getDecentIoT().schedule(taskId, interval, cb);
        }
        else
        {
            // If no interval, just register the callback
            getDecentIoT().onSend(pin, cb);
        }
    }
};

// Macro for user-friendly receive handler definition
#define DECENTIOT_RECEIVE(PIN_NAME)                                                                                        \
    void DECENTIOT_RECEIVE_HANDLER_##PIN_NAME(const DecentIoTValue& value);                                    \
    static DecentIoTReceiveRegistrar _decentiot_receive_registrar_##PIN_NAME(#PIN_NAME, DECENTIOT_RECEIVE_HANDLER_##PIN_NAME); \
    void DECENTIOT_RECEIVE_HANDLER_##PIN_NAME(const DecentIoTValue& value)

// Macro for user-friendly send handler definition with optional interval
#define DECENTIOT_SEND(PIN_NAME, ...)                                                                                            \
    void DECENTIOT_SEND_HANDLER_##PIN_NAME();                                                                                    \
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
#define P11 "P11"
#define P12 "P12"
#define P13 "P13"
#define P14 "P14"
#define P15 "P15"
#define P16 "P16"
#define P17 "P17"
#define P18 "P18"
#define P19 "P19"
#define P20 "P20"
#define P21 "P21"
#define P22 "P22"
#define P23 "P23"
#define P24 "P24"
#define P25 "P25"
#define P26 "P26"
#define P27 "P27"
#define P28 "P28"
#define P29 "P29"
#define P30 "P30"
#define P31 "P31"
#define P32 "P32"
#define P33 "P33"
#define P34 "P34"
#define P35 "P35"
#define P36 "P36"
#define P37 "P37"
#define P38 "P38"
#define P39 "P39"
#define P40 "P40"
#define P41 "P41"
#define P42 "P42"
#define P43 "P43"
#define P44 "P44"
#define P45 "P45"
#define P46 "P46"
#define P47 "P47"
#define P48 "P48"
#define P49 "P49"
#define P50 "P50"