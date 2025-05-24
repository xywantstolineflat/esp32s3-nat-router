#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

void app_main(void)
{
    // Init NVS
    nvs_flash_init();
    // Init TCP/IP
    tcpip_adapter_init();
    // Init event loop
    esp_event_loop_create_default();

    // TODO: Add WiFi AP+STA init, NAT config, serial commands, client listing, etc.
    printf("ESP32S3 NAT Router Project Started\n");
}
