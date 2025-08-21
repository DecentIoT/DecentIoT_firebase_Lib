// this is documnentation of the DecentIoT Library

# DecentIoT Library

A simple and powerful library for connecting IoT devices to Firebase Realtime Database. DecentIoT makes it easy to build IoT projects with real-time data synchronization, working seamlessly with the DecentIoT Dashboard.

## Features

- 🔥 Easy Firebase Realtime Database integration
- 📡 Real-time data synchronization
- 🛠️ Path-based callbacks (matching your dashboard components)
- 📊 Automatic JSON parsing
- ⚡ Optimized for ESP32 and ESP8266
- 🔄 Bi-directional communication with DecentIoT Dashboard

## Philosophy: User-Managed WiFi, Library-Managed Cloud

1. Create your project in the DecentIoT Dashboard
2. Add components (buttons, sliders, gauges, etc.)
3. Get the generated paths for each component
4. Use these paths in your IoT code
5. Everything syncs automatically!

## Installation

1. Download the latest release from GitHub
2. In Arduino IDE: Sketch -> Include Library -> Add .ZIP Library
3. Select the downloaded zip file

DecentIoT leaves WiFi connection and reconnection fully in your control. This gives you maximum flexibility and transparency for your IoT projects. The library only manages the cloud (Firebase) connection.

## Quick Start

```cpp
#include <DecentIoT.h>
#include <ESP8266WiFi.h> // or <WiFi.h> for ESP32

#define FIREBASE_URL "https://your-project.firebasedatabase.app"
#define FIREBASE_AUTH "your-auth-token"
#define PROJECT_ID "your-project-id"
#define DEVICE_ID "your-device-id"
#define AUTH_EMAIL "your@email.com"
#define AUTH_PASS "yourpassword"
#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-wifi-password"

void setup() {
    Serial.begin(115200);
    // User handles WiFi connection
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected!");
    // Now start DecentIoT (cloud connection)
    DecentIoT.begin(FIREBASE_URL, FIREBASE_AUTH, PROJECT_ID, DEVICE_ID, AUTH_EMAIL, AUTH_PASS);
}

void loop() {
    // User checks WiFi and reconnects if needed
    if (WiFi.status() != WL_CONNECTED) {
        // User handles reconnection and re-calls DecentIoT.begin() if needed
    }
    DecentIoT.run();
}
```

## Migration Note

**Migrating from previous versions:**
If you used to pass WiFi credentials to `DecentIoT.begin()`, you now need to connect WiFi yourself in your `setup()` before calling `DecentIoT.begin()`.

## Component Paths

When you add components in the DecentIoT Dashboard, each component gets a unique path:

| Component Type | Example Path                                           | Description         |
| -------------- | ------------------------------------------------------ | ------------------- |
| Toggle Button  | `/project-id/devices/device-id/components/component-id`| Control an LED      |
| Slider         | `/project-id/devices/device-id/components/component-id`| Control brightness  |
| Gauge          | `/project-id/devices/device-id/components/component-id`| Display sensor data |
| Graph          | `/project-id/devices/device-id/components/component-id`| Show data history   |

Use these exact paths in your code to ensure proper communication between your device and dashboard.

## Detailed Usage

### Initialization

```cpp
// User handles WiFi connection
WiFi.begin(WIFI_SSID, WIFI_PASS);
while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
}
Serial.println("WiFi connected!");
// Then initialize DecentIoT
DecentIoT.begin(FIREBASE_URL, FIREBASE_AUTH, PROJECT_ID, DEVICE_ID, AUTH_EMAIL, AUTH_PASS);
```

### Path Callbacks

```cpp
// Digital output example
DECENTIOT_RECEIVE(P0)
{
    digitalWrite(LED_PIN, value);
}

// Analog output example
DECENTIOT_RECEIVE(P1)
{
    analogWrite(LED_PIN, value);
}

// JSON object example
DECENTIOT_RECEIVE(P2)
{
    int r = value["red"].as<int>();
    int g = value["green"].as<int>();
    int b = value["blue"].as<int>();
    setRGBColor(r, g, b);
}
```

### Writing Data

```cpp
// Write single values
DecentIoT.write("P1", 25.5);
DecentIoT.write("P2", true);

// Write strings
DecentIoT.write("P3", "Hello World");
```

### Firebase Setup

1. Create a Firebase project at [Firebase Console](https://console.firebase.google.com/)
2. Enable Realtime Database
3. Set up security rules for your database
4. Get your Firebase project credentials:
   - Database URL
   - Authentication token

Example security rules for testing:

```json
{
  "rules": {
    ".read": true,
    ".write": true
  }
}
```

## Examples

The library comes with several examples:

1. **Basic LED Control**: Control an LED through Firebase
2. **Sensor Monitoring**: Upload sensor data to Firebase
3. **RGB LED Control**: Control RGB LED with JSON data
4. **Multiple Devices**: Manage multiple devices
5. **Secure Communication**: Implementation with security best practices

Check the `examples` folder for complete code.

## Supported Hardware

- ESP32 (all variants)
- ESP8266
- More platforms coming soon...

## Dependencies

- ArduinoJson (>= 6.0.0)
- WiFi library for your board

## Contributing

We welcome contributions! Please feel free to submit a Pull Request.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Support

- Create an issue on GitHub
- Join our Discord community
- Check out our documentation website

## Acknowledgments

- Inspired by the simplicity of Blynk
- Built for the IoT community
- Special thanks to all contributors

---

Made with ❤️ by the DecentIoT Team





## **Current Implementation about Device Status Time:**

```cpp
// In DecentIoT.cpp line 63-70
configTime(0, 0, "pool.ntp.org", "time.nist.gov");
//                    ^^^^ 0 = UTC offset (no timezone adjustment)

// In updateDeviceStatus()
time_t unixTimestamp = time(nullptr);
json.set("t", (unsigned long)unixTimestamp);
```

## **📊 What's Stored in Firebase Database:**

### **Database Value:**
```json
{
  "s": 1,
  "t": 1755201555
}
```

### **What `t=1755201555` means:**
- **Type**: Unix timestamp (seconds since January 1, 1970)
- **Timezone**: **UTC/GMT** (Coordinated Universal Time)
- **Format**: Raw seconds, no timezone conversion

## **🌍 Timezone Confirmation:**

✅ **Device sends**: **UTC time** (Universal Coordinated Time)  
✅ **No local timezone**: Device doesn't know user's location  
✅ **Standard format**: Unix timestamp in UTC  

## **🔧 For Our Web App:**

```javascript
// Convert UTC timestamp to user's local timezone
const utcTimestamp = 1755201555; // From Firebase
const userTimezone = "Asia/Dhaka"; // User selects in webapp

const localTime = new Date(utcTimestamp * 1000).toLocaleString("en-US", {
    timeZone: userTimezone
});
// Result: Local time in user's timezone
```

## **📝 Summary:**
- **Database stores**: UTC Unix timestamps
- **No timezone info**: Pure UTC seconds
- **Web app converts**: Based on user's selected timezone

**Our web app will handle all timezone conversions!** 🎯