#include "display_ui.h"

#include <TFT_eSPI.h>

#include "protocol.h"

static TFT_eSPI g_tft;

void displayInit() {
  g_tft.init();
  g_tft.setRotation(1);
  g_tft.fillScreen(TFT_BLACK);
}

static void drawLine(int y, const char* text, uint8_t font) {
  g_tft.fillRect(0, y, 320, font == 4 ? 32 : 18, TFT_BLACK);
  g_tft.setTextColor(TFT_WHITE, TFT_BLACK);
  g_tft.drawString(text, 10, y, font);
}

void displayUpdate(const HeatingSnapshot& state, const char* ip_text) {
  char line[64];

  drawLine(8, "Termostato", 4);

  if (state.sensor_valid) {
    snprintf(line, sizeof(line), "Temp: %.1f C", state.current_temp_c);
  } else {
    snprintf(line, sizeof(line), "Temp: -- (no sensor)");
  }
  drawLine(48, line, 2);

  snprintf(line, sizeof(line), "Target: %.1f C", state.target_c);
  drawLine(68, line, 2);

  const char* mode =
      state.mode == TERMO_MODE_AUTO       ? "Mode: AUTO"
      : state.mode == TERMO_MODE_MANUAL_ON ? "Mode: MANUAL ON"
                                           : "Mode: MANUAL OFF";
  drawLine(88, mode, 2);

  snprintf(line, sizeof(line), "Caldaia: %s", state.relay_on ? "ON" : "OFF");
  g_tft.setTextColor(state.relay_on ? TFT_RED : TFT_GREEN, TFT_BLACK);
  drawLine(108, line, 2);
  g_tft.setTextColor(TFT_WHITE, TFT_BLACK);

  if (ip_text && ip_text[0]) {
    snprintf(line, sizeof(line), "IP: %s", ip_text);
    drawLine(136, line, 2);
  }

  snprintf(line, sizeof(line), "API %s", TERMO_API_VERSION);
  drawLine(156, line, 2);
}
