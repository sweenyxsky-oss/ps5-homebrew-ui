// ps5-homebrew-ui - ShadowMount Plus native catalog parser.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "library/shadowmount_catalog.hpp"

#include <cstring>
#include <vector>

namespace hui::library
{
namespace
{

constexpr std::size_t kMaxJsonBytes = 2u * 1024u * 1024u;
constexpr std::size_t kMaxTokens = 16384;
constexpr unsigned kMaxDepth = 32;

enum class Kind : unsigned char
{
    object,
    array,
    string,
    primitive,
};

struct Token
{
    Kind kind = Kind::primitive;
    std::size_t begin = 0;
    std::size_t end = 0;
    std::size_t first = static_cast<std::size_t>(-1);
    std::size_t last = static_cast<std::size_t>(-1);
    std::size_t next = static_cast<std::size_t>(-1);
};

bool valid_primitive(const char *text, std::size_t length);

class JsonParser
{
  public:
    JsonParser(const char *source, std::size_t length) : source_(source), length_(length)
    {
        tokens_.reserve(1024);
    }

    bool parse(std::size_t &root)
    {
        skip_space();
        if (!value(root, 0))
            return false;
        skip_space();
        return position_ == length_;
    }

    const Token &token(std::size_t index) const { return tokens_[index]; }
    const char *source() const { return source_; }

  private:
    void skip_space()
    {
        while (position_ < length_ && (source_[position_] == ' ' || source_[position_] == '\n' ||
                                       source_[position_] == '\r' || source_[position_] == '\t'))
            ++position_;
    }

    std::size_t add(Kind kind, std::size_t begin)
    {
        if (tokens_.size() >= kMaxTokens)
            return static_cast<std::size_t>(-1);
        tokens_.push_back(Token{});
        Token &item = tokens_.back();
        item.kind = kind;
        item.begin = begin;
        return tokens_.size() - 1;
    }

    bool append(std::size_t parent, std::size_t child)
    {
        if (child == static_cast<std::size_t>(-1))
            return false;
        Token &owner = tokens_[parent];
        if (owner.first == static_cast<std::size_t>(-1))
            owner.first = child;
        else
            tokens_[owner.last].next = child;
        owner.last = child;
        return true;
    }

    bool string(std::size_t &index)
    {
        const std::size_t begin = ++position_;
        index = add(Kind::string, begin);
        if (index == static_cast<std::size_t>(-1))
            return false;
        while (position_ < length_)
        {
            const unsigned char ch = static_cast<unsigned char>(source_[position_++]);
            if (ch == '"')
            {
                tokens_[index].end = position_ - 1;
                return true;
            }
            if (ch < 0x20)
                return false;
            if (ch == '\\')
            {
                if (position_ >= length_)
                    return false;
                const char escaped = source_[position_++];
                if (escaped == 'u')
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        if (position_ >= length_ || !hex(source_[position_++]))
                            return false;
                    }
                }
                else if (escaped != '"' && escaped != '\\' && escaped != '/' &&
                         escaped != 'b' && escaped != 'f' && escaped != 'n' &&
                         escaped != 'r' && escaped != 't')
                    return false;
            }
        }
        return false;
    }

    static bool hex(char ch)
    {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
               (ch >= 'A' && ch <= 'F');
    }

    bool value(std::size_t &index, unsigned depth)
    {
        if (depth > kMaxDepth || position_ >= length_)
            return false;
        const char ch = source_[position_];
        if (ch == '"')
            return string(index);
        if (ch == '{' || ch == '[')
        {
            const bool object = ch == '{';
            ++position_;
            index = add(object ? Kind::object : Kind::array, position_ - 1);
            if (index == static_cast<std::size_t>(-1))
                return false;
            skip_space();
            const char close = object ? '}' : ']';
            if (position_ < length_ && source_[position_] == close)
            {
                tokens_[index].end = ++position_;
                return true;
            }
            while (position_ < length_)
            {
                if (object)
                {
                    if (source_[position_] != '"')
                        return false;
                    std::size_t key = 0;
                    if (!string(key) || !append(index, key))
                        return false;
                    skip_space();
                    if (position_ >= length_ || source_[position_++] != ':')
                        return false;
                    skip_space();
                }
                std::size_t child = 0;
                if (!value(child, depth + 1) || !append(index, child))
                    return false;
                skip_space();
                if (position_ >= length_)
                    return false;
                if (source_[position_] == close)
                {
                    tokens_[index].end = ++position_;
                    return true;
                }
                if (source_[position_++] != ',')
                    return false;
                skip_space();
            }
            return false;
        }
        const std::size_t begin = position_;
        while (position_ < length_ && source_[position_] != ',' && source_[position_] != '}' &&
               source_[position_] != ']' && source_[position_] != ' ' &&
               source_[position_] != '\n' && source_[position_] != '\r' &&
               source_[position_] != '\t')
            ++position_;
        if (position_ == begin)
            return false;
        index = add(Kind::primitive, begin);
        if (index == static_cast<std::size_t>(-1))
            return false;
        tokens_[index].end = position_;
        return valid_primitive(source_ + begin, position_ - begin);
    }

    const char *source_;
    std::size_t length_;
    std::size_t position_ = 0;
    std::vector<Token> tokens_;
};

int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

bool valid_primitive(const char *text, std::size_t length)
{
    if ((length == 4 && std::memcmp(text, "true", 4) == 0) ||
        (length == 5 && std::memcmp(text, "false", 5) == 0) ||
        (length == 4 && std::memcmp(text, "null", 4) == 0))
        return true;
    std::size_t i = 0;
    if (i < length && text[i] == '-') ++i;
    if (i >= length) return false;
    if (text[i] == '0')
    {
        ++i;
        if (i < length && text[i] >= '0' && text[i] <= '9') return false;
    }
    else
    {
        if (text[i] < '1' || text[i] > '9') return false;
        while (i < length && text[i] >= '0' && text[i] <= '9') ++i;
    }
    if (i < length && text[i] == '.')
    {
        ++i;
        const std::size_t digits = i;
        while (i < length && text[i] >= '0' && text[i] <= '9') ++i;
        if (i == digits) return false;
    }
    if (i < length && (text[i] == 'e' || text[i] == 'E'))
    {
        ++i;
        if (i < length && (text[i] == '+' || text[i] == '-')) ++i;
        const std::size_t digits = i;
        while (i < length && text[i] >= '0' && text[i] <= '9') ++i;
        if (i == digits) return false;
    }
    return i == length;
}

bool append_utf8(std::uint32_t cp, char *out, std::size_t capacity, std::size_t &used)
{
    unsigned char bytes[4];
    std::size_t count = 0;
    if (cp <= 0x7f) bytes[count++] = static_cast<unsigned char>(cp);
    else if (cp <= 0x7ff)
    {
        bytes[count++] = static_cast<unsigned char>(0xc0 | (cp >> 6));
        bytes[count++] = static_cast<unsigned char>(0x80 | (cp & 0x3f));
    }
    else if (cp <= 0xffff && !(cp >= 0xd800 && cp <= 0xdfff))
    {
        bytes[count++] = static_cast<unsigned char>(0xe0 | (cp >> 12));
        bytes[count++] = static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3f));
        bytes[count++] = static_cast<unsigned char>(0x80 | (cp & 0x3f));
    }
    else if (cp <= 0x10ffff)
    {
        bytes[count++] = static_cast<unsigned char>(0xf0 | (cp >> 18));
        bytes[count++] = static_cast<unsigned char>(0x80 | ((cp >> 12) & 0x3f));
        bytes[count++] = static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3f));
        bytes[count++] = static_cast<unsigned char>(0x80 | (cp & 0x3f));
    }
    else return false;
    if (used + count >= capacity) return false;
    for (std::size_t i = 0; i < count; ++i) out[used++] = static_cast<char>(bytes[i]);
    return true;
}

bool decode(const JsonParser &parser, std::size_t index, char *out, std::size_t capacity)
{
    const Token &token = parser.token(index);
    if (token.kind != Kind::string || capacity == 0)
        return false;
    const char *source = parser.source();
    std::size_t used = 0;
    for (std::size_t i = token.begin; i < token.end; ++i)
    {
        unsigned char ch = static_cast<unsigned char>(source[i]);
        if (ch == '\\')
        {
            if (++i >= token.end) return false;
            switch (source[i])
            {
            case '"': ch = '"'; break;
            case '\\': ch = '\\'; break;
            case '/': ch = '/'; break;
            case 'b': ch = '\b'; break;
            case 'f': ch = '\f'; break;
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case 'u':
            {
                if (i + 4 >= token.end) return false;
                std::uint32_t cp = 0;
                for (int n = 0; n < 4; ++n)
                {
                    const int value = hex_value(source[++i]);
                    if (value < 0) return false;
                    cp = (cp << 4) | static_cast<std::uint32_t>(value);
                }
                if (cp >= 0xd800 && cp <= 0xdbff)
                {
                    if (i + 6 >= token.end || source[i + 1] != '\\' || source[i + 2] != 'u')
                        return false;
                    i += 2;
                    std::uint32_t low = 0;
                    for (int n = 0; n < 4; ++n)
                    {
                        const int value = hex_value(source[++i]);
                        if (value < 0) return false;
                        low = (low << 4) | static_cast<std::uint32_t>(value);
                    }
                    if (low < 0xdc00 || low > 0xdfff) return false;
                    cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
                }
                else if (cp >= 0xdc00 && cp <= 0xdfff)
                    return false;
                if (!append_utf8(cp, out, capacity, used)) return false;
                continue;
            }
            default: return false;
            }
        }
        if (ch == 0 || used + 1 >= capacity) return false;
        out[used++] = static_cast<char>(ch);
    }
    out[used] = '\0';
    return true;
}

std::size_t property(const JsonParser &parser, std::size_t object, const char *name)
{
    if (parser.token(object).kind != Kind::object)
        return static_cast<std::size_t>(-1);
    for (std::size_t key = parser.token(object).first; key != static_cast<std::size_t>(-1);)
    {
        const std::size_t value = parser.token(key).next;
        if (value == static_cast<std::size_t>(-1)) return value;
        char decoded[64];
        if (decode(parser, key, decoded, sizeof(decoded)) && std::strcmp(decoded, name) == 0)
            return value;
        key = parser.token(value).next;
    }
    return static_cast<std::size_t>(-1);
}

bool string_property(const JsonParser &parser, std::size_t object, const char *key,
                     char *out, std::size_t capacity, bool required)
{
    const std::size_t item = property(parser, object, key);
    if (item == static_cast<std::size_t>(-1))
        return !required;
    return decode(parser, item, out, capacity);
}

bool boolean_property(const JsonParser &parser, std::size_t object, const char *key, bool &out)
{
    const std::size_t item = property(parser, object, key);
    if (item == static_cast<std::size_t>(-1)) return true;
    const Token &token = parser.token(item);
    if (token.kind != Kind::primitive) return false;
    const std::size_t length = token.end - token.begin;
    if (length == 4 && std::memcmp(parser.source() + token.begin, "true", 4) == 0) out = true;
    else if (length == 5 && std::memcmp(parser.source() + token.begin, "false", 5) == 0) out = false;
    else return false;
    return true;
}

bool uint64_property(const JsonParser &parser, std::size_t object, const char *key,
                     std::uint64_t &out, bool &present)
{
    const std::size_t item = property(parser, object, key);
    if (item == static_cast<std::size_t>(-1)) return true;
    const Token &token = parser.token(item);
    if (token.kind != Kind::primitive || token.begin == token.end) return false;
    std::uint64_t value = 0;
    for (std::size_t i = token.begin; i < token.end; ++i)
    {
        const char digit = parser.source()[i];
        if (digit < '0' || digit > '9' || value > (UINT64_MAX - (digit - '0')) / 10)
            return false;
        value = value * 10 + static_cast<unsigned>(digit - '0');
    }
    out = value;
    present = true;
    return true;
}

bool valid_status(const JsonParser &parser, std::size_t root)
{
    const std::size_t status = property(parser, root, "status");
    if (status == static_cast<std::size_t>(-1)) return false;
    const Token &token = parser.token(status);
    return token.kind == Kind::primitive && token.end - token.begin == 1 &&
           parser.source()[token.begin] == '0';
}

bool read_game(const JsonParser &parser, std::size_t object, Game &game)
{
    if (!string_property(parser, object, "title_id", game.title_id, sizeof(game.title_id), true) ||
        !valid_title_id(game.title_id, std::strlen(game.title_id)) ||
        !string_property(parser, object, "title_name", game.name, sizeof(game.name), true) ||
        !string_property(parser, object, "path", game.path, sizeof(game.path), true) ||
        !string_property(parser, object, "source_type", game.source_type, sizeof(game.source_type), true) ||
        !string_property(parser, object, "version", game.version, sizeof(game.version), false) ||
        !boolean_property(parser, object, "installed", game.installed) ||
        !boolean_property(parser, object, "installed_pkg", game.installed_pkg) ||
        !uint64_property(parser, object, "size_bytes", game.size_bytes, game.has_size))
        return false;
    game.is_ps5 = std::strncmp(game.title_id, "PPSA", 4) == 0;
    return true;
}

} // namespace

bool valid_title_id(const char *text, std::size_t length)
{
    if (!text || length != 9) return false;
    const bool prefix = std::memcmp(text, "PPSA", 4) == 0 || std::memcmp(text, "CUSA", 4) == 0 ||
                        std::memcmp(text, "LAPY", 4) == 0 || std::memcmp(text, "FAKE", 4) == 0;
    if (!prefix) return false;
    for (std::size_t i = 4; i < 9; ++i)
        if (text[i] < '0' || text[i] > '9') return false;
    return true;
}

ParseResult parse_shadowmount_games(const char *json, std::size_t length, Catalog &out)
{
    if (!json || length > kMaxJsonBytes)
        return {ParseError::input_too_large, 0};
    JsonParser parser(json, length);
    std::size_t root = 0;
    if (!parser.parse(root) || parser.token(root).kind != Kind::object)
        return {ParseError::malformed_json, 0};
    if (!valid_status(parser, root))
        return {ParseError::unsuccessful_response, 0};
    const std::size_t games = property(parser, root, "games");
    if (games == static_cast<std::size_t>(-1) || parser.token(games).kind != Kind::array)
        return {ParseError::missing_games, 0};

    Catalog candidate{};
    for (std::size_t item = parser.token(games).first; item != static_cast<std::size_t>(-1);
         item = parser.token(item).next)
    {
        if (candidate.count == kMaxGames)
            return {ParseError::too_many_games, 0};
        Game game{};
        if (!read_game(parser, item, game))
            return {ParseError::malformed_json, 0};
        candidate.games[candidate.count++] = game;
    }
    out = candidate;
    return {ParseError::none, out.count};
}

} // namespace hui::library
