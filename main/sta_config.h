#pragma once
#include <stddef.h>
#include <stdbool.h>

#define MAX_STA_LIST 5

typedef struct {
    char ssid[33], pass[65];
    bool blacklisted;
} sta_entry_t;

void sta_list_save(const sta_entry_t *list, size_t n);
size_t sta_list_load(sta_entry_t *list, size_t max);
void sta_auto_switch();
void sta_blacklist(const char *ssid);
void sta_add(const char *ssid, const char *pass);
void sta_del(const char *ssid);
