[简体中文](README.zh_CN.md)

# AI Passport · Codex Resets

A pocket Codex reset tracker for **FoloToy AI Passport**. Native firmware and a local web preview adapt [Codex Resets](https://codex-resets.com/zh-CN) and the [Tibo 28-day challenge](https://codex-resets.com/zh-CN/tibo-28) to a **240 × 320 display and three physical buttons**.

| Reset tracker | 28-day challenge |
| --- | --- |
| ![Reset tracker](docs/assets/reset-home.png) | ![Challenge calendar](docs/assets/reset-challenge.png) |

Screenshots render the actual LVGL views on the host with fixed sample data. They are not device photographs or current statistics.

## What it does

- Tracks time since the latest executed reset, total reset count, average interval, and the latest announcement.
- Displays a 28-day calendar with daily results and Pacific time, including daylight-saving changes.
- Connects directly over HTTPS every five minutes; retains the last successful data when offline.
- Provides phone-based Wi-Fi setup. No account, API key, computer relay, or subscription is required on the device.
- Includes an interactive browser preview with live public data and an offline simulation.

This tracks public announcements, not personal account limits, and does not predict the next reset. Fixed device labels are Simplified Chinese; announcements retain their English originals. This project is not affiliated with OpenAI or the Codex Resets site.

## Try the preview

Requires Python 3.10 or later; no Python packages are needed.

```sh
git clone --branch feature/codex-resets https://github.com/onthebigtree/ai-passport-codex-resets.git
cd ai-passport-codex-resets
python3 preview/server.py --port 8765
```

Open **http://127.0.0.1:8765**. Use the on-screen buttons or arrow keys and Enter. The server only listens locally. This is a browser recreation, not a firmware emulator; the device does not depend on it.

## Device controls

| Input | Action |
| --- | --- |
| Up / Down | Switch between the two main pages |
| OK on tracker | Open the latest announcement; Up / Down scroll, OK returns |
| OK on challenge | Enter date selection; Up / Down select, OK returns |
| Hold OK | Refresh, limited to once per 15 seconds |
| Hold Up | Open or close Wi-Fi setup |

On first boot, connect a phone to the hotspot shown on the screen using its displayed password. Open **http://192.168.4.1** and enter a **2.4 GHz** Wi-Fi network. Credentials are saved to device NVS after obtaining an IP address. The setup hotspot closes on success or after ten minutes. Long-press Up to reopen it.

## Build the firmware

Target: **ESP32-C3, 8 MB Flash, no PSRAM**, with **ESP-IDF 5.5.3**. Follow the [environment setup](docs/development/engineering/environment-setup.md), activate ESP-IDF, then run:

```sh
./tools/validate.sh
```

The complete gate runs repository checks, host tests, actual LVGL rendering with a 24 KiB pool, the firmware build, and merged-image verification. The generated font is committed, so Node.js is only needed when regenerating it. CI runs host and firmware checks on this application's `feature/codex-resets` branch, which is also the repository default.

Output: **`build/FoloToy-AI-Passport-full.bin`**, a merged image for offset **`0x0`**. The content-addressed archive under `build/firmware/` retains its matching ELF, MAP, and manifest. After confirming the serial port, a complete refresh can be flashed with the activated toolchain:

```sh
python -m esptool --chip esp32c3 --port YOUR_SERIAL_PORT --baud 460800 write_flash 0x0 build/FoloToy-AI-Passport-full.bin
```

Writing the merged image can reset Wi-Fi settings and cached data. To preserve settings, follow the [component-image flashing policy](docs/development/engineering/firmware-layout.md#flashing-and-stored-data). No firmware binary, local build logs, or credentials are stored in Git.

## Validation and limitations

The 2026-10-05 local build and host tests passed. The exact tested image was flashed with a verified write hash; startup logs passed, and the device owner confirmed Wi-Fi provisioning, both data pages, Chinese text, and buttons. See the [acceptance record and image identity](docs/apps/codex-resets.md#physical-acceptance).

Power-cycle persistence, network-failure recovery, long-running operation, and battery life remain unverified. The challenge uses a bounded HTML adapter because the source has no documented challenge API; source markup changes can require an update. Dynamic non-ASCII announcement characters are replaced with `?`. Device NVS encryption and secure boot are not enabled by default.

## Project layout and credits

| Path | Purpose |
| --- | --- |
| `main/reset_*.c` | App UI, date/navigation model, data parsing, and Wi-Fi |
| `components/bsp/` | Upstream board support |
| `preview/` | Local preview and read-only data proxy |
| `tests/` | Host tests, fixtures, and native LVGL rendering |
| [Application guide](docs/apps/codex-resets.md) | Data contracts, controls, build, and acceptance details |

Built on [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport), retaining upstream history and the [MIT license](LICENSE). Noto Sans SC is redistributed under [SIL OFL 1.1](assets/fonts/OFL.txt); see [font provenance](assets/README.md#codex-resets-ui-font). Public data comes from [Codex Resets](https://codex-resets.com/zh-CN).
