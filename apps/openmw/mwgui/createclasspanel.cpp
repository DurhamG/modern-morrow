#include "createclasspanel.hpp"

#include <algorithm>

#include <MyGUI_EditBox.h>
#include <MyGUI_Gui.h>
#include <MyGUI_ImageBox.h>
#include <MyGUI_LayerManager.h>

#include <components/esm/attr.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadskil.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/esmstore.hpp"

#include "class.hpp"
#include "textinput.hpp"
#include "widgets.hpp"

namespace MWGui
{
    namespace
    {
        using namespace ControllerUi;

        constexpr int sLineHeight = 30;
        constexpr int sHeaderHeight = 28;
        constexpr int sChoiceHeight = 36;

        const MWWorld::ESMStore& store()
        {
            return *MWBase::Environment::get().getESMStore();
        }

        MWBase::WindowManager& windowManager()
        {
            return *MWBase::Environment::get().getWindowManager();
        }

        std::string specializationName(int specialization)
        {
            return std::string(windowManager().getGameSettingString(ESM::Class::sGmstSpecializationIds[specialization],
                ESM::Class::sGmstSpecializationIds[specialization]));
        }

        std::string iconPath(const auto& record)
        {
            return std::string(record.mIcon.getNormalized().value());
        }
    }

    CreateClassPanel::CreateClassPanel(CreateClassDialog& window)
        : mWindow(window)
    {
        mLines = { { Line::Kind::Name }, { Line::Kind::Description }, { Line::Kind::Specialization },
            { Line::Kind::Attribute, 0 }, { Line::Kind::Attribute, 1 } };
        for (int i = 0; i < 5; ++i)
            mLines.push_back({ Line::Kind::MajorSkill, i });
        for (int i = 0; i < 5; ++i)
            mLines.push_back({ Line::Kind::MinorSkill, i });
    }

    CreateClassPanel::~CreateClassPanel()
    {
        if (mRoot != nullptr)
            MyGUI::Gui::getInstance().destroyWidget(mRoot);
    }

    void CreateClassPanel::show()
    {
        if (mRoot == nullptr)
            mRoot = createPanelRoot("ControllerModal");
        if (mRoot == nullptr)
            return;
        mRoot->setVisible(true);
        MyGUI::LayerManager::getInstance().upLayerItem(mRoot);
        mLineFocus = 0;
        mChoicesFocused = false;
        mHeldDirection.reset();
        readChoices();
        update();
    }

    void CreateClassPanel::hide()
    {
        if (mRoot != nullptr)
            mRoot->setVisible(false);
    }

    ESM::RefId CreateClassPanel::currentId(const Line& line) const
    {
        switch (line.mKind)
        {
            case Line::Kind::Attribute:
                return (line.mIndex == 0 ? mWindow.mFavoriteAttribute0 : mWindow.mFavoriteAttribute1)->getAttributeId();
            case Line::Kind::MajorSkill:
                return mWindow.mMajorSkill[line.mIndex]->getSkillId();
            case Line::Kind::MinorSkill:
                return mWindow.mMinorSkill[line.mIndex]->getSkillId();
            default:
                return {};
        }
    }

    void CreateClassPanel::readChoices()
    {
        mChoices.clear();
        const Line& line = mLines[mLineFocus];
        switch (line.mKind)
        {
            case Line::Kind::Name:
            case Line::Kind::Description:
                break;
            case Line::Kind::Specialization:
                for (int specialization = 0; specialization < 3; ++specialization)
                {
                    Choice choice;
                    choice.mSpecialization = specialization;
                    choice.mName = escaped(specializationName(specialization));
                    std::string skills;
                    for (const ESM::Skill& skill : store().get<ESM::Skill>())
                        if (skill.mData.mSpecialization == specialization)
                            skills += (skills.empty() ? "" : ", ") + skill.mName;
                    choice.mDescription = localised("#{Interface:ClassSpecializationHelp}") + " " + skills;
                    choice.mCurrent = specialization == mWindow.mSpecializationId;
                    mChoices.push_back(std::move(choice));
                }
                break;
            case Line::Kind::Attribute:
            {
                const ESM::RefId other = currentId({ Line::Kind::Attribute, 1 - line.mIndex });
                for (const ESM::Attribute& attribute : store().get<ESM::Attribute>())
                {
                    Choice choice;
                    choice.mId = attribute.mId;
                    choice.mIcon = iconPath(attribute);
                    choice.mName = escaped(attribute.mName);
                    choice.mDescription = attribute.mDescription;
                    choice.mCurrent = attribute.mId == currentId(line);
                    if (attribute.mId == other)
                        choice.mMarker = "#{Interface:ClassChosen}";
                    mChoices.push_back(std::move(choice));
                }
                break;
            }
            case Line::Kind::MajorSkill:
            case Line::Kind::MinorSkill:
            {
                // Skills are grouped by specialization, as in the game's own skill picker. Skills already on the
                // class say so, because picking one swaps the two.
                for (int specialization = 0; specialization < 3; ++specialization)
                {
                    mChoices.push_back(Choice{ escaped(specializationName(specialization)) });
                    for (const ESM::Skill& skill : store().get<ESM::Skill>())
                    {
                        if (skill.mData.mSpecialization != specialization)
                            continue;
                        Choice choice;
                        choice.mId = skill.mId;
                        choice.mIcon = iconPath(skill);
                        choice.mName = escaped(skill.mName);
                        if (const ESM::Attribute* governing
                            = store().get<ESM::Attribute>().search(skill.mData.mAttribute))
                            choice.mDetail = escaped(governing->mName);
                        choice.mDescription = skill.mDescription;
                        choice.mCurrent = skill.mId == currentId(line);
                        for (int i = 0; i < 5 && !choice.mCurrent; ++i)
                        {
                            if (mWindow.mMajorSkill[i]->getSkillId() == skill.mId)
                                choice.mMarker = "#{sSkillClassMajor}";
                            else if (mWindow.mMinorSkill[i]->getSkillId() == skill.mId)
                                choice.mMarker = "#{sSkillClassMinor}";
                        }
                        mChoices.push_back(std::move(choice));
                    }
                }
                break;
            }
        }

        // The highlight starts on the current choice.
        mChoiceFocus = 0;
        int index = 0;
        for (const Choice& choice : mChoices)
        {
            if (!choice.mHeader.empty())
                continue;
            if (choice.mCurrent)
                mChoiceFocus = index;
            ++index;
        }
        mChoiceScroll = 0;
    }

    int CreateClassPanel::choiceCount() const
    {
        return static_cast<int>(std::count_if(
            mChoices.begin(), mChoices.end(), [](const Choice& choice) { return choice.mHeader.empty(); }));
    }

    const CreateClassPanel::Choice* CreateClassPanel::focusedChoice() const
    {
        int index = 0;
        for (const Choice& choice : mChoices)
        {
            if (!choice.mHeader.empty())
                continue;
            if (index++ == mChoiceFocus)
                return &choice;
        }
        return nullptr;
    }

    void CreateClassPanel::update()
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;
        clearPanel(mRoot);
        const MyGUI::IntSize size = mRoot->getSize();
        addRect(mRoot, 0, 0, size.width, size.height, MyGUI::Colour::Black)->setNeedMouseFocus(true);
        addText(mRoot, sMargin, 20, size.width - 2 * sMargin, 40, "#{Interface:ClassCreateTitle}", sFontTitle,
            sBrightColour);
        addRect(mRoot, sMargin, 66, size.width - 2 * sMargin, 1, sAccentColour, 0.2f);

        const int top = 82;
        const int bottom = size.height - sContentBottomMargin;
        const int leftW = (size.width - 2 * sMargin - 16) * 45 / 100;
        drawLines(sMargin, top, leftW, bottom - top);
        const int px = sMargin + leftW + 16;
        drawChoices(px, top, size.width - sMargin - px, bottom - top);
    }

    void CreateClassPanel::drawLines(int x, int y, int w, int h)
    {
        addRect(mRoot, x, y, w, h, sPanelColour, 0.9f);
        int ry = y + 10;
        Line::Kind previous = Line::Kind::Name;
        for (std::size_t i = 0; i < mLines.size(); ++i)
        {
            const Line& line = mLines[i];
            // A heading above each group of attributes and skills.
            if (line.mKind != previous
                && (line.mKind == Line::Kind::Attribute || line.mKind == Line::Kind::MajorSkill
                    || line.mKind == Line::Kind::MinorSkill))
            {
                const std::string heading = line.mKind == Line::Kind::Attribute ? "#{sChooseClassMenu2}"
                    : line.mKind == Line::Kind::MajorSkill                      ? "#{sSkillClassMajor}"
                                                                                : "#{sSkillClassMinor}";
                addText(mRoot, x + 16, ry + 4, w - 32, 22, heading, sFontSmall, sDimColour);
                addRect(mRoot, x + 16, ry + 26, w - 32, 1, sAccentColour, 0.25f);
                ry += sHeaderHeight + 2;
            }
            previous = line.mKind;

            const bool focused = static_cast<int>(i) == mLineFocus;
            if (focused)
            {
                addRect(mRoot, x + 4, ry, w - 8, sLineHeight - 2, sAccentColour, mChoicesFocused ? 0.08f : 0.16f);
                addRect(mRoot, x + 4, ry, 3, sLineHeight - 2, sAccentColour);
            }
            const MyGUI::Colour colour = focused && !mChoicesFocused ? sActiveColour : sBrightColour;
            std::string label;
            std::string value;
            std::string icon;
            switch (line.mKind)
            {
                case Line::Kind::Name:
                    label = "#{sName}";
                    value = escaped(mWindow.getName());
                    if (value.empty())
                        value = "#{Interface:ClassNoName}";
                    break;
                case Line::Kind::Description:
                    label = "#{Interface:ClassDescription}";
                    value = mWindow.mDescription.empty() ? "-" : escaped(mWindow.mDescription);
                    break;
                case Line::Kind::Specialization:
                    label = "#{sChooseClassMenu1}";
                    value = escaped(specializationName(mWindow.mSpecializationId));
                    break;
                case Line::Kind::Attribute:
                    if (const ESM::Attribute* attribute = store().get<ESM::Attribute>().search(currentId(line)))
                    {
                        value = escaped(attribute->mName);
                        icon = iconPath(*attribute);
                    }
                    break;
                case Line::Kind::MajorSkill:
                case Line::Kind::MinorSkill:
                    if (const ESM::Skill* skill = store().get<ESM::Skill>().search(currentId(line)))
                    {
                        value = escaped(skill->mName);
                        icon = iconPath(*skill);
                    }
                    break;
            }
            if (!label.empty())
            {
                addText(mRoot, x + 20, ry, 140, sLineHeight - 2, label, sFontNormal, sTextColour);
                const bool missing = line.mKind == Line::Kind::Name && mWindow.getName().empty();
                addText(
                    mRoot, x + 160, ry, w - 180, sLineHeight - 2, value, sFontNormal, missing ? sDimColour : colour);
            }
            else
            {
                if (!icon.empty())
                    addImage(mRoot, x + 20, ry + 1, sLineHeight - 4, icon);
                addText(mRoot, x + 20 + sLineHeight + 4, ry, w - 44 - sLineHeight, sLineHeight - 2, value, sFontNormal,
                    colour);
            }
            ry += sLineHeight;
            if (line.mKind == Line::Kind::Specialization)
                ry += 4;
        }
    }

    void CreateClassPanel::drawChoices(int x, int y, int w, int h)
    {
        addRect(mRoot, x, y, w, h, sPanelColour, 0.9f);
        addRect(mRoot, x, y, w, 1, sAccentColour, 0.3f);
        const Line& line = mLines[mLineFocus];

        std::vector<Hint> hints;
        if (mChoicesFocused)
            hints.emplace_back(SDL_CONTROLLER_BUTTON_A, "#{Interface:EnchantChoose}");
        else
            hints.emplace_back(SDL_CONTROLLER_BUTTON_A,
                line.mKind == Line::Kind::Name || line.mKind == Line::Kind::Description ? "#{Interface:EnchantEdit}"
                                                                                        : "#{Interface:EnchantChange}");
        hints.emplace_back(SDL_CONTROLLER_BUTTON_X, "#{sDone}");
        const int hintsTop = drawHints(mRoot, x + 16, w - 32, y + h, hints);

        // The name and description are typed, so there is nothing to pick from.
        if (line.mKind == Line::Kind::Name || line.mKind == Line::Kind::Description)
        {
            std::string help = "#{Interface:ClassDescriptionHelp}";
            if (line.mKind == Line::Kind::Name)
                help = mWindow.getName().empty() ? "#{Interface:ClassNameHelp}" : "#{Interface:ClassNameChangeHelp}";
            int ty = y + 16;
            for (const std::string& text : wrapText(mRoot, localised(help), w - 32, sFontNormal, 4))
            {
                addText(mRoot, x + 16, ty, w - 32, 26, escaped(text), sFontNormal, sTextColour);
                ty += 26;
            }
            return;
        }

        const std::string title = line.mKind == Line::Kind::Specialization ? "#{sChooseClassMenu1}"
            : line.mKind == Line::Kind::Attribute                          ? "#{sChooseClassMenu2}"
            : line.mKind == Line::Kind::MajorSkill                         ? "#{sSkillClassMajor}"
                                                                           : "#{sSkillClassMinor}";
        addText(mRoot, x + 16, y + 8, w - 32, 30, title, sFontLarge, sBrightColour);

        // The description of the highlighted choice, at the bottom.
        const Choice* focused = focusedChoice();
        int descriptionTop = hintsTop - 12;
        if (focused != nullptr && !focused->mDescription.empty())
        {
            const std::vector<std::string> lines = wrapText(mRoot, focused->mDescription, w - 32, sFontSmall, 5);
            descriptionTop -= static_cast<int>(lines.size()) * 21;
            int ty = descriptionTop;
            for (const std::string& text : lines)
            {
                addText(mRoot, x + 16, ty, w - 32, 21, escaped(text), sFontSmall, sDimColour);
                ty += 21;
            }
            descriptionTop -= 10;
        }

        // The choices, scrolled so the highlighted one shows.
        const int listTop = y + 44;
        const int listBottom = descriptionTop;
        int focusRow = 0;
        for (int i = 0, index = 0; i < static_cast<int>(mChoices.size()); ++i)
        {
            if (!mChoices[i].mHeader.empty())
                continue;
            if (index++ == mChoiceFocus)
            {
                focusRow = i;
                break;
            }
        }
        const auto rowHeight = [&](int i) { return mChoices[i].mHeader.empty() ? sChoiceHeight : sHeaderHeight; };
        if (focusRow < mChoiceScroll)
            mChoiceScroll = focusRow > 0 && !mChoices[focusRow - 1].mHeader.empty() ? focusRow - 1 : focusRow;
        while (true)
        {
            int bottomOfFocus = listTop;
            for (int i = mChoiceScroll; i <= focusRow; ++i)
                bottomOfFocus += rowHeight(i);
            if (bottomOfFocus <= listBottom || mChoiceScroll >= focusRow)
                break;
            ++mChoiceScroll;
        }

        int ry = listTop;
        int index = 0;
        for (int i = 0; i < mChoiceScroll; ++i)
            index += mChoices[i].mHeader.empty() ? 1 : 0;
        for (int i = mChoiceScroll; i < static_cast<int>(mChoices.size()); ++i)
        {
            const Choice& choice = mChoices[i];
            if (ry + rowHeight(i) > listBottom)
                break;
            if (!choice.mHeader.empty())
            {
                addText(mRoot, x + 12, ry + 4, w - 24, 22, choice.mHeader, sFontSmall, sBrightColour);
                addRect(mRoot, x + 12, ry + 26, w - 24, 1, sAccentColour, 0.25f);
                ry += sHeaderHeight;
                continue;
            }
            const bool highlighted = mChoicesFocused && index == mChoiceFocus;
            if (highlighted)
            {
                addRect(mRoot, x + 4, ry, w - 8, sChoiceHeight - 2, sAccentColour, 0.16f);
                addRect(mRoot, x + 4, ry, 3, sChoiceHeight - 2, sAccentColour);
            }
            int tx = x + 20;
            if (!choice.mIcon.empty())
            {
                addImage(mRoot, tx, ry + 3, sChoiceHeight - 8, choice.mIcon);
                tx += sChoiceHeight;
            }
            MyGUI::TextBox* name = addText(mRoot, tx, ry, x + w - 16 - tx, sChoiceHeight - 2, choice.mName, sFontNormal,
                highlighted ? sActiveColour : (choice.mCurrent ? sNewColour : sBrightColour));
            if (!choice.mDetail.empty())
                addText(mRoot, tx + name->getTextSize().width + 12, ry + 2, 200, sChoiceHeight - 4, choice.mDetail,
                    sFontSmall, sDimColour);
            std::string marker = choice.mCurrent ? "#{Interface:ClassCurrent}" : choice.mMarker;
            addText(mRoot, x + 16, ry, w - 32, sChoiceHeight - 2, marker, sFontSmall,
                choice.mCurrent ? sNewColour : sDimColour, MyGUI::Align::Right | MyGUI::Align::VCenter);
            ry += sChoiceHeight;
            ++index;
        }
    }

    void CreateClassPanel::onFrame(float duration)
    {
        if (mRoot == nullptr || !mRoot->getVisible())
            return;
        if (windowManager().getActiveControllerWindow() != &mWindow)
        {
            mHeldDirection.reset();
            return;
        }
        int dx = 0;
        int dy = 0;
        if (!mHeldDirection.update(duration, dx, dy))
            return;
        const Line::Kind kind = mLines[mLineFocus].mKind;
        const bool hasChoices = kind != Line::Kind::Name && kind != Line::Kind::Description;
        if (dx > 0 && !mChoicesFocused && hasChoices)
            mChoicesFocused = true;
        else if (dx < 0 && mChoicesFocused)
            mChoicesFocused = false;
        else if (dy != 0 && mChoicesFocused)
            mChoiceFocus = std::clamp(mChoiceFocus + dy, 0, std::max(0, choiceCount() - 1));
        else if (dy != 0)
        {
            mLineFocus = std::clamp(mLineFocus + dy, 0, static_cast<int>(mLines.size()) - 1);
            readChoices();
        }
        update();
    }

    bool CreateClassPanel::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                if (mChoicesFocused)
                {
                    if (const Choice* choice = focusedChoice())
                        pick(*choice);
                }
                else
                    chooseLine();
                break;
            case SDL_CONTROLLER_BUTTON_B:
                if (mChoicesFocused)
                {
                    mChoicesFocused = false;
                    readChoices();
                }
                else
                {
                    mWindow.onBackClicked(nullptr);
                    return true;
                }
                break;
            case SDL_CONTROLLER_BUTTON_X:
                // The class needs a name, so a class without one asks for it first.
                if (mWindow.getName().empty())
                    editText(false);
                else
                    mWindow.onOkClicked(nullptr);
                return true;
            default:
                // The d-pad and the left stick are read every frame in onFrame, so they repeat while held.
                return true;
        }
        update();
        return true;
    }

    void CreateClassPanel::chooseLine()
    {
        const Line::Kind kind = mLines[mLineFocus].mKind;
        if (kind == Line::Kind::Name || kind == Line::Kind::Description)
        {
            editText(kind == Line::Kind::Description);
            return;
        }
        mChoicesFocused = true;
        windowManager().playSound(ESM::RefId::stringRefId("Menu Click"));
    }

    void CreateClassPanel::pick(const Choice& choice)
    {
        // The same rules as the custom class window: picking something already on the class swaps the two.
        const Line& line = mLines[mLineFocus];
        switch (line.mKind)
        {
            case Line::Kind::Specialization:
                mWindow.setSpecialization(choice.mSpecialization);
                break;
            case Line::Kind::Attribute:
            {
                Widgets::MWAttributePtr target
                    = line.mIndex == 0 ? mWindow.mFavoriteAttribute0 : mWindow.mFavoriteAttribute1;
                Widgets::MWAttributePtr other
                    = line.mIndex == 0 ? mWindow.mFavoriteAttribute1 : mWindow.mFavoriteAttribute0;
                if (other->getAttributeId() == choice.mId)
                    other->setAttributeId(target->getAttributeId());
                target->setAttributeId(choice.mId);
                mWindow.update();
                break;
            }
            case Line::Kind::MajorSkill:
            case Line::Kind::MinorSkill:
            {
                Widgets::MWSkillPtr target = line.mKind == Line::Kind::MajorSkill ? mWindow.mMajorSkill[line.mIndex]
                                                                                  : mWindow.mMinorSkill[line.mIndex];
                for (Widgets::MWSkillPtr& skill : mWindow.mSkills)
                {
                    if (skill != target && skill->getSkillId() == choice.mId)
                    {
                        skill->setSkillId(target->getSkillId());
                        break;
                    }
                }
                target->setSkillId(choice.mId);
                mWindow.update();
                break;
            }
            default:
                break;
        }
        windowManager().playSound(ESM::RefId::stringRefId("Menu Click"));
        mChoicesFocused = false;
        readChoices();
    }

    void CreateClassPanel::editText(bool description)
    {
        mEditingDescription = description;
        mTextDialog = std::make_unique<TextInputDialog>();
        mTextDialog->setTextLabel(description ? "#{Interface:ClassDescription}" : "#{sName}");
        mTextDialog->setTextInput(description ? mWindow.mDescription : mWindow.getName());
        mTextDialog->setNextButtonShow(false);
        mTextDialog->eventDone += MyGUI::newDelegate(this, &CreateClassPanel::onTextDone);
        mTextDialog->setVisible(true);
    }

    void CreateClassPanel::onTextDone(WindowBase* /*sender*/)
    {
        const std::string text = mTextDialog->getTextInput();
        if (mEditingDescription)
            mWindow.mDescription = text;
        else
            mWindow.mEditName->setCaption(MyGUI::UString(text));
        windowManager().removeDialog(std::move(mTextDialog));
        MyGUI::LayerManager::getInstance().upLayerItem(mRoot);
        update();
    }
}
