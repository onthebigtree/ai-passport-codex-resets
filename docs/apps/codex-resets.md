[简体中文](codex-resets.zh_CN.md)

# Codex Resets for AI Passport

A standalone ESP32-C3 application for the 240 x 320 portrait display and three physical keys. The application replaces the baseline test menu. Its companion web preview uses live public data and the same screen layout; it is a browser recreation, not an emulator of the firmware.

## Screens and controls

| View | Content | Up / Down | OK |
| --- | --- | --- | --- |
| Reset tracker | Elapsed time since the last executed reset; total announcements; average interval; latest reset time | Switch between tracker and challenge | Open the latest announcement |
| Latest announcement | Regular/banked classification; English original; planned-reset indicator | Scroll | Return |
| 28-day challenge | Pacific time; challenge day; 7 x 4 calendar; selected day's result | Switch pages | Select calendar days |
| Calendar selection | Upcoming, waiting, improvement, reset, both, checking, or closed | Previous/next day, wrapping at the ends | Return |

Long-press **OK** to refresh (at most once per 15 seconds). Long-press **Up** to open or close network setup. Double-clicks have no special action. The preview also accepts arrow keys, Enter, Escape, Shift+Up (setup preview), and Shift+Enter (refresh).

The tracker uses China Standard Time (UTC+8). The challenge follows `America/Los_Angeles`, including US daylight-saving transitions. A planned reset is never counted as an executed reset. Closed days are shown neutrally; the app does not infer that a promise failed. There is no predicted next-reset countdown or personal account quota.

## First boot and Wi-Fi

1. The first boot opens a temporary WPA2 hotspot. Read its unique name and random password on the device.
2. Connect a phone to that hotspot; open `http://192.168.4.1` manually. Automatic captive-portal pop-up is not implemented.
3. Enter a 2.4 GHz Wi-Fi network. The device tests the connection and saves credentials only after receiving an IP address. A failed attempt preserves previously saved credentials. Check the device for the result.
4. On success the hotspot and configuration web server stop. The device synchronizes its clock, then fetches data directly over HTTPS. No laptop, preview server, account, or API key is required.

Setup expires after 10 minutes. Long-press Up to reopen it. Each new session has a new hotspot password. Wi-Fi credentials live only in the device's NVS; they are never printed or embedded in a build. The base firmware does not enable encrypted NVS or secure boot. Network setup, TLS, RF behavior, and persistence require physical-device acceptance.

## Sources, freshness, and limits

Data from [Codex Resets](https://codex-resets.com/zh-CN), an unofficial site with no OpenAI affiliation. [Public API documentation](https://codex-resets.com/api/docs).

| Screen | Source | Handling |
| --- | --- | --- |
| Tracker | `GET https://codex-resets.com/api/v1/status` | Bounded 12 KiB JSON response, schema checks; up to 511 ASCII bytes of the latest announcement |
| Challenge | `GET https://codex-resets.com/zh-CN/tibo-28` | Streaming HTML tags, 768-byte tag buffer, 256 KiB response ceiling; exactly 28 recognized cells and a valid start date required |

Poll every five minutes. Each section has its own successful-sync timestamp. A failed or malformed response does not overwrite good data; firmware retains a versioned NVS snapshot and displays an offline/error indicator. Before the first successful sync values remain unknown. The challenge has no documented public API; HTML changes can break its adapter and are reported as update failures. All data requests are read-only. No voting, subscription, or notification requests are sent.

The native challenge view summarizes daily results; complete posts remain on the source site. Fixed Chinese labels are covered by a generated 14 px font. The reset API supplies English announcement originals. Unsupported non-ASCII announcement characters are deliberately replaced with `?`; a full dynamic CJK/emoji font is not bundled. Website statistics and device battery are separate; the preview reports battery as unknown.

## Web preview

From the repository root:

```sh
python3 preview/server.py --port 8765
```

Open `http://127.0.0.1:8765`. The standard-library server proxies only the two public read endpoints to avoid browser CORS restrictions, caches for five minutes, and binds to loopback. It never proxies credentials or arbitrary URLs. The offline switch simulates a disconnected device and retains last-known data. The live preview is **not** the dependency used by firmware.

## Build and validation

Use the repository's ESP-IDF **5.5.3** environment and the complete gate:

```sh
./tools/validate.sh
```

The verified merged image is `build/FoloToy-AI-Passport-full.bin`; its content-addressed archive contains the matching ELF/MAP and manifest. Flash the merged image at **0x0** only after explicit approval. Merged flashing may reset NVS, including network settings and cached data. No full-chip erase is required.

Pure model tests cover chunked HTML, unknown markup, truncated responses, timezone boundaries, navigation, and text bounds. JSON tests use ESP-IDF's actual cJSON. Preview tests cover partial failures and stale-cache preservation. The firmware gate also renders the actual LVGL UI with its 24 KiB pool, checks real glyph descriptors, and switches pages 100 times. The calendar uses custom drawing rather than 56 child objects. Host rendering does not reproduce board RAM, SPI timing, or the physical rounded-corner mask. The original repository checks remain enabled. The gate uses the appropriate dead-section linker flag on macOS, and the actionlint download is SHA-256-checked using Python. On machines with mismatched Xcode/Command Line Tools SDKs, select a compatible installed SDK for this invocation via `SDKROOT`; do not change global developer settings.

Regenerate fonts only when UI text changes:

```sh
npm ci --ignore-scripts
python3 -m venv .toolchain/font-env
.toolchain/font-env/bin/pip install fonttools brotli
.toolchain/font-env/bin/python tools/generate_reset_font.py
python3 tools/check_reset_font.py
```

## Physical acceptance

On 2026-10-05 the user authorized flashing the verified merged image with SHA-256 `cbd8324e7d5d0cd76d43fae9671a1ee374c8b4b42d82f3bdd482ca2e6dfaaead` to an ESP32-C3 with 8 MB Flash. The write hash matched. A 35-second startup capture confirmed the matching ELF identity and display, LVGL, battery-chip, and button initialization without a panic, assertion, or watchdog event. The user then confirmed successful Wi-Fi provisioning, data on both pages, readable Chinese, and working buttons.

Build: PASS. Host tests: PASS. Device tests: PASS for the observed startup and user-confirmed checks above. Network synchronization logs were not captured because the USB device was no longer available after provisioning. No public release was made.

Unverified:

- Detailed calendar focus, announcement scrolling, long presses, rounded-corner clipping, and battery-unavailable behavior.
- Wrong passwords, unavailable routers, recovery, portal expiry, reboot persistence, and offline timestamp retention.
- Repeated on-device synchronization and heap measurements, battery life, and radio performance.
