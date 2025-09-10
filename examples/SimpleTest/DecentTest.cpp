#include <Arduino.h>
#include <DecentIoT.h>

// Firebase credentials
#define FIREBASE_URL "https://test-database-5d47e-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "AIzaSyC4_BzsQuXEdn2JDI6e9USWi6dmZL56EGc"
#define AUTH_EMAIL "test@user.com"
#define AUTH_PASS "testuser"
#define PROJECT_ID "689fb23987401e09a38f7e19"
#define USER_ID "689fb21187401e09a38f7e15"
#define DEVICE_ID "689fb2c787401e09a38f7e2b"

#define WIFI_SSID "victim trash"
#define WIFI_PASS "123456789"
const int ledPin = D6; // D6 = GPIO12 on ESP8266
const int rgbPin = D7; // D7 = GPIO13 on ESP8266

// LED state handler
DECENTIOT_RECEIVE(P0)
{
  digitalWrite(ledPin, value);
  Serial.printf("[P0] LED state = %s\n", value ? "ON" : "OFF");
}

DECENTIOT_SEND(P1, 60*1000)
{
  // Generate random temperature value between 0-100
  int temperature = random(0, 101); // random(min, max) generates numbers from min to max-1
  DecentIoT.write(P1, temperature); // Send temperature to Firebase
  Serial.printf("[P1] Temperature = %d\n", temperature );
}

DECENTIOT_SEND(P2, 60*1000)
{
  // Generate random humidity value between 0-100
  int humidity = random(0, 101); // random(min, max) generates numbers from min to max-1
  DecentIoT.write(P2, humidity); // Send humidity to Firebase
  Serial.printf("[P2] Humidity = %d\n", humidity );
}

/*
DECENTIOT_RECEIVE(P3)
{
  digitalWrite(rgbPin, value);
  Serial.printf("[P3] RGB Brightness = %2d\n", value );
}*/

DECENTIOT_RECEIVE(P3)
{
  int sliderValue = value;
  int pwmValue = sliderValue * 255 / 100;
  analogWrite(rgbPin, pwmValue);
  Serial.printf("[P3] RGB Brightness = %d (slider: %d)\n", pwmValue, sliderValue);
}


void setup()
{
  Serial.begin(115200);
  Serial.println("\n--- Initializing DecentIoT Device ---");

  // Initialize LED pin
  pinMode(ledPin, OUTPUT);
  pinMode(rgbPin, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(ledPin, LOW); // Ensure LED is off initially
  digitalWrite(LED_BUILTIN, LOW);

  // User handles WiFi connection
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  Serial.println("WiFi connected!");
  digitalWrite(LED_BUILTIN, HIGH);

  // Now start DecentIoT (cloud connection)
  DecentIoT.begin(FIREBASE_URL, FIREBASE_AUTH, PROJECT_ID, USER_ID, DEVICE_ID, AUTH_EMAIL, AUTH_PASS);
}

void loop()
{
  // User checks WiFi and reconnects if needed
  if (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("[WiFi] Disconnected! Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    // Optionally: wait for connection, show status, etc.
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
      delay(500);
      Serial.print(".");
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("WiFi reconnected! Restarting device...");
      delay(1000);
      ESP.restart();
    }
  }

  DecentIoT.run();
  delay(10);
}
