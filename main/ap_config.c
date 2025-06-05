#include "nvs_flash.h"
#include "esp_wifi.h"
#include <string.h>

#define NVS_NS_AP "ap_conf"
void ap_config_save(const char *ssid, const char *pass) {
    nvs_handle_t nvs;
    nvs_open(NVS_NS_AP, NVS_READWRITE, &nvs);
    nvs_set_str(nvs, "ap_ssid", ssid);
    nvs_set_str(nvs, "ap_pass", pass);
    nvs_commit(nvs); nvs_close(nvs);
}
void ap_config_load(char *ssid, size_t ssid_len, char *pass, size_t pass_len) {
    nvs_handle_t nvs;
    nvs_open(NVS_NS_AP, NVS_READONLY, &nvs);
    nvs_get_str(nvs, "ap_ssid", ssid, &ssid_len);
    nvs_get_str(nvs, "ap_pass", pass, &pass_len);
    nvs_close(nvs);
}
