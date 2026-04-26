#!/usr/bin/env node
// tools/aseprite-export.js — exports tagged frames from .aseprite files
// under assets/screens/ as individual PNGs that bitmap2header.js then
// converts to C++ bitmaps.
//
// Convention (opt-in per-frame export via tagging):
//   - Single-frame tag (from == to) → exports as `<base>-<tag>.png`.
//     Examples: key-A.png, key-Csharp.png, splash-1.png.
//   - Multi-frame tag (e.g. "Loop" covering all frames) → ignored.
//     If you want every frame exported, tag each one individually.
//   - Untagged frames → ignored (so existing manual PNGs are preserved).
//
// Why per-tag CLI invocations instead of `--save-as 'name_{tag}.png'`:
// the `{tag}` placeholder is broken on Aseprite 1.3.17.1 — it produces
// no output even when tags exist. Per-tag CLI calls work reliably.
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
const LIST_TAGS_LUA = path.join(__dirname, 'aseprite-list-tags.lua');

function findAsepriteFiles(dir) {
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...findAsepriteFiles(full));
    else if (entry.name.toLowerCase().endsWith('.aseprite')) out.push(full);
  }
  return out;
}

function listTags(file) {
  const r = spawnSync(ASEPRITE,
    ['-b', file, '--script', LIST_TAGS_LUA],
    { encoding: 'utf8' });
  if (r.status !== 0) {
    throw new Error(`aseprite list-tags failed for ${file}:\n${r.stderr}`);
  }
  // Aseprite may print other things; pick the JSON line.
  const lines = r.stdout.trim().split('\n');
  const jsonLine = lines.find(l => l.startsWith('['));
  if (!jsonLine) {
    throw new Error(`no tag JSON in stdout for ${file}: ${r.stdout}`);
  }
  return JSON.parse(jsonLine);
}

function exportFrame(file, frameNum, outPath) {
  // --frame-range is 0-based and inclusive on both ends. Aseprite's Lua
  // API (used by aseprite-list-tags.lua) reports tag fromFrame.frameNumber
  // as 1-based, so we subtract 1 here. Verified empirically against the
  // 1.3.17.1 CLI — bench-checked 2026-04-26.
  const cliFrame = frameNum - 1;
  const r = spawnSync(ASEPRITE, [
    '-b', file,
    '--frame-range', `${cliFrame},${cliFrame}`,
    '--save-as', outPath,
  ], { encoding: 'utf8' });
  if (r.status !== 0) {
    throw new Error(`aseprite export failed: ${r.stderr}`);
  }
}

function processFile(file) {
  const dir  = path.dirname(file);
  const base = path.basename(file, '.aseprite');
  const tags = listTags(file);

  // Filter for single-frame tags only.
  const singleFrameTags = tags.filter(t => t.from === t.to);

  if (singleFrameTags.length === 0) {
    const ignored = tags.length > 0
      ? ` (${tags.length} multi-frame tag(s) ignored: ${tags.map(t => `"${t.name}"`).join(', ')})`
      : ' (no tags)';
    console.log(`  ${path.relative(ASSETS_DIR, file)}  →  skipped${ignored}`);
    return 0;
  }

  for (const tag of singleFrameTags) {
    const outName = `${base}-${tag.name}.png`;
    const outPath = path.join(dir, outName);
    exportFrame(file, tag.from, outPath);
    console.log(`  ${path.relative(ASSETS_DIR, file)}  →  ${path.relative(ASSETS_DIR, outPath)}`);
  }
  return singleFrameTags.length;
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
  let total = 0;
  for (const f of files) total += processFile(f);
  console.log(`Done — exported ${total} frame(s).`);
}

main();
