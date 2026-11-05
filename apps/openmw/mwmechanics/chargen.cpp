#include "chargen.hpp"

#include <format>

#include <components/esm3/loadclas.hpp>
#include <components/fallback/fallback.hpp>

#include "../mwbase/environment.hpp"
#include "../mwworld/esmstore.hpp"

namespace MWMechanics
{
    std::vector<ClassQuestion> getClassQuestions()
    {
        std::vector<ClassQuestion> questions;
        questions.reserve(sClassQuestionCount);
        for (unsigned number = 1; number <= sClassQuestionCount; ++number)
        {
            ClassQuestion& question = questions.emplace_back();
            question.mQuestion = Fallback::Map::getString(std::format("Question_{}_Question", number));
            question.mAnswers[ESM::Class::Combat]
                = Fallback::Map::getString(std::format("Question_{}_AnswerOne", number));
            question.mAnswers[ESM::Class::Magic]
                = Fallback::Map::getString(std::format("Question_{}_AnswerTwo", number));
            question.mAnswers[ESM::Class::Stealth]
                = Fallback::Map::getString(std::format("Question_{}_AnswerThree", number));
            question.mSound = VFS::Path::Normalized(std::format("vo/misc/chargen qa{}.wav", number));
        }
        return questions;
    }

    ESM::RefId getGeneratedClass(unsigned combat, unsigned magic, unsigned stealth)
    {
        std::string_view className;
        if (combat > 7)
        {
            className = "Warrior";
        }
        else if (magic > 7)
        {
            className = "Mage";
        }
        else if (stealth > 7)
        {
            className = "Thief";
        }
        else
        {
            switch (combat)
            {
                case 4:
                    className = "Rogue";
                    break;
                case 5:
                    if (stealth == 3)
                        className = "Scout";
                    else
                        className = "Archer";
                    break;
                case 6:
                    if (stealth == 1)
                        className = "Barbarian";
                    else if (stealth == 3)
                        className = "Crusader";
                    else
                        className = "Knight";
                    break;
                case 7:
                    className = "Warrior";
                    break;
                default:
                    switch (magic)
                    {
                        case 4:
                            className = "Spellsword";
                            break;
                        case 5:
                            className = "Witchhunter";
                            break;
                        case 6:
                            if (combat == 2)
                                className = "Sorcerer";
                            else if (combat == 3)
                                className = "Healer";
                            else
                                className = "Battlemage";
                            break;
                        case 7:
                            className = "Mage";
                            break;
                        default:
                            switch (stealth)
                            {
                                case 3:
                                    if (magic == 3)
                                        className = "Bard"; // unreachable
                                    else
                                        className = "Warrior";
                                    break;
                                case 5:
                                    if (magic == 3)
                                        className = "Monk";
                                    else
                                        className = "Pilgrim";
                                    break;
                                case 6:
                                    if (magic == 1)
                                        className = "Agent";
                                    else if (magic == 3)
                                        className = "Assassin";
                                    else
                                        className = "Acrobat";
                                    break;
                                case 7:
                                    className = "Thief";
                                    break;
                                default:
                                    className = "Warrior";
                            }
                    }
            }
        }
        return ESM::RefId::stringRefId(className);
    }

    std::vector<ESM::RefId> getChargenBodyParts(ESM::BodyPart::MeshPart part, const ESM::RefId& race, bool isMale)
    {
        std::vector<ESM::RefId> result;
        const MWWorld::Store<ESM::BodyPart>& store = MWBase::Environment::get().getESMStore()->get<ESM::BodyPart>();

        for (const ESM::BodyPart& bodypart : store)
        {
            if (bodypart.mData.mFlags & ESM::BodyPart::BPF_NotPlayable)
                continue;
            if (bodypart.mData.mType != ESM::BodyPart::MT_Skin)
                continue;
            if (bodypart.mData.mPart != part)
                continue;
            if (isMale == ((bodypart.mData.mFlags & ESM::BodyPart::BPF_Female) != 0))
                continue;
            if (ESM::isFirstPersonBodyPart(bodypart))
                continue;
            if (bodypart.mRace == race)
                result.push_back(bodypart.mId);
        }
        return result;
    }
}
