#ifndef MWGUI_JOURNALLINKS_H
#define MWGUI_JOURNALLINKS_H

#include <cstddef>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace MWDialogue
{
    class KeywordSearch;
    class Topic;
}

namespace Translation
{
    class Storage;
}

namespace MWGui
{
    /// Text with the topics in it marked, as start and end offsets in the text.
    struct LinkedText
    {
        std::string mText;
        std::vector<std::tuple<std::size_t, std::size_t, const MWDialogue::Topic*>> mSpans;
    };

    /// Finds the topics the player has asked about in text, as the journal book does, leaving out the topic whose
    /// page the text is on. The text shows each topic by its display name. The keyword search finds every topic the
    /// player knows, and topics holds the ones with pages, by their lower case names.
    LinkedText linkTopics(const std::string& text, const MWDialogue::KeywordSearch& keywordSearch,
        const Translation::Storage& translations,
        const std::unordered_map<std::string, const MWDialogue::Topic*>& topics, const MWDialogue::Topic* page);
}

#endif
