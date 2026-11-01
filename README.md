# Rrptr

A bare-bones, battery-first Reticulum transport node for LoRa, built on
[microReticulum](https://github.com/attermann/microReticulum) and
[RadioLib](https://github.com/jgromes/RadioLib).

Rrptr talks to existing RNode-based Reticulum nodes over the air. It is not an
RNode: there is no host mode, KISS protocol or rnodeconf support. Bring-up is
done from a serial CLI, further configuration over Reticulum.

Status: early development.

## Boards

| Env | Board | MCU | Radio |
|---|---|---|---|
| `promicro_sx1262` | Nologo ProMicro nRF52840 / nice!nano with SX1262 module ("FakeTec") | nRF52840 | SX1262 |

## Building

```
pio run -e promicro_sx1262
```

## Layout

```
boards/                  PlatformIO board definitions, one per MCU board
    bsp/<board>/         Arduino core variant files for that board (nRF52 only)
scripts/                 build scripts (version stamping)
src/                     firmware
variants/<variant>/      one folder per variant (MCU board + radio)
    board_config.h       pins, radio and hardware specifics
    platformio.ini       the board's build env
```

## Licence

MIT, see [LICENSE](LICENSE).

Third-party code keeps its own licence:

- `boards/bsp/` Arduino core variant files are LGPL-2.1, from the Adafruit
  nRF52 Arduino core templates.
- microReticulum and microStore are Apache-2.0; RadioLib is MIT.
