#include "nvs_flash.h"
#include "esp_wifi.h"
#include <string.h>

#define MAX_STA 5
#define NVS_NS "sta_conf"

typedef struct {
    char ssid[33];
    char pass[65];
    bool blacklisted;
} sta_entry_t;

void sta_list_save(sta_entry_t *list, size_t n) {
    nvs_handle_t nvs;
    nvs_open(NVS_NS, NVS_READWRITE, &nvs);
    nvs_set_blob(nvs, "sta_list", list, sizeof(sta_entry_t)*n);
    nvs_set_u32(nvs, "sta_count", n);
    nvs_commit(nvs);
    nvs_close(nvs);
}

size_t sta_list_load(sta_entry_t *list, size_t max) {
    nvs_handle_t nvs;
    size_t n = max, out = 0;
    nvs_open(NVS_NS, NVS_READONLY, &nvs);
    nvs_get_u32(nvs, "sta_count", (uint32_t*)&n);
    size_t sz = sizeof(sta_entry_t)*n;
    nvs_get_blob(nvs, "sta_list", list, &sz);
    out = sz/sizeof(sta_entry_t);
    nvs_close(nvs);
    return out;
}

void sta_blacklist(const char *ssid) {
    sta_entry_t list[MAX_STA];
    size_t n = sta_list_load(list, MAX_STA);
    for (int i=0; i<n; ++i) if (strcmp(list[i].ssid, ssid)==0) list[i].blacklisted=true;
    sta_list_save(list, n);
}

void sta_autoswitch() {
    sta_entry_t list[MAX_STA];
    size_t n = sta_list_load(list, MAX_STA);
    for (int i=0; i<n; ++i) {
        if (!list[i].blacklisted) {
            wifi_config_t sta_cfg = {0};
            strncpy((char*)sta_cfg.sta.ssid, list[i].ssid, sizeof(sta_cfg.sta.ssid));
            strncpy((char*)sta_cfg.sta.password, list[i].pass, sizeof(sta_cfg.sta.password));
            esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
            esp_wifi_connect();
            break;
        }
    }
}
