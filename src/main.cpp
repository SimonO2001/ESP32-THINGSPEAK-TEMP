#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "time.h"

// Wi-Fi credentials
const char* ssid = "E308";
const char* password = "98806829";

// ThingSpeak settings
const char* server = "api.thingspeak.com";
String apiKey = "ZSBLLEJR2OISZZ0V"; // Replace with your actual API Key

// NTP Server
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;        // Adjust for your timezone
const int   daylightOffset_sec = 3600;   // Adjust if daylight saving time

// Temperature Sensor
#define ONE_WIRE_BUS 5
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected.");

  // Initialize NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Initialize temperature sensor
  sensors.begin();
}

void loop() {
  // Request temperature
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC != DEVICE_DISCONNECTED_C)
  {
    // Get current time
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
    }

    // Format time if needed (optional)
    char timeString[64];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);

    // Send data to ThingSpeak
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;

      if (client.connect(server, 80)) {
        String postStr = "api_key=" + apiKey + "&field1=" + String(tempC);

        client.println("POST /update HTTP/1.1");
        client.println("Host: api.thingspeak.com");
        client.println("Connection: close");
        client.println("Content-Type: application/x-www-form-urlencoded");
        client.print("Content-Length: ");
        client.println(postStr.length());
        client.println();
        client.print(postStr);

        Serial.println("Data sent to ThingSpeak:");
        Serial.println(postStr);

        // Read and print the response from ThingSpeak
        while (client.connected() || client.available()) {
          if (client.available()) {
            String line = client.readStringUntil('\n');
            Serial.println(line);
          }
        }
        client.stop();
      } else {
        Serial.println("Connection to ThingSpeak failed.");
      }
    } else {
      Serial.println("WiFi Disconnected");
    }

    // Delay before next reading
    delay(20000); // Wait 20 seconds before sending the next data
  }
  else
  {
    Serial.println("Error: Could not read temperature data");
    delay(5000);
  }
}
