#include <components/lua/luastate.hpp>
#include <components/lua/scriptscontainer.hpp>
#include <components/lua/utilpackage.hpp>
#include <components/testing/util.hpp>

#include <gtest/gtest.h>

#include <osg/Vec2f>
#include <osg/Vec3f>

#include <cmath>
#include <fstream>
#include <iterator>
#include <numbers>

namespace
{
    // Runs the real files/data/scripts/omw/camera/dialogue_framing.lua with stand-ins for the engine's packages. The
    // camera stands at the origin looking along +y, with a 90 degree field of view, on a 1600x900 screen.
    constexpr VFS::Path::NormalizedView sScriptPath("scripts/omw/camera/dialogue_framing.lua");

    std::string readScript()
    {
        std::ifstream file(std::filesystem::path{ OPENMW_PROJECT_SOURCE_DIR } / "files" / "data" / "scripts" / "omw"
            / "camera" / "dialogue_framing.lua");
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    constexpr float sFov = std::numbers::pi_v<float> / 2;
    constexpr float sFirstPerson = 0;
    constexpr float sThirdPerson = 1;

    struct LuaDialogueFramingTest : ::testing::Test
    {
        TestingOpenMW::VFSTestFile mScriptFile{ readScript() };
        std::unique_ptr<VFS::Manager> mVFS = TestingOpenMW::createTestVFS({ { sScriptPath, &mScriptFile } });
        LuaUtil::ScriptsConfiguration mConfig;
        LuaUtil::LuaState mLua{ mVFS.get(), &mConfig };
        sol::table mFraming;
        sol::table mSpeaker;

        // What the stand-in packages report, and what the script last set.
        float mCameraMode = sFirstPerson;
        bool mControllerMenus = true;
        float mFrameDuration = 0.1f;
        float mExtraYaw = 0;
        float mExtraPitch = 0;
        osg::Vec3f mFace{ 100, 100, 0 };
        bool mSpeakerValid = true;

        LuaDialogueFramingTest()
        {
            mLua.addInternalLibSearchPath(std::filesystem::path{ OPENMW_PROJECT_SOURCE_DIR } / "components" / "lua");
            sol::state_view lua = mLua.unsafeState();
            mLua.addCommonPackage("openmw.util", LuaUtil::initUtilPackage(lua));

            sol::table camera(lua, sol::create);
            camera["MODE"] = lua.create_table_with("FirstPerson", sFirstPerson, "ThirdPerson", sThirdPerson);
            camera["getMode"] = [this] { return mCameraMode; };
            camera["getPosition"] = [] { return osg::Vec3f(0, 0, 0); };
            camera["getYaw"] = [] { return 0.f; };
            camera["getPitch"] = [] { return 0.f; };
            camera["getFieldOfView"] = [] { return sFov; };
            camera["setExtraYaw"] = [this](float value) { mExtraYaw = value; };
            camera["setExtraPitch"] = [this](float value) { mExtraPitch = value; };
            mLua.addCommonPackage("openmw.camera", camera);

            sol::table core(lua, sol::create);
            core["getRealFrameDuration"] = [this] { return mFrameDuration; };
            mLua.addCommonPackage("openmw.core", core);

            sol::table input(lua, sol::create);
            input["_isControllerMenusEnabled"] = [this] { return mControllerMenus; };
            mLua.addCommonPackage("openmw.input", input);

            sol::table ui(lua, sol::create);
            ui["screenSize"] = [] { return osg::Vec2f(1600, 900); };
            mLua.addCommonPackage("openmw.ui", ui);

            // The speaker's face is three quarters of the way up from the middle of its bounding box.
            mSpeaker = sol::table(lua, sol::create);
            mSpeaker["enabled"] = true;
            mSpeaker["isValid"] = [this](sol::table) { return mSpeakerValid; };
            mSpeaker["getBoundingBox"] = [this](sol::table) {
                return sol::state_view(mLua.unsafeState())
                    .create_table_with("center", mFace - osg::Vec3f(0, 0, 30), "halfSize", osg::Vec3f(20, 20, 40));
            };

            mFraming = mLua.runInNewSandbox(VFS::Path::Normalized(sScriptPath)).get<sol::table>();
        }

        void changeMode(std::string_view mode, bool withSpeaker = false)
        {
            sol::state_view lua = mLua.unsafeState();
            sol::table data = lua.create_table_with("newMode", mode);
            if (withSpeaker)
                data["arg"] = mSpeaker;
            LuaUtil::call(mFraming["onUiModeChanged"], data);
        }

        void update(int frames)
        {
            for (int i = 0; i < frames; ++i)
                LuaUtil::call(mFraming["update"]);
        }

        bool active() { return LuaUtil::call(mFraming["active"]).get<bool>(); }

        // The pitch that puts a face level with the camera above the middle of the screen, where the dialogue panel
        // leaves room for it.
        static float faceAboveMiddle() { return std::atan((1 - 2 * 0.28f) * std::tan(sFov / 2)); }
    };

    TEST_F(LuaDialogueFramingTest, shouldDoNothingBeforeAConversation)
    {
        update(10);
        EXPECT_FALSE(active());
        EXPECT_FLOAT_EQ(mExtraYaw, 0);
        EXPECT_FLOAT_EQ(mExtraPitch, 0);
    }

    TEST_F(LuaDialogueFramingTest, shouldTurnTowardsTheSpeakerInFirstPerson)
    {
        changeMode("Dialogue", true);
        update(100);
        EXPECT_TRUE(active());
        EXPECT_NEAR(mExtraYaw, std::numbers::pi_v<float> / 4, 1e-3f);
    }

    TEST_F(LuaDialogueFramingTest, shouldPutTheFaceAboveTheMiddleOfTheScreen)
    {
        changeMode("Dialogue", true);
        update(100);
        EXPECT_NEAR(mExtraPitch, faceAboveMiddle(), 1e-3f);
    }

    TEST_F(LuaDialogueFramingTest, shouldNotFrameInThirdPerson)
    {
        mCameraMode = sThirdPerson;
        changeMode("Dialogue", true);
        update(100);
        EXPECT_FALSE(active());
        EXPECT_FLOAT_EQ(mExtraYaw, 0);
    }

    TEST_F(LuaDialogueFramingTest, shouldNotFrameWhenControllerMenusAreOff)
    {
        mControllerMenus = false;
        changeMode("Dialogue", true);
        update(100);
        EXPECT_FALSE(active());
        EXPECT_FLOAT_EQ(mExtraYaw, 0);
    }

    TEST_F(LuaDialogueFramingTest, shouldKeepFramingDuringBarterWithTheFaceLeftOfThePanel)
    {
        changeMode("Dialogue", true);
        update(100);
        changeMode("Barter");
        update(100);
        EXPECT_TRUE(active());
        // Looking to the right of the face puts it in the middle of the space left of the panel.
        const float halfWidth = std::atan(std::tan(sFov / 2) * 1600 / 900);
        EXPECT_NEAR(mExtraYaw, std::numbers::pi_v<float> / 4 + std::atan((1 - 2 * 0.2f) * std::tan(halfWidth)), 1e-3f);
    }

    TEST_F(LuaDialogueFramingTest, shouldTurnBackWhenTheConversationEnds)
    {
        changeMode("Dialogue", true);
        update(100);
        changeMode("Interface");
        update(200);
        EXPECT_FALSE(active());
        EXPECT_EQ(mExtraYaw, 0);
        EXPECT_EQ(mExtraPitch, 0);
    }

    TEST_F(LuaDialogueFramingTest, shouldEaseRatherThanJump)
    {
        mFrameDuration = 0.01f;
        changeMode("Dialogue", true);
        update(1);
        EXPECT_GT(mExtraYaw, 0);
        EXPECT_LT(mExtraYaw, std::numbers::pi_v<float> / 4 * 0.1f);
    }

    TEST_F(LuaDialogueFramingTest, speakerThatBecomesInvalidShouldTurnBack)
    {
        changeMode("Dialogue", true);
        update(100);
        mSpeakerValid = false;
        update(200);
        EXPECT_NEAR(mExtraYaw, 0, 1e-3f);
        EXPECT_NEAR(mExtraPitch, 0, 1e-3f);
    }
}
