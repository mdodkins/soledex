#pragma once

#include <string>

namespace pokedex {

// Bring up WiFi station mode and connect to (ssid, password), blocking until
// connected or timed out. On success returns true and writes the assigned IPv4
// address into ip_out. NVS must be initialised before calling.
bool wifiConnect(const char* ssid, const char* password, std::string& ip_out,
                 int timeout_ms = 20000);

}  // namespace pokedex
