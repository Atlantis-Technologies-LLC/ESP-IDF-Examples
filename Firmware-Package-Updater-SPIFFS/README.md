# Firmware Package Updater (SPIFFS Edition)

This example uses Python to create a firmware upate package that includes the .bin firmware file and any files within a set folder.

## Features
- **WiFi Access Point:**
  - Hosts a network called `ESP32` (last four digits of MAC address)
  - Default password: `12345678`
  - Default webpage IP address: `192.168.4.1`
- **Firmware Update Utility:**
  - Update firmware via `/firmware-update.html`
  - Requires a firmware package provided the Python script

---

## Creating a Firmware Update Package
To generate a firmware update package, run:
```sh
python create_firmware_update_package.py build/Firmware-Package-Updater-SPIFFS.bin html/ update_v0.0.1.pkg
```
from the project's root directory.

The package file combines the project's .bin firmware file and any files in the html/ directory.
Once uploaded, the web_server.c package handler writes the firmware via OTA and moves the html/ files to the SPIFFS storage partition.

---

## Versioning
- **ESP-IDF, Compiler, and OS Versioning is tracked in `build_version.txt`**

---