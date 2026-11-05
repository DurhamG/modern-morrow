#ifndef MWGUI_MESSAGE_BOX_H
#define MWGUI_MESSAGE_BOX_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "windowbase.hpp"

namespace MyGUI
{
    class Widget;
    class Button;
    class EditBox;
}

namespace MWGui
{
    class InteractiveMessageBox;
    class MessageBoxManager;
    class MessageBox;
    class MessageBoxManager
    {
    public:
        MessageBoxManager(float timePerChar);
        ~MessageBoxManager();
        void onFrame(float frameDuration);
        void createMessageBox(std::string_view message, bool stat = false);
        void removeStaticMessageBox();
        /// @param luaCanAnswer Whether the player's Lua scripts may show and answer this box, if they answer
        /// interactive message boxes. A caller that waits for the answer in a loop that doesn't run Lua must pass
        /// false.
        /// @param onButtonPressed Called with the index of the pressed button, unless another interactive message box
        /// replaces this one first.
        bool createInteractiveMessageBox(std::string_view message, const std::vector<std::string>& buttons,
            bool immediate = false, int defaultFocus = -1, bool luaCanAnswer = true,
            std::function<void(int)> onButtonPressed = {});
        bool isInteractiveMessageBox() const;

        /// While set, the manager doesn't show the interactive message boxes that Lua can answer. It sends each one
        /// to the player's Lua scripts instead. They show it in their own way, and answer with
        /// answerInteractiveMessage.
        void setLuaAnswersInteractiveMessages(bool answers);

        /// Presses the button with the given index in the interactive message box that was sent to Lua with the
        /// given id. Does nothing if that box is no longer waiting for an answer.
        void answerInteractiveMessage(int id, int button);

        std::size_t getMessagesCount();

        const InteractiveMessageBox* getInteractiveMessageBox() const { return mInterMessageBoxe.get(); }

        /// Remove all message boxes
        void clear();

        bool removeMessageBox(MessageBox* msgbox);

        /// @param reset Reset the pressed button to -1 after reading it.
        int readPressedButton(bool reset = true);

        void resetInteractiveMessageBox();

        void setLastButtonPressed(int index);

        typedef MyGUI::delegates::MultiDelegate<int> EventHandle_Int;

        // Note: this delegate unassigns itself after it was fired, i.e. works once.
        EventHandle_Int eventButtonPressed;

        void onButtonPressed(int button)
        {
            eventButtonPressed(button);
            eventButtonPressed.clear();
            if (std::function<void(int)> callback = std::exchange(mButtonPressedCallback, nullptr))
                callback(button);
        }

        void setVisible(bool value);

        const std::vector<std::unique_ptr<MessageBox>>& getActiveMessageBoxes() const;

    private:
        /// An interactive message box that was sent to the player's Lua scripts instead of being shown.
        struct LuaInteractiveMessage
        {
            int mId;
            std::string mMessage;
            std::vector<std::string> mButtons;
            bool mImmediate;
            int mDefaultFocus;
            int mButtonPressed = -1;
        };

        std::vector<std::unique_ptr<MessageBox>> mMessageBoxes;
        std::unique_ptr<InteractiveMessageBox> mInterMessageBoxe;
        std::optional<LuaInteractiveMessage> mLuaInteractiveMessage;
        std::function<void(int)> mButtonPressedCallback;
        int mLastLuaInteractiveMessageId = 0;
        bool mLuaAnswersInteractiveMessages = false;
        MessageBox* mStaticMessageBox;
        float mMessageBoxSpeed;
        int mLastButtonPressed;
        bool mVisible = true;
    };

    class MessageBox : public Layout
    {
    public:
        MessageBox(MessageBoxManager& parMessageBoxManager, std::string_view message);
        const std::string& getMessage() { return mMessage; }
        int getHeight();
        void update(int height);
        void setVisible(bool value);

        float mCurrentTime;
        float mMaxTime;

    protected:
        MessageBoxManager& mMessageBoxManager;
        std::string mMessage;
        MyGUI::EditBox* mMessageWidget;
        int mBottomPadding;
        int mNextBoxPadding;
    };

    class InteractiveMessageBox : public WindowModal
    {
    public:
        InteractiveMessageBox(MessageBoxManager& parMessageBoxManager, const std::string& message,
            const std::vector<std::string>& buttons, bool immediate, size_t defaultFocus);
        void mousePressed(MyGUI::Widget* widget);
        int readPressedButton();

        MyGUI::Widget* getDefaultKeyFocus() override;

        bool exit() override { return false; }

        bool mMarkedToDelete;

        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;

    private:
        void buttonActivated(MyGUI::Widget* widget);

        MessageBoxManager& mMessageBoxManager;
        MyGUI::EditBox* mMessageWidget;
        MyGUI::Widget* mButtonsWidget;
        std::vector<MyGUI::Button*> mButtons;

        int mButtonPressed;
        size_t mDefaultFocus;
        bool mImmediate;
        size_t mControllerFocus = 0;
    };

}

#endif
