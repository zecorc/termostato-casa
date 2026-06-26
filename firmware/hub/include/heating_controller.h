#pragma once

#include <stdint.h>

enum TermoHeatMode : uint8_t {
  TERMO_MODE_AUTO = 0,
  TERMO_MODE_MANUAL_ON,
  TERMO_MODE_MANUAL_OFF,
};

struct HeatingSnapshot {
  TermoHeatMode mode;
  float target_c;
  float hysteresis_c;
  float current_temp_c;
  bool sensor_valid;
  bool heat_demand;
  bool relay_on;
  unsigned long sensor_age_ms;
};

void heatingInit();
void heatingSetMode(TermoHeatMode mode);
void heatingSetTarget(float target_c);
void heatingSetHysteresis(float hysteresis_c);
TermoHeatMode heatingGetMode();
float heatingGetTarget();
float heatingGetHysteresis();
bool heatingIsRelayOn();

void heatingUpdate(unsigned long now_ms);
HeatingSnapshot heatingSnapshot(unsigned long now_ms);
