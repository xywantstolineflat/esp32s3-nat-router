#pragma once
#include <stddef.h>

void ap_config_save(const char *ssid, const char *password);
void ap_config_load(char *ssid, size_t ssid_len, char *password, size_t pass_len);
