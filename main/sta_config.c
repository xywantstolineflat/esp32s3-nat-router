#include "sta_config.h"
#include <string.h>
#include "nvs_flash.h"
#include "esp_wifi.h"

#define NVS_STA_NS "sta_config"

void sta_list_save(const sta_entry_t *list, size_t n) {
    nvs_handle_t nvs;
    nvs_open(NVS_STA_NS, NVS_READWRITE, &nvs);
    nvs_set_blob(nvs, "list", list, sizeof(sta_entry_t)*n);
    nvs_set_u32(nvs, "count", n);
    nvs_commit(nvs);
    nvs_close(nvs);
}

size_t sta_list_load(sta_entry_t *list, size_t max) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_STA_NS, NVS_READONLY, &nvs) != ESP_OK) return 0;
    uint32_t n = 0;
    nvs_get_u32(nvs, "count", &n);
    size_t sz = sizeof(sta_entry_t)*n;
    nvs_get_blob(nvs, "list", list, &sz);
    nvs_close(nvs);
    return n;
}

void sta_blacklist(const char *ssid) {
    sta_entry_t list[MAX_STA_LIST];
    size_t n = sta_list_load(list, MAX_STA_LIST);
    for (size_t i=0; i<n; ++i)
        if (strcmp(list[i].ssid, ssid)==0)
            list[i].blacklisted = true;
    sta_list_save(list, n);
}

void sta_add(const char *ssid, const char *pass) {
    sta_entry_t list[MAX_STA_LIST];
    size_t n = sta_list_load(list, MAX_STA_LIST);
    if (n < MAX_STA_LIST) {
        strcpy(list[n].ssid, ssid);
        strcpy(list[n].pass, pass);
        list[n].blacklisted = false;
        sta_list_save(list, n+1);
    }
}

void sta_del(const char *ssid) {
    sta_entry_t list[MAX_STA_LIST];
    size_t n = sta_list_load(list, MAX_STA_LIST), j=0;
    for (size_t i=0; i<n; ++i)
        if (strcmp(list[i].ssid, ssid) != 0)
            list[j++] = list[i];
    sta_list_save(list, j);
}

void sta_auto_switch() {
    sta_entry_t list[MAX_STA_LIST];
    size_t n = sta_list_load(list, MAX_STA_LIST);
    for (size_t i=0; i<n; ++i) {
        if (!list[i].blacklisted) {
            wifi_config_t cfg = {0};
            strncpy((char*)cfg.sta.ssid, list[i].ssid, sizeof(cfg.sta.ssid));
            strncpy((char*)cfg.sta.password, list[i].pass, sizeof(cfg.sta.password));
            esp_wifi_set_config(WIFI_IF_STA, &cfg);
            esp_wifi_connect();
            break;
        }
    }
}
