#!/usr/bin/env node
// tools/bitmap2header.js — converts single-colour PNG files under
// assets/screens/ into a C++ screens library consumable by the firmware via
// Adafruit_GFX's drawBitmap() call.
//
// Workflow:
//   1. Drop PNGs into assets/screens/  (e.g. assets/screens/boot.png)
//      OR a subdirectory for animation frames (e.g.
//      assets/screens/splash-screen-animation/frame1.png ... frame9.png).
//   2. Run `npm run build:screens`.
//   3. firmware/arp/lib/screens/{screens.h,screens.cpp} are regenerated.
//   4. Rebuild firmware with `pio run -d firmware/arp`.
//
// Emits TWO C++ types:
//
//   struct Screen    — a single bitmap (data + width + height).
//   struct Animation — an ordered array of Screen* + frame count.
//
// Every PNG becomes a `Screen`. Every subdirectory of PNGs ALSO becomes an
// `Animation` named after the directory. Filenames and directory names are
// sanitized to valid C++ identifiers: hyphens and spaces → underscores.
//
// Thresholding rule: a pixel is LIT (white on the OLED) if its perceived
// luminance is >= 128 AND its alpha is > 127. This matches the OLED-native
// mental model — what you draw is what lights up:
//   white pixel in PNG → lit pixel on OLED
//   black pixel in PNG → unlit pixel on OLED
//   transparent pixel  → unlit
// Draw with light strokes on a black canvas (how the display physically looks).
//
// Packing: Adafruit GFX `drawBitmap` byte order — row-major, MSB = leftmost
// pixel, one byte per 8 horizontal pixels, each row padded up to the next
// byte boundary.
//
// Frame order within an animation directory is NATURAL SORT: `frame2.png`
// comes before `frame10.png`.

const fs   = require('node:fs');
const path = require('node:path');
const { PNG } = require('pngjs');

// --------------------------------- paths ---------------------------------
const REPO_ROOT  = path.resolve(__dirname, '..');
const ASSETS_DIR = path.join(REPO_ROOT, 'assets', 'screens');
const OUT_DIR    = path.join(REPO_ROOT, 'firmware', 'arp', 'lib', 'screens');
const OUT_H      = path.join(OUT_DIR, 'screens.h');
const OUT_CPP    = path.join(OUT_DIR, 'screens.cpp');

// ----------------------------- thresholding ------------------------------
function luminance(r, g, b) {
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}
function isLit(r, g, b, a) {
    if (a <= 127) return false;         // transparent pixels are unlit
    return luminance(r, g, b) >= 128;   // bright pixels become lit (OLED-native)
}

// -------------------------------- helpers --------------------------------
function sanitizeIdent(raw) {
    const name = raw.replace(/[^a-zA-Z0-9_]/g, '_');
    if (!/^[a-zA-Z_]/.test(name)) {
        throw new Error(`Identifier must start with a letter or underscore: ${raw}`);
    }
    return name;
}

// Natural sort: "frame2" < "frame10". Compares strings by splitting into
// alternating runs of digits and non-digits, comparing digit runs numerically.
function naturalCompare(a, b) {
    const ax = a.match(/(\d+|\D+)/g) || [];
    const bx = b.match(/(\d+|\D+)/g) || [];
    for (let i = 0; i < Math.max(ax.length, bx.length); i++) {
        const an = ax[i];
        const bn = bx[i];
        if (an === undefined) return -1;
        if (bn === undefined) return  1;
        const aIsNum = /^\d+$/.test(an);
        const bIsNum = /^\d+$/.test(bn);
        if (aIsNum && bIsNum) {
            const d = parseInt(an, 10) - parseInt(bn, 10);
            if (d !== 0) return d;
        } else {
            const c = an.localeCompare(bn);
            if (c !== 0) return c;
        }
    }
    return 0;
}

// ------------------------------ conversion -------------------------------
function convertPng(filePath) {
    const buf  = fs.readFileSync(filePath);
    const png  = PNG.sync.read(buf);
    const { width: w, height: h, data } = png;
    const bytesPerRow = Math.ceil(w / 8);
    const bytes = new Uint8Array(bytesPerRow * h);

    for (let y = 0; y < h; y++) {
        for (let x = 0; x < w; x++) {
            const i = (y * w + x) * 4;
            if (isLit(data[i], data[i + 1], data[i + 2], data[i + 3])) {
                const byteIdx = y * bytesPerRow + (x >> 3);
                const bitIdx  = 7 - (x & 7);
                bytes[byteIdx] |= (1 << bitIdx);
            }
        }
    }

    const base = path.basename(filePath, path.extname(filePath));
    return { name: sanitizeIdent(base), width: w, height: h, bytes };
}

// --------------------------------- emit ----------------------------------
function emitBytes(bytes) {
    const lines = [];
    for (let i = 0; i < bytes.length; i += 12) {
        const chunk = [];
        for (let j = i; j < Math.min(i + 12, bytes.length); j++) {
            chunk.push('0x' + bytes[j].toString(16).padStart(2, '0'));
        }
        lines.push('    ' + chunk.join(', ') + ',');
    }
    if (lines.length > 0) {
        lines[lines.length - 1] = lines[lines.length - 1].replace(/,$/, '');
    }
    return lines.join('\n');
}

function emitHeader(screens, animations) {
    const screenDecls = screens.map(s => `extern const Screen ${s.name};`).join('\n');
    const animDecls   = animations.map(a => `extern const Animation ${a.name};`).join('\n');

    return `// Auto-generated by tools/bitmap2header.js — do not edit by hand.
// Source PNGs live in assets/screens/; regenerate with \`npm run build:screens\`.
#pragma once
#include <cstdint>

namespace screens {

// A monochrome bitmap packed in Adafruit GFX \`drawBitmap\` byte order:
// row-major, MSB = leftmost pixel, one byte per 8 horizontal pixels,
// rows padded up to the next byte boundary.
struct Screen {
    const uint8_t* data;
    uint8_t        width;
    uint8_t        height;
};

// An ordered sequence of Screen pointers — one directory of PNGs under
// assets/screens/ becomes one Animation, named after the directory.
// Frames are in natural-sort order of their filenames.
struct Animation {
    const Screen* const* frames;
    uint8_t              count;
};

${screenDecls}

${animDecls}

} // namespace screens
`;
}

function emitSource(screens, animations) {
    const dataDefs = screens.map(s =>
`// ${s.name} — ${s.width}x${s.height}
static const uint8_t ${s.name}_data[] = {
${emitBytes(s.bytes)}
};
`
    ).join('\n');

    const screenDefs = screens.map(s =>
        `const Screen ${s.name} = { ${s.name}_data, ${s.width}, ${s.height} };`
    ).join('\n');

    const animDefs = animations.map(a => {
        const arrayName = `${a.name}_frames`;
        const ptrs = a.frameNames.map(n => `&${n}`).join(', ');
        return `// ${a.name} — ${a.frameNames.length} frames
static const Screen* const ${arrayName}[] = { ${ptrs} };
const Animation ${a.name} = { ${arrayName}, ${a.frameNames.length} };`;
    }).join('\n\n');

    return `// Auto-generated by tools/bitmap2header.js — do not edit by hand.
// Source PNGs live in assets/screens/; regenerate with \`npm run build:screens\`.
#include "screens.h"

namespace screens {

${dataDefs}
${screenDefs}

${animDefs}

} // namespace screens
`;
}

// ---------------------------------- run ----------------------------------
function main() {
    if (!fs.existsSync(ASSETS_DIR)) {
        console.error(`assets/screens/ not found at ${ASSETS_DIR}`);
        process.exit(1);
    }

    const allScreens = [];     // {name, width, height, bytes}
    const allAnims   = [];     // {name, frameNames: [str]}
    const seenNames  = new Set();

    function addScreen(s, source) {
        if (seenNames.has(s.name)) {
            throw new Error(`Duplicate screen name '${s.name}' — from ${source}. Rename one of them.`);
        }
        seenNames.add(s.name);
        allScreens.push(s);
    }

    // 1. Top-level PNGs → Screens only.
    const topEntries = fs.readdirSync(ASSETS_DIR, { withFileTypes: true })
        .filter(e => e.isFile() && e.name.toLowerCase().endsWith('.png'))
        .sort((a, b) => naturalCompare(a.name, b.name));
    for (const e of topEntries) {
        const s = convertPng(path.join(ASSETS_DIR, e.name));
        addScreen(s, `assets/screens/${e.name}`);
        console.log(`  ${e.name}  →  screens::${s.name}  (${s.width}×${s.height}, ${s.bytes.length} bytes)`);
    }

    // 2. Subdirectories of PNGs → Screens + Animation.
    const subdirs = fs.readdirSync(ASSETS_DIR, { withFileTypes: true })
        .filter(e => e.isDirectory())
        .sort((a, b) => naturalCompare(a.name, b.name));
    for (const d of subdirs) {
        const subPath = path.join(ASSETS_DIR, d.name);
        const pngFiles = fs.readdirSync(subPath)
            .filter(f => f.toLowerCase().endsWith('.png'))
            .sort(naturalCompare);
        if (pngFiles.length === 0) continue;

        const animName = sanitizeIdent(d.name);
        const frameNames = [];
        for (const f of pngFiles) {
            const s = convertPng(path.join(subPath, f));
            addScreen(s, `assets/screens/${d.name}/${f}`);
            frameNames.push(s.name);
            console.log(`  ${d.name}/${f}  →  screens::${s.name}  (${s.width}×${s.height}, ${s.bytes.length} bytes)`);
        }
        allAnims.push({ name: animName, frameNames });
        console.log(`  ↳ animation screens::${animName}  (${frameNames.length} frames)`);
    }

    if (allScreens.length === 0) {
        console.warn('No PNGs found — emitting empty screens library.');
    }

    fs.mkdirSync(OUT_DIR, { recursive: true });
    fs.writeFileSync(OUT_H,   emitHeader(allScreens, allAnims));
    fs.writeFileSync(OUT_CPP, emitSource(allScreens, allAnims));

    const totalBytes = allScreens.reduce((a, s) => a + s.bytes.length, 0);
    console.log(`\nWrote ${OUT_H} and ${OUT_CPP}`);
    console.log(`(${allScreens.length} screens, ${allAnims.length} animations, ${totalBytes} bytes of bitmap data)`);
}

main();
