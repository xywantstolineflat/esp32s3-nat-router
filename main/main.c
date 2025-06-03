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

#define AP_SSID      "ESP32-NAT"
#define AP_PASS      "esp32pass"
#define AP_CHANNEL   1
#define MAX_STA_CONN 8

static const char *TAG = "esp32nat";

// --- Add CORS headers for API endpoints (optional, for fetch from browser) ---
esp_err_t add_cors_headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    return ESP_OK;
}

// --- /api/clients: List connected clients (with CORS) ---
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

// --- /api/kick: Deauth client by MAC ---
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

// --- Serve style.css from SPIFFS ---
esp_err_t css_get_handler(httpd_req_t *req) {
    FILE *f = fopen("/spiffs/style.css", "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    char line[256];
    httpd_resp_set_type(req, "text/css");
    while (fgets(line, sizeof(line), f)) {
        httpd_resp_sendstr_chunk(req, line);
    }
    fclose(f);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
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

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = AP_CHANNEL,
            .password = AP_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(AP_PASS) == 0) {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = "",
            .password = ""
        }
    };

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);
    esp_wifi_start();
    ESP_LOGI(TAG, "WiFi AP+STA started. AP SSID:%s PASS:%s", AP_SSID, AP_PASS);
}

// --- NAT Setup (IDF v4.1+ has built-in) ---
void nat_enable(void) {
    // For ESP-IDF v4.1+ only
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

    httpd_uri_t css_uri = {
        .uri = "/style.css",
        .method = HTTP_GET,
        .handler = css_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &css_uri);

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
}

// --- Serial Task for CLI ---
void serial_task(void *arg) {
    char line[64];
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
        }
        // Add more commands as needed
    }
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
