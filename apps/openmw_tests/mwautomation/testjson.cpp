#include "apps/openmw/mwautomation/json.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace MWAutomation
{
    namespace
    {
        TEST(MWAutomationJsonTest, shouldParseAnObjectWithEachKindOfValue)
        {
            const Json json = Json::parse(
                R"({"text": "hi", "number": 3, "yes": true, "no": false, "nothing": null, "list": [1, "a"],)"
                R"( "inner": {"key": "value"}})");
            ASSERT_TRUE(json.isObject());
            EXPECT_EQ(json.getString("text"), "hi");
            EXPECT_EQ(json.getNumber("number"), 3);
            EXPECT_TRUE(json.getBool("yes"));
            EXPECT_FALSE(json.getBool("no", true));
            ASSERT_NE(json.find("nothing"), nullptr);
            EXPECT_TRUE(json.find("nothing")->isNull());
            ASSERT_TRUE(json.find("list")->isArray());
            EXPECT_EQ(json.find("list")->asArray().size(), 2);
            EXPECT_EQ(json.find("list")->asArray()[1].asString(), "a");
            EXPECT_EQ(json.find("inner")->getString("key"), "value");
        }

        TEST(MWAutomationJsonTest, missingKeysShouldGiveTheFallback)
        {
            const Json json = Json::parse("{}");
            EXPECT_EQ(json.find("missing"), nullptr);
            EXPECT_EQ(json.getString("missing", "fallback"), "fallback");
            EXPECT_EQ(json.getNumber("missing", 7), 7);
        }

        TEST(MWAutomationJsonTest, wrongTypeShouldThrowNamingTheField)
        {
            const Json json = Json::parse(R"({"frames": "ten"})");
            try
            {
                json.getNumber("frames");
                FAIL() << "expected an error";
            }
            catch (const std::runtime_error& e)
            {
                EXPECT_NE(std::string(e.what()).find("frames"), std::string::npos);
            }
        }

        TEST(MWAutomationJsonTest, shouldParseEscapes)
        {
            const Json json = Json::parse(R"(["quote \" backslash \\ newline \n tab \t", "café"])");
            EXPECT_EQ(json.asArray()[0].asString(), "quote \" backslash \\ newline \n tab \t");
            EXPECT_EQ(json.asArray()[1].asString(), "caf\xC3\xA9");
        }

        TEST(MWAutomationJsonTest, shouldParseNegativeAndFractionalNumbers)
        {
            const Json json = Json::parse("[-1, 0.5, -2.25, 1e3, 12]");
            const Json::Array& numbers = json.asArray();
            EXPECT_EQ(numbers[0].asNumber(), -1);
            EXPECT_EQ(numbers[1].asNumber(), 0.5);
            EXPECT_EQ(numbers[2].asNumber(), -2.25);
            EXPECT_EQ(numbers[3].asNumber(), 1000);
            EXPECT_EQ(numbers[4].asNumber(), 12);
        }

        TEST(MWAutomationJsonTest, dumpShouldRoundTrip)
        {
            Json json = Json::object({ { "name", "Fargoth's \"ring\"" }, { "count", 3 }, { "flag", true } });
            json.set("list", Json::array({ 1, "two", Json() }));
            const Json read = Json::parse(json.dump());
            EXPECT_EQ(read.dump(), json.dump());
            EXPECT_EQ(read.getString("name"), "Fargoth's \"ring\"");
            EXPECT_EQ(read.find("list")->asArray().size(), 3);
        }

        TEST(MWAutomationJsonTest, dumpShouldEscapeQuotesAndNewlines)
        {
            const std::string dumped = Json("say \"hi\"\nthen go").dump();
            EXPECT_EQ(dumped.find('\n'), std::string::npos);
            EXPECT_EQ(Json::parse(dumped).asString(), "say \"hi\"\nthen go");
        }

        TEST(MWAutomationJsonTest, shouldRejectTrailingText)
        {
            EXPECT_THROW(Json::parse("{} x"), std::runtime_error);
        }

        TEST(MWAutomationJsonTest, shouldRejectAnUnclosedString)
        {
            EXPECT_THROW(Json::parse(R"({"key": "value)"), std::runtime_error);
        }

        TEST(MWAutomationJsonTest, shouldRejectAnUnclosedObject)
        {
            EXPECT_THROW(Json::parse(R"({"key": 1)"), std::runtime_error);
            EXPECT_THROW(Json::parse("[1, 2"), std::runtime_error);
        }

        TEST(MWAutomationJsonTest, shouldRejectEmptyInput)
        {
            EXPECT_THROW(Json::parse(""), std::runtime_error);
            EXPECT_THROW(Json::parse("   "), std::runtime_error);
        }

        TEST(MWAutomationJsonTest, nestingShouldBeAllowedTo64Levels)
        {
            const std::string text = std::string(64, '[') + std::string(64, ']');
            EXPECT_NO_THROW(Json::parse(text));
        }

        TEST(MWAutomationJsonTest, deeplyNestedArraysShouldFailCleanly)
        {
            EXPECT_THROW(Json::parse(std::string(65, '[') + std::string(65, ']')), std::runtime_error);
            EXPECT_THROW(Json::parse(std::string(10000, '[')), std::runtime_error);
        }
    }
}
