#include <Arduino.h>

#include "display_ui.h"
#include "heating_controller.h"
#include "protocol.h"
#include "readings_store.h"
#include "web_server.h"

static unsigned long g_last_display_ms = 0;
static unsigned long g_last_heat_ms = 0;

void setup() {
  Serial.begin(115200);
  Serial.printf("termostato hub (%s)\n", TERMO_API_VERSION);

  displayInit();
  readingsInit();
  heatingInit();

  displayUpdate(heatingSnapshot(millis()), "WiFi...");

  if (!webInit()) {
    return;
  }

  displayUpdate(heatingSnapshot(millis()), webIpText());
}

void loop() {
  const unsigned long now_ms = millis();

  webLoop();

  if (now_ms - g_last_heat_ms >= 1000UL) {
    g_last_heat_ms = now_ms;
    heatingUpdate(now_ms);
  }

  if (now_ms - g_last_display_ms >= 2000UL) {
    g_last_display_ms = now_ms;
    displayUpdate(heatingSnapshot(now_ms), webIpText());
  }
}
