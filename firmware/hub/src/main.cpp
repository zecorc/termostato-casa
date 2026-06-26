#include <Arduino.h>
#include <TFT_eSPI.h>

#include "pins.h"
#include "protocol.h"

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  pinMode(TERMO_RELAY_PIN, OUTPUT);
  digitalWrite(TERMO_RELAY_PIN, LOW);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Termostato", 10, 10, 4);
  tft.drawString(String("API ") + TERMO_API_VERSION, 10, 50, 2);
  tft.drawString("Relay OFF", 10, 80, 2);

  Serial.printf("termostato hub (%s)\n", TERMO_API_VERSION);
}

void loop() {
  delay(1000);
}
