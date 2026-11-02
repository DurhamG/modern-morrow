#ifndef OPENMW_MWAUTOMATION_JSON_H
#define OPENMW_MWAUTOMATION_JSON_H

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace MWAutomation
{
    // A small JSON value type for the automation protocol. Objects keep their keys in insertion order.
    class Json
    {
    public:
        using Array = std::vector<Json>;
        using Object = std::vector<std::pair<std::string, Json>>;

        Json() = default;
        Json(std::nullptr_t) {}
        Json(bool value)
            : mValue(value)
        {
        }
        Json(int value)
            : mValue(static_cast<double>(value))
        {
        }
        Json(unsigned value)
            : mValue(static_cast<double>(value))
        {
        }
        Json(std::int64_t value)
            : mValue(static_cast<double>(value))
        {
        }
        Json(std::uint64_t value)
            : mValue(static_cast<double>(value))
        {
        }
        Json(float value)
            : mValue(static_cast<double>(value))
        {
        }
        Json(double value)
            : mValue(value)
        {
        }
        Json(const char* value)
            : mValue(std::string(value))
        {
        }
        Json(std::string_view value)
            : mValue(std::string(value))
        {
        }
        Json(std::string value)
            : mValue(std::move(value))
        {
        }
        Json(Array value)
            : mValue(std::move(value))
        {
        }
        Json(Object value)
            : mValue(std::move(value))
        {
        }

        static Json object(std::initializer_list<std::pair<std::string, Json>> members = {});
        static Json array(std::initializer_list<Json> items = {});

        // Throws std::runtime_error on malformed input.
        static Json parse(std::string_view text);

        // Writes the value on a single line.
        std::string dump() const;

        bool isNull() const { return std::holds_alternative<std::nullptr_t>(mValue); }
        bool isBool() const { return std::holds_alternative<bool>(mValue); }
        bool isNumber() const { return std::holds_alternative<double>(mValue); }
        bool isString() const { return std::holds_alternative<std::string>(mValue); }
        bool isArray() const { return std::holds_alternative<Array>(mValue); }
        bool isObject() const { return std::holds_alternative<Object>(mValue); }

        bool asBool() const { return std::get<bool>(mValue); }
        double asNumber() const { return std::get<double>(mValue); }
        const std::string& asString() const { return std::get<std::string>(mValue); }
        const Array& asArray() const { return std::get<Array>(mValue); }
        Array& asArray() { return std::get<Array>(mValue); }
        const Object& asObject() const { return std::get<Object>(mValue); }

        // Returns nullptr when this is not an object or the key is missing.
        const Json* find(std::string_view key) const;

        // Turns a null value into an object, then sets or replaces the key.
        void set(std::string_view key, Json value);

        // Turns a null value into an array, then appends.
        void push(Json value);

        // Typed lookups for request fields. They throw std::runtime_error naming the field when the field is
        // present but has the wrong type.
        std::string getString(std::string_view key, std::string_view fallback = {}) const;
        double getNumber(std::string_view key, double fallback = 0) const;
        bool getBool(std::string_view key, bool fallback = false) const;

    private:
        std::variant<std::nullptr_t, bool, double, std::string, Array, Object> mValue;

        void dump(std::string& out) const;
    };
}

#endif
