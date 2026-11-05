-- Measuring, wrapping and fitting text with ui.measureText.
-- ui.measureText only works on the main thread: in onFrame, input handlers and the show and hide functions of a
-- window. The screens only draw from those places.

local ui = require('openmw.ui')

local text = {}

local widths = {}
local widthCount = 0

-- Text for a caption. MyGUI reads `#` as the start of a colour code, so names must escape it.
function text.escape(s)
    return (tostring(s):gsub('#', '##'))
end

-- The width of one line of text, as a Text widget with this text size draws it. `s` is a caption, already escaped.
function text.width(s, size)
    local key = size .. '\0' .. s
    local width = widths[key]
    if width == nil then
        if widthCount > 4000 then
            widths = {}
            widthCount = 0
        end
        width = ui.measureText(s, { textSize = size }).x
        widths[key] = width
        widthCount = widthCount + 1
    end
    return width
end

-- The height of text wrapped at a width, as a Text widget with `multiline` and `wordWrap` draws it. `font` is the
-- widget's font, or nil for the normal one.
function text.height(s, size, width, font)
    return ui.measureText(s, { textSize = size, multiline = true, wordWrap = true, width = width, font = font }).y
end

-- Splits text into lines that fit the width, at most maxLines of them (2 by default). The last line holds whatever is
-- left. The text is plain, and each returned line is escaped.
function text.wrap(s, width, size, maxLines)
    maxLines = maxLines or 2
    local lines = { '' }
    for word in tostring(s):gmatch('%S+') do
        local line = lines[#lines]
        local longer = line == '' and word or (line .. ' ' .. word)
        if line ~= '' and #lines < maxLines and text.width(text.escape(longer), size) > width then
            lines[#lines + 1] = word
        else
            lines[#lines] = longer
        end
    end
    for i, line in ipairs(lines) do
        lines[i] = text.escape(line)
    end
    return lines
end

-- Shortens plain text with "..." until it fits the width. Returns the escaped caption.
function text.fit(s, width, size)
    local caption = text.escape(s)
    if text.width(caption, size) <= width then
        return caption
    end
    local chars = {}
    for _, code in utf8.codes(tostring(s)) do
        chars[#chars + 1] = utf8.char(code)
    end
    local low, high = 0, #chars
    while low < high do
        local mid = math.ceil((low + high) / 2)
        if text.width(text.escape(table.concat(chars, '', 1, mid)) .. '...', size) <= width then
            low = mid
        else
            high = mid - 1
        end
    end
    return text.escape(table.concat(chars, '', 1, low)) .. '...'
end

-- Formats a number with at most two decimals, and none for whole numbers.
function text.number(value)
    if math.abs(value - math.floor(value + 0.5)) < 0.005 then
        return tostring(math.floor(value + 0.5))
    end
    local s = string.format('%.2f', value):gsub('0+$', ''):gsub('%.$', '')
    return s
end

return text
