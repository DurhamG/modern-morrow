#ifndef OPENMW_MECHANICS_CHARGEN_H
#define OPENMW_MECHANICS_CHARGEN_H

#include <array>
#include <string_view>
#include <vector>

#include <components/esm/refid.hpp>
#include <components/esm3/loadbody.hpp>
#include <components/vfs/pathutil.hpp>

namespace ESM
{
    struct Class;
}

namespace MWMechanics
{
    /// One of the questions that character creation asks to find a class for the player.
    struct ClassQuestion
    {
        std::string_view mQuestion;
        /// The answers for each specialization, indexed by ESM::Class::Specialization.
        std::array<std::string_view, 3> mAnswers;
        /// The voice that reads out the question.
        VFS::Path::Normalized mSound;
    };

    /// The number of questions that character creation asks to find a class for the player.
    constexpr unsigned sClassQuestionCount = 10;

    /// The questions that character creation asks to find a class for the player, in the order it asks them.
    /// The texts come from the game's fallback settings.
    std::vector<ClassQuestion> getClassQuestions();

    /// The class that the answers to the class questions lead to. The arguments count the answers that the player
    /// chose for each specialization.
    ESM::RefId getGeneratedClass(unsigned combat, unsigned magic, unsigned stealth);

    /// The heads or the hairs that the player may choose for a race and a gender in character creation. These are the
    /// playable skin body parts of that race and gender, except the ones for the first person view.
    std::vector<ESM::RefId> getChargenBodyParts(ESM::BodyPart::MeshPart part, const ESM::RefId& race, bool isMale);

    /// Why character creation can't give the player @a value as a class that they made up, or an empty string if it
    /// can. Such a class needs a name, a specialization, two different favorite attributes, and ten different skills.
    std::string_view getCustomClassProblem(const ESM::Class& value);
}

#endif
