#!/usr/bin/env node
// tools/aseprite-export.js — exports tagged frames from .aseprite files
// under assets/screens/ as individual PNGs that bitmap2header.js then
// converts to C++ bitmaps.
//
// Convention:
//   - Tag a frame in Aseprite (Window → Timeline → right-click → New Tag).
//   - Single-frame tags export as `<base>-<tag>.png` (e.g. `key-A.png`).
//   - **Multi-frame tags are bad news.** If a tag spans multiple frames,
//     Aseprite's `{tag}` placeholder generates `<base>-<tag>1.png`,
//     `<base>-<tag>2.png`, ... `<base>-<tag>N.png` — one file per frame.
//     These pollute the screens namespace with redundant bitmaps. The tool
//     auto-detects them after export, deletes them, and warns. Remove the
//     multi-frame tag in Aseprite to silence the warning.
//
// Why no Lua introspection: Aseprite 1.3.17.1's CLI placeholder substitution
// (`{tag}`) works fine for direct exports. The earlier Lua-based approach
// (kept at tools/aseprite-list-tags.lua, deprecated for now) was needed for
// per-tag CLI invocations to avoid multi-frame pollution — but the post-
// export cleanup below handles that without needing per-tag invocations.
//
// Aseprite path defaults to the macOS standard install. Set
// ASEPRITE_PATH=/path/to/aseprite to override.

const fs   = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const ASEPRITE = process.env.ASEPRITE_PATH
  || '/Applications/Aseprite.app/Contents/MacOS/aseprite';

const REPO_ROOT  = path.resolve(__dirname, '..');
const ASSETS_DIR = path.join(REPO_ROOT, 'assets', 'screens');

function findAsepriteFiles(dir) {
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...findAsepriteFiles(full));
    else if (entry.name.toLowerCase().endsWith('.aseprite')) out.push(full);
  }
  return out;
}

// List PNGs in a directory that match the basename's prefix.
function pngsForBase(dir, base) {
  if (!fs.existsSync(dir)) return new Set();
  return new Set(
    fs.readdirSync(dir).filter(f =>
      f.startsWith(`${base}-`) && f.toLowerCase().endsWith('.png')
    )
  );
}

// Detect multi-frame-tag pollution among NEW files: any group of >=2 files
// sharing the same `<base>-<tag>` prefix where each ends with a digit
// suffix (e.g. key-Loop1.png, key-Loop2.png, ...). Returns a Map of
// prefix → [filenames].
function detectMultiFrameGroups(base, newFiles) {
  const groups = new Map();
  for (const f of newFiles) {
    // Strip "<base>-" then look for trailing digits before .png.
    const stripped = f.slice(base.length + 1, -4);  // remove "<base>-" prefix and ".png"
    const m = stripped.match(/^(.+?)(\d+)$/);
    if (!m) continue;
    const tagName = m[1];
    if (!groups.has(tagName)) groups.set(tagName, []);
    groups.get(tagName).push(f);
  }
  // Keep only groups with multiple files (true multi-frame tag pattern).
  for (const [k, v] of groups.entries()) {
    if (v.length < 2) groups.delete(k);
  }
  return groups;
}

function exportFile(file) {
  const dir  = path.dirname(file);
  const base = path.basename(file, '.aseprite');

  const before = pngsForBase(dir, base);
  const r = spawnSync(ASEPRITE, [
    '-b', file,
    '--save-as', path.join(dir, `${base}-{tag}.png`),
  ], { encoding: 'utf8' });
  if (r.status !== 0) {
    throw new Error(`aseprite export failed for ${file}:\n${r.stderr}`);
  }
  const after = pngsForBase(dir, base);
  const newFiles = [...after].filter(f => !before.has(f));

  // Empty-tag artifact: an untagged .aseprite produces `<base>-.png` (the
  // {tag} substitution is empty, leaving a trailing dash). Detect + delete.
  const emptyTagFile = `${base}-.png`;
  const hasEmptyTag = newFiles.includes(emptyTagFile);
  if (hasEmptyTag) {
    console.warn(`  ⚠  no tags in ${path.relative(ASSETS_DIR, file)} — deleting empty-tag artifact: ${emptyTagFile}`);
    console.warn(`     → tag at least one frame in Aseprite, or move the .aseprite out of assets/screens/ to skip it`);
    fs.unlinkSync(path.join(dir, emptyTagFile));
  }

  const polluted = detectMultiFrameGroups(base, newFiles.filter(f => f !== emptyTagFile));

  for (const f of newFiles) {
    if (f === emptyTagFile) continue;
    const isPolluted = [...polluted.values()].some(grp => grp.includes(f));
    if (!isPolluted) {
      console.log(`  ${path.relative(ASSETS_DIR, file)}  →  ${f}`);
    }
  }

  if (polluted.size > 0) {
    for (const [tagName, files] of polluted.entries()) {
      console.warn(`  ⚠  multi-frame tag detected: "${tagName}" in ${path.relative(ASSETS_DIR, file)} (${files.length} frames)`);
      console.warn(`     deleting: ${files.join(', ')}`);
      console.warn(`     → remove the "${tagName}" tag in Aseprite to silence this`);
      for (const f of files) fs.unlinkSync(path.join(dir, f));
    }
  }
}

function main() {
  if (!fs.existsSync(ASEPRITE)) {
    console.warn(`aseprite not found at ${ASEPRITE} — skipping aseprite-export.`);
    console.warn('Set ASEPRITE_PATH if installed elsewhere, or commit PNGs by hand.');
    return;
  }

  if (!fs.existsSync(ASSETS_DIR)) {
    console.warn(`assets/screens/ not found — nothing to export.`);
    return;
  }

  const files = findAsepriteFiles(ASSETS_DIR);
  if (files.length === 0) {
    console.log('No .aseprite files found.');
    return;
  }

  console.log(`Aseprite: ${ASEPRITE}`);
  console.log(`Scanning ${files.length} .aseprite file(s):`);
  for (const f of files) exportFile(f);
}

main();
