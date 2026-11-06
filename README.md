# Rrptr

A low power Reticulum transport node for LoRa, built around [microReticulum](https://github.com/attermann/microReticulum)

Development status: alpha

License: MIT

## What is Rrptr?

* Rrptr is not an RNode, but Rrptr talks over LoRa to existing RNode-based Reticulum nodes.
* Rrptr is a shell around [microReticulum](https://github.com/attermann/microReticulum) with a lean energy profile.
* The goal is to enable building Reticulum based LoRa radio meshes with solar powered devices, in the same power consumption class as MeshCore or Meshtastic.
* Bring-up is done from a serial CLI, further configuration over RNS can be done using the web console packed with [microReticulum_firmware](https://github.com/attermann/microReticulum_Firmware).

AI disclosure: I have leveraged AI in the creation of this code, but not without an understanding what it's done. As a complete AI noob it's been an interesting learning experience for me in what modern AI is capable of.

## Supported Hardware

| Env | Board | MCU | Radio | Idle Current |
|---|---|---|---|---|
| `promicro_sx1262` | ProMicro nRF52840 with SX1262 module ("FakeTec") + 25Q32/64/128 flash | nRF52840 | SX1262 | 8.3 mA |
| `xiao_nrf_sx1262` | Seeed XIAO nRF52840 with Wio-SX1262 | nRF52840 | SX1262 | |
| `lilygo_t3s3_sx1262` | LilyGO T3S3 V1.2 SX1262 | ESP32-S3 | SX1262 | 8.8 mA |
| `rak4631_sx1262` | RAK4631 with RAK15001 flash (required), not yet tested on hardware | nRF52840 | SX1262 | |

## Building

"pio run -e environment", or "pio run -e environment -t upload" to flash to the device
```
pio run -e xiao_nrf_sx1262
```

## Acknowledgements

I would like to acknowledge the following projects that either play a big part or inspired aspects of Rrptr.

- Chad Attermann's [microReticulum](https://github.com/attermann/microReticulum) and [microStore](https://github.com/attermann/microStore) (Apache-2.0)
- [openHop_modem](https://github.com/dkmr27/openhop_modem) (MIT) (src/radio/ is based on the radio-hal branch from my fork)
- [MeshCore](https://github.com/meshcore-dev/meshcore) (MIT) 
- [RadioLib](https://github.com/jgromes/RadioLib/) (MIT)
