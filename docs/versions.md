# Version and transport evidence

Verified 2026-11-02; pins are deliberate compatibility choices, not claims of being
latest releases.

- [PlatformIO platform manifest at v6.10.0](https://github.com/platformio/platform-espressif32/blob/v6.10.0/platform.json)
  selects framework `~3.20017.0`; this repository narrows it to `3.20017.0`.
- [PlatformIO Core 6.1.18 release metadata](https://pypi.org/project/platformio/6.1.18/)
  and the successfully executed build establish the selected Core version.
- Actual downloaded `cores/esp32/esp_arduino_version.h`: 2.0.17.
- Actual downloaded `tools/sdk/esp32c3/include/esp_common/include/esp_idf_version.h`:
  4.4.7. The platform's optional standalone IDF package is not the Arduino SDK.
- Actual bundled `tools/sdk/esp32c3/include/esp_wifi/include/esp_now.h`:
  `ESP_NOW_MAX_TOTAL_PEER_NUM=20`, encrypted limit 6, maximum payload 250 bytes;
  receive callback `(const uint8_t*, const uint8_t*, int)`.
  [Tagged IDF API](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32c3/api-reference/network/esp_now.html).
  Eight devices need at most seven other member unicast peers plus broadcast = 8
  local entries. Even conservatively counting eight plus broadcast = 9 fits 20.
  No encrypted peers are used. Compile-time assertions check limits. The peer
  cache evicts unicast entries when full; membership is independent of that cache.
- Actual bundled `variants/XIAO_ESP32C3/pins_arduino.h`: D2=GPIO4, D3=GPIO5.
  Avoid GPIO2/8/9 strapping pins for the external button, particularly held at boot.
- [Seeed board guide](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/)
  and [schematic](https://files.seeedstudio.com/wiki/XIAO_WiFi/Resources/Seeeduino-XIAO-ESP32C3-SCH.pdf)
  are hardware references. Exact purchased board revision still needs inspection.
- [uv environment management](https://docs.astral.sh/uv/pip/environments/)
  documents managed interpreters and isolated environments. CI uses uv 0.9.26.

No third-party Arduino libraries are added. Python dependencies are exact-version
pins. PlatformIO's platform supplies target tool dependencies, resolved locally as
RISC-V toolchain 8.4.0+2021r2-patch5, esptool 1.40501.0, SCons 4.40801.0.
These tool package versions are explicitly pinned in `platformio.ini`.
