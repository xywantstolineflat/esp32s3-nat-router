// AP config: setap <ssid> <pass>
if (strncmp(line, "setap ", 6)==0) {
    char ssid[33], pass[65];
    sscanf(line+6, "%32s %64s", ssid, pass);
    ap_config_save(ssid, pass);
    printf("AP config saved! Reboot to apply.\n");
}
// Add/blacklist/del STA: addsta <ssid> <pass>, delsta <ssid>, blsta <ssid>
if (strncmp(line,"addsta ",7)==0) { /* parse, add to sta_list, save */ }
if (strncmp(line,"blsta ",6)==0) { sta_blacklist(line+6); }
