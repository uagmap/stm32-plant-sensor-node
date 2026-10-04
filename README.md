# STM32 Plant Environment Sensor Node

Learning FreeRTOS by building a small plant/environment node on **Nucleo-F411RE**.
Right now it’s still bare-metal HAL: SHT40 over I2C, on-board button + LED, UART as a stand-in for a future 1602 LCD.

## Pinout

| Function | Nucleo / Arduino | STM32 pin | Wire to |
|----------|------------------|-----------|---------|
| LED LD2 | — | PA5 | on-board |
| User button B1 | — | PC13 | on-board (active low) |
| UART TX (ST-LINK VCP) | — | PA2 | USB serial |
| UART RX | — | PA3 | USB serial |
| I2C1 SCL | D15 | PB8 | SHT40 SCL |
| I2C1 SDA | D14 | PB9 | SHT40 SDA |
| 3.3 V | 3V3 | — | SHT40 Vin |
| GND | GND | — | SHT40 GND |

**SHT40:** I2C `0x44`. Power from **3.3 V** only.

## Serial

```text
115200 8N1  (ST-LINK Virtual COM Port)
```

UART prints a 2-line “page” of an emulated 1602A lcd (title + data).

## Controls (B1)

| Gesture | What it does |
|---------|----------------|
| Short press | Next page |
| Long press (~800 ms) on **MAINT** | One SHT40 heater pulse |

Page order:

```text
LIVE CLIMATE → LIVE AIR → LIVE SOIL → STATUS → MAINT → …
```

- **LIVE CLIMATE** — T + RH (updates ~1 Hz while OK)
- **LIVE AIR / SOIL** — placeholders until more sensors
- **STATUS** — `OK` or `ERR …` (auto-jumps here on a new sensor fault)
- **MAINT** — hold for heater

## LED

- Slow blink — OK  
- Fast blink — sensor error  
- (log-error pattern reserved for later)
