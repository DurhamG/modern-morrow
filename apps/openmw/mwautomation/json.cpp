#include "json.hpp"

#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace MWAutomation
{
    namespace
    {
        class Parser
        {
        public:
            explicit Parser(std::string_view text)
                : mText(text)
            {
            }

            Json parseDocument()
            {
                Json value = parseValue();
                skipSpace();
                if (mPos != mText.size())
                    fail("unexpected text after the value");
                return value;
            }

        private:
            std::string_view mText;
            std::size_t mPos = 0;

            [[noreturn]] void fail(std::string_view message) const
            {
                throw std::runtime_error(
                    "JSON parse error at offset " + std::to_string(mPos) + ": " + std::string(message));
            }

            void skipSpace()
            {
                while (mPos < mText.size()
                    && (mText[mPos] == ' ' || mText[mPos] == '\t' || mText[mPos] == '\n' || mText[mPos] == '\r'))
                    ++mPos;
            }

            char peek()
            {
                skipSpace();
                if (mPos >= mText.size())
                    fail("unexpected end of input");
                return mText[mPos];
            }

            void expect(std::string_view word)
            {
                if (mText.substr(mPos, word.size()) != word)
                    fail("expected " + std::string(word));
                mPos += word.size();
            }

            Json parseValue()
            {
                switch (peek())
                {
                    case '{':
                        return parseObject();
                    case '[':
                        return parseArray();
                    case '"':
                        return Json(parseString());
                    case 't':
                        expect("true");
                        return Json(true);
                    case 'f':
                        expect("false");
                        return Json(false);
                    case 'n':
                        expect("null");
                        return Json();
                    default:
                        return parseNumber();
                }
            }

            Json parseObject()
            {
                ++mPos;
                Json::Object members;
                if (peek() == '}')
                {
                    ++mPos;
                    return Json(std::move(members));
                }
                while (true)
                {
                    if (peek() != '"')
                        fail("expected a string key");
                    std::string key = parseString();
                    if (peek() != ':')
                        fail("expected ':'");
                    ++mPos;
                    Json value = parseValue();
                    members.emplace_back(std::move(key), std::move(value));
                    const char next = peek();
                    ++mPos;
                    if (next == '}')
                        return Json(std::move(members));
                    if (next != ',')
                        fail("expected ',' or '}'");
                }
            }

            Json parseArray()
            {
                ++mPos;
                Json::Array items;
                if (peek() == ']')
                {
                    ++mPos;
                    return Json(std::move(items));
                }
                while (true)
                {
                    items.push_back(parseValue());
                    const char next = peek();
                    ++mPos;
                    if (next == ']')
                        return Json(std::move(items));
                    if (next != ',')
                        fail("expected ',' or ']'");
                }
            }

            static void appendUtf8(std::string& out, std::uint32_t code)
            {
                if (code < 0x80)
                    out += static_cast<char>(code);
                else if (code < 0x800)
                {
                    out += static_cast<char>(0xC0 | (code >> 6));
                    out += static_cast<char>(0x80 | (code & 0x3F));
                }
                else if (code < 0x10000)
                {
                    out += static_cast<char>(0xE0 | (code >> 12));
                    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (code & 0x3F));
                }
                else
                {
                    out += static_cast<char>(0xF0 | (code >> 18));
                    out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
                    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (code & 0x3F));
                }
            }

            std::uint32_t parseHex4()
            {
                if (mPos + 4 > mText.size())
                    fail("truncated \\u escape");
                std::uint32_t code = 0;
                const auto result = std::from_chars(mText.data() + mPos, mText.data() + mPos + 4, code, 16);
                if (result.ptr != mText.data() + mPos + 4)
                    fail("bad \\u escape");
                mPos += 4;
                return code;
            }

            std::string parseString()
            {
                ++mPos;
                std::string out;
                while (true)
                {
                    if (mPos >= mText.size())
                        fail("unterminated string");
                    const char c = mText[mPos++];
                    if (c == '"')
                        return out;
                    if (c != '\\')
                    {
                        out += c;
                        continue;
                    }
                    if (mPos >= mText.size())
                        fail("unterminated escape");
                    const char escaped = mText[mPos++];
                    switch (escaped)
                    {
                        case '"':
                        case '\\':
                        case '/':
                            out += escaped;
                            break;
                        case 'b':
                            out += '\b';
                            break;
                        case 'f':
                            out += '\f';
                            break;
                        case 'n':
                            out += '\n';
                            break;
                        case 'r':
                            out += '\r';
                            break;
                        case 't':
                            out += '\t';
                            break;
                        case 'u':
                        {
                            std::uint32_t code = parseHex4();
                            if (code >= 0xD800 && code < 0xDC00 && mText.substr(mPos, 2) == "\\u")
                            {
                                mPos += 2;
                                const std::uint32_t low = parseHex4();
                                code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                            }
                            appendUtf8(out, code);
                            break;
                        }
                        default:
                            fail("unknown escape");
                    }
                }
            }

            Json parseNumber()
            {
                const std::size_t start = mPos;
                while (mPos < mText.size()
                    && (std::isdigit(static_cast<unsigned char>(mText[mPos])) || mText[mPos] == '-'
                        || mText[mPos] == '+' || mText[mPos] == '.' || mText[mPos] == 'e' || mText[mPos] == 'E'))
                    ++mPos;
                double value = 0;
                const auto result = std::from_chars(mText.data() + start, mText.data() + mPos, value);
                if (start == mPos || result.ptr != mText.data() + mPos)
                {
                    mPos = start;
                    fail("expected a value");
                }
                return Json(value);
            }
        };

        void dumpString(std::string& out, std::string_view value)
        {
            out += '"';
            for (const char c : value)
            {
                switch (c)
                {
                    case '"':
                        out += "\\\"";
                        break;
                    case '\\':
                        out += "\\\\";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    case '\r':
                        out += "\\r";
                        break;
                    case '\t':
                        out += "\\t";
                        break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20)
                        {
                            char buffer[8];
                            std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(c));
                            out += buffer;
                        }
                        else
                            out += c;
                }
            }
            out += '"';
        }
    }

    Json Json::object(std::initializer_list<std::pair<std::string, Json>> members)
    {
        return Json(Object(members));
    }

    Json Json::array(std::initializer_list<Json> items)
    {
        return Json(Array(items));
    }

    Json Json::parse(std::string_view text)
    {
        return Parser(text).parseDocument();
    }

    std::string Json::dump() const
    {
        std::string out;
        dump(out);
        return out;
    }

    void Json::dump(std::string& out) const
    {
        if (isNull())
            out += "null";
        else if (isBool())
            out += asBool() ? "true" : "false";
        else if (isNumber())
        {
            const double value = asNumber();
            if (!std::isfinite(value))
                out += "null";
            else if (value == std::floor(value) && std::abs(value) < 1e15)
                out += std::to_string(static_cast<std::int64_t>(value));
            else
            {
                char buffer[32];
                const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
                out.append(buffer, result.ptr);
            }
        }
        else if (isString())
            dumpString(out, asString());
        else if (isArray())
        {
            out += '[';
            bool first = true;
            for (const Json& item : asArray())
            {
                if (!first)
                    out += ',';
                first = false;
                item.dump(out);
            }
            out += ']';
        }
        else
        {
            out += '{';
            bool first = true;
            for (const auto& [key, value] : asObject())
            {
                if (!first)
                    out += ',';
                first = false;
                dumpString(out, key);
                out += ':';
                value.dump(out);
            }
            out += '}';
        }
    }

    const Json* Json::find(std::string_view key) const
    {
        if (!isObject())
            return nullptr;
        for (const auto& [name, value] : asObject())
            if (name == key)
                return &value;
        return nullptr;
    }

    void Json::set(std::string_view key, Json value)
    {
        if (isNull())
            mValue = Object();
        Object& members = std::get<Object>(mValue);
        for (auto& [name, existing] : members)
        {
            if (name == key)
            {
                existing = std::move(value);
                return;
            }
        }
        members.emplace_back(std::string(key), std::move(value));
    }

    void Json::push(Json value)
    {
        if (isNull())
            mValue = Array();
        std::get<Array>(mValue).push_back(std::move(value));
    }

    std::string Json::getString(std::string_view key, std::string_view fallback) const
    {
        const Json* value = find(key);
        if (value == nullptr || value->isNull())
            return std::string(fallback);
        if (!value->isString())
            throw std::runtime_error("field '" + std::string(key) + "' must be a string");
        return value->asString();
    }

    double Json::getNumber(std::string_view key, double fallback) const
    {
        const Json* value = find(key);
        if (value == nullptr || value->isNull())
            return fallback;
        if (!value->isNumber())
            throw std::runtime_error("field '" + std::string(key) + "' must be a number");
        return value->asNumber();
    }

    bool Json::getBool(std::string_view key, bool fallback) const
    {
        const Json* value = find(key);
        if (value == nullptr || value->isNull())
            return fallback;
        if (!value->isBool())
            throw std::runtime_error("field '" + std::string(key) + "' must be true or false");
        return value->asBool();
    }
}
