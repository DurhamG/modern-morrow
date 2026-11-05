#ifndef GAME_MWDIALOGUE_KEYWORDSEARCH_H
#define GAME_MWDIALOGUE_KEYWORDSEARCH_H

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Translation
{
    class Storage;
}

namespace MWDialogue
{

    class KeywordSearch
    {
        struct Entry
        {
            std::string mKeyword;
            std::string mTopicId;
            std::map<char, Entry> mChildren;
        };

        void buildTrie(std::string_view keyword, std::string_view value, std::size_t depth, Entry& entry);
        Entry mRoot;

    public:
        using Point = std::string::const_iterator;

        struct Match
        {
            Point mBeg;
            Point mEnd;
            std::string mTopicId;
            bool mExplicit{ false };

            std::string_view getDisplayName() const;
        };

        struct Link
        {
            /// Where the link starts and ends in the display text, in bytes
            std::size_t mBegin;
            std::size_t mEnd;
            std::string mTopicId;
        };

        /// Text as the dialogue window shows it, and the topics that parts of it link to
        struct DisplayText
        {
            std::string mText;
            std::vector<Link> mLinks;
        };

        void seed(std::string_view keyword, std::string_view value);
        void clear();
        void highlightKeywords(Point beg, Point end, std::vector<Match>& out) const;
        std::vector<KeywordSearch::Match> parseHyperText(
            const std::string& text, const Translation::Storage& storage) const;

        /// Replaces each explicit link (@...#) by the name it shows, and lists the explicit links and the seeded
        /// keywords that the text contains
        DisplayText getDisplayText(const std::string& text, const Translation::Storage& storage) const;
    };

}

#endif
