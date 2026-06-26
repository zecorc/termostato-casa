#pragma once

#include <stddef.h>

#include "protocol.h"

struct StoredReading {
  TermoReading reading;
  unsigned long updated_ms;
  bool valid;
};

void readingsInit();
bool readingsUpsert(const TermoReading& reading, unsigned long now_ms);
const StoredReading* readingsFind(const char* sensor_id);
size_t readingsCount();
const StoredReading* readingsAt(size_t index);

// Latest valid reading used for heating control (MVP: most recently updated).
bool readingsGetControl(float* temp_c, unsigned long now_ms, unsigned long* age_ms);
