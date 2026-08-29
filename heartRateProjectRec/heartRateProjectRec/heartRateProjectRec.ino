/*
 * ============================================================
 * Project      : ESP32 Patient Monitor
 * Author       : Abdullah Abu Halawa
 * Date         : 4-08-2026
 * Version      : 1.0
 * Device       : ESP32 Receiver / WiFi Access Point + MQTT Broker
 * ============================================================
 *
 * Description:
 * ESP32-based patient monitoring receiver that hosts a local
 * WiFi network and MQTT broker, receives real-time patient
 * health data over MQTT, and displays it on a TFT screen.
 *
 * The receiver subscribes to the patientData MQTT topic
 * and displays heart rate, SpO2, and temperature readings.
 *
 * Features:
 * - WiFi Access Point
 * - MQTT Broker
 * - Real-time patient data reception
 * - TFT display
 * - Heart rate display
 * - SpO2 display
 * - Temperature display
 * - Patient health alerts
 * - 30-second standby mode
 * - Animated blue ECG waveform
 *
 * ============================================================
 */


#include <WiFi.h>
#include <PicoMQTT.h>
#include <ArduinoJson.h>
#include "Ucglib.h"

// ============================================================
// TFT DISPLAY
// ============================================================
#define TFT_DC   2
#define TFT_CS   15
#define TFT_RST  4

Ucglib_ILI9341_18x240x320_HWSPI ucg(TFT_DC, TFT_CS, TFT_RST);

// ============================================================
// LOCAL WIFI AP CONFIGURATION
// ============================================================
const char* ap_ssid = "ESP32_Medical_Net";
const char* ap_pass = "123456789";

// ============================================================
// LOCAL MQTT BROKER
// ============================================================
PicoMQTT::Server broker;
const char* topic_telemetry = "patientData";

// ============================================================
// STANDBY SYSTEM & ECG
// ============================================================
const unsigned long STANDBY_TIMEOUT = 30000;
unsigned long lastDataTime = 0;
bool standbyMode = false;

unsigned long lastECGUpdate = 0;
const int ECG_LEFT = 20;
const int ECG_RIGHT = 300;
const int ECG_CENTER_Y = 120;
const int ECG_TOP = 80;
const int ECG_BOTTOM = 160;

int currentX = ECG_LEFT;
int prevY = ECG_CENTER_Y;

// ============================================================
// HELPER FUNCTIONS & DRAWING
// ============================================================
void drawHeart(int x, int y) {
  ucg.setColor(255, 0, 0);
  ucg.drawDisc(x - 5, y - 3, 5, UCG_DRAW_ALL);
  ucg.drawDisc(x + 5, y - 3, 5, UCG_DRAW_ALL);
  ucg.drawTriangle(x - 10, y - 2, x + 10, y - 2, x, y + 10);
}

void drawUIBase() {
  ucg.clearScreen();
  ucg.setFont(ucg_font_ncenR14_tr);
  ucg.setColor(255, 255, 255);
  ucg.setPrintPos(20, 30);
  ucg.print("Patient Monitor");

  ucg.setColor(100, 100, 100);
  ucg.drawLine(10, 40, 310, 40);

  ucg.setFont(ucg_font_6x12_tr);
  ucg.setColor(0, 255, 0);
  ucg.setPrintPos(15, 75);
  ucg.print("Heart Rate (BPM):");

  ucg.setColor(0, 255, 255);
  ucg.setPrintPos(15, 135);
  ucg.print("Oxygen SpO2 (%):");

  ucg.setColor(255, 255, 0);
  ucg.setPrintPos(15, 195);
  ucg.print("Temperature (C):");
}

void drawStandbyBase() {
  ucg.clearScreen();
  ucg.setFont(ucg_font_ncenR14_tr);
  ucg.setColor(255, 255, 255);
  ucg.setPrintPos(90, 30);
  ucg.print("Patient Monitor");

  drawHeart(160, 50);

  ucg.setColor(80, 80, 80);
  ucg.drawLine(10, 68, 310, 68);
  ucg.drawLine(10, 172, 310, 172);

  ucg.setFont(ucg_font_6x12_tr);
  ucg.setColor(255, 255, 255);
  ucg.setPrintPos(95, 195);
  ucg.print("WAITING FOR PATIENT DATA");

  ucg.setColor(120, 120, 120);
  ucg.setPrintPos(125, 215);
  ucg.print("Monitoring...");
}

int getECGY(int x) {
  int phase = (x - ECG_LEFT) % 100;
  if (phase > 20 && phase <= 25) return ECG_CENTER_Y - 5;
  else if (phase > 32 && phase <= 35) return ECG_CENTER_Y + 4;
  else if (phase > 35 && phase <= 42) return ECG_CENTER_Y - 35;
  else if (phase > 42 && phase <= 48) return ECG_CENTER_Y + 20;
  else if (phase > 55 && phase <= 68) return ECG_CENTER_Y - 10;
  return ECG_CENTER_Y;
}

void updateECG() {
  if (millis() - lastECGUpdate < 15) return;
  lastECGUpdate = millis();

  int nextX = currentX + 2;
  if (nextX > ECG_RIGHT) {
    nextX = ECG_LEFT;
    currentX = ECG_LEFT;
    prevY = ECG_CENTER_Y;
  }

  ucg.setColor(0, 0, 0);
  ucg.drawBox(nextX, ECG_TOP, 12, ECG_BOTTOM - ECG_TOP);

  int nextY = getECGY(nextX);
  ucg.setColor(255, 0, 0);
  ucg.drawLine(currentX, prevY, nextX, nextY);

  currentX = nextX;
  prevY = nextY;
}

// ============================================================
// DATA PROCESSING
// ============================================================
void processData(const String& payload) {
  lastDataTime = millis();

  if (standbyMode) {
    standbyMode = false;
    drawUIBase();
  }

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("JSON Parsing failed: ");
    Serial.println(error.f_str());
    return;
  }

  int heartRate = doc["heartRate"];
  int spo2 = doc["spo2"];
  float temp = doc["temp"];

  ucg.setColor(0, 0, 0);
  ucg.drawBox(15, 80, 200, 30);
  ucg.drawBox(15, 140, 200, 30);
  ucg.drawBox(15, 200, 200, 30);
  ucg.drawBox(10, 250, 220, 50);

  ucg.setFont(ucg_font_ncenR18_tr);

  ucg.setColor(0, 255, 0);
  ucg.setPrintPos(25, 105);
  ucg.print(heartRate);

  ucg.setColor(0, 255, 255);
  ucg.setPrintPos(25, 165);
  ucg.print(spo2);

  ucg.setColor(255, 255, 0);
  ucg.setPrintPos(25, 225);
  ucg.print(temp, 1);

  bool isDanger = false;
  String alertText = "";

  if (spo2 < 92 && spo2 > 0) {
    isDanger = true;
    alertText = "LOW SPO2!";
  } else if ((heartRate > 120 || heartRate < 50) && heartRate > 0) {
    isDanger = true;
    alertText = "ABNORMAL HR!";
  } else if (temp > 38.0 || temp < 35.0) {
    isDanger = true;
    alertText = "HIGH TEMP!";
  }

  if (isDanger) {
    ucg.setColor(255, 0, 0);
    ucg.drawBox(10, 250, 220, 45);
    ucg.setFont(ucg_font_ncenR12_tr);
    ucg.setColor(255, 255, 255);
    ucg.setPrintPos(20, 280);
    ucg.print("ALERT: ");
    ucg.print(alertText);
  } else {
    ucg.setColor(0, 150, 0);
    ucg.drawFrame(10, 250, 220, 45);
    ucg.setFont(ucg_font_6x12_tr);
    ucg.setColor(0, 255, 0);
    ucg.setPrintPos(30, 277);
    ucg.print("Status: Normal");
  }
}

// ============================================================
// SETUP
// ============================================================
void setup(void) {
  Serial.begin(115200);
  delay(500);

  ucg.begin(UCG_FONT_MODE_SOLID);
  ucg.setRotate90();

  drawUIBase();
  lastDataTime = millis();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);

  Serial.println("\n--- Local Access Point Started ---");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  broker.subscribe(topic_telemetry, [](const char* topic, const char* payload) {
    Serial.print("[Local MQTT Received] ");
    Serial.println(payload);
    processData(String(payload));
  });

  broker.begin();
}

// ============================================================
// LOOP
// ============================================================
void loop(void) {
  broker.loop(); 

  if (!standbyMode && lastDataTime > 0 && millis() - lastDataTime >= STANDBY_TIMEOUT) {
    standbyMode = true;
    drawStandbyBase();

    currentX = ECG_LEFT;
    prevY = ECG_CENTER_Y;
    lastECGUpdate = millis();
  }

  if (standbyMode) {
    updateECG();
  }
}