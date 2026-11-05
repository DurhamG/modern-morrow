#ifndef MWLUA_PLAYERSCRIPTS_H
#define MWLUA_PLAYERSCRIPTS_H

#include <string>
#include <string_view>
#include <vector>

#include <SDL_events.h>

#include <components/lua/util.hpp>
#include <components/sdlutil/events.hpp>

#include "../mwbase/luamanager.hpp"

#include "inputprocessor.hpp"
#include "localscripts.hpp"

namespace MWLua
{

    class PlayerScripts : public LocalScripts
    {
    public:
        PlayerScripts(LuaUtil::LuaState* lua, const LObject& obj)
            : LocalScripts(lua, obj)
            , mInputProcessor(this)
        {
            registerEngineHandlers({ &mConsoleCommandHandlers, &mOnFrameHandlers, &mQuestUpdate,
                &mViewportResizedHandlers, &mUiModeChanged, &mInteractiveMessageRequested, &mMessageRequested });
        }

        void processInputEvent(const MWBase::LuaManager::InputEvent& event)
        {
            mInputProcessor.processInputEvent(event);
        }

        void onFrame(float dt) { callEngineHandlers(mOnFrameHandlers, dt); }
        void onQuestUpdate(std::string_view questId, int stage) { callEngineHandlers(mQuestUpdate, questId, stage); }

        bool consoleCommand(
            const std::string& consoleMode, const std::string& command, const sol::object& selectedObject)
        {
            callEngineHandlers(mConsoleCommandHandlers, consoleMode, command, selectedObject);
            return !mConsoleCommandHandlers.mList.empty();
        }

        void onViewportResized(int width, int height) { callEngineHandlers(mViewportResizedHandlers, width, height); }

        // `arg` is either forwarded from MWGui::pushGuiMode or empty
        void uiModeChanged(ObjectId arg, bool byLuaAction)
        {
            if (arg.isZeroOrUnset())
                callEngineHandlers(mUiModeChanged, byLuaAction);
            else
                callEngineHandlers(mUiModeChanged, byLuaAction, LObject(arg));
        }

        void interactiveMessageRequested(int id, std::string_view message, const std::vector<std::string>& buttons)
        {
            sol::table buttonTable(mLua.unsafeState(), sol::create);
            for (std::size_t i = 0; i < buttons.size(); ++i)
                buttonTable[LuaUtil::toLuaIndex(i)] = buttons[i];
            callEngineHandlers(mInteractiveMessageRequested, id, message, buttonTable);
        }

        void messageRequested(std::string_view message) { callEngineHandlers(mMessageRequested, message); }

    private:
        friend class MWLua::InputProcessor<PlayerScripts>;
        InputProcessor<PlayerScripts> mInputProcessor;
        EngineHandlerList mConsoleCommandHandlers{ "onConsoleCommand" };
        EngineHandlerList mOnFrameHandlers{ "onFrame" };
        EngineHandlerList mQuestUpdate{ "onQuestUpdate" };
        EngineHandlerList mViewportResizedHandlers{ "onViewportResized" };
        EngineHandlerList mUiModeChanged{ "_onUiModeChanged" };
        EngineHandlerList mInteractiveMessageRequested{ "_onInteractiveMessage" };
        EngineHandlerList mMessageRequested{ "_onMessage" };
    };

}

#endif // MWLUA_PLAYERSCRIPTS_H
