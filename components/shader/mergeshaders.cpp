#include "mergeshaders.hpp"

namespace Shader
{
    std::string declareUniformOnce(std::string_view line)
    {
        const std::size_t first = line.find_first_not_of(" \t");
        if (first == std::string_view::npos || !line.substr(first).starts_with("uniform "))
            return std::string(line);
        std::string_view declaration = line.substr(first);
        const std::size_t last = declaration.find_last_not_of(" \t\r");
        if (declaration[last] != ';' || declaration.find_first_of("=,(") != std::string_view::npos)
            return std::string(line);
        declaration = declaration.substr(0, last);
        if (declaration.ends_with(']'))
        {
            const std::size_t open = declaration.rfind('[');
            if (open == std::string_view::npos)
                return std::string(line);
            declaration = declaration.substr(0, open);
        }
        const std::size_t end = declaration.find_last_not_of(" \t");
        const std::size_t begin = declaration.find_last_of(" \t", end);
        if (end == std::string_view::npos || begin == std::string_view::npos)
            return std::string(line);
        const std::string guard = "OMW_UNIFORM_" + std::string(declaration.substr(begin + 1, end - begin));
        return "#ifndef " + guard + "\n#define " + guard + "\n" + std::string(line) + "\n#endif";
    }

    std::string foldForMerge(const std::string& source, bool linked)
    {
        std::string out;
        out.reserve(source.size() + source.size() / 8);
        std::size_t pos = 0;
        while (pos < source.size())
        {
            const std::size_t end = source.find('\n', pos);
            const std::size_t lineEnd = end == std::string::npos ? source.size() : end;
            const std::string_view line(source.data() + pos, lineEnd - pos);
            const std::size_t first = line.find_first_not_of(" \t");
            if (!linked || first == std::string_view::npos || !line.substr(first).starts_with("#version"))
                out += declareUniformOnce(line);
            if (end != std::string::npos)
                out.push_back('\n');
            pos = lineEnd + 1;
        }
        return out;
    }
}
