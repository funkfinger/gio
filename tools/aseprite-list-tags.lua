-- aseprite-list-tags.lua — print sprite.tags as one JSON line on stdout.
-- Used by tools/aseprite-export.js to introspect tags reliably (the
-- standard `--list-tags` and `--data` JSON export are broken in 1.3.17.1).
--
-- Run as: aseprite -b <file>.aseprite --script aseprite-list-tags.lua
--
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
