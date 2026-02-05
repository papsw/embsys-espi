ESP32 firmware

This repo includes an ESP-IDF firmware node for ESP32 plus host-side C unit tests.

main/ contains the firmware code and small reusable modules (e.g. telemetry buffer)
host_tests/ contains host unit tests you can run without flashing hardware

Hardware flashing and on-device ESP-IDF tests will be added once my ESP32 setup is ready.

Host tests
Build and run the host tests:

gcc -O2 -Wall -Wextra -std=c11 host_tests/test_telemetry_buf.c main/telemetry_buf.c -o host_tests/test_telemetry_buf
./host_tests/test_telemetry_buf

gcc -O2 -Wall -Wextra -std=c11 -I managed_components/espressif__cjson/cJSON host_tests/test_cmd_parse.c main/cmd_parse.c managed_components/espressif__cjson/cJSON/cJSON.c -lm -o host_tests/test_cmd_parse
./host_tests/test_cmd_parse

Firmware build
. ~/esp/esp-idf/export.sh
idf.py set-target esp32
idf.py build
