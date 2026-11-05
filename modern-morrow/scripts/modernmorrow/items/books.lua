-- A book's text, read the way the engine's book formatter reads it, and laid out as one tall column.
--
-- Book text is HTML-like markup. This follows the engine's rules in apps/openmw/mwgui/formatting.cpp:
--   * <BR> starts a line and <P> a paragraph. Line endings in the record are ignored, and so are the breaks before
--     the first text.
--   * <P ALIGN=...> and <DIV ALIGN=...> set the alignment of what follows: left, center or right.
--   * <FONT COLOR=... FACE=...> sets the colour and the font of the text that follows, and </FONT> sets the colour
--     back. The Daedric face shows in the engine's ScrollFont, and any other face in the normal font.
--   * <IMG SRC=... WIDTH=... HEIGHT=...> shows a picture from the bookart folder.
--   * Variables such as %PCName are replaced with types.Book.substituteVariables, as in the engine's books.
--   * Morrowind's books show nothing after the last <BR> or <P>.
-- Every other tag is left out.

local types = require('openmw.types')
local ui = require('openmw.ui')
local util = require('openmw.util')
local vfs = require('openmw.vfs')

local draw = require('scripts.modernmorrow.ui.draw')
local style = require('scripts.modernmorrow.ui.style')
local text = require('scripts.modernmorrow.ui.text')

local v2 = util.vector2

local books = {}

-- ---------------------------------------------------------------------------------------------------------------
-- Reading the markup

-- A tag's attributes by lowercase name. Values may be in quotes or not, such as ALIGN="CENTER" or WIDTH=128.
local function parseAttributes(s)
    local attributes = {}
    for key, quoted in s:gmatch('([%w_]+)%s*=%s*(%b"")') do
        attributes[key:lower()] = quoted:sub(2, -2)
    end
    for key, bare in s:gmatch('([%w_]+)%s*=%s*([^%s"]+)') do
        key = key:lower()
        if attributes[key] == nil then
            attributes[key] = bare
        end
    end
    return attributes
end

local function parseColor(value)
    local hex = value:gsub('^#', ''):match('^%x+')
    if hex == nil then
        return nil
    end
    local number = tonumber(hex, 16) or 0
    return util.color.rgb(math.floor(number / 65536) % 256 / 255, math.floor(number / 256) % 256 / 255,
        number % 256 / 255)
end

local ALIGN = {
    left = ui.ALIGNMENT.Start,
    center = ui.ALIGNMENT.Center,
    right = ui.ALIGNMENT.End,
}

-- The texture for an <IMG> tag, or nil if there is none. Pictures are in the bookart folder. Books often name them
-- with another extension than the file has, and sometimes without the size that the file name ends with.
local function imagePath(src, width, height)
    local path = src:gsub('\\', '/'):lower()
    if path:sub(1, 6) == 'img://' then
        path = path:sub(7)
        return vfs.fileExists(path) and path or nil
    end
    if path:sub(1, 8) ~= 'bookart/' then
        path = 'bookart/' .. path
    end
    local function find(candidate)
        if vfs.fileExists(candidate) then
            return candidate
        end
        local dds = candidate:gsub('%.[^%./]*$', '') .. '.dds'
        if vfs.fileExists(dds) then
            return dds
        end
        return nil
    end
    local found = find(path)
    if found == nil then
        local base, extension = path:match('^(.*)(%.[^%./]*)$')
        if base then
            found = find(string.format('%s_%d_%d%s', base, width, height, extension))
        end
    end
    return found
end

-- The font that the engine's book formatter uses for a FONT tag's FACE.
local function fontForFace(face)
    if face:lower() == 'daedric' then
        return 'ScrollFont'
    end
    return nil
end

-- The book's text as a list of blocks, one under another. Each block is one of:
--   { text = ..., align = ..., color = ..., font = ... }  text with line breaks, to wrap at the page's width. The
--                                                          font is nil for the normal font.
--   { image = path, width = ..., height = ..., align = ... }
-- `isBook` is true for Morrowind's books and scrolls, which show nothing after the last <BR> or <P>.
local function parse(raw, isBook)
    local s = types.Book.substituteVariables(raw):gsub('\r', '')
    -- Text from this position on isn't shown.
    local plainTextEnd = #s + 1
    if isBook then
        local lower = s:lower()
        local lastBr, lastP = 0, 0
        for position in lower:gmatch('()<br>') do
            lastBr = position
        end
        for position in lower:gmatch('()<p>') do
            lastP = position
        end
        plainTextEnd = math.max(1, lastBr, lastP)
    end

    local blocks = {}
    local align = ALIGN.left
    local color = nil
    local font = nil
    local buffer = {}
    local ignoreNewlineTags = true
    local ignoreLineEndings = true
    -- The engine's formatter shows each piece of text between two tags as its own block. These two say how it joins
    -- the line breaks at the ends of the pieces.
    local brBeforeLastTag = false
    local isPrevImage = false

    local function flush()
        local plain = table.concat(buffer)
        buffer = {}
        if plain == '' then
            brBeforeLastTag = true
            return
        end
        local brAtStart = plain:sub(1, 1) == '\n'
        local brAtEnd = plain:sub(-1) == '\n'
        if brAtStart and not brBeforeLastTag and not isPrevImage then
            plain = plain:sub(2)
        end
        if plain ~= '' and brAtEnd then
            plain = plain:sub(1, -2)
        end
        if plain ~= '' or brBeforeLastTag or isPrevImage then
            blocks[#blocks + 1] = { text = plain, align = align, color = color, font = font }
        end
        brBeforeLastTag = brAtEnd
    end

    local i = 1
    while i <= #s do
        local c = s:sub(i, i)
        if c == '[' and s:sub(i, i + 11):lower() == '[pagebreak]\n' then
            -- The page breaks of the engine's two-page books mean nothing in one column.
            flush()
            isPrevImage = false
            ignoreNewlineTags = false
            i = i + 12
        elseif c == '<' then
            local close = s:find('>', i + 1, true)
            if close == nil then
                break
            end
            local name, rest = s:sub(i + 1, close - 1):match('^(/?%a*)(.*)$')
            name = name:lower()
            local closing = name:sub(1, 1) == '/'
            if closing then
                name = name:sub(2)
            end
            local attributes = closing and {} or parseAttributes(rest)
            if name == 'br' or name == 'p' then
                if not ignoreNewlineTags then
                    buffer[#buffer + 1] = name == 'br' and '\n' or '\n\n'
                end
                ignoreLineEndings = true
                if name == 'p' and next(attributes) then
                    flush()
                    isPrevImage = false
                    align = ALIGN[(attributes.align or ''):lower()] or align
                end
            elseif name == 'img' or name == 'div' or name == 'font' then
                flush()
                isPrevImage = name == 'img'
                if name == 'img' then
                    ignoreNewlineTags = false
                    local width = tonumber(attributes.width) or 0
                    local height = tonumber(attributes.height) or 0
                    local src = attributes.src
                    if src and src:lower():sub(1, 6) == 'img://' then
                        width = width > 0 and width or 50
                        height = height > 0 and height or 50
                    end
                    local path = src and width > 0 and height > 0 and imagePath(src, width, height)
                    if path then
                        blocks[#blocks + 1] = { image = path, width = width, height = height, align = align }
                    end
                elseif name == 'div' then
                    align = ALIGN[(attributes.align or ''):lower()] or align
                elseif closing then
                    color = nil
                else
                    if attributes.color then
                        color = parseColor(attributes.color)
                    end
                    if attributes.face then
                        font = fontForFace(attributes.face)
                    end
                end
            end
            i = close + 1
        else
            if not ignoreLineEndings or c ~= '\n' then
                if i < plainTextEnd then
                    buffer[#buffer + 1] = c
                end
                ignoreLineEndings = false
                ignoreNewlineTags = false
            end
            i = i + 1
        end
    end
    flush()
    return blocks
end

-- ---------------------------------------------------------------------------------------------------------------
-- Layout

local measured = {}
local measuredCount = 0

-- The blocks of a book with their positions and sizes at a width. They are worked out once per book and width.
local function measure(raw, isBook, width, textSize)
    local key = table.concat({ width, textSize, isBook and 1 or 0, raw }, '\0')
    local result = measured[key]
    if result then
        return result
    end
    if measuredCount > 50 then
        measured = {}
        measuredCount = 0
    end
    local placed = {}
    local y = 0
    for _, block in ipairs(parse(raw, isBook)) do
        if block.image then
            local scale = math.min(1, width / block.width)
            local w = math.floor(block.width * scale)
            local h = math.floor(block.height * scale)
            local x = 0
            if block.align == ALIGN.center then
                x = math.floor((width - w) / 2)
            elseif block.align == ALIGN.right then
                x = width - w
            end
            placed[#placed + 1] = { image = block.image, x = x, y = y, w = w, h = h }
            y = y + h
        else
            local caption = text.escape(block.text)
            -- An empty piece of text still takes a line, as in the engine's books.
            local h = text.height(caption ~= '' and caption or ' ', textSize, width, block.font)
            placed[#placed + 1] = { caption = caption, align = block.align, color = block.color, font = block.font,
                y = y, h = h }
            y = y + h
        end
    end
    result = { blocks = placed, height = y }
    measured[key] = result
    measuredCount = measuredCount + 1
    return result
end

-- The height of a book's text in a column of the given width. `options` are as for books.layout.
function books.height(raw, width, options)
    options = options or {}
    return measure(raw or '', options.isBook ~= false, width, options.textSize or style.textSize.normal).height
end

-- Lays out a book's text in a column of the given width. `options` may have:
--   isBook     true for Morrowind's books and scrolls, which show nothing after the last <BR> or <P> (the default)
--   textSize   the size of the text (normal by default)
--   offset     how far the column is scrolled up (0 by default)
--   clipH      the height of the area that shows; blocks outside it are left out
-- Returns the content of the column, to put in a widget of the column's width, and the column's height.
function books.layout(raw, width, options)
    options = options or {}
    local textSize = options.textSize or style.textSize.normal
    local isBook = options.isBook ~= false
    local offset = options.offset or 0
    local result = measure(raw or '', isBook, width, textSize)
    local content = {}
    for _, block in ipairs(result.blocks) do
        local y = block.y - offset
        if options.clipH == nil or (y + block.h > 0 and y < options.clipH) then
            if block.image then
                content[#content + 1] = {
                    type = ui.TYPE.Image,
                    props = {
                        resource = draw.texture(block.image),
                        position = v2(block.x, y),
                        size = v2(block.w, block.h),
                    },
                }
            else
                content[#content + 1] = {
                    type = ui.TYPE.Text,
                    props = {
                        text = block.caption,
                        font = block.font,
                        textSize = textSize,
                        -- Text that sets no colour is black, as in the engine's books.
                        textColor = block.color or style.color.pageText,
                        multiline = true,
                        wordWrap = true,
                        autoSize = false,
                        position = v2(0, y),
                        size = v2(width, block.h),
                        textAlignH = block.align,
                        textAlignV = ui.ALIGNMENT.Start,
                    },
                }
            end
        end
    end
    return ui.content(content), result.height
end

return books
