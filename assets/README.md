<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

### Codex Resets UI font

- `fonts/NotoSansSC.ttf`: Noto Sans SC variable font, from [Google Fonts](https://github.com/google/fonts/tree/main/ofl/notosanssc), licensed under SIL OFL 1.1 (`fonts/OFL.txt`).
- `fonts/reset-ui-medium.ttf`: weight-500 subset shared by the LVGL converter and browser.
- `fonts/reset_font_14.c`: 14 px, 4 bpp, uncompressed LVGL subset, generated with `lv_font_conv@1.5.3` by `tools/generate_reset_font.py`. Compiled into the application from `main/CMakeLists.txt`.
- `fonts/reset_glyphs.json`: complete fixed-UI inventory; ASCII plus the Chinese and punctuation in application strings. `preview/passport.woff2` is the same source subset for the browser. Generation verifies the source cmap; `tools/check_reset_font.py` independently checks the generated glyph comments, including a negative coverage case.
- Dynamic reset announcements use ASCII originals with visible `?` replacement for unsupported code points. This is not a full CJK font. See [the application guide](../docs/apps/codex-resets.md).

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
