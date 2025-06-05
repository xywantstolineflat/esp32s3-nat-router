# esp32s3-nat-router

ESP32S3 WiFi NAT Router with client listing, serial deauth, and network monitoring

---

## Project Overview

A simple NAT router implementation for the ESP32-S3 DevKitC-1. This project aims to provide WiFi NAT routing capabilities with features such as device listing, serial deauthentication, and easy configuration—all managed directly on GitHub.

---

## Features & Roadmap

- [x] NAT Router for ESP32-S3 DevKitC-1
- [x] Deauthenticate users via serial monitor
- [ ] Forward traffic from upstream network
- [x] List all connected devices
- [x] Customizable STA List (automatically switch if disconnected/blacklisted)
- [x] Customizable AP SSID and password
- [x] Configure all settings via Serial Monitor
- [ ] Fully develop the project on GitHub (no use of ESP-IDF, esptool, PlatformIO, Arduino IDE, etc.)

---

## Getting Started

### Prerequisites

- ESP32-S3 DevKitC-1 board
- [Optional] USB-to-serial adapter
- WiFi network(s) for testing

### Build & Flash

> ⚠️ This project is designed to be built and managed entirely through GitHub workflows.  
> When available, instructions and automation steps will be added here.

---

## Usage

1. Connect your ESP32-S3 DevKitC-1 to power and serial.
2. Monitor the serial output at 115200 baud.
3. Use the serial monitor to configure SSID, password, and other settings.

---

## Contributing

Pull requests and suggestions are welcome!  
If you have ideas or find bugs, please open an issue.

---

## License

This project is free to use for any purpose.  
However, you must give credit to the original author (Prinsx-py) in any derivative works or distributions.  
See the [LICENSE](LICENSE) file for details.

---

## Credits

Made by a single STE Student from GEANHS, Philippines.

---

## Acknowledgements

Thanks to the ESP32 and open source communities for inspiration and resources.
