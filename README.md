# STM32 Plant Environment Sensor Node

Learning FreeRTOS by building a small plant/environment node on **Nucleo-F411RE**.
Right now it’s still bare-metal HAL: SHT40 and BH1750 on one I2C bus, on-board button + LED, UART as a stand-in for a future 1602 LCD.

## Pinout

| Function | Nucleo / Arduino | STM32 pin | Wire to |
|----------|------------------|-----------|---------|
| LED LD2 | — | PA5 | on-board |
| User button B1 | — | PC13 | on-board (active low) |
| UART TX (ST-LINK VCP) | — | PA2 | USB serial |
| UART RX | — | PA3 | USB serial |
| I2C1 SCL | D15 | PB8 | SHT40 SCL, BH1750 SCL |
| I2C1 SDA | D14 | PB9 | SHT40 SDA, BH1750 SDA |
| 3.3 V | 3V3 | — | both modules Vin |
| GND | GND | — | both modules GND |
| BH1750 ADDR | — | — | GND (`0x23`) |

**SHT40:** I2C `0x44`. **BH1750:** I2C `0x23`. Power from **3.3 V** only.

## Serial

```text
115200 8N1  (ST-LINK Virtual COM Port)
```

UART prints a 2-line page (title + data), refreshed about once a second.

## Controls (B1)

| Gesture | What it does |
|---------|----------------|
| Short press | Next page |
| Long press (~800 ms) on **MAINT** | One SHT40 heater pulse |

Page order:

```text
LIVE CLIMATE → LIVE AIR → LIVE SOIL → STATUS → MAINT → …
```

- **LIVE CLIMATE** — T + RH
- **LIVE AIR** — lux, CO2 still `--`
- **LIVE SOIL** — placeholder
- **STATUS** — `OK`, one sensor’s error, or both failed. Jumps here on a new fault
- **MAINT** — hold for heater

## LED

- 1 Hz blink — OK
- Double blink, (3 s cycle) — SHT40 fault
- Triple blink, (3 s cycle) — BH1750 fault
- Log-error pattern reserved for later
- Fast blinking — generic (multiple) sensor error

Heater readings are not ambient-accurate while the sensor is hot.
