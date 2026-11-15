#include "journallinks.hpp"

#include <string_view>

#include "../mwdialogue/keywordsearch.hpp"

namespace MWGui
{
    LinkedText linkTopics(const std::string& text, const MWDialogue::KeywordSearch& keywordSearch,
        const Translation::Storage& translations,
        const std::unordered_map<std::string, const MWDialogue::Topic*>& topics, const MWDialogue::Topic* page)
    {
        LinkedText result;
        MWDialogue::KeywordSearch::Point position = text.begin();
        for (const MWDialogue::KeywordSearch::Match& match : keywordSearch.parseHyperText(text, translations))
        {
            const std::string_view name = match.getDisplayName();
            result.mText.append(position, match.mBeg);
            const std::size_t start = result.mText.size();
            result.mText.append(name);
            position = match.mEnd;
            const auto found = topics.find(match.mTopicId);
            if (found != topics.end() && found->second != page)
                result.mSpans.emplace_back(start, result.mText.size(), found->second);
        }
        result.mText.append(position, text.end());
        return result;
    }
}
