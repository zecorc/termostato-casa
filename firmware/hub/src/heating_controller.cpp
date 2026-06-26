#include "heating_controller.h"

#include <Arduino.h>

#include "hub_config.h"
#include "pins.h"
#include "readings_store.h"

static TermoHeatMode g_mode = TERMO_MODE_AUTO;
static float g_target_c = TERMO_DEFAULT_TARGET_C;
static float g_hysteresis_c = TERMO_DEFAULT_HYSTERESIS_C;
static bool g_relay_on = false;
static bool g_heat_demand = false;
static unsigned long g_relay_changed_ms = 0;

static void applyRelay(bool on, unsigned long now_ms) {
  if (on == g_relay_on) {
    return;
  }
  g_relay_on = on;
  g_relay_changed_ms = now_ms;
  digitalWrite(TERMO_RELAY_PIN, on ? HIGH : LOW);
}

void heatingInit() {
  pinMode(TERMO_RELAY_PIN, OUTPUT);
  applyRelay(false, millis());
}

void heatingSetMode(TermoHeatMode mode) { g_mode = mode; }

void heatingSetTarget(float target_c) {
  if (target_c >= 5.0f && target_c <= 35.0f) {
    g_target_c = target_c;
  }
}

void heatingSetHysteresis(float hysteresis_c) {
  if (hysteresis_c >= 0.1f && hysteresis_c <= 3.0f) {
    g_hysteresis_c = hysteresis_c;
  }
}

TermoHeatMode heatingGetMode() { return g_mode; }
float heatingGetTarget() { return g_target_c; }
float heatingGetHysteresis() { return g_hysteresis_c; }
bool heatingIsRelayOn() { return g_relay_on; }

void heatingUpdate(unsigned long now_ms) {
  float temp_c = 0.0f;
  unsigned long sensor_age_ms = 0;
  const bool sensor_valid = readingsGetControl(&temp_c, now_ms, &sensor_age_ms);

  switch (g_mode) {
    case TERMO_MODE_MANUAL_ON:
      g_heat_demand = true;
      applyRelay(true, now_ms);
      return;
    case TERMO_MODE_MANUAL_OFF:
      g_heat_demand = false;
      applyRelay(false, now_ms);
      return;
    case TERMO_MODE_AUTO:
      break;
  }

  if (!sensor_valid) {
    g_heat_demand = false;
    applyRelay(false, now_ms);
    return;
  }

  const float on_below = g_target_c - g_hysteresis_c;
  const unsigned long since_change = now_ms - g_relay_changed_ms;

  if (!g_relay_on) {
    g_heat_demand = temp_c < on_below;
    if (g_heat_demand && since_change >= TERMO_MIN_BOILER_OFF_MS) {
      applyRelay(true, now_ms);
    }
  } else {
    g_heat_demand = temp_c < g_target_c;
    if (!g_heat_demand && since_change >= TERMO_MIN_BOILER_ON_MS) {
      applyRelay(false, now_ms);
    }
  }
}

HeatingSnapshot heatingSnapshot(unsigned long now_ms) {
  float temp_c = 0.0f;
  unsigned long sensor_age_ms = 0;
  const bool sensor_valid = readingsGetControl(&temp_c, now_ms, &sensor_age_ms);

  HeatingSnapshot snap{};
  snap.mode = g_mode;
  snap.target_c = g_target_c;
  snap.hysteresis_c = g_hysteresis_c;
  snap.current_temp_c = temp_c;
  snap.sensor_valid = sensor_valid;
  snap.heat_demand = g_heat_demand;
  snap.relay_on = g_relay_on;
  snap.sensor_age_ms = sensor_age_ms;
  return snap;
}
