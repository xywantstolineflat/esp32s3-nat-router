#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "esp_spiffs.h"
#include "ap_config.h"
#include "sta_config.h"

#define AP_CHANNEL   1
#define MAX_STA_CONN 8

static const char *TAG = "esp32nat";

// --- CORS ---
esp_err_t add_cors_headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    return ESP_OK;
}

// --- /api/clients ---
esp_err_t api_clients_handler(httpd_req_t *req) {
    add_cors_headers(req);
    wifi_sta_list_t sta_list;
    tcpip_adapter_sta_list_t ip_list;
    esp_wifi_ap_get_sta_list(&sta_list);
    tcpip_adapter_get_sta_list(&sta_list, &ip_list);
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < ip_list.num; ++i) {
        cJSON *cli = cJSON_CreateObject();
        char ip[16], mac[18];
        sprintf(ip, IPSTR, IP2STR(&ip_list.sta[i].ip));
        sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
            ip_list.sta[i].mac[0], ip_list.sta[i].mac[1], ip_list.sta[i].mac[2],
            ip_list.sta[i].mac[3], ip_list.sta[i].mac[4], ip_list.sta[i].mac[5]);
        cJSON_AddStringToObject(cli, "ip", ip);
        cJSON_AddStringToObject(cli, "mac", mac);
        cJSON_AddItemToArray(root, cli);
    }
    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    cJSON_Delete(root);
    free(json);
    return ESP_OK;
}

// --- /api/kick ---
esp_err_t api_kick_handler(httpd_req_t *req) {
    add_cors_headers(req);
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = 0;
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }
    const char *mac = cJSON_GetObjectItem(root, "mac")->valuestring;
    uint8_t mac_bin[6];
    sscanf(mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
        &mac_bin[0], &mac_bin[1], &mac_bin[2], &mac_bin[3], &mac_bin[4], &mac_bin[5]);
    esp_wifi_deauth_sta(mac_bin);
    ESP_LOGI(TAG, "Deauthed %s", mac);
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// --- /api/ap-config ---
esp_err_t ap_config_get_handler(httpd_req_t *req) {
    add_cors_headers(req);
    char ssid[33], pass[65];
    ap_config_load(ssid, sizeof(ssid), pass, sizeof(pass));
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ssid", ssid);
    cJSON_AddStringToObject(root, "password", pass);
    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    cJSON_Delete(root); free(json);
    return ESP_OK;
}
esp_err_t ap_config_post_handler(httpd_req_t *req) {
    add_cors_headers(req);
    char buf[128]={0};
    httpd_req_recv(req, buf, sizeof(buf));
    cJSON *root = cJSON_Parse(buf);
    if (!root) return httpd_resp_send_400(req);
    const char *ssid = cJSON_GetObjectItem(root, "ssid")->valuestring;
    const char *password = cJSON_GetObjectItem(root, "password")->valuestring;
    ap_config_save(ssid, password);
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// --- /api/sta-list ---
esp_err_t sta_list_get_handler(httpd_req_t *req) {
    add_cors_headers(req);
    sta_entry_t list[MAX_STA_LIST];
    size_t n = sta_list_load(list, MAX_STA_LIST);
    cJSON *arr = cJSON_CreateArray();
    for (size_t i=0; i<n; ++i) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "ssid", list[i].ssid);
        cJSON_AddBoolToObject(obj, "blacklisted", list[i].blacklisted);
        cJSON_AddItemToArray(arr, obj);
    }
    char *json = cJSON_PrintUnformatted(arr);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    cJSON_Delete(arr); free(json);
    return ESP_OK;
}

// --- /api/sta-list (POST) ---
esp_err_t sta_list_post_handler(httpd_req_t *req) {
    add_cors_headers(req);
    char buf[128]={0};
    httpd_req_recv(req, buf, sizeof(buf));
    cJSON *root = cJSON_Parse(buf);
    if (!root) return httpd_resp_send_400(req);
    const char *action = cJSON_GetObjectItem(root, "action")->valuestring;
    const char *ssid = cJSON_GetObjectItem(root, "ssid")->valuestring;
    if (strcmp(action,"add")==0) {
        const char *pass = cJSON_GetObjectItem(root, "password")->valuestring;
        sta_add(ssid, pass);
    } else if (strcmp(action,"del")==0) {
        sta_del(ssid);
    } else if (strcmp(action,"blacklist")==0) {
        sta_blacklist(ssid);
    }
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// --- Serve index.html from SPIFFS ---
esp_err_t index_get_handler(httpd_req_t *req) {
    FILE *f = fopen("/spiffs/index.html", "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    char line[256];
    httpd_resp_set_type(req, "text/html");
    while (fgets(line, sizeof(line), f)) {
        httpd_resp_sendstr_chunk(req, line);
    }
    fclose(f);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

// --- SPIFFS Init ---
void spiffs_init(void) {
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);
}

// --- WiFi Event Handler ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Client connected: "MACSTR, MAC2STR(event->mac));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Client disconnected: "MACSTR, MAC2STR(event->mac));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "STA disconnected, auto-switching...");
        sta_auto_switch();
    }
}

// --- WiFi Init (AP+STA) ---
void wifi_init_softap(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_any_id);

    char ap_ssid[33], ap_pass[65];
    ap_config_load(ap_ssid, sizeof(ap_ssid), ap_pass, sizeof(ap_pass));
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = "",
            .ssid_len = 0,
            .channel = AP_CHANNEL,
            .password = "",
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    strncpy((char*)wifi_ap_config.ap.ssid, ap_ssid, sizeof(wifi_ap_config.ap.ssid));
    strncpy((char*)wifi_ap_config.ap.password, ap_pass, sizeof(wifi_ap_config.ap.password));
    wifi_ap_config.ap.ssid_len = strlen(ap_ssid);
    if (strlen(ap_pass) == 0) {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_config_t wifi_sta_config = {0};
    sta_entry_t list[MAX_STA_LIST];
    size_t n = sta_list_load(list, MAX_STA_LIST);
    if (n > 0) {
        strncpy((char*)wifi_sta_config.sta.ssid, list[0].ssid, sizeof(wifi_sta_config.sta.ssid));
        strncpy((char*)wifi_sta_config.sta.password, list[0].pass, sizeof(wifi_sta_config.sta.password));
    }

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);
    esp_wifi_start();
    ESP_LOGI(TAG, "WiFi AP+STA started. AP SSID:%s PASS:%s", ap_ssid, ap_pass);
}

// --- NAT Setup (IDF v4.1+ has built-in) ---
void nat_enable(void) {
    esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_nat_enable(ap_netif);
    ESP_LOGI(TAG, "NAT enabled");
}

// --- HTTP Server Setup ---
void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    httpd_start(&server, &config);

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &index_uri);

    httpd_uri_t api_clients_uri = {
        .uri = "/api/clients",
        .method = HTTP_GET,
        .handler = api_clients_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &api_clients_uri);

    httpd_uri_t api_kick_uri = {
        .uri = "/api/kick",
        .method = HTTP_POST,
        .handler = api_kick_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &api_kick_uri);

    httpd_uri_t ap_cfg_get = {
        .uri = "/api/ap-config",
        .method = HTTP_GET,
        .handler = ap_config_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ap_cfg_get);

    httpd_uri_t ap_cfg_post = {
        .uri = "/api/ap-config",
        .method = HTTP_POST,
        .handler = ap_config_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ap_cfg_post);

    httpd_uri_t sta_list_get = {
        .uri = "/api/sta-list",
        .method = HTTP_GET,
        .handler = sta_list_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &sta_list_get);

    httpd_uri_t sta_list_post = {
        .uri = "/api/sta-list",
        .method = HTTP_POST,
        .handler = sta_list_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &sta_list_post);
}

// --- Serial Task for CLI ---
void serial_task(void *arg) {
    char line[128];
    while (1) {
        fgets(line, sizeof(line), stdin);
        if (strncmp(line, "list", 4) == 0) {
            wifi_sta_list_t sta_list;
            tcpip_adapter_sta_list_t ip_list;
            esp_wifi_ap_get_sta_list(&sta_list);
            tcpip_adapter_get_sta_list(&sta_list, &ip_list);
            printf("Connected clients:\n");
            for (int i = 0; i < ip_list.num; ++i) {
                printf("%02X:%02X:%02X:%02X:%02X:%02X %d.%d.%d.%d\n",
                    ip_list.sta[i].mac[0], ip_list.sta[i].mac[1], ip_list.sta[i].mac[2],
                    ip_list.sta[i].mac[3], ip_list.sta[i].mac[4], ip_list.sta[i].mac[5],
                    IP2STR(&ip_list.sta[i].ip));
            }
        } else if (strncmp(line, "kick ", 5) == 0) {
            uint8_t mac[6];
            sscanf(line+5, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
            esp_wifi_deauth_sta(mac);
            printf("Kicked %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        } else if (strncmp(line, "setap ", 6) == 0) {
            char ssid[33], pass[65];
            if (sscanf(line+6, "%32s %64s", ssid, pass) == 2) {
                ap_config_save(ssid, pass);
                printf("AP config saved! Reboot to apply.\n");
            } else {
                printf("Usage: setap <ssid> <password>\n");
            }
        } else if (strncmp(line, "addsta ", 7) == 0) {
            char ssid[33], pass[65];
            if (sscanf(line+7, "%32s %64s", ssid, pass) == 2) {
                sta_add(ssid, pass);
                printf("STA profile added.\n");
            } else {
                printf("Usage: addsta <ssid> <password>\n");
            }
        } else if (strncmp(line, "delsta ", 7) == 0) {
            char ssid[33];
            if (sscanf(line+7, "%32s", ssid) == 1) {
                sta_del(ssid);
                printf("STA profile deleted.\n");
            } else {
                printf("Usage: delsta <ssid>\n");
            }
        } else if (strncmp(line, "blsta ", 6) == 0) {
            char ssid[33];
            if (sscanf(line+6, "%32s", ssid) == 1) {
                sta_blacklist(ssid);
                printf("STA profile blacklisted.\n");
            } else {
                printf("Usage: blsta <ssid>\n");
            }
        }
    }
}

void app_main(void)
{
    nvs_flash_init();
    spiffs_init();
    wifi_init_softap();
    nat_enable();
    start_webserver();
    xTaskCreate(serial_task, "serial_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "ESP32S3 NAT Router Project Started");
}
