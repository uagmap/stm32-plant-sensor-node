# STM32 Plant Environment Sensor Node

The project is my attempt at learning freeRTOS through building a working data-gathering device.
More info will be added as functionality expands.
Bare-metal firmware on **Nucleo-F411RE** (STM32F411RE).

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

- Normal: `T=xx.xx C rh=yy.yy %`
- B1: `[heater] T=...` (not ambient-accurate during heat)
- Driver: `0` OK, `-1` I2C fail, `-2` CRC fail
