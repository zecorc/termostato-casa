#pragma once

#include <stdint.h>

#define TERMO_API_VERSION "v1"
#define TERMO_READINGS_PATH "/api/v1/readings"

#define TERMO_SENSOR_ID_MAX 32
#define TERMO_ROOM_NAME_MAX 32

struct TermoReading {
  char sensor_id[TERMO_SENSOR_ID_MAX];
  char room[TERMO_ROOM_NAME_MAX];
  float temp_c;
  float humidity;
  int8_t battery_pct;
  int8_t rssi;
};
