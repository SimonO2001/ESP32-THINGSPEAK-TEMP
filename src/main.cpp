#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiManager.h> // Include WiFiManager
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <SPIFFS.h>
#include <esp_sleep.h>

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ThingSpeak settings
const char* server = "api.thingspeak.com";
String apiKey = "ZSBLLEJR2OISZZ0V";

// Temperature and Humidity Sensors
#define ONE_WIRE_BUS 5
#define DHTPIN 33
#define DHTTYPE DHT11

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DHT dht(DHTPIN, DHTTYPE);

// LED and Button Pins
#define LED_READINGS 23
#define LED_PIN 4
#define BUTTON_SLEEP 12  // GPIO12 for deep sleep button
#define BUTTON_PRINT 14  // GPIO14 for printing last 10 entries

// Debounce variables
unsigned long lastPressTimeSleep = 0;
unsigned long lastPressTimePrint = 0;
const unsigned long debounceDelay = 200; // 200 milliseconds

// ThingSpeak timing
unsigned long lastThingSpeakTime = 0;
const unsigned long thingSpeakInterval = 20000;

// Data collection timing
unsigned long lastDataCollectionTime = 0;
const unsigned long dataCollectionInterval = 1000; // 1 second
int bufferIndex = 0;
bool collectingData = false;

// Data buffer
float tempBuffer[10] = {0};
float humBuffer[10] = {0};

// Function prototypes
void handleDeepSleepButton();
void handlePrintButton();
void handleDataCollection();
void handleThingSpeak();
void printLastEntries();
void updateOLED(float temp, float hum, const String &timestamp);

void setup() {
  Serial.begin(115200);

  // Configure LED and button pins
  pinMode(LED_READINGS, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_SLEEP, INPUT_PULLUP);
  pinMode(BUTTON_PRINT, INPUT_PULLUP);
  digitalWrite(LED_READINGS, LOW);
  digitalWrite(LED_PIN, LOW);

  // Check if waking up from deep sleep
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Woke up from deep sleep!");
    delay(1000); // Allow time for button to be released
  } else {
    Serial.println("Normal boot");
  }

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
    while (1);
  }

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();

  // Use WiFiManager to manage Wi-Fi connection
  WiFiManager wifiManager;

  // Uncomment to reset saved Wi-Fi credentials (for testing)
  // wifiManager.resetSettings();

  if (!wifiManager.autoConnect("ESP32_WiFiManager")) {
    Serial.println("Failed to connect to Wi-Fi.");
    while (1);
  }

  Serial.println("Connected to Wi-Fi.");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Wi-Fi Connected:");
  display.println(WiFi.SSID());
  display.display();

  // Initialize sensors
  sensors.begin();
  dht.begin();

  // Configure the sleep button as a wake-up source
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_SLEEP, 0); // Wake up when button is pressed
}

void loop() {
  handleDeepSleepButton();
  handlePrintButton();
  handleDataCollection();
  handleThingSpeak();
}

void handleDeepSleepButton() {
  if (digitalRead(BUTTON_SLEEP) == LOW) {
    unsigned long currentTime = millis();
    if (currentTime - lastPressTimeSleep > debounceDelay) {
      lastPressTimeSleep = currentTime;

      Serial.println("Button pressed, entering deep sleep...");
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Sleeping...");
      display.display();

      delay(500); // Stabilize
      esp_deep_sleep_start();
    }
  }
}

void handlePrintButton() {
  if (digitalRead(BUTTON_PRINT) == LOW) {
    unsigned long currentTime = millis();
    if (currentTime - lastPressTimePrint > debounceDelay) {
      lastPressTimePrint = currentTime;

      Serial.println("Button pressed, printing last 10 entries...");
      printLastEntries();
    }
  }
}

void handleDataCollection() {
  if (millis() - lastDataCollectionTime >= dataCollectionInterval) {
    lastDataCollectionTime = millis();

    if (bufferIndex < 10) {
      sensors.requestTemperatures();
      float temp = sensors.getTempCByIndex(0);
      float hum = dht.readHumidity();

      tempBuffer[bufferIndex] = temp;
      humBuffer[bufferIndex] = hum;

      String timestamp = String(millis() / 1000) + "s"; // Example timestamp in seconds

      digitalWrite(LED_READINGS, HIGH);
      delay(100);
      digitalWrite(LED_READINGS, LOW);

      updateOLED(temp, hum, timestamp);

      bufferIndex++;
    } else if (!collectingData) {
      collectingData = true;

      float avgTemp = 0, avgHum = 0;
      for (int i = 0; i < 10; i++) {
        avgTemp += tempBuffer[i];
        avgHum += humBuffer[i];
      }
      avgTemp /= 10;
      avgHum /= 10;

      File file = SPIFFS.open("/data.txt", FILE_APPEND);
      if (file) {
        file.printf("Avg Temp: %.2f C, Avg Hum: %.2f %%\n", avgTemp, avgHum);
        file.close();
        Serial.println("Data saved to SPIFFS");
      } else {
        Serial.println("Failed to open file for writing");
      }

      bufferIndex = 0;
      collectingData = false;
    }
  }
}

void handleThingSpeak() {
  if (millis() - lastThingSpeakTime >= thingSpeakInterval) {
    lastThingSpeakTime = millis();

    float avgTemp = 0, avgHum = 0;
    for (int i = 0; i < 10; i++) {
      avgTemp += tempBuffer[i];
      avgHum += humBuffer[i];
    }
    avgTemp /= 10;
    avgHum /= 10;

    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      if (client.connect(server, 80)) {
        String postStr = "api_key=" + apiKey + "&field1=" + String(avgTemp) + "&field2=" + String(avgHum);

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

        client.stop();
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
      } else {
        Serial.println("Failed to connect to ThingSpeak");
      }
    } else {
      Serial.println("WiFi disconnected");
    }
  }
}

void printLastEntries() {
  if (SPIFFS.exists("/data.txt")) {
    File file = SPIFFS.open("/data.txt", FILE_READ);
    Serial.println("Reading last 10 entries:");

    String lines[10];
    int lineCount = 0;

    while (file.available()) {
      String line = file.readStringUntil('\n');
      lines[lineCount % 10] = line;
      lineCount++;
    }

    int start = max(0, lineCount - 10);
    for (int i = start; i < lineCount; i++) {
      Serial.println(lines[i % 10]);
    }

    file.close();
  } else {
    Serial.println("data.txt does not exist.");
  }
}

void updateOLED(float temp, float hum, const String &timestamp) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Temp: ");
  display.print(temp);
  display.println(" C");
  display.print("Hum: ");
  display.print(hum);
  display.println(" %");
  display.print("Time: ");
  display.println(timestamp);
  display.display();
}
