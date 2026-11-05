#include "chargenbindings.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include <components/esm3/loadbody.hpp>
#include <components/esm3/loadbsgn.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/lua/luastate.hpp>
#include <components/misc/resourcehelpers.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwmechanics/chargen.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/globals.hpp"

#include "classbindings.hpp"
#include "context.hpp"
#include "luamanagerimp.hpp"
#include "object.hpp"

namespace MWLua
{
    namespace
    {
        void verifyPlayer(const Object& player)
        {
            if (player.ptr() != MWBase::Environment::get().getWorld()->getPlayerPtr())
                throw std::runtime_error("The argument must be a player!");
        }

        // Character creation changes the player's record and rebuilds their stats and spells, so only the player's own
        // scripts and global scripts can do it, and only until the character is finished.
        void verifyCreator(const Object& player)
        {
            verifyPlayer(player);
            if (player.isLObject() && !player.isSelfObject())
                throw std::runtime_error("Only player and global scripts can create the player's character");
            if (MWBase::Environment::get().getWorld()->getGlobalFloat(MWWorld::Globals::sCharGenState) == -1)
                throw std::runtime_error("Character creation is finished");
        }

        template <class T>
        const T* toRecord(const sol::object& recordOrId, std::string_view kind)
        {
            if (recordOrId.is<T>())
                return recordOrId.as<const T*>();
            const std::string_view textId = LuaUtil::cast<std::string_view>(recordOrId);
            const T* record
                = MWBase::Environment::get().getESMStore()->get<T>().search(ESM::RefId::deserializeText(textId));
            if (record == nullptr)
                throw std::runtime_error(std::string(kind) + " '" + std::string(textId) + "' does not exist");
            return record;
        }

        sol::table toIdList(sol::state_view lua, const std::vector<ESM::RefId>& ids)
        {
            sol::table result(lua, sol::create);
            for (const ESM::RefId& id : ids)
                result.add(id.serializeText());
            return result;
        }

        // The head or the hair that the options name, which must be one that character creation offers. Without one,
        // the first that it offers, as the race window chooses when the race or gender changes.
        ESM::RefId toBodyPart(const sol::object& id, ESM::BodyPart::MeshPart part, const ESM::RefId& race, bool isMale,
            const ESM::RefId& current, std::string_view kind)
        {
            const std::vector<ESM::RefId> choices = MWMechanics::getChargenBodyParts(part, race, isMale);
            if (id == sol::nil)
                return choices.empty() ? current : choices.front();
            const std::string_view textId = LuaUtil::cast<std::string_view>(id);
            const ESM::RefId result = ESM::RefId::deserializeText(textId);
            if (std::find(choices.begin(), choices.end(), result) == choices.end())
                throw std::runtime_error(
                    "'" + std::string(textId) + "' is not a " + std::string(kind) + " that the player can choose");
            return result;
        }

        unsigned toAnswerCount(const sol::table& answers, std::string_view specialization)
        {
            const sol::object value = answers.get<sol::object>(specialization);
            if (value == sol::nil)
                return 0;
            const double count = LuaUtil::cast<double>(value);
            if (count < 0 || count > MWMechanics::sClassQuestionCount || count != std::floor(count))
                throw std::runtime_error("The number of " + std::string(specialization) + " answers is not valid");
            return static_cast<unsigned>(count);
        }
    }

    void addCharGenBindings(sol::table& player, const Context& context)
    {
        player["setCharGenName"] = [context](const Object& object, std::string name) {
            verifyCreator(object);
            if (name.empty())
                throw std::runtime_error("The name must not be empty");
            context.mLuaManager->addAction(
                [name = std::move(name)] { MWBase::Environment::get().getMechanicsManager()->setPlayerName(name); },
                "SetCharGenNameAction");
        };

        player["setCharGenRace"] = [context](const Object& object, const sol::table& options) {
            verifyCreator(object);
            const ESM::Race& race = *toRecord<ESM::Race>(options.get<sol::object>("race"), "Race");
            if (!(race.mData.mFlags & ESM::Race::Playable))
                throw std::runtime_error("Race '" + race.mId.toDebugString() + "' is not playable");
            const ESM::NPC& current = *object.ptr().get<ESM::NPC>()->mBase;
            const bool isMale = options.get_or("isMale", current.isMale());
            const ESM::RefId head = toBodyPart(
                options.get<sol::object>("head"), ESM::BodyPart::MP_Head, race.mId, isMale, current.mHead, "head");
            const ESM::RefId hair = toBodyPart(
                options.get<sol::object>("hair"), ESM::BodyPart::MP_Hair, race.mId, isMale, current.mHair, "hair");
            context.mLuaManager->addAction(
                [raceId = race.mId, isMale, head, hair] {
                    MWBase::Environment::get().getMechanicsManager()->setPlayerRace(raceId, isMale, head, hair);
                    // The windows show the player as the race window leaves them, such as the inventory's preview
                    MWBase::Environment::get().getWindowManager()->updatePlayer();
                },
                "SetCharGenRaceAction");
        };

        player["setCharGenClass"] = [context](const Object& object, const sol::object& classOrTable) {
            verifyCreator(object);
            if (classOrTable.is<sol::table>())
            {
                ESM::Class custom = tableToClass(classOrTable.as<sol::table>());
                custom.mData.mIsPlayable = true;
                custom.mRecordFlags = 0;
                const std::string_view problem = MWMechanics::getCustomClassProblem(custom);
                if (!problem.empty())
                    throw std::runtime_error(std::string(problem));
                context.mLuaManager->addAction(
                    [custom = std::move(custom)] {
                        MWBase::Environment::get().getMechanicsManager()->setPlayerClass(custom);
                    },
                    "SetCharGenClassAction");
                return;
            }
            const ESM::Class& chosen = *toRecord<ESM::Class>(classOrTable, "Class");
            if (!chosen.mData.mIsPlayable)
                throw std::runtime_error("Class '" + chosen.mId.toDebugString() + "' is not playable");
            context.mLuaManager->addAction(
                [classId = chosen.mId] { MWBase::Environment::get().getMechanicsManager()->setPlayerClass(classId); },
                "SetCharGenClassAction");
        };

        player["setCharGenBirthSign"] = [context](const Object& object, const sol::object& signOrId) {
            verifyCreator(object);
            const ESM::RefId signId = toRecord<ESM::BirthSign>(signOrId, "Birth sign")->mId;
            context.mLuaManager->addAction(
                [signId] { MWBase::Environment::get().getMechanicsManager()->setPlayerBirthsign(signId); },
                "SetCharGenBirthSignAction");
        };

        player["getCharGenHeads"] = [](sol::this_state thisState, const sol::object& raceOrId, bool isMale) {
            const ESM::Race& race = *toRecord<ESM::Race>(raceOrId, "Race");
            return toIdList(thisState, MWMechanics::getChargenBodyParts(ESM::BodyPart::MP_Head, race.mId, isMale));
        };
        player["getCharGenHairs"] = [](sol::this_state thisState, const sol::object& raceOrId, bool isMale) {
            const ESM::Race& race = *toRecord<ESM::Race>(raceOrId, "Race");
            return toIdList(thisState, MWMechanics::getChargenBodyParts(ESM::BodyPart::MP_Hair, race.mId, isMale));
        };

        player["getCharGenClassQuestions"] = [](sol::this_state thisState) {
            sol::state_view lua(thisState);
            sol::table result(lua, sol::create);
            for (const MWMechanics::ClassQuestion& question : MWMechanics::getClassQuestions())
            {
                sol::table answers(lua, sol::create);
                for (int specialization = ESM::Class::Combat; specialization <= ESM::Class::Stealth; ++specialization)
                    answers[ESM::Class::specializationIndexToLuaId[specialization]] = question.mAnswers[specialization];
                sol::table entry(lua, sol::create);
                entry["question"] = question.mQuestion;
                entry["answers"] = answers;
                entry["sound"] = Misc::ResourceHelpers::correctSoundPath(question.mSound).value();
                result.add(entry);
            }
            return result;
        };
        player["getCharGenClassForAnswers"] = [](const sol::table& answers) -> std::string {
            const unsigned combat = toAnswerCount(answers, ESM::Class::specializationIndexToLuaId[ESM::Class::Combat]);
            const unsigned magic = toAnswerCount(answers, ESM::Class::specializationIndexToLuaId[ESM::Class::Magic]);
            const unsigned stealth
                = toAnswerCount(answers, ESM::Class::specializationIndexToLuaId[ESM::Class::Stealth]);
            if (combat + magic + stealth != MWMechanics::sClassQuestionCount)
                throw std::runtime_error(
                    "The answers must add up to " + std::to_string(MWMechanics::sClassQuestionCount) + " questions");
            return MWMechanics::getGeneratedClass(combat, magic, stealth).serializeText();
        };
    }
}
