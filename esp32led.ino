#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ── WIFI ──────────────────────────────────────────────
const char* WIFI_SSID     = "POCO X6 Pro 5G";
const char* WIFI_PASSWORD = "pocox6pro5g";

// ── SUPABASE ──────────────────────────────────────────
const char* SUPABASE_URL = "https://bcdhzlnmylxcfyweabbu.supabase.co";
const char* SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImJjZGh6bG5teWx4Y2Z5d2VhYmJ1Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzU2NzY0NjcsImV4cCI6MjA5MTI1MjQ2N30.CmAYu0OrcVkyQy36W5Wnr21zv1rbxZfEb_xHrvz7Og4";

// ── SETTINGS ──────────────────────────────────────────
#define LED_PIN        2
#define POLL_INTERVAL  3000

unsigned long lastPollTime = 0;
bool lastLedState = false;

// ═════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("\n=== ESP32 Supabase LED Controller ===");
  connectToWiFi();
}

// ═════════════════════════════════════════════════════
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Disconnected! Reconnecting...");
    connectToWiFi();
    return;
  }

  unsigned long now = millis();
  if (now - lastPollTime >= POLL_INTERVAL) {
    lastPollTime = now;
    fetchLedStatus();
  }
}

// ── WIFI CONNECT ──────────────────────────────────────
void connectToWiFi() {
  Serial.print("[WiFi] Connecting to: ");
  Serial.println(WIFI_SSID);

  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] Connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] FAILED to connect!");
  }
}

// ── FETCH LED STATUS FROM SUPABASE ────────────────────
void fetchLedStatus() {
  // FIX 1: Use WiFiClientSecure correctly
  WiFiClientSecure* client = new WiFiClientSecure;
  if (!client) {
    Serial.println("[ERROR] Failed to create client");
    return;
  }

  // FIX 2: Skip certificate verification (needed for Supabase on ESP32)
  client->setInsecure();

  {
    HTTPClient http;

    String endpoint = String(SUPABASE_URL)
      + "/rest/v1/device_control?id=eq.1&select=led_status";

    Serial.print("[HTTP] Fetching: ");
    Serial.println(endpoint);

    // FIX 3: Pass client pointer + url separately (correct overload)
    http.begin(*client, endpoint);

    // FIX 4: Set timeout to avoid hanging
    http.setTimeout(8000);

    // Headers
    http.addHeader("apikey",        SUPABASE_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
    http.addHeader("Content-Type",  "application/json");

    int httpCode = http.GET();
    Serial.print("[HTTP] Response code: ");
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.print("[Payload] ");
      Serial.println(payload);

      // FIX 5: Use JsonDocument instead of DynamicJsonDocument (ArduinoJson v7)
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        // FIX 6: Safe array access — check if array has at least 1 element
        if (doc.is<JsonArray>() && doc.as<JsonArray>().size() > 0) {
          bool ledStatus = doc[0]["led_status"] | false;

          // FIX 7: Only print/update if state actually changed
          if (ledStatus != lastLedState) {
            lastLedState = ledStatus;
            digitalWrite(LED_PIN, ledStatus ? HIGH : LOW);
            Serial.println(ledStatus ? "[LED] >>> ON <<<" : "[LED] >>> OFF <<<");
          } else {
            Serial.println("[LED] No change.");
          }
        } else {
          Serial.println("[WARN] Empty or unexpected JSON array");
        }
      } else {
        Serial.print("[ERROR] JSON parse failed: ");
        Serial.println(error.c_str());
      }

    } else {
      Serial.print("[HTTP ERROR] Code: ");
      Serial.println(httpCode);
      String errMsg = http.getString();
      if (errMsg.length() > 0) {
        Serial.print("[HTTP Body] ");
        Serial.println(errMsg);
      }
    }

    http.end();
  }

  // FIX 8: Always delete the client to free memory
  delete client;
}
