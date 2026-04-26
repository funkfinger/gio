-- aseprite-list-tags.lua — DEPRECATED (2026-04-26).
--
-- The current tools/aseprite-export.js does not need this script — it uses
-- Aseprite's built-in {tag} substitution in --save-as and post-prunes
-- multi-frame tag pollution in JS. This file is kept around for any future
-- task that needs richer Aseprite introspection from the CLI (the standard
-- --list-tags and --data JSON export are broken in 1.3.17.1; the Lua API
-- reads tags reliably).
--
-- Print sprite.tags as one JSON line on stdout.
-- Run as: aseprite -b <file>.aseprite --script aseprite-list-tags.lua
-- Output: [{"name":"A","from":1,"to":1},{"name":"Loop","from":1,"to":6}, ...]

local s = app.activeSprite
if not s then
  io.write("[]\n")
  return
end

local function escape(str)
  return (str:gsub('\\', '\\\\'):gsub('"', '\\"'))
end

io.write("[")
for i, tag in ipairs(s.tags) do
  if i > 1 then io.write(",") end
  io.write(string.format('{"name":"%s","from":%d,"to":%d}',
    escape(tag.name),
    tag.fromFrame.frameNumber,
    tag.toFrame.frameNumber))
end
io.write("]\n")
