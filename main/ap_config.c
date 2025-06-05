#include "ap_config.h"
#include <string.h>
#include "nvs_flash.h"

#define NVS_AP_NAMESPACE "ap_config"

void ap_config_save(const char *ssid, const char *password) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_AP_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "password", password);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

void ap_config_load(char *ssid, size_t ssid_len, char *password, size_t pass_len) {
    nvs_handle_t nvs;
    strcpy(ssid, "ESP32-NAT");
    strcpy(password, "esp32pass");
    if (nvs_open(NVS_AP_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        nvs_get_str(nvs, "ssid", ssid, &ssid_len);
        nvs_get_str(nvs, "password", password, &pass_len);
        nvs_close(nvs);
    }
}
