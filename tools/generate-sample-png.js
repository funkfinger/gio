#!/usr/bin/env node
// One-shot helper: generates a sample 32×64 PNG at assets/screens/sample.png
// so you can verify the converter round-trip end-to-end on a fresh clone.
// Delete the generated sample.png once you have real art.
//
// Run from repo root:
//   node tools/generate-sample-png.js

const fs   = require('node:fs');
const path = require('node:path');
const { PNG } = require('pngjs');

const W = 32, H = 64;
const png = new PNG({ width: W, height: H, colorType: 2 });

for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
        const idx = (y * W + x) * 4;
        // Border (lit) + two diagonals crossing in the middle + a "gio"
        // block at the top. Lots of test patterns for rotation/packing.
        const border = (x === 0 || x === W - 1 || y === 0 || y === H - 1);
        const diagA  = (x === y * (W / H));                       // \
        const diagB  = (x === (W - 1) - y * (W / H));             // /
        const block  = (y >= 8 && y < 16 && x >= 4 && x < W - 4); // filled bar
        const lit = border || diagA || diagB || block;
        // OLED-native convention: white in PNG → lit on OLED.
        const v = lit ? 255 : 0;
        png.data[idx]     = v;
        png.data[idx + 1] = v;
        png.data[idx + 2] = v;
        png.data[idx + 3] = 255;
    }
}

const outDir = path.join(__dirname, '..', 'assets', 'screens');
fs.mkdirSync(outDir, { recursive: true });
const outPath = path.join(outDir, 'sample.png');
png.pack().pipe(fs.createWriteStream(outPath)).on('finish', () => {
    console.log(`Wrote ${outPath}  (${W}×${H})`);
});
