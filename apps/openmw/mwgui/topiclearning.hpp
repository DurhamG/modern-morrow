#ifndef MWGUI_TOPICLEARNING_H
#define MWGUI_TOPICLEARNING_H

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <components/esm/refid.hpp>
#include <components/esm3/dialoguecondition.hpp>

namespace MWGui
{
    /// Works out when the player can first learn a topic, by a quest's stage. The player learns a topic when a line of
    /// dialogue names it, at the start of a word, or when a line's result or a script runs AddTopic. A line that moves
    /// a quest on in a topic the player can't know yet isn't a way forward yet, such as Caius Cosades's line in "Blades
    /// Apprentice", which only lines for a later stage of the same quest name.
    class TopicLearning
    {
    public:
        /// Notes a topic whose learning matters, such as one with lines that move quests on. Lines that name other
        /// topics are not looked at, to keep this quick.
        void addTopic(std::string_view topic);

        /// Notes a line of dialogue. The topics its text names, and those its result adds, can be learned at the
        /// stages its conditions allow.
        void addLine(
            std::string_view text, std::string_view result, const std::vector<ESM::DialogueCondition>& conditions);

        /// Notes a script. The topics it adds can be learned at any stage.
        void addScript(std::string_view text);

        /// The lowest stage of a quest at which the player can learn a topic. That is 0 if any line or script can
        /// teach it without the quest having reached a stage, or if nothing teaches it, since a topic can also be
        /// known some other way, such as from a mod's Lua script.
        int learnedAt(std::string_view topic, const ESM::RefId& quest) const;

    private:
        // The lines that need a quest to have reached a stage, and the lowest stage any of them needs.
        struct Needs
        {
            int mLines = 0;
            int mLowest = 0;
        };

        struct Topic
        {
            // Whether something teaches the topic whatever the stages of quests.
            bool mAnyTime = false;
            // How many lines teach the topic, and what they need, by quest.
            int mLines = 0;
            std::unordered_map<ESM::RefId, Needs> mNeeds;
        };

        void learn(const std::string& topic, const std::vector<ESM::DialogueCondition>* conditions);
        static void learn(Topic& topic, const std::vector<ESM::DialogueCondition>* conditions);

        // The topics that matter, by their names in lowercase.
        std::unordered_map<std::string, Topic> mTopics;
        // The topics that matter by the first three letters of their name, as a number, to find them in a line
        // quickly.
        std::unordered_map<std::uint32_t, std::vector<std::pair<std::string, Topic*>>> mByStart;
        // Whether any topic starts with three letters, by the letters as a number, so that most words of a line are
        // passed over without looking them up.
        std::vector<bool> mStarts = std::vector<bool>(1 << 24);
        // The topics with names shorter than three letters, which are looked for at every word.
        std::vector<std::pair<std::string, Topic*>> mShortTopics;
    };
}

#endif
