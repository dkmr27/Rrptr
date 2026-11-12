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
| `xiao_nrf_sx1262` | Seeed XIAO nRF52840 with Wio-SX1262 | nRF52840 | SX1262 | 7.7 mA |
| `lilygo_t3s3_sx1262` | LilyGO T3S3 V1.2 SX1262 | ESP32-S3 | SX1262 | 8.8 mA |
| `xiao_s3_sx1262` | Seeed XIAO ESP32-S3 (8 MB flash) + Wio-SX1262 kit, joined by the B2B connector | ESP32-S3 | SX1262 | 7.3 mA |
| `xiao_s3_sx1262_alt` | Seeed XIAO ESP32-S3 (8 MB flash) + SX1262 on the outer pins, wired as on the XIAO nRF52840 kit | ESP32-S3 | SX1262 | 7.3 mA |
| `heltec_v3_sx1262` | Heltec WiFi LoRa 32 V3 | ESP32-S3 | SX1262 | 11.5 mA |
| `heltec_v4_sx1262` | Heltec WiFi LoRa 32 V4 (not the V4R8) | ESP32-S3 | SX1262 | 14.5 mA |
| `heltec_v4r8_sx1262` | Heltec WiFi LoRa 32 V4 R8 | ESP32-S3 | SX1262 | 18.4 mA |
| `xiao_s3_lora2021f33` | Seeed XIAO ESP32-S3 (8 MB flash) on the Xiao-LoRa2021F33-2G4 board (NiceRF LoRa2021F33, sub-GHz and 2.4 GHz) | ESP32-S3 | LR2021 | |
| `xiao_nrf_lora2021f33` | Seeed XIAO nRF52840 on the Xiao-LoRa2021F33-2G4 board | nRF52840 | LR2021 | |
| `ebyte_eora_hub_lr1121` | Ebyte EoRa-HUB-900TB (E80 module, sub-GHz and 2.4 GHz); battery reading is wrong, a known hardware fault | ESP32-S3 | LR1121 | |
| `rak4631_sx1262` | RAK4631 with RAK15001 flash (required) | nRF52840 | SX1262 | |
| `rak3401_rak13302` | RAK3401 with RAK13302 1 W LoRa module (CS moved to IO7: 0 ohm from R7 to R4) and RAK15001 flash in slot A (required), not yet tested on hardware | nRF52840 | SX1262 | |

TX power is always the radio chip's drive power. Boards with a front-end module, such as the Heltec V4, put out more at the antenna: about 7 to 11 dB more on the V4, so 22 dBm gives about 28 dBm. The Xiao-LoRa2021F33-2G4's module has a PA on both bands: 2.4 GHz is capped at 0 dBm drive, which already gives about 30 dBm out.

## Building

"pio run -e environment", or "pio run -e environment -t upload" to flash to the device
```
pio run -e xiao_nrf_sx1262
```

### ESP32 boards: first install

Rrptr and its OTA updater are separate apps. A merged image holds both, along with the bootloader and flash layout, for a first install without PlatformIO:
```
pio run -e lilygo_t3s3_sx1262 -t merged
esptool.py --chip esp32s3 write_flash 0x0 rrptr-lilygo_t3s3_sx1262-merged.bin
```
The image is built in `.pio/build/lilygo_t3s3_sx1262/`. Flashing it leaves the filesystem alone, so a node already set up keeps its identity and settings.

During development, `-t upload` only writes Rrptr. To flash the updater by itself:
```
pio run -e lilygo_t3s3_updater -t upload --upload-port <port>
```

## Firmware updates

Run `ota` on the serial console and follow what it says. nRF52 boards update over Bluetooth from the bootloader, and ESP32 boards over an open Wi-Fi network from the updater, which anyone in range can reach while it runs.

## Acknowledgements

I would like to acknowledge the following projects that either play a big part or inspired aspects of Rrptr.

- Chad Attermann's [microReticulum](https://github.com/attermann/microReticulum) and [microStore](https://github.com/attermann/microStore) (Apache-2.0)
- [openHop_modem](https://github.com/dkmr27/openhop_modem) (MIT) (src/radio/ is based on the radio-hal branch from my fork)
- [MeshCore](https://github.com/meshcore-dev/meshcore) (MIT) 
- [RadioLib](https://github.com/jgromes/RadioLib/) (MIT)
