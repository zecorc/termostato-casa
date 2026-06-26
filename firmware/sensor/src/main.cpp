#include <Arduino.h>

#include "protocol.h"

#ifndef TERMO_ONEWIRE_PIN
#define TERMO_ONEWIRE_PIN 4  // D4 — DS18B20 data
#endif

void setup() {
  Serial.begin(115200);
  Serial.printf("termostato sensor (%s)\n", TERMO_API_VERSION);
  Serial.printf("onewire GPIO%d, endpoint %s\n", TERMO_ONEWIRE_PIN, TERMO_READINGS_PATH);
}

void loop() {
  delay(1000);
}
