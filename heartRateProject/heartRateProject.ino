/*
 * ============================================================
 * Project      : ESP32 Patient Monitor
 * Author       : Abdullah Abu Halawa
 * Date         : 3-08-2026
 * Version      : 1.0
 * Device       : ESP32 Sender / MQTT Client (Publisher)
 * ============================================================
 *
 * Description:
 * ESP32-based patient monitoring system that collects
 * patient health data and transmits it in real time
 * using MQTT over a local WiFi network.
 *
 * The ESP32 connects to the receiver's local WiFi network
 * and publishes patient data as an MQTT client.
 *
 * Features:
 * - Patient health data collection
 * - MQTT communication (Publisher)
 * - Local WiFi network
 * - Real-time data transmission
 * - JSON formatted patient data
 *
 * ============================================================
 */
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MLX90614.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

// ============================================================
// Display
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// ============================================================
// MAX30102 Settings & Constants
// ============================================================

#define bufferSize 100
#define FINGER_THRESHOLD 50000

#define MIN_NATURAL_HR 60
#define MAX_NATURAL_HR 100

#define NUM_SAMPLES_TO_AVERAGE 3

// ============================================================
// WiFi (Local Access Point Connection)
// ============================================================

const char* ssid = "ESP32_Medical_Net";
const char* password = "123456789";

// ============================================================
// MQTT - Local Server (ESP32 Receiver IP)
// ============================================================

const char* mqttServer = "192.168.4.1";
const int mqttPort = 1883;

const char* device_id = "ESP32_Pro_Unit_01";

const char* topic_telemetry = "patientData";
const char* topic_command = "esp32/unit01/cmd";
const char* topic_status = "esp32/unit01/status";

// ============================================================
// MAX30102 buffers
// ============================================================

uint32_t irBuffer[bufferSize];
uint32_t redBuffer[bufferSize];

int32_t spo2;
int32_t heartRate;

int8_t validspo2;
int8_t validHeartRate;

// ============================================================
// MQTT reset command
// ============================================================

volatile bool resetTimerTriggered = false;

// ============================================================
// Objects
// ============================================================

WiFiClient espClient;
PubSubClient client(espClient);

MAX30105 particleSensor;

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MLX90614 tempSensor = Adafruit_MLX90614();

// ============================================================
// Data structure
// ============================================================

typedef struct structMsg {
    int32_t spo2;
    int32_t heartRate;
    int8_t validspo2;
    int8_t validHeartRate;
    float objectTemp;
    float ambientTemp;
} structMsg;

structMsg collectedData;

// ============================================================
// Display function
// ============================================================

void displayMessage(const char* line1, const char* line2 = "") {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    display.setCursor(0, 20);
    display.println(line1);

    if (strlen(line2) > 0) {
        display.setCursor(0, 35);
        display.println(line2);
    }

    display.display();
}

// ============================================================
// MQTT callback
// ============================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }

    Serial.println();
    Serial.println("========== MQTT MESSAGE ==========");
    Serial.print("Topic: ");
    Serial.println(topic);
    Serial.print("Message: ");
    Serial.println(msg);
    Serial.println("==================================");

    if (String(topic) == topic_command && msg == "RESET_TIMER") {
        Serial.println("[MQTT] RESET_TIMER command received!");
        resetTimerTriggered = true;
    }
}

// ============================================================
// WiFi setup
// ============================================================

void wifiSetup() {
    Serial.println();
    Serial.println("==================================");
    Serial.println("           WIFI SETUP");
    Serial.println("==================================");
    Serial.print("Connecting to AP: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        attempts++;

        if (attempts >= 60) {
            Serial.println("\n[WiFi] Connection failed!");
            return;
        }
    }

    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
}

// ============================================================
// MQTT connection
// ============================================================

void mqttConnect() {
    while (!client.connected()) {
        Serial.print("[MQTT] Connecting to Local Receiver Broker...");
        if (client.connect(device_id, topic_status, 1, true, "Offline")) {
            Serial.println(" Connected!");
            client.publish(topic_status, "Online", true);
            client.subscribe(topic_command);
        } else {
            Serial.print(" Failed, rc=");
            Serial.println(client.state());
            delay(5000);
        }
    }
}

// ============================================================
// I2C Scanner
// ============================================================

void scanI2C() {
    byte error;
    int devices = 0;
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("[I2C] Device found at 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
            devices++;
        }
    }
}

// ============================================================
// Setup
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Wire.begin(21, 22);
    scanI2C();

    wifiSetup();

    client.setServer(mqttServer, mqttPort);
    client.setCallback(mqttCallback);

    if (!tempSensor.begin()) {
        Serial.println("[MLX90614] ERROR: Sensor NOT found!");
    }

    if (!display.begin(0x3C, true)) {
        while (1) delay(1000);
    }

    displayMessage("Initializing...", "Please wait");
    delay(1000);

    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        displayMessage("MAX30102 Error!", "Check wiring");
        while (1) delay(1000);
    }

    particleSensor.setup(0x1F, 4, 2, 400, 411, 4096); 
    particleSensor.setPulseAmplitudeRed(0x1F);
    particleSensor.setPulseAmplitudeIR(0x1F);

    displayMessage("Ready!", "Place your finger");
}

// ============================================================
// Loop
// ============================================================

void loop() {
    if (WiFi.status() != WL_CONNECTED) wifiSetup();
    if (!client.connected()) mqttConnect();
    client.loop();

    while (!particleSensor.available()) {
        particleSensor.check();
        client.loop();
    }

    uint32_t irValue = particleSensor.getIR();
    particleSensor.nextSample();

    if (irValue < FINGER_THRESHOLD) {
        displayMessage("Place your finger");
        delay(200);
        return;
    }

    displayMessage("Finger detected", "Measuring...");
    delay(500);

 
    int32_t totalHR = 0;
    int32_t totalSpO2 = 0;
    int validReadingsCount = 0;

    for (int run = 0; run < NUM_SAMPLES_TO_AVERAGE; run++) {
        displayMessage("Scanning...", "Hold Still");

        for (int i = 0; i < bufferSize; i++) {
            while (!particleSensor.available()) {
                particleSensor.check();
                client.loop();
            }
            redBuffer[i] = particleSensor.getRed();
            irBuffer[i] = particleSensor.getIR();
            particleSensor.nextSample();
        }

        maxim_heart_rate_and_oxygen_saturation(
            irBuffer, bufferSize, redBuffer,
            &spo2, &validspo2, &heartRate, &validHeartRate
        );

        if (validHeartRate && validspo2 && heartRate >= MIN_NATURAL_HR && heartRate <= MAX_NATURAL_HR) {
            totalHR += heartRate;
            totalSpO2 += spo2;
            validReadingsCount++;
        }
    }

 
    if (validReadingsCount == 0) {
        Serial.println("[MAX30102] ERROR: Invalid or out-of-range readings!");
        
        displayMessage("Incorrect Reading", "Retrying test...");
        delay(2500);
        return;
    }

    int32_t avgHeartRate = totalHR / validReadingsCount;
    int32_t avgSpO2 = totalSpO2 / validReadingsCount;

    float objectTemp = tempSensor.readObjectTempC();
    float ambientTemp = tempSensor.readAmbientTempC();

    collectedData.heartRate = avgHeartRate;
    collectedData.spo2 = avgSpO2;
    collectedData.objectTemp = objectTemp;
    collectedData.ambientTemp = ambientTemp;
    collectedData.validHeartRate = 1;
    collectedData.validspo2 = 1;

   

    String payload = "{";
    payload += "\"heartRate\":" + String(collectedData.heartRate) + ",";
    payload += "\"spo2\":" + String(collectedData.spo2) + ",";
    payload += "\"temp\":" + String(collectedData.objectTemp, 2) + ",";
    payload += "\"ambientTemp\":" + String(collectedData.ambientTemp, 2);
    payload += "}";

    if (client.publish(topic_telemetry, payload.c_str())) {
        displayMessage("Data Sent!", "MQTT OK");
    } else {
        displayMessage("Publish Failed!", "Check MQTT");
    }

    delay(2000);

   

    resetTimerTriggered = false;

    for (int secondsLeft = 30; secondsLeft > 0; secondsLeft--) {
        if (WiFi.status() != WL_CONNECTED) wifiSetup();
        if (!client.connected()) mqttConnect();
        client.loop();

        if (resetTimerTriggered) {
            resetTimerTriggered = false;
            break;
        }

        char timeStr[10];
        sprintf(timeStr, "%02d:%02d", secondsLeft / 60, secondsLeft % 60);
        displayMessage("Next test in:", timeStr);

        delay(1000);
    }

    displayMessage("Place your finger");
    delay(500);
}