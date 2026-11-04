#include "race.hpp"

#include <cmath>

#include <MyGUI_Gui.h>
#include <MyGUI_ITexture.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_LayerManager.h>
#include <MyGUI_ListBox.h>
#include <MyGUI_ScrollBar.h>
#include <MyGUI_UString.h>

#include <osg/Math>

#include <components/debug/debuglog.hpp>
#include <components/esm3/loadbody.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/esm3/loadspel.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwrender/characterpreview.hpp"
#include "../mwrender/renderingmanager.hpp"
#include "../mwworld/esmstore.hpp"

#include "controllerscreen.hpp"
#include "tooltips.hpp"

namespace
{
    bool sortRaces(const std::pair<ESM::RefId, std::string>& left, const std::pair<ESM::RefId, std::string>& right)
    {
        return left.second.compare(right.second) < 0;
    }

}

namespace MWGui
{

    RaceDialog::RaceDialog()
        : WindowModal("openmw_chargen_race.layout")
        , mGenderIndex(0)
        , mFaceIndex(0)
        , mHairIndex(0)
        , mCurrentAngle(0)
        , mPreviewDirty(true)
    {
        // Centre dialog
        center();

        setText("AppearanceT",
            MWBase::Environment::get().getWindowManager()->getGameSettingString("sRaceMenu1", "Appearance"));
        getWidget(mPreviewImage, "PreviewImage");

        mPreviewImage->eventMouseWheel += MyGUI::newDelegate(this, &RaceDialog::onPreviewScroll);

        getWidget(mHeadRotate, "HeadRotate");

        mHeadRotate->setScrollRange(1000);
        mHeadRotate->setScrollPosition(500);
        mHeadRotate->setScrollViewPage(50);
        mHeadRotate->setScrollPage(50);
        mHeadRotate->setScrollWheelPage(50);
        mHeadRotate->eventScrollChangePosition += MyGUI::newDelegate(this, &RaceDialog::onHeadRotate);

        // Set up next/previous buttons
        MyGUI::Button *prevButton, *nextButton;

        setText("GenderChoiceT",
            MWBase::Environment::get().getWindowManager()->getGameSettingString("sRaceMenu2", "Change Sex"));
        getWidget(prevButton, "PrevGenderButton");
        getWidget(nextButton, "NextGenderButton");
        prevButton->eventMouseButtonClick += MyGUI::newDelegate(this, &RaceDialog::onSelectPreviousGender);
        nextButton->eventMouseButtonClick += MyGUI::newDelegate(this, &RaceDialog::onSelectNextGender);

        setText("FaceChoiceT",
            MWBase::Environment::get().getWindowManager()->getGameSettingString("sRaceMenu3", "Change Face"));
        getWidget(prevButton, "PrevFaceButton");
        getWidget(nextButton, "NextFaceButton");
        prevButton->eventMouseButtonClick += MyGUI::newDelegate(this, &RaceDialog::onSelectPreviousFace);
        nextButton->eventMouseButtonClick += MyGUI::newDelegate(this, &RaceDialog::onSelectNextFace);

        setText("HairChoiceT",
            MWBase::Environment::get().getWindowManager()->getGameSettingString("sRaceMenu4", "Change Hair"));
        getWidget(prevButton, "PrevHairButton");
        getWidget(nextButton, "NextHairButton");
        prevButton->eventMouseButtonClick += MyGUI::newDelegate(this, &RaceDialog::onSelectPreviousHair);
        nextButton->eventMouseButtonClick += MyGUI::newDelegate(this, &RaceDialog::onSelectNextHair);

        setText("RaceT", MWBase::Environment::get().getWindowManager()->getGameSettingString("sRaceMenu5", "Race"));
        getWidget(mRaceList, "RaceList");
        mRaceList->setScrollVisible(true);
        mRaceList->eventListSelectAccept += MyGUI::newDelegate(this, &RaceDialog::onAccept);
        mRaceList->eventListChangePosition += MyGUI::newDelegate(this, &RaceDialog::onSelectRace);

        setText("SkillsT",
            MWBase::Environment::get().getWindowManager()->getGameSettingString("sBonusSkillTitle", "Skill Bonus"));
        getWidget(mSkillList, "SkillList");
        setText("SpellPowerT",
            MWBase::Environment::get().getWindowManager()->getGameSettingString("sRaceMenu7", "Specials"));
        getWidget(mSpellPowerList, "SpellPowerList");

        getWidget(mBackButton, "BackButton");
        mBackButton->eventMouseButtonClick += MyGUI::newDelegate(this, &RaceDialog::onBackClicked);

        getWidget(mOkButton, "OKButton");
        mOkButton->setCaption(
            MyGUI::UString(MWBase::Environment::get().getWindowManager()->getGameSettingString("sOK", {})));
        mOkButton->eventMouseButtonClick += MyGUI::newDelegate(this, &RaceDialog::onOkClicked);

        if (Settings::gui().mControllerMenus)
        {
            mControllerButtons.mLStick = "#{Interface:Mouse}";
            mControllerButtons.mA = "#{Interface:Select}";
            mControllerButtons.mB = "#{Interface:Back}";
            mControllerButtons.mY = "#{Interface:Sex}";
            mControllerButtons.mL1 = "#{Interface:Hair}";
            mControllerButtons.mR1 = "#{Interface:Face}";
        }

        updateRaces();
        updateSkills();
        updateSpellPowers();
    }

    RaceDialog::~RaceDialog() = default;

    void RaceDialog::drawPanel()
    {
        using namespace ControllerUi;
        if (mPanel == nullptr || !mPanel->getVisible())
            return;
        clearPanel(mPanel);
        const MyGUI::IntSize size = mPanel->getSize();
        addRect(mPanel, 0, 0, size.width, size.height, MyGUI::Colour::Black)->setNeedMouseFocus(true);
        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();

        addText(mPanel, sMargin, 20, size.width - 2 * sMargin, 40, "#{Interface:ChargenRaceTitle}", sFontTitle,
            sBrightColour);
        addRect(mPanel, sMargin, 66, size.width - 2 * sMargin, 1, sAccentColour, 0.2f);
        const int top = 82;
        const int bottom = size.height - 64;

        // The races on the left.
        constexpr int entryH = 44;
        const int listW = 230;
        const int count = static_cast<int>(mRaceList->getItemCount());
        const int focus
            = mRaceList->getIndexSelected() == MyGUI::ITEM_NONE ? 0 : static_cast<int>(mRaceList->getIndexSelected());
        const int visible = std::max(1, (bottom - top) / entryH);
        mPanelScroll = std::clamp(mPanelScroll, std::max(0, focus - visible + 1), focus);
        addRect(mPanel, sMargin, top, listW, bottom - top, sPanelColour, 0.9f);
        for (int i = mPanelScroll; i < count && i < mPanelScroll + visible; ++i)
        {
            const int ry = top + 6 + (i - mPanelScroll) * entryH;
            const bool focused = i == focus;
            if (focused)
            {
                addRect(mPanel, sMargin + 4, ry, listW - 8, entryH - 4, sAccentColour, 0.16f);
                addRect(mPanel, sMargin + 4, ry, 3, entryH - 4, sAccentColour);
            }
            addText(mPanel, sMargin + 18, ry, listW - 30, entryH - 4, escaped(mRaceList->getItemNameAt(i).asUTF8()),
                sFontNormal, focused ? sActiveColour : sBrightColour);
        }

        // The head, and how it looks, in the middle.
        const int px = sMargin + listW + 20;
        const int previewH = std::min(bottom - top - 120, 400);
        const int previewW = previewH;
        addRect(mPanel, px, top, previewW, previewH, MyGUI::Colour::Black);
        if (mPreviewTexture != nullptr)
        {
            MyGUI::ImageBox* head = mPanel->createWidget<MyGUI::ImageBox>(
                "ImageBox", MyGUI::IntCoord(px, top, previewW, previewH), MyGUI::Align::Default);
            head->setRenderItemTexture(mPreviewTexture.get());
            // The widget is Y-down, the RTT image is Y-up, so this UV is inverted
            head->getSubWidgetMain()->_setUVSet(MyGUI::FloatRect(0.f, 1.f, 1.f, 0.f));
            head->setNeedMouseFocus(false);
        }
        int ay = top + previewH + 12;
        const auto appearance
            = [&](SDL_GameControllerButton button, const std::string& label, const std::string& value) {
                  addGlyph(mPanel, px, ay + 4, 22, button);
                  addText(mPanel, px + 32, ay, 120, 30, label, sFontNormal, sTextColour);
                  addText(mPanel, px + 32, ay, previewW - 32, 30, value, sFontNormal, sBrightColour,
                      MyGUI::Align::Right | MyGUI::Align::VCenter);
                  ay += 34;
              };
        appearance(SDL_CONTROLLER_BUTTON_Y, "#{Interface:ChargenSex}",
            mGenderIndex == 0 ? "#{Interface:ChargenMale}" : "#{Interface:ChargenFemale}");
        appearance(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "#{Interface:ChargenFace}",
            std::to_string(mFaceIndex + 1) + " / " + std::to_string(mAvailableHeads.size()));
        appearance(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, "#{Interface:ChargenHair}",
            std::to_string(mHairIndex + 1) + " / " + std::to_string(mAvailableHairs.size()));

        // The race's description, skill bonuses and specials on the right.
        const int dx = px + previewW + 24;
        const int dw = size.width - sMargin - dx;
        const ESM::Race* race = store.get<ESM::Race>().search(mCurrentRaceId);
        if (race != nullptr && dw > 100)
        {
            int ty = top;
            addText(mPanel, dx, ty, dw, 34, escaped(race->mName), sFontLarge, sBrightColour);
            ty += 38;
            for (const std::string& line : wrapText(mPanel, race->mDescription, dw, sFontSmall, 8))
            {
                addText(mPanel, dx, ty, dw, 22, escaped(line), sFontSmall, sTextColour);
                ty += 22;
            }
            ty += 10;
            addText(mPanel, dx, ty, dw, 24, "#{sBonusSkillTitle}", sFontSmall, sDimColour);
            ty += 26;
            for (const auto& bonus : race->mData.mBonus)
            {
                const ESM::Skill* skill = store.get<ESM::Skill>().search(bonus.mSkill);
                if (skill == nullptr || ty + 24 > bottom)
                    continue;
                addText(mPanel, dx, ty, dw, 24, escaped(skill->mName), sFontNormal, sTextColour);
                addText(mPanel, dx, ty, dw, 24, "+" + std::to_string(bonus.mBonus), sFontNormal, sBetterColour,
                    MyGUI::Align::Right | MyGUI::Align::VCenter);
                ty += 24;
            }
            ty += 10;
            if (!race->mPowers.mList.empty() && ty + 26 < bottom)
            {
                addText(mPanel, dx, ty, dw, 24, "#{sRaceMenu7}", sFontSmall, sDimColour);
                ty += 26;
                for (const ESM::RefId& id : race->mPowers.mList)
                    if (const ESM::Spell* spell = store.get<ESM::Spell>().search(id))
                        if (ty + 24 <= bottom)
                        {
                            addText(mPanel, dx, ty, dw, 24, escaped(spell->mName), sFontNormal, sNewColour);
                            ty += 24;
                        }
            }
        }

        // The button hints along the bottom.
        int hx = sMargin;
        const int hintY = size.height - 48;
        const auto hint = [&](SDL_GameControllerButton button, const std::string& label) {
            addGlyph(mPanel, hx, hintY + 4, 22, button);
            MyGUI::TextBox* text = addText(mPanel, hx + 30, hintY, 220, 30, label, sFontNormal, sBrightColour);
            hx += 30 + text->getTextSize().width + 22;
        };
        hint(SDL_CONTROLLER_BUTTON_A, "#{sDone}");
        hint(SDL_CONTROLLER_BUTTON_RIGHTSTICK, "#{Interface:ChargenTurn}");
    }

    void RaceDialog::setNextButtonShow(bool shown)
    {
        MyGUI::Button* okButton;
        getWidget(okButton, "OKButton");

        if (shown)
        {
            okButton->setCaption(
                MyGUI::UString(MWBase::Environment::get().getWindowManager()->getGameSettingString("sNext", {})));
            mControllerButtons.mX = "#{Interface:Next}";
        }
        else if (Settings::gui().mControllerMenus)
        {
            okButton->setCaption(
                MyGUI::UString(MWBase::Environment::get().getWindowManager()->getGameSettingString("sDone", {})));
            mControllerButtons.mX = "#{Interface:Done}";
        }
        else
            okButton->setCaption(
                MyGUI::UString(MWBase::Environment::get().getWindowManager()->getGameSettingString("sOK", {})));
    }

    void RaceDialog::onOpen()
    {
        WindowModal::onOpen();

        updateRaces();
        updateSkills();
        updateSpellPowers();

        mPreviewImage->setRenderItemTexture(nullptr);

        mPreview.reset(nullptr);
        mPreviewTexture.reset(nullptr);

        mPreview = MWBase::Environment::get().getWorld()->getRenderingManager()->createRaceSelectionPreview();
        mPreview->rebuild();
        mPreview->setAngle(mCurrentAngle);

        mPreviewTexture = mPreview->makeGuiTexture();
        mPreviewImage->setRenderItemTexture(mPreviewTexture.get());
        // The widget is Y-down, the RTT image is Y-up, so this UV is inverted
        mPreviewImage->getSubWidgetMain()->_setUVSet(MyGUI::FloatRect(0.f, 1.f, 1.f, 0.f));

        const ESM::NPC& proto = mPreview->getPrototype();
        setRaceId(proto.mRace);
        setGender(proto.isMale() ? GM_Male : GM_Female);
        recountParts();

        for (size_t i = 0; i < mAvailableHeads.size(); ++i)
        {
            if (mAvailableHeads[i] == proto.mHead)
                mFaceIndex = i;
        }

        for (size_t i = 0; i < mAvailableHairs.size(); ++i)
        {
            if (mAvailableHairs[i] == proto.mHair)
                mHairIndex = i;
        }

        mPreviewDirty = true;

        size_t initialPos = mHeadRotate->getScrollRange() / 2 + mHeadRotate->getScrollRange() / 10;
        mHeadRotate->setScrollPosition(initialPos);
        onHeadRotate(mHeadRotate, initialPos);

        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mRaceList);

        if (Settings::gui().mControllerMenus && mPanel == nullptr)
            mPanel = ControllerUi::createPanelRoot("ControllerModal");
        if (mPanel != nullptr)
        {
            ControllerUi::hideWindow(mMainWidget);
            mPanelScroll = 0;
            mPanel->setVisible(true);
            MyGUI::LayerManager::getInstance().upLayerItem(mPanel);
            drawPanel();
        }
    }

    void RaceDialog::setRaceId(const ESM::RefId& raceId)
    {
        mCurrentRaceId = raceId;
        mRaceList->setIndexSelected(MyGUI::ITEM_NONE);
        size_t count = mRaceList->getItemCount();
        for (size_t i = 0; i < count; ++i)
        {
            if (*mRaceList->getItemDataAt<ESM::RefId>(i) == raceId)
            {
                mRaceList->setIndexSelected(i);
                break;
            }
        }

        updateSkills();
        updateSpellPowers();
    }

    void RaceDialog::onClose()
    {
        WindowModal::onClose();
        if (mPanel != nullptr)
        {
            // The panel shows the preview's texture, which goes away below.
            mPanel->setVisible(false);
            ControllerUi::clearPanel(mPanel);
        }

        mPreviewImage->setRenderItemTexture(nullptr);

        mPreviewTexture.reset(nullptr);
        mPreview.reset(nullptr);
    }

    // widget controls

    void RaceDialog::onOkClicked(MyGUI::Widget* /*sender*/)
    {
        if (mRaceList->getIndexSelected() == MyGUI::ITEM_NONE)
            return;
        eventDone(this);
    }

    void RaceDialog::onBackClicked(MyGUI::Widget* /*sender*/)
    {
        eventBack();
    }

    void RaceDialog::onPreviewScroll(MyGUI::Widget*, int delta)
    {
        size_t oldPos = mHeadRotate->getScrollPosition();
        size_t maxPos = mHeadRotate->getScrollRange() - 1;
        size_t scrollPage = mHeadRotate->getScrollWheelPage();
        if (delta < 0)
            mHeadRotate->setScrollPosition(oldPos + std::min(maxPos - oldPos, scrollPage));
        else
            mHeadRotate->setScrollPosition(oldPos - std::min(oldPos, scrollPage));

        onHeadRotate(mHeadRotate, mHeadRotate->getScrollPosition());
    }

    void RaceDialog::onHeadRotate(MyGUI::ScrollBar* scroll, size_t position)
    {
        float angle = (float(position) / (scroll->getScrollRange() - 1) - 0.5f) * osg::PIf * 2;
        mPreview->setAngle(angle);

        mCurrentAngle = angle;
    }

    void RaceDialog::onSelectPreviousGender(MyGUI::Widget*)
    {
        mGenderIndex = wrap(mGenderIndex, 2, -1);

        recountParts();
        updatePreview();
    }

    void RaceDialog::onSelectNextGender(MyGUI::Widget*)
    {
        mGenderIndex = wrap(mGenderIndex, 2, 1);

        recountParts();
        updatePreview();
    }

    void RaceDialog::onSelectPreviousFace(MyGUI::Widget*)
    {
        mFaceIndex = wrap(mFaceIndex, mAvailableHeads.size(), -1);
        updatePreview();
    }

    void RaceDialog::onSelectNextFace(MyGUI::Widget*)
    {
        mFaceIndex = wrap(mFaceIndex, mAvailableHeads.size(), 1);
        updatePreview();
    }

    void RaceDialog::onSelectPreviousHair(MyGUI::Widget*)
    {
        mHairIndex = wrap(mHairIndex, mAvailableHairs.size(), -1);
        updatePreview();
    }

    void RaceDialog::onSelectNextHair(MyGUI::Widget*)
    {
        mHairIndex = wrap(mHairIndex, mAvailableHairs.size(), 1);
        updatePreview();
    }

    void RaceDialog::onSelectRace(MyGUI::ListBox* sender, size_t index)
    {
        if (index == MyGUI::ITEM_NONE)
            return;

        ESM::RefId& raceId = *mRaceList->getItemDataAt<ESM::RefId>(index);
        if (mCurrentRaceId == raceId)
            return;

        mCurrentRaceId = raceId;

        recountParts();

        updatePreview();
        updateSkills();
        updateSpellPowers();
    }

    void RaceDialog::onAccept(MyGUI::ListBox* sender, size_t index)
    {
        onSelectRace(sender, index);
        if (mRaceList->getIndexSelected() == MyGUI::ITEM_NONE)
            return;
        eventDone(this);
    }

    void RaceDialog::getBodyParts(int part, std::vector<ESM::RefId>& out)
    {
        out.clear();
        const MWWorld::Store<ESM::BodyPart>& store = MWBase::Environment::get().getESMStore()->get<ESM::BodyPart>();

        for (const ESM::BodyPart& bodypart : store)
        {
            if (bodypart.mData.mFlags & ESM::BodyPart::BPF_NotPlayable)
                continue;
            if (bodypart.mData.mType != ESM::BodyPart::MT_Skin)
                continue;
            if (bodypart.mData.mPart != static_cast<ESM::BodyPart::MeshPart>(part))
                continue;
            if (mGenderIndex != (bodypart.mData.mFlags & ESM::BodyPart::BPF_Female))
                continue;
            if (ESM::isFirstPersonBodyPart(bodypart))
                continue;
            if (bodypart.mRace == mCurrentRaceId)
                out.push_back(bodypart.mId);
        }
    }

    void RaceDialog::recountParts()
    {
        getBodyParts(ESM::BodyPart::MP_Hair, mAvailableHairs);
        getBodyParts(ESM::BodyPart::MP_Head, mAvailableHeads);

        mFaceIndex = 0;
        mHairIndex = 0;
    }

    // update widget content

    void RaceDialog::updatePreview()
    {
        ESM::NPC record = mPreview->getPrototype();
        record.mRace = mCurrentRaceId;
        record.setIsMale(mGenderIndex == 0);

        if (mFaceIndex < mAvailableHeads.size())
            record.mHead = mAvailableHeads[mFaceIndex];

        if (mHairIndex < mAvailableHairs.size())
            record.mHair = mAvailableHairs[mHairIndex];

        try
        {
            mPreview->setPrototype(record);
        }
        catch (std::exception& e)
        {
            Log(Debug::Error) << "Error creating preview: " << e.what();
        }
    }

    void RaceDialog::updateRaces()
    {
        mRaceList->removeAllItems();

        const MWWorld::Store<ESM::Race>& races = MWBase::Environment::get().getESMStore()->get<ESM::Race>();

        std::vector<std::pair<ESM::RefId, std::string>> items; // ID, name
        for (const ESM::Race& race : races)
        {
            bool playable = race.mData.mFlags & ESM::Race::Playable;
            if (!playable) // Only display playable races
                continue;

            items.emplace_back(race.mId, race.mName);
        }
        std::sort(items.begin(), items.end(), sortRaces);

        int index = 0;
        for (auto& item : items)
        {
            mRaceList->addItem(item.second, item.first);
            if (item.first == mCurrentRaceId)
                mRaceList->setIndexSelected(index);
            ++index;
        }
    }

    void RaceDialog::updateSkills()
    {
        for (MyGUI::Widget* widget : mSkillItems)
        {
            MyGUI::Gui::getInstance().destroyWidget(widget);
        }
        mSkillItems.clear();

        if (mCurrentRaceId.empty())
            return;

        Widgets::MWSkillPtr skillWidget;
        const int lineHeight = Settings::gui().mFontSize + 2;
        MyGUI::IntCoord coord1(0, 0, mSkillList->getWidth(), 18);

        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
        const ESM::Race* race = store.get<ESM::Race>().find(mCurrentRaceId);
        for (const auto& bonus : race->mData.mBonus)
        {
            if (bonus.mSkill.empty()) // Skip unknown skill indexes
                continue;

            skillWidget = mSkillList->createWidget<Widgets::MWSkill>("MW_StatNameValue", coord1, MyGUI::Align::Default);
            skillWidget->setSkillId(bonus.mSkill);
            skillWidget->setSkillValue(Widgets::MWSkill::SkillValue(static_cast<float>(bonus.mBonus), 0.f));
            ToolTips::createSkillToolTip(skillWidget, bonus.mSkill);

            mSkillItems.push_back(skillWidget);

            coord1.top += lineHeight;
        }
    }

    void RaceDialog::updateSpellPowers()
    {
        for (MyGUI::Widget* widget : mSpellPowerItems)
        {
            MyGUI::Gui::getInstance().destroyWidget(widget);
        }
        mSpellPowerItems.clear();

        if (mCurrentRaceId.empty())
            return;

        const int lineHeight = Settings::gui().mFontSize + 2;
        MyGUI::IntCoord coord(0, 0, mSpellPowerList->getWidth(), lineHeight);

        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
        const ESM::Race* race = store.get<ESM::Race>().find(mCurrentRaceId);

        int i = 0;
        for (const ESM::RefId& spellpower : race->mPowers.mList)
        {
            if (!store.get<ESM::Spell>().search(spellpower))
                continue;
            Widgets::MWSpellPtr spellPowerWidget = mSpellPowerList->createWidget<Widgets::MWSpell>(
                "MW_StatName", coord, MyGUI::Align::Default, std::string("SpellPower") + MyGUI::utility::toString(i));
            spellPowerWidget->setSpellId(spellpower);
            spellPowerWidget->setUserString("ToolTipType", "Spell");
            spellPowerWidget->setUserString("Spell", spellpower.serialize());

            mSpellPowerItems.push_back(spellPowerWidget);

            coord.top += lineHeight;
            ++i;
        }
    }

    bool RaceDialog::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        if (arg.button == SDL_CONTROLLER_BUTTON_B)
        {
            onBackClicked(mBackButton);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_A)
        {
            onOkClicked(mOkButton);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_Y)
        {
            onSelectNextGender(nullptr);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
        {
            onSelectNextHair(nullptr);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
        {
            onSelectNextFace(nullptr);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP)
        {
            MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
            winMgr->setKeyFocusWidget(mRaceList);
            winMgr->injectKeyPress(MyGUI::KeyCode::ArrowUp, 0, false);
        }
        else if (arg.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
        {
            MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
            winMgr->setKeyFocusWidget(mRaceList);
            winMgr->injectKeyPress(MyGUI::KeyCode::ArrowDown, 0, false);
        }

        drawPanel();
        return true;
    }

    bool RaceDialog::onControllerThumbstickEvent(const SDL_ControllerAxisEvent& arg)
    {
        // onFrame turns the head from where the stick is. Each movement of the stick sends many events, so turning
        // a step for each one spun the head round.
        return arg.axis == SDL_CONTROLLER_AXIS_RIGHTX;
    }

    void RaceDialog::onFrame(float duration)
    {
        // How far the head turns per second with the stick pushed all the way, in radians.
        const float turnSpeed = osg::PIf;
        constexpr float deadzone = 0.15f;
        if (mPreview == nullptr)
            return;

        const float stick
            = MWBase::Environment::get().getInputManager()->getControllerAxisValue(SDL_CONTROLLER_AXIS_RIGHTX);
        const float magnitude = std::abs(stick);
        if (magnitude < deadzone)
            return;

        // Squaring the deflection gives finer control near the centre of the stick.
        const float scaled = (magnitude - deadzone) / (1.f - deadzone);
        float angle = mCurrentAngle + std::copysign(scaled * scaled, stick) * turnSpeed * duration;
        if (angle > osg::PIf)
            angle -= 2 * osg::PIf;
        else if (angle < -osg::PIf)
            angle += 2 * osg::PIf;

        // Keep the window's rotation slider in step, as the mouse wheel does.
        const float range = static_cast<float>(mHeadRotate->getScrollRange() - 1);
        mHeadRotate->setScrollPosition(static_cast<size_t>(std::lround((angle / (2 * osg::PIf) + 0.5f) * range)));
        mPreview->setAngle(angle);
        mCurrentAngle = angle;
    }

    const ESM::NPC& RaceDialog::getResult() const
    {
        return mPreview->getPrototype();
    }
}
