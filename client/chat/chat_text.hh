#ifndef NX3D_CLIENT_CHAT_CHAT_TEXT_HH
#define NX3D_CLIENT_CHAT_CHAT_TEXT_HH

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nx3d::client::chat
{

inline constexpr std::size_t kMaxMessageCharacters = 200;
inline constexpr std::size_t kLineCharacters = 34;

inline bool isUtf8Continuation(unsigned char byte)
{
    return (byte & 0xc0u) == 0x80u;
}

inline std::size_t characterCount(std::string_view text)
{
    std::size_t count = 0;
    for (const unsigned char byte : text)
    {
        if (!isUtf8Continuation(byte))
            ++count;
    }
    return count;
}

inline std::size_t byteOffsetForCharacters(std::string_view text, std::size_t characters)
{
    if (characters == 0)
        return 0;

    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (!isUtf8Continuation(static_cast<unsigned char>(text[i])) && ++count > characters)
            return i;
    }
    return text.size();
}

inline std::string truncate(std::string_view text, std::size_t maxCharacters)
{
    return std::string(text.substr(0, byteOffsetForCharacters(text, maxCharacters)));
}

inline void appendWithinLimit(std::string& destination,
                              std::string_view addition,
                              std::size_t maxCharacters = kMaxMessageCharacters)
{
    const auto used = characterCount(destination);
    if (used >= maxCharacters)
        return;
    destination.append(addition.substr(0, byteOffsetForCharacters(addition, maxCharacters - used)));
}

inline void popBack(std::string& text)
{
    if (text.empty())
        return;

    auto offset = text.size() - 1;
    while (offset > 0 && isUtf8Continuation(static_cast<unsigned char>(text[offset])))
        --offset;
    text.erase(offset);
}

inline std::string tail(std::string_view text, std::size_t maxCharacters)
{
    const auto count = characterCount(text);
    if (count <= maxCharacters)
        return std::string(text);
    const auto offset = byteOffsetForCharacters(text, count - maxCharacters);
    return std::string(text.substr(offset));
}

inline std::vector<std::string> wrapMessage(const std::string& username,
                                            const std::string& payload,
                                            std::size_t lineCharacters = kLineCharacters)
{
    std::string remaining = username + ": " + truncate(payload, kMaxMessageCharacters);
    for (char& character : remaining)
    {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20u || byte == 0x7fu)
            character = ' ';
    }

    std::vector<std::string> lines;
    while (characterCount(remaining) > lineCharacters)
    {
        const auto limit = byteOffsetForCharacters(remaining, lineCharacters);
        auto split = remaining.rfind(' ', limit - 1);
        if (split == std::string::npos || split == 0)
            split = limit;

        lines.push_back(remaining.substr(0, split));
        remaining.erase(0, split);
        while (!remaining.empty() && remaining.front() == ' ')
            remaining.erase(remaining.begin());
    }
    if (!remaining.empty())
        lines.push_back(std::move(remaining));
    return lines;
}

} // namespace nx3d::client::chat

#endif
