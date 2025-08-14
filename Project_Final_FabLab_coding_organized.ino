#include <WiFi.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <DHTesp.h>
#include <HTTPClient.h> // Include the HTTPClient library

/* -- Pin Definitions -- */
#define SMOKE_SENSOR_PIN 18
#define BUZZER_PIN 17
#define LED_PIN 14
#define LED_COUNT 12 // Set this to the number of LEDs you have
#define SMOKE_THRESHOLD 700

#define DHT_PIN 13
#define BUTTON_PIN 6

#define RainDropPin 15

unsigned long lastDebounceTime = 0;  
unsigned long debounceDelay = 200;

/* -- WiFi and MQTT Credentials -- */
const char* ssid = "";
const char* password = "";

const char* mqttServer = "";
const int mqttPort = ;
const char* mqttUsername = ""; // ThingsBoard Token
const char* mqttTopic = "";

/* -- Telegram Credentials -- */
const char* botToken1 = "";
const char* chatIDbot1 = "";

// const char* botToken2 = "";
// const char* chatIDbot2 = "";

/* -- Hardware Objects -- */
WiFiClient espClient;
PubSubClient client(espClient);

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
DHTesp dht;

/* -- Global Variables -- */
bool whiteLightState = false; // White light state

/* -- Setup Function -- */
void setup() {
  Serial.begin(115200);
  initializePins();
  initializeWiFi();
  initializeMQTT();
  initializeDHT();
  strip.begin();
  strip.show(); // Initialize the NeoPixel strip
}

/* -- Main Loop Function -- */
void loop() {
  // Functions that run continuously, non-blocking
  checkSmokeLevel();          // Check smoke levels
  handleButtonPress();        // Handle button presses
  sendTemperatureHumidity();
  rainDetection();

  if (!client.connected()) {
    reconnectMQTT();           // Reconnect MQTT if disconnected
  }
  client.loop();               // Handle MQTT client loop
  delay(100);                  // Optional delay to control loop speed (if needed)
}

/******************************************
 * INITIALIZATION FUNCTIONS
 ******************************************/
void initializePins() {
  pinMode(SMOKE_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void initializeWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();

  // Attempt to connect for 5 seconds
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.println("IP Address: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nFailed to connect to WiFi. Skipping WiFi setup.");
    // Serial.println("Skipping connect to MQTT.");

  }
}

void initializeMQTT() {
  client.setServer(mqttServer, mqttPort);
}

void initializeDHT() {
  dht.setup(DHT_PIN, DHTesp::DHT22);
}

/******************************************
 * SMOKE DETECTION
 ******************************************/
void checkSmokeLevel() {
  int smokeLevel = analogRead(SMOKE_SENSOR_PIN); // Use analogRead instead of digitalRead
  Serial.print("Smoke Level: ");
  Serial.println(smokeLevel);

  if (smokeLevel > SMOKE_THRESHOLD) {
    Serial.println("Smoke detected!");
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1000);
    sendToTelegram(botToken1, chatIDbot1, "SMOKE ALERT! Sensor Value: " + String(smokeLevel));
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    Serial.println("No smoke detected.");
  }
}

// Function to read average moisture over a set number of readings
int readAverageMoisture(int numReadings = 10) {
  long sum = 0;
  for (int i = 0; i < numReadings; i++) {
    sum += analogRead(RainDropPin);
    delay(50); // Short delay between readings
  }
  return sum / numReadings;  // Return the average value
}

void rainDetection(){
   // Read the average moisture level
  int moistureLevel = readAverageMoisture();
  String moistureString = String(moistureLevel);  // Convert moisture level to string

  // Read wet/dry state from digital pin (D0)
  int isWet = digitalRead(RainDropPin);
  String rainState = (isWet == LOW ? "YES" : "NO");

  // Calculate percentage based on moisture reading (adjust based on your calibration)
  int percentage = map(moistureLevel, 0, 4095, 0, 100);  // Convert analog value to a percentage

  // If percentage is more than 8%, consider it raining
  if (percentage < 75) {
    rainState = "Raining";
  } else {
    rainState = "Not Raining";
  }
  // Create JSON Payload for ThingsBoard
  String payload = "{\"moistureLevel\":" + moistureString + ", \"rainDetected\":\"" + rainState + "\", \"moisturePercentage\":" + String(percentage) + "}";
  Serial.println("Publishing to MQTT Topic: " + String(mqttTopic));
  Serial.println(payload);  // Debugging print to see the exact payload

  // // Send telemetry data to ThingsBoard
  if (client.publish(mqttTopic, payload.c_str())) {
    Serial.println("Data published successfully.");
  } else {
    Serial.println("Failed to publish data.");
  }
  // Print the data to the serial monitor for debugging
  Serial.print("Moisture Level (Analog): ");
  Serial.println(moistureLevel);  // Moisture value ranges from 0 (wet) to 4095 (dry)
  Serial.print("Rain Detected (Digital): ");
  Serial.println(rainState);  // "Raining" or "Not Raining"
  Serial.print("Moisture Percentage: ");
  Serial.println(percentage);  // Moisture percentage for more clarity
  // Delay for readability and avoid overwhelming the MQTT broker
  delay(2500);  
  }
  
/******************************************
 * TEMPERATURE & HUMIDITY MONITORING
 ******************************************/
void sendTemperatureHumidity() {
  float temperature = dht.getTemperature();
  float humidity = dht.getHumidity();
  if (!isnan(temperature) && !isnan(humidity)) {
    String payload = "{\"temperature\": " + String(temperature) + ", \"humidity\": " + String(humidity) + "}";
   // client.publish(mqttTopic, payload.c_str());
    Serial.println("Telemetry sent: " + payload);
  }else {
    Serial.println("Failed to read DHT sensor!");
  }
}

/******************************************
 * BUTTON PRESS HANDLER
 ******************************************/
void handleButtonPress() {
  if (buttonPressed()) { // Check if the button is pressed
    whiteLightState = !whiteLightState;

    Serial.println("Button Pressed");
    Serial.print("White Light State: ");
    Serial.println(whiteLightState ? "ON" : "OFF");

    if (whiteLightState) {
      setLEDColor(255, 255, 255); // Turn on white light for all LEDs
      Serial.println("White light turned ON");
    } else {
      setLEDColor(0, 0, 0); // Turn off all LEDs
      Serial.println("White light turned OFF");
    }
  }
}

bool buttonPressed() {
  // Check button press logic, assuming active LOW button
  return digitalRead(BUTTON_PIN) == LOW;
}

/******************************************
 * LED CONTROL
 ******************************************/
void setLEDColor(int r, int g, int b) {
  // Loop through all LEDs and set their color
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show(); // Update the LEDs
}

/******************************************
 * HELPERS
 ******************************************/
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Reconnecting to MQTT...");
    if (client.connect("ESP32_Client", mqttUsername, "")) {
      Serial.println("Connected!");
    } else {
      Serial.print("Failed. State: ");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void sendToTelegram(const char* botToken, const char* chatID, String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage?chat_id=" + String(chatID) + "&text=" + message;
    http.begin(url); // Start the request
    int httpCode = http.GET(); // Send the GET request

    if (httpCode > 200) {
      Serial.println("Telegram message sent!");
    } else {
      Serial.println("Error sending Telegram message: " + String(httpCode));
    }

    http.end(); // End the HTTP request
  }
}