#include <DecentIoT.h>
#include <WiFi.h>

// Firebase credentials
#define FIREBASE_URL "your-firebase-url"
#define FIREBASE_AUTH "your-web-api-key"
#define AUTH_EMAIL "your-auth-email"
#define AUTH_PASS "your-auth-password"
#define PROJECT_ID "your-project-id"
#define USER_ID "your-user-id"
#define DEVICE_ID "your-device-id"
// WiFi credentials
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASS "your-wifi-password"

// Pin definitions
const int ledPin = 2;

// LED state handler
DECENTIOT_RECEIVE(P0)
{
  digitalWrite(ledPin, value);
  Serial.printf("[P0] LED state = %s\n", value ? "ON" : "OFF");
}


void setup()
{
  Serial.begin(115200);
  Serial.println("\n--- Initializing DecentIoT Device ---");

  // Initialize LED pin
  pinMode(ledPin, OUTPUT);  
  digitalWrite(ledPin, LOW); // Ensure LED is off initially


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

  // Now start DecentIoT (Firebase connection)
  DecentIoT.begin(FIREBASE_URL, FIREBASE_AUTH, PROJECT_ID, USER_ID, DEVICE_ID, AUTH_EMAIL, AUTH_PASS);
}

void loop()
{
  DecentIoT.run();
  delay(10);
}