#include "web_server.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <math.h>
#include <string.h>

#include "heating_controller.h"
#include "hub_config.h"
#include "protocol.h"
#include "readings_store.h"

static WebServer g_server(80);
static WiFiManager g_wifiManager;
static char g_ip_text[16] = "";

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Termostato</title>
  <style>
    body { font-family: system-ui, sans-serif; margin: 1.5rem; background: #111; color: #eee; }
    h1 { margin-top: 0; }
    .card { background: #222; border-radius: 12px; padding: 1rem 1.2rem; margin-bottom: 1rem; }
    .big { font-size: 2.4rem; font-weight: 700; }
    .on { color: #f66; }
    .off { color: #6f6; }
    label { display: block; margin: 0.8rem 0 0.3rem; }
    input, select, button { font-size: 1rem; padding: 0.5rem; border-radius: 8px; border: 1px solid #444; background: #1a1a1a; color: #eee; }
    button { margin-top: 1rem; cursor: pointer; background: #2a5; border: none; color: #fff; }
    .muted { color: #999; font-size: 0.9rem; }
    ul { padding-left: 1.2rem; }
  </style>
</head>
<body>
  <h1>Termostato</h1>
  <div class="card">
    <div class="muted">Temperatura attuale</div>
    <div class="big" id="temp">--</div>
    <div id="boiler" class="big off">OFF</div>
    <div class="muted" id="meta"></div>
  </div>
  <div class="card">
    <label for="target">Target (&deg;C)</label>
    <input id="target" type="number" step="0.5" min="5" max="35">
    <label for="mode">Modalit&agrave;</label>
    <select id="mode">
      <option value="auto">Auto</option>
      <option value="manual_on">Manuale ON</option>
      <option value="manual_off">Manuale OFF</option>
    </select>
    <button id="save">Salva</button>
  </div>
  <div class="card">
    <div class="muted">Sensori</div>
    <ul id="sensors"></ul>
  </div>
  <script>
    async function refresh() {
      const res = await fetch('/api/v1/status');
      const s = await res.json();
      document.getElementById('temp').textContent =
        s.sensor_valid ? s.current_temp_c.toFixed(1) + ' \u00b0C' : 'Nessun sensore';
      const boiler = document.getElementById('boiler');
      boiler.textContent = s.relay_on ? 'CALDAIA ON' : 'CALDAIA OFF';
      boiler.className = 'big ' + (s.relay_on ? 'on' : 'off');
      document.getElementById('meta').textContent =
        'Target ' + s.target_c.toFixed(1) + ' \u00b0C | ' + s.mode + ' | ' + (s.ip || '');
      document.getElementById('target').value = s.target_c;
      document.getElementById('mode').value = s.mode;
      const list = document.getElementById('sensors');
      list.innerHTML = '';
      (s.sensors || []).forEach(function(item) {
        const li = document.createElement('li');
        li.textContent = item.room + ': ' + item.temp_c.toFixed(1) + ' \u00b0C (' + item.sensor_id + ')';
        list.appendChild(li);
      });
    }
    document.getElementById('save').addEventListener('click', async function() {
      await fetch('/api/v1/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          target_c: parseFloat(document.getElementById('target').value),
          mode: document.getElementById('mode').value
        })
      });
      refresh();
    });
    refresh();
    setInterval(refresh, 5000);
  </script>
</body>
</html>
)rawliteral";

static const char* modeToString(TermoHeatMode mode) {
  switch (mode) {
    case TERMO_MODE_AUTO:
      return "auto";
    case TERMO_MODE_MANUAL_ON:
      return "manual_on";
    case TERMO_MODE_MANUAL_OFF:
      return "manual_off";
  }
  return "auto";
}

static bool parseMode(const char* value, TermoHeatMode* mode) {
  if (!value) {
    return false;
  }
  if (strcmp(value, "auto") == 0) {
    *mode = TERMO_MODE_AUTO;
    return true;
  }
  if (strcmp(value, "manual_on") == 0) {
    *mode = TERMO_MODE_MANUAL_ON;
    return true;
  }
  if (strcmp(value, "manual_off") == 0) {
    *mode = TERMO_MODE_MANUAL_OFF;
    return true;
  }
  return false;
}

static void sendJson(int code, const String& body) {
  g_server.send(code, "application/json", body);
}

static void handleIndex() {
  g_server.send_P(200, "text/html", INDEX_HTML);
}

static void handleStatus() {
  const unsigned long now_ms = millis();
  const HeatingSnapshot state = heatingSnapshot(now_ms);

  JsonDocument doc;
  doc["api_version"] = TERMO_API_VERSION;
  doc["mode"] = modeToString(state.mode);
  doc["target_c"] = state.target_c;
  doc["hysteresis_c"] = state.hysteresis_c;
  doc["relay_on"] = state.relay_on;
  doc["heat_demand"] = state.heat_demand;
  doc["sensor_valid"] = state.sensor_valid;
  doc["current_temp_c"] = state.current_temp_c;
  doc["sensor_age_s"] = state.sensor_valid ? (state.sensor_age_ms / 1000UL) : 0;
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();

  JsonArray sensors = doc["sensors"].to<JsonArray>();
  for (size_t i = 0; i < readingsCount(); ++i) {
    const StoredReading* item = readingsAt(i);
    if (!item) {
      continue;
    }
    JsonObject row = sensors.add<JsonObject>();
    row["sensor_id"] = item->reading.sensor_id;
    row["room"] = item->reading.room;
    row["temp_c"] = item->reading.temp_c;
    row["age_s"] = (now_ms - item->updated_ms) / 1000UL;
  }

  String body;
  serializeJson(doc, body);
  sendJson(200, body);
}

static void handleReadingsPost() {
  if (g_server.method() != HTTP_POST) {
    sendJson(405, "{\"error\":\"method not allowed\"}");
    return;
  }

  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, g_server.arg("plain"));
  if (err) {
    sendJson(400, "{\"error\":\"invalid json\"}");
    return;
  }

  TermoReading reading{};
  const char* sensor_id = doc["sensor_id"] | "";
  const char* room = doc["room"] | "";
  if (sensor_id[0] == '\0') {
    sendJson(400, "{\"error\":\"sensor_id required\"}");
    return;
  }
  if (!doc["temp_c"].is<float>() && !doc["temp_c"].is<int>()) {
    sendJson(400, "{\"error\":\"temp_c required\"}");
    return;
  }

  strncpy(reading.sensor_id, sensor_id, TERMO_SENSOR_ID_MAX - 1);
  strncpy(reading.room, room, TERMO_ROOM_NAME_MAX - 1);
  reading.temp_c = doc["temp_c"].as<float>();
  reading.humidity = doc["humidity"] | NAN;
  reading.battery_pct = doc["battery_pct"] | int8_t(-1);
  reading.rssi = doc["rssi"] | int8_t(0);

  readingsUpsert(reading, millis());
  sendJson(200, "{\"ok\":true}");
}

static void handleConfigPost() {
  if (g_server.method() != HTTP_POST) {
    sendJson(405, "{\"error\":\"method not allowed\"}");
    return;
  }

  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, g_server.arg("plain"));
  if (err) {
    sendJson(400, "{\"error\":\"invalid json\"}");
    return;
  }

  if (doc["target_c"].is<float>() || doc["target_c"].is<int>()) {
    heatingSetTarget(doc["target_c"].as<float>());
  }
  if (doc["hysteresis_c"].is<float>() || doc["hysteresis_c"].is<int>()) {
    heatingSetHysteresis(doc["hysteresis_c"].as<float>());
  }
  if (doc["mode"].is<const char*>()) {
    TermoHeatMode mode = TERMO_MODE_AUTO;
    if (parseMode(doc["mode"], &mode)) {
      heatingSetMode(mode);
    }
  }

  sendJson(200, "{\"ok\":true}");
}

static void handleNotFound() {
  sendJson(404, "{\"error\":\"not found\"}");
}

static bool startMdns() {
  if (!MDNS.begin(TERMO_MDNS_HOSTNAME)) {
    Serial.println("mDNS start failed");
    return false;
  }
  MDNS.addService("http", "tcp", 80);
  Serial.printf("mDNS: http://%s.local\n", TERMO_MDNS_HOSTNAME);
  return true;
}

bool webInit() {
  g_wifiManager.setConfigPortalTimeout(180);
  g_wifiManager.setConnectTimeout(20);

  if (!g_wifiManager.autoConnect(TERMO_WIFI_AP_NAME)) {
    Serial.println("WiFi setup failed, restarting...");
    delay(3000);
    ESP.restart();
    return false;
  }

  WiFi.localIP().toString().toCharArray(g_ip_text, sizeof(g_ip_text));
  Serial.printf("WiFi connected: %s (%s)\n", g_ip_text, WiFi.SSID().c_str());

  startMdns();

  g_server.on("/", HTTP_GET, handleIndex);
  g_server.on("/api/v1/status", HTTP_GET, handleStatus);
  g_server.on(TERMO_READINGS_PATH, HTTP_POST, handleReadingsPost);
  g_server.on("/api/v1/config", HTTP_POST, handleConfigPost);
  g_server.onNotFound(handleNotFound);
  g_server.begin();

  Serial.println("HTTP server started");
  return true;
}

void webLoop() { g_server.handleClient(); }

WebServer& webServer() { return g_server; }

const char* webIpText() { return g_ip_text; }
