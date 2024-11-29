#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Existing libraries
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "time.h"

// OLED display settings
#define SCREEN_WIDTH 128 // OLED width in pixels
#define SCREEN_HEIGHT 32 // OLED height in pixels
#define OLED_RESET    -1 // Reset pin (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Wi-Fi credentials
const char* ssid = "Oksen";
const char* password = "!55Oksen6792";

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

// LED Pin
#define LED_PIN 4 // GPIO4 connected to the LED

void setup() {
  Serial.begin(115200);

  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Ensure LED is off initially

  // Initialize the OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // 0x3C is the I2C address for the display
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Loop forever if initialization fails
  }
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi...");
  display.println("Connecting to Wi-Fi...");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.print(".");
    display.display();
  }
  Serial.println(" Connected.");
  display.println("Connected.");
  display.display();

  // Initialize NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Initialize temperature sensor
  sensors.begin();

  delay(1000); // Wait for a moment
  display.clearDisplay();
}

void loop() {
  // Request temperature
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  // Get current time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }

  // Format time
  char timeString[64];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);

  if (tempC != DEVICE_DISCONNECTED_C)
  {
    // Display temperature and time on OLED
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Temp: ");
    display.print(tempC);
    display.println(" C");
    display.println(timeString);
    display.display();

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

        // Flash the LED for 500 milliseconds
        digitalWrite(LED_PIN, HIGH); // Turn LED on
        delay(500);                  // Wait for 500 milliseconds
        digitalWrite(LED_PIN, LOW);  // Turn LED off

      } else {
        Serial.println("Connection to ThingSpeak failed.");
        display.println("Failed to send data.");
        display.display();
      }
    } else {
      Serial.println("WiFi Disconnected");
      display.println("WiFi Disconnected");
      display.display();
    }

    // Delay before next reading
    delay(20000); // Wait 20 seconds before sending the next data
  }
  else
  {
    Serial.println("Error: Could not read temperature data");
    display.clearDisplay();
    display.println("Temp Sensor Error");
    display.display();
    delay(5000);
  }
}
