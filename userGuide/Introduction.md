# DecentIoT Firebase Library - Complete Introduction

## 🚀 What is DecentIoT Firebase Library?

**DecentIoT Firebase Library** is a revolutionary Arduino library that transforms IoT development! It's the **best alternative to existing IoT platforms** with a unique architecture that gives you complete control. Here's what makes it special:

### **🎯 The DecentIoT Revolution**

**DecentIoT** is a **complete IoT platform** that combines:
- 🌐 **Web Dashboard** - Create beautiful dashboards with widgets
- 🔧 **Your Own Cloud** - Use your own Firebase Realtime Database
- 📱 **Smart Library** - This Arduino library that connects everything

**No more vendor lock-in!** Use your own Firebase infrastructure while getting a professional dashboard experience.

### **Why Choose DecentIoT?**

- 🎯 **Virtual Pin Architecture**: P0, P1, P2, P3... represent virtual pins that map to your web dashboard widgets
- 🔒 **Your Own Cloud**: Connect to your own Firebase Realtime Database
- 🌐 **Professional Dashboard**: Create beautiful, interactive dashboards with your own backend
- ⚡ **Easy to Use**: Simple macros and intuitive API design
- 🔄 **Real-time Communication**: Bidirectional data flow between device and cloud
- 🛠️ **Production Ready**: Robust, reliable, and well-tested
- 💰 **Cost Effective**: No subscription fees, use your own infrastructure

---

## 🎯 Key Features

### **Core Features**
- ✅ **Secure Firebase Communication** - SSL/TLS encrypted with Firebase authentication
- ✅ **Pin-Based Data Handling** - P0, P1, P2, P3... intuitive pin system
- ✅ **Bidirectional Communication** - Send data to cloud, receive commands from cloud
- ✅ **Scheduled Tasks** - Automatic data sending at specified intervals
- ✅ **Real-time Commands** - Instant cloud-to-device control
- ✅ **Auto-reconnection** - Robust connection management
- ✅ **Memory Efficient** - Optimized for ESP8266/ESP32

### **Advanced Features**
- 🔄 **Scheduling System** - Recurring and one-time tasks
- 🎛️ **Dynamic Task Management** - Start/stop tasks based on conditions
- 📊 **Device Status Monitoring** - Automatic online/offline status
- 🔧 **Flexible Configuration** - Easy setup and customization
- 📚 **Comprehensive Documentation** - Detailed guides and examples

---

## 🚀 Quick Start Guide

### **1. Installation**
```bash
# Install via Arduino Library Manager
# Search for "DecentIoT Firebase" and install

# Or download from GitHub and place in your libraries folder
```
## 🔧 Complete Platform Setup

### **Step 1: Set Up Your Firebase Project**
1. Go to [Google Firebase Console](https://console.firebase.google.com/)
2. Create a new Firebase project
3. Enable Realtime Database
4. Set up Firebase Authentication (Email/Password)
5. Configure database security rules
6. Get your project credentials (Database URL and Web API Key)

### **Step 2: Create DecentIoT Web Dashboard**
1. Go to DecentIoT web platform
2. Create your project
3. Add your Firebase project credentials
4. Create dashboard with widgets
5. Map widgets to virtual pins (P0, P1, P2, etc.)

### **Step 3: Install Library**
```bash
# Install via Arduino Library Manager
# Search for "DecentIoT Firebase" and install
```

### **Prerequisites**
- Arduino IDE 1.8.0 or later
- ESP8266 or ESP32 board
- WiFi connection
- Firebase project (with Realtime Database)
- DecentIoT web dashboard account

### **Dependencies**
The library requires these dependencies:
- `Firebase ESP Client` by Mobizt (MIT License) - Firebase Realtime Database client
- `ArduinoJson` by Benoit Blanchon (>=6.0.0, MIT License) - JSON data handling

**Installation:**
- **Arduino Library Manager**: Dependencies are automatically installed ✅
- **Install from ZIP**: Dependencies are NOT automatically installed ❌
- **PlatformIO**: Add to `platformio.ini`:
  ```ini
  lib_deps = 
      mobizt/Firebase-ESP-Client
      bblanchon/ArduinoJson
  ```

**Manual Dependency Installation (if needed):**
1. Go to **Sketch → Include Library → Manage Libraries**
2. Search and install:
   - `Firebase ESP Client` by Mobizt (MIT License)
   - `ArduinoJson` by Benoit Blanchon (version 6.0.0 or higher, MIT License)


### **2. Basic Setup**
```cpp
#include <DecentIoT.h>
#include <WiFi.h>

// Firebase Configuration
#define FIREBASE_URL "https://your-project.firebaseio.com"
#define FIREBASE_AUTH "your-web-api-key"
#define AUTH_EMAIL "your-auth-email"
#define AUTH_PASS "your-auth-password"
#define PROJECT_ID "your-project-id"
#define USER_ID "your-user-id"
#define DEVICE_ID "your-device-id"
// WiFi credentials
#define WIFI_SSID "your-wifi-name"
#define WIFI_PASS "wifi-password"
// Pin definitions
#define LED_PIN 12
#define SENSOR_PIN A0

// Pin-based handlers
DECENTIOT_RECEIVE(P0)  // Handle LED control from cloud
{
    digitalWrite(LED_PIN, value);
    Serial.printf("[P0] LED: %s\n", value ? "ON" : "OFF");
}

DECENTIOT_SEND(P1, 5000)  // Send sensor data every 5 seconds
{
    int sensorValue = analogRead(SENSOR_PIN);
    DecentIoT.write(P1, sensorValue);
    Serial.printf("[P1] Sensor: %d\n", sensorValue);
}

void setup() {
    Serial.begin(115200);
    
    // Initialize pins
    pinMode(LED_PIN, OUTPUT);
    
    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Wifi Connecting ");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    // Now start DecentIoT (cloud connection)
   DecentIoT.begin(FIREBASE_URL, FIREBASE_AUTH, PROJECT_ID, USER_ID, DEVICE_ID, AUTH_EMAIL, AUTH_PASS);
}

void loop() {
    DecentIoT.run();  // Process all communication
    delay(10);
}
```

### **3. That's It!**
Your device is now:
- ✅ Connected to WiFi
- ✅ Securely connected to Firebase
- ✅ Sending sensor data to P1 every 5 seconds
- ✅ Ready to receive LED commands on P0 from the cloud

---

## 🎯 Core Concepts

### **Virtual Pin Architecture**
DecentIoT uses a **virtual pin system** where P0, P1, P2, P3... represent **virtual pins** that map to your web dashboard widgets:

```cpp
// P0 = Virtual Pin for LED Control (maps to Button widget in web dashboard)
DECENTIOT_RECEIVE(P0) { 
    digitalWrite(LED_PIN, value);  // Physical pin D6 controls actual LED
}

// P1 = Virtual Pin for Temperature (maps to Gauge widget in web dashboard)
DECENTIOT_SEND(P1, 10000) { 
    DecentIoT.write(P1, readTemperature()); 
}

// P2 = Virtual Pin for Humidity (maps to Chart widget in web dashboard)
DECENTIOT_SEND(P2, 15000) { 
    DecentIoT.write(P2, readHumidity()); 
}
```

### **How It Works**
```
Physical Device          Virtual Pins          Web Dashboard
     ↓                       ↓                      ↓
  D6 (LED)    ←→    P0 (Virtual)    ←→    Button Widget
  A0 (Sensor) ←→    P1 (Virtual)    ←→    Gauge Widget
  A1 (Sensor) ←→    P2 (Virtual)    ←→    Chart Widget
```

### **The DecentIoT Platform Architecture**
```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Your Device   │    │  Your Firebase  │    │  DecentIoT      │
│   (ESP8266/32)  │◄──►│  Database       │◄──►│  Web Dashboard  │
│                 │    │  (Realtime)     │    │  (Professional) │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

### **Communication Types**
- **📤 Send (Device → Dashboard)**: Use `DECENTIOT_SEND` or `DecentIoT.write()`
- **📥 Receive (Dashboard → Device)**: Use `DECENTIOT_RECEIVE` handlers
- **⏰ Scheduled**: Automatic sending at intervals
- **⚡ Real-time**: Instant commands from dashboard

---

## 🏆 Why Choose DecentIoT?

### **Unique Advantages**
- ✅ **No Vendor Lock-in**: Use your own infrastructure
- ✅ **Professional Dashboard**: Create beautiful, interactive interfaces
- ✅ **Complete Control**: Your data, your servers, your rules
- ✅ **Cost Effective**: No subscription fees, use your own servers
- ✅ **Scalable**: Use Google's enterprise-grade Firebase infrastructure
- ✅ **Flexible**: Connect to any IoT service or platform
- ✅ **Privacy First**: Your data stays on your servers
- ✅ **Open Architecture**: No proprietary limitations

---

## 📚 Basic Usage Examples

### **Example 1: Simple LED Control**
```cpp
// Control LED from cloud
DECENTIOT_RECEIVE(P0)
{
    digitalWrite(LED_PIN, value);
    Serial.printf("LED: %s\n", value ? "ON" : "OFF");
}
```

### **Example 2: Sensor Data Sending**
```cpp
// Send temperature every 10 seconds
DECENTIOT_SEND(P1, 10000)
{
    float temp = readTemperature();
    DecentIoT.write(P1, temp);
    Serial.printf("Temperature: %.2f°C\n", temp);
}
```

### **Example 3: Multiple Sensors**
```cpp
// Temperature sensor
DECENTIOT_SEND(P1, 10000) {
    DecentIoT.write(P1, readTemperature());
}

// Humidity sensor
DECENTIOT_SEND(P2, 15000) {
    DecentIoT.write(P2, readHumidity());
}

// Status updates
DECENTIOT_SEND(P3, 30000) {
    DecentIoT.write(P3, "Device Online");
}
```

### **Example 4: Advanced Control**
```cpp
// LED control
DECENTIOT_RECEIVE(P0) {
    digitalWrite(LED_PIN, value);
}

// RGB control
DECENTIOT_RECEIVE(P1) {
    int brightness = value;
    analogWrite(RGB_PIN, brightness);
}

// Motor control
DECENTIOT_RECEIVE(P2) {
    int speed = value;
    analogWrite(MOTOR_PIN, speed);
}
```

---


### **Board Setup**
```cpp
// For ESP8266
#include <ESP8266WiFi.h>

// For ESP32
#include <WiFi.h>
```

### **Connection Parameters**
```cpp
DecentIoT.begin(
    "https://your-project.firebaseio.com",  // Firebase Database URL
    "your-web-api-key",                     // Firebase Web API Key
    "project-id",                           // Your DecentIoT project identifier
    "user-id",                              // Your DecentIoT user identifier  
    "device-id",                            // Unique device identifier
    "your-email@example.com",               // Firebase auth email
    "your-password"                         // Firebase auth password
);
```

### **Example with Firebase**
```cpp
// Your Firebase project credentials
#define FIREBASE_URL "https://your-project.firebaseio.com"
#define FIREBASE_AUTH "your-web-api-key"
#define AUTH_EMAIL "your-email@example.com"
#define AUTH_PASS "your-password"

// Your DecentIoT project credentials
#define PROJECT_ID "your-project-id"
#define USER_ID "your-user-id"
#define DEVICE_ID "your-device-id"

DecentIoT.begin(FIREBASE_URL, FIREBASE_AUTH, PROJECT_ID, USER_ID, DEVICE_ID, AUTH_EMAIL, AUTH_PASS);
```

---

## 🎯 Virtual Pin Assignment Strategy

### **Virtual Pin Design Philosophy**

**Virtual pins are completely flexible!** You design them based on your project needs:

```
Example Project 1 - Smart Home:
P0  → Living Room Light (bool) - Maps to Switch widget
P1  → Bedroom Temperature (float) - Maps to Gauge widget  
P2  → Kitchen Humidity (float) - Maps to Chart widget
P3  → Garage Door (bool) - Maps to Button widget
P4  → Security Status (string) - Maps to Label widget

Example Project 2 - Weather Station:
P0  → Temperature (float) - Maps to Gauge widget
P1  → Humidity (float) - Maps to Gauge widget
P2  → Pressure (float) - Maps to Value Display widget
P3  → Wind Speed (int) - Maps to Graph widget
P4  → Weather Status (string) - Maps to Label widget

Example Project 3 - Industrial Monitor:
P0  → Machine Status (bool) - Maps to Status widget
P1  → Production Count (int) - Maps to Counter widget
P2  → Temperature (float) - Maps to Gauge widget
P3  → Error Messages (string) - Maps to Terminal widget
P4  → Maintenance Alert (bool) - Maps to Alert widget
```

### **Virtual Pin Best Practices**
- **Design Based on Data Type**: Choose widgets that match your data (bool→Switch, float→Gauge, string→Label)
- **Project-Specific**: Design pins for your specific use case, not predefined categories
- **Consistent Naming**: Use meaningful names in your code comments
- **Document Your Design**: Keep track of your virtual pin to widget mappings
- **Flexible Architecture**: Any pin can be used for any purpose - it's your design!

---

## 🔄 Scheduling System

### **Automatic Data Sending**
```cpp
// Send every 10 seconds
DECENTIOT_SEND(P1, 10000) {
    DecentIoT.write(P1, readSensor());
}

// Send every 30 seconds
DECENTIOT_SEND(P2, 30000) {
    DecentIoT.write(P2, getStatus());
}
```

### **Dynamic Task Management**
```cpp
// Cancel sending when sensor fails
if (sensorFailed) {
    DecentIoT.cancelSend("P1");
}

// Restart when sensor recovers
if (sensorRecovered) {
    DecentIoT.schedule("temp_restart", 10000, []() {
        DecentIoT.write(P1, readTemperature());
    });
}
```

---

## 🛠️ Advanced Features

### **Device Status Monitoring**
```cpp
// Automatic online/offline status
DecentIoT.publishStatus();  // Manually publish status
```

### **Connection Management**
```cpp
// Check connection status
if (DecentIoT.connected()) {
    Serial.println("Connected to cloud");
} else {
    Serial.println("Disconnected from cloud");
}
```

### **Error Handling**
```cpp
DECENTIOT_SEND(P1, 10000) {
    if (sensorAvailable()) {
        float value = readSensor();
        if (!isnan(value)) {
            DecentIoT.write(P1, value);
        } else {
            Serial.println("Sensor reading failed");
        }
    }
}
```

---

## 📖 Documentation Structure

### **Available Guides**
- 📋 **[Introduction.md](Introduction.md)** - This guide (getting started)
- ⏰ **[ScheduleGuidelines.md](ScheduleGuidelines.md)** - Complete scheduling system guide
- 🔧 **Examples** - Ready-to-use code examples
- 📚 **API Reference** - Detailed method documentation

### **Example Projects**
- 🌡️ **Temperature Monitor** - Basic sensor monitoring
- 🏠 **Smart Home Controller** - Multi-device control
- 🚗 **Vehicle Tracker** - GPS and sensor data
- 🏭 **Industrial Monitor** - Advanced sensor network

---

## 🚀 Next Steps

### **1. Try the Examples**
Start with the basic examples in the `examples/` folder:
- `BasicExample/` - Simple LED control
- `DualPortExample/` - Multiple data channels
- `SecureMQTTExample/` - SSL/TLS setup

### **2. Read the Scheduling Guide**
For advanced features, read the **[ScheduleGuidelines.md](ScheduleGuidelines.md)**:
- Recurring tasks
- One-time tasks
- Dynamic task management
- Best practices

### **3. Build Your Project**
- Plan your pin assignments
- Set up your Firebase project
- Implement your handlers
- Test and deploy

---

## 🆘 Getting Help

### **Common Issues**
- **Connection Problems**: Check WiFi and Firebase credentials
- **Data Not Sending**: Verify pin assignments and scheduling
- **Commands Not Working**: Check receive handlers
- **Memory Issues**: Optimize task intervals

### **Support Resources**
- 📖 **Documentation**: Comprehensive guides and examples
- 💬 **Community**: GitHub discussions and issues
- 🐛 **Bug Reports**: Report issues on GitHub
- 💡 **Feature Requests**: Suggest new features

---

## 🎯 Summary

**DecentIoT** is a **revolutionary IoT platform** that gives you the best of both worlds:

✅ **Professional Dashboard** - Create beautiful interfaces like Blynk  
✅ **Your Own Cloud** - Use your own Firebase Realtime Database  
✅ **Virtual Pin Architecture** - Intuitive P0, P1, P2 system  
✅ **No Vendor Lock-in** - Complete control over your data and infrastructure  
✅ **Cost Effective** - No subscription fees, use your own servers  
✅ **Secure Communication** - SSL/TLS encrypted  
✅ **Bidirectional Data** - Send sensors, receive commands  
✅ **Scheduling System** - Automatic data sending  
✅ **Production Ready** - Robust and reliable  

### **The DecentIoT Advantage**
- 🌐 **Dashboard**: Professional web interface with beautiful widgets
- 🔧 **Infrastructure**: Your own Firebase project and Google's servers
- 💰 **Cost**: Free (no subscription fees)
- 🔒 **Privacy**: Your data stays on your servers
- 🚀 **Scalability**: Enterprise-grade infrastructure
- 🎯 **Flexibility**: Connect to any IoT service

**Ready to revolutionize your IoT development?** Get started with DecentIoT today!

---

*For detailed scheduling information, see [ScheduleGuidelines.md](ScheduleGuidelines.md)*
