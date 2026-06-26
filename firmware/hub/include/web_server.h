#pragma once

#include <WebServer.h>

bool webInit();
void webLoop();
const char* webIpText();

WebServer& webServer();
