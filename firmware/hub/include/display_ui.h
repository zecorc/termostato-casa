#pragma once

#include "heating_controller.h"

void displayInit();
void displayUpdate(const HeatingSnapshot& state, const char* ip_text);
