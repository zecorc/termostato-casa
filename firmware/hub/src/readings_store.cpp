#include "readings_store.h"

#include <Arduino.h>
#include <string.h>

#include "hub_config.h"

static StoredReading g_readings[TERMO_MAX_SENSORS];

void readingsInit() {
  memset(g_readings, 0, sizeof(g_readings));
}

bool readingsUpsert(const TermoReading& reading, unsigned long now_ms) {
  size_t empty_idx = TERMO_MAX_SENSORS;
  size_t oldest_idx = 0;
  unsigned long oldest_ms = ULONG_MAX;

  for (size_t i = 0; i < TERMO_MAX_SENSORS; ++i) {
    if (g_readings[i].valid &&
        strncmp(g_readings[i].reading.sensor_id, reading.sensor_id,
                TERMO_SENSOR_ID_MAX) == 0) {
      g_readings[i].reading = reading;
      g_readings[i].updated_ms = now_ms;
      return true;
    }
    if (!g_readings[i].valid && empty_idx == TERMO_MAX_SENSORS) {
      empty_idx = i;
    }
    if (g_readings[i].valid && g_readings[i].updated_ms < oldest_ms) {
      oldest_ms = g_readings[i].updated_ms;
      oldest_idx = i;
    }
  }

  size_t idx = empty_idx != TERMO_MAX_SENSORS ? empty_idx : oldest_idx;
  g_readings[idx].reading = reading;
  g_readings[idx].updated_ms = now_ms;
  g_readings[idx].valid = true;
  return true;
}

const StoredReading* readingsFind(const char* sensor_id) {
  for (size_t i = 0; i < TERMO_MAX_SENSORS; ++i) {
    if (g_readings[i].valid &&
        strncmp(g_readings[i].reading.sensor_id, sensor_id, TERMO_SENSOR_ID_MAX) == 0) {
      return &g_readings[i];
    }
  }
  return nullptr;
}

size_t readingsCount() {
  size_t count = 0;
  for (size_t i = 0; i < TERMO_MAX_SENSORS; ++i) {
    if (g_readings[i].valid) {
      ++count;
    }
  }
  return count;
}

const StoredReading* readingsAt(size_t index) {
  size_t seen = 0;
  for (size_t i = 0; i < TERMO_MAX_SENSORS; ++i) {
    if (!g_readings[i].valid) {
      continue;
    }
    if (seen == index) {
      return &g_readings[i];
    }
    ++seen;
  }
  return nullptr;
}

bool readingsGetControl(float* temp_c, unsigned long now_ms, unsigned long* age_ms) {
  const StoredReading* best = nullptr;

  for (size_t i = 0; i < TERMO_MAX_SENSORS; ++i) {
    if (!g_readings[i].valid) {
      continue;
    }
    if (now_ms - g_readings[i].updated_ms > TERMO_SENSOR_TIMEOUT_MS) {
      continue;
    }
    if (!best || g_readings[i].updated_ms > best->updated_ms) {
      best = &g_readings[i];
    }
  }

  if (!best) {
    return false;
  }

  *temp_c = best->reading.temp_c;
  if (age_ms) {
    *age_ms = now_ms - best->updated_ms;
  }
  return true;
}
