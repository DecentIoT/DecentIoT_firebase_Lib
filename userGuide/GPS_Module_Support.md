# Firebase GPS Module Guide

This guide shows how to integrate a GPS module with the **DecentIoT Firebase library**.

## Prerequisites
- **ESP8266** (NodeMCU, Wemos D1 Mini, etc.)
- A GPS module that streams NMEA sentences over a serial interface (e.g., `SoftwareSerial`).
- A Firebase Realtime Database project with the **Database URL** and **Database Secret** (or API key) ready.
- The **DecentIoT Firebase library** already added to your PlatformIO project (`lib_deps` or local copy).

## Wiring
| GPS Pin | ESP8266 Pin |
|---------|-------------|
| VCC     | 3.3 V / Vin (or external 3.3 V regulator) |
| GND     | GND |
| TX      | D5 (GPIO14) – **RX** for ESP8266 |
| RX      | D6 (GPIO12) – **TX** for ESP8266 |

> **Tip:** Power the GPS module from a dedicated 3.3 V regulator if you experience brown‑outs during the SSL handshake.

## Sketch Overview
```cpp
#include <DecentIoT.h>
#include <SoftwareSerial.h>

// ---------- Wi‑Fi ----------
#define WIFI_SSID   "YOUR_SSID"
#define WIFI_PASS   "YOUR_PASSWORD"

// ---------- Firebase ----------
#define FIREBASE_URL   "https://your-project-id.firebaseio.com/"
#define FIREBASE_AUTH  "YOUR_DATABASE_SECRET_OR_API_KEY"
#define PROJECT_ID     "your-project-id"
#define USER_ID        "user-id"
#define DEVICE_ID      "device-id"
#define AUTH_EMAIL     "your@email.com"
#define AUTH_PASS      "yourPassword"

// ---------- GPS ----------
SoftwareSerial gpsSerial(D5, D6); // RX, TX (TX not used)

void setup() {
    Serial.begin(115200);
    gpsSerial.begin(9600);

    // Connect to Wi‑Fi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("[WiFi] Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print('.');
    }
    Serial.println("\n[WiFi] Connected!");

    // ---- NTP time sync (required for TLS) ----
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("[NTP] Syncing time");
    while (time(nullptr) < 1600000000L) { // wait until we have a reasonable epoch (>2020)
        delay(500);
        Serial.print('.');
    }
    Serial.println("\n[NTP] Time synchronized!");

    // Initialize DecentIoT (Firebase backend)
    DecentIoT.begin(FIREBASE_URL, FIREBASE_AUTH,
                    PROJECT_ID, USER_ID, DEVICE_ID,
                    AUTH_EMAIL, AUTH_PASS);

    Serial.println("[GPS] Firebase demo ready!");
}

void loop() {
    // Re‑connect Wi‑Fi if needed
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Disconnected! Reconnecting...");
        WiFi.reconnect();
        delay(5000);
    }

    // Feed raw NMEA bytes into the internal parser
    while (gpsSerial.available() > 0) {
        DecentIoT.feedGPS(gpsSerial.read());
    }

    // Run DecentIoT – handles Firebase uploads and scheduled tasks
    DecentIoT.run();

    // Small delay to avoid hogging the CPU
    delay(10);
}
```

## How It Works
1. **Wi‑Fi connection** – Standard `WiFi.begin()` loop.
2. **NTP sync** – Required so the TLS handshake for Firebase succeeds (the ESP‑8266 starts at 1970).
3. **Firebase initialization** – `DecentIoT.begin()` receives the URL, auth token, project ID, user/device identifiers, and optional email/password for admin actions.
4. **GPS parsing** – The core library parses NMEA sentences internally. You only need to push raw bytes with `feedGPS()`.
5. **Data upload** – The library builds a single pipe‑delimited payload (`lat|lon|alt|speed|time`) and sends it to the datastream you specify (e.g., "GPS"). The data is stored at:
```
/<projectId>/.........../datastreams/<pin>/value
```
where `<pin>` is the identifier passed to `writeGPS`/`feedGPS`. Example payload: `37.774900|-122.419400|15.2|0.0|123456`.
   The upload interval can be tuned with `DecentIoT.setPushInterval(seconds)` if you need a different cadence.

## Common Pitfalls
- **Missing NTP sync** → TLS handshake hangs at `Firebase.begin()`.
- **Incorrect argument order** in `DecentIoT.begin()` – ensure the order matches the signature shown above.
- **Power brown‑outs** – the SSL handshake spikes current. Use a stable 3.3 V source for the GPS module.
- **Firewall / DNS** – the ESP8266 must resolve `*.firebaseio.com` and reach port 443.

## Further Reading
- **MQTT GPS Module guide** – the same parser is used; see `DecentIoT mqtt Lib/userGuide/GPS_Module.md` for deeper parser details.
- **DecentIoT API reference** – `DecentIoT.h` contains all public methods (`writeGPS`, `feedGPS`, `run`, etc.).

---

