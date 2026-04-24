# assets/screens/

Source PNGs for the module's OLED graphics. Converted to packed C++ byte
arrays by `tools/bitmap2header.js` and consumed by the firmware via
`firmware/arp/lib/screens/`.

## Workflow

1. Design a PNG in any image editor — **recommended size: 32×64** (the 0.49"
   OLED in portrait orientation). Smaller icons also fine; the converter reads
   dimensions from each file.
2. Save it here as `<name>.png`. Hyphens and spaces get converted to
   underscores in the C++ identifier: `arp-idle.png` → `screens::arp_idle`.
3. From the repo root, run:
   ```bash
   npm install               # once — installs pngjs
   npm run build:screens     # regenerates screens.h / screens.cpp
   ```
4. Build firmware:
   ```bash
   pio run -d firmware/arp
   ```
5. Use in firmware:
   ```cpp
   #include "screens.h"
   ui.drawScreen(screens::arp_idle);          // full-screen at 0, 0
   ui.drawBitmap(16, 20, screens::icon_scale); // icon at arbitrary position
   ```

## Colour and threshold rules

- Input format: 8-bit PNG, any colour mode (RGB / RGBA / greyscale).
  Transparent pixels (alpha ≤ 127) render as **unlit**.
- **OLED-native convention:** what you draw is what lights up.
  - **White pixel in the PNG → lit (white) pixel on the OLED.**
  - **Black pixel in the PNG → unlit (off) pixel on the OLED.**
  - Threshold: luminance ≥ 128 (Rec.709 weights) is treated as white.
- Draw with **light strokes on a black canvas** — the image on your screen
  will look exactly like the OLED will display it.
- No anti-aliasing survives the conversion — draw with hard edges (pencil
  tool, no brush softness) or accept the threshold behaviour.

## Tips

- **Aseprite** and **Pixaki** are purpose-built for pixel art at this scale.
- **macOS Preview** can resize and threshold in a pinch.
- **Figma** exports PNG at exact pixel sizes — disable anti-aliasing in your
  layer's render settings if you want crisp output.
- Keep a psd/aseprite working file alongside each PNG for later editing;
  only the PNG needs to be committed for the firmware build.

## Conventions

- Full-screen art: 32×64 (portrait-oriented 0.49" panel).
- Status icons: typically 8×8 or 16×16, drawn at an offset.
- Naming: `<app>_<state>.png` for app screens (e.g. `arp_idle.png`,
  `arp_playing.png`, `clock_out.png`); `icon_<thing>.png` for reusable
  glyphs (`icon_scale.png`, `icon_clock.png`).
