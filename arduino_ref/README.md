# arduino_ref — ESP32-C6 test sketch

Files:

- `esp32c6_test.ino` — minimal blink + serial test for ESP32-C6 WROOM
- `.vscode/settings.json` — points VS Code to `arduino-cli` and default FQBN
- `.vscode/tasks.json` — tasks to compile, upload, and monitor

Build & upload from the project folder:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32c6 .
arduino-cli upload -p /dev/cu.usbmodem1201 --fqbn esp32:esp32:esp32c6 .
arduino-cli monitor -p /dev/cu.usbmodem1201 -b 115200
```
