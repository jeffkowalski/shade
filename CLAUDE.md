# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP8266 (NodeMCU) firmware for an Alexa-enabled Somfy roller shade controller. Controls motorized window shades via 433.42MHz radio using the Somfy RTS protocol. Three control interfaces: Alexa voice commands, REST API (port 8080), and serial console (9600 baud).

## Build Commands

```bash
platformio run                       # Build
platformio run --target upload       # Build and flash to device
platformio device monitor            # Serial monitor (9600 baud)
```

Serial port is auto-configured in `platformio.ini`. No test suite or linter is configured.

## Architecture

Single-file firmware (`src/shade.cpp`, ~330 lines) with these subsystems:

- **Somfy Radio** — `SomfyRemote` library sends UP/DOWN/MY/PROGRAM commands over 433.42MHz
- **Alexa Integration** — `weenyMo` library (in `lib/weenymo/`) emulates a Belkin Wemo device on port 80 for SSDP/UPnP discovery. "switch on" = DOWN, "switch off" = UP
- **REST API** — `ESPAsyncWebServer` on port 8080. Key endpoints: `/shade?action=up|down|stop|program`, `/status` (JSON), `/reboot`
- **mDNS** — Advertises as `shade.local` with `_http._tcp` and `_shade-api._tcp` services
- **Serial CLI** — Single-character commands: U (up), D (down), M (my/stop), P (program)
- **Health** — Daily auto-reboot timer, WiFi disconnect detection with restart

## Key Files

- `src/shade.cpp` — All firmware logic
- `src/credentials.h` — WiFi SSID/PSK (gitignored, must be created manually)
- `lib/weenymo/` — Bundled Wemo emulator library for Alexa
- `platformio.ini` — Board config (esp12e), library deps, serial port settings
- `README.org` — Hardware wiring diagrams and protocol research

## Dependencies

Managed via PlatformIO. Key libraries:
- `Somfy_Remote` — Custom fork at github.com/jeffkowalski/Somfy_Remote
- `ESPAsyncWebServer` / `ESPAsyncUDP` — Async networking
- `WiFi` — ESP8266 WiFi stack
