#include "trainingwindow.hpp"

#include <MyGUI_Button.h>
#include <MyGUI_Gui.h>
#include <MyGUI_TextIterator.h>

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/npcstats.hpp"
#include "../mwmechanics/services.hpp"

#include <components/esm3/loadclas.hpp>
#include <components/settings/values.hpp>

#include "tooltips.hpp"

namespace MWGui
{

    TrainingWindow::TrainingWindow()
        : WindowBase("openmw_trainingwindow.layout")
    {
        getWidget(mTrainingOptions, "TrainingOptions");
        getWidget(mCancelButton, "OkButton");
        getWidget(mPlayerGold, "PlayerGold");

        mCancelButton->eventMouseButtonClick += MyGUI::newDelegate(this, &TrainingWindow::onCancelButtonClicked);

        mTimeAdvancer.eventProgressChanged += MyGUI::newDelegate(this, &TrainingWindow::onTrainingProgressChanged);
        mTimeAdvancer.eventFinished += MyGUI::newDelegate(this, &TrainingWindow::onTrainingFinished);

        if (Settings::gui().mControllerMenus)
        {
            mDisableGamepadCursor = true;
            mControllerButtons.mA = "#{Interface:Buy}";
            mControllerButtons.mB = "#{Interface:Cancel}";
        }
    }

    void TrainingWindow::onOpen()
    {
        if (mTimeAdvancer.isRunning())
        {
            mProgressBar.setVisible(true);
            setVisible(false);
        }
        else
            mProgressBar.setVisible(false);

        center();
    }

    void TrainingWindow::setPtr(const MWWorld::Ptr& actor)
    {
        if (actor.isEmpty() || !actor.getClass().isActor())
            throw std::runtime_error("Invalid argument in TrainingWindow::setPtr");
        mPtr = actor;

        MWWorld::Ptr player = MWMechanics::getPlayer();
        int playerGold = player.getClass().getContainerStore(player).count(MWWorld::ContainerStore::sGoldId);

        mPlayerGold->setCaptionWithReplacing("#{sGold}: " + MyGUI::utility::toString(playerGold));

        const MWWorld::Store<ESM::Skill>& skillStore = MWBase::Environment::get().getESMStore()->get<ESM::Skill>();
        const std::vector<ESM::RefId> skills = MWMechanics::getTrainingSkills(actor);

        MyGUI::EnumeratorWidgetPtr widgets = mTrainingOptions->getEnumerator();
        MyGUI::Gui::getInstance().destroyWidgets(widgets);

        const int lineHeight = Settings::gui().mFontSize + 2;

        mTrainingButtons.clear();
        for (size_t i = 0; i < skills.size(); ++i)
        {
            const ESM::Skill* skill = skillStore.find(skills[i]);
            const int price = MWMechanics::getTrainingPrice(mPtr, skill->mId);

            MyGUI::Button* button = mTrainingOptions->createWidget<MyGUI::Button>(price <= playerGold
                    ? "SandTextButton"
                    : "SandTextButtonDisabled", // can't use setEnabled since that removes tooltip
                MyGUI::IntCoord(4, static_cast<int>(3 + i * lineHeight), mTrainingOptions->getWidth() - 10, lineHeight),
                MyGUI::Align::Default);

            button->setUserData(skill);
            button->eventMouseButtonClick += MyGUI::newDelegate(this, &TrainingWindow::onTrainingSelected);

            button->setCaptionWithReplacing(
                MyGUI::TextIterator::toTagsString(skill->mName) + "  - " + MyGUI::utility::toString(price) + "#{sgp}");

            button->setSize(button->getTextSize().width + 12, button->getSize().height);

            ToolTips::createSkillToolTip(button, skill->mId);

            if (price <= playerGold)
                mTrainingButtons.emplace_back(button);
        }

        if (Settings::gui().mControllerMenus)
        {
            mControllerFocus = 0;
            if (mTrainingButtons.size() > 0)
                mTrainingButtons[0]->setStateSelected(true);
        }

        center();
    }

    void TrainingWindow::onReferenceUnavailable()
    {
        MWBase::Environment::get().getWindowManager()->removeGuiMode(GM_Training);
    }

    void TrainingWindow::onCancelButtonClicked(MyGUI::Widget* /*sender*/)
    {
        MWBase::Environment::get().getWindowManager()->removeGuiMode(GM_Training);
    }

    void TrainingWindow::onTrainingSelected(MyGUI::Widget* sender)
    {
        const ESM::Skill* skill = *sender->getUserData<const ESM::Skill*>();

        MWWorld::Ptr player = MWBase::Environment::get().getWorld()->getPlayerPtr();

        const int price = MWMechanics::getTrainingPrice(mPtr, skill->mId);

        if (price > player.getClass().getContainerStore(player).count(MWWorld::ContainerStore::sGoldId))
            return;

        switch (MWMechanics::getTrainingRefusal(mPtr, skill->mId))
        {
            case MWMechanics::TrainingRefusal::None:
                break;
            case MWMechanics::TrainingRefusal::TrainerSkillTooLow:
                MWBase::Environment::get().getWindowManager()->messageBox("#{sServiceTrainingWords}");
                return;
            case MWMechanics::TrainingRefusal::AttributeTooLow:
                MWBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage17}");
                return;
        }

        // increase skill
        MWBase::Environment::get().getLuaManager()->skillLevelUp(player, skill->mId, "trainer");

        // remove gold
        player.getClass().getContainerStore(player).remove(MWWorld::ContainerStore::sGoldId, price);

        // add gold to NPC trading gold pool
        MWMechanics::NpcStats& npcStats = mPtr.getClass().getNpcStats(mPtr);
        npcStats.setGoldPool(npcStats.getGoldPool() + price);

        setVisible(false);
        mProgressBar.setVisible(true);
        mProgressBar.setProgress(0, 2);
        mTimeAdvancer.run(2);

        MWBase::Environment::get().getWindowManager()->fadeScreenOut(0.2f);
        MWBase::Environment::get().getWindowManager()->fadeScreenIn(0.2f, false, 0.2f);
    }

    void TrainingWindow::onTrainingProgressChanged(int cur, int total)
    {
        mProgressBar.setProgress(cur, total);
    }

    void TrainingWindow::onTrainingFinished()
    {
        mProgressBar.setVisible(false);

        // advance time
        MWBase::Environment::get().getMechanicsManager()->rest(2, false);
        MWBase::Environment::get().getWorld()->advanceTime(2);

        // go back to game mode
        MWBase::Environment::get().getWindowManager()->removeGuiMode(GM_Training);
        MWBase::Environment::get().getWindowManager()->exitCurrentGuiMode();
    }

    void TrainingWindow::onFrame(float dt)
    {
        checkReferenceAvailable();
        mTimeAdvancer.onFrame(dt);
    }

    bool TrainingWindow::exit()
    {
        return !mTimeAdvancer.isRunning();
    }

    bool TrainingWindow::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (arg.button == SDL_CONTROLLER_BUTTON_A)
        {
            if (mControllerFocus < mTrainingButtons.size())
                onTrainingSelected(mTrainingButtons[mControllerFocus]);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_B)
        {
            onCancelButtonClicked(mCancelButton);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP)
        {
            if (mTrainingButtons.size() <= 1)
                return true;

            setControllerFocus(mTrainingButtons, mControllerFocus, false);
            mControllerFocus = wrap(mControllerFocus, mTrainingButtons.size(), -1);
            setControllerFocus(mTrainingButtons, mControllerFocus, true);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
        {
            if (mTrainingButtons.size() <= 1)
                return true;

            setControllerFocus(mTrainingButtons, mControllerFocus, false);
            mControllerFocus = wrap(mControllerFocus, mTrainingButtons.size(), 1);
            setControllerFocus(mTrainingButtons, mControllerFocus, true);
        }

        return true;
    }
}
