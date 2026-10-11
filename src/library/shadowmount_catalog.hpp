// ps5-homebrew-ui - ShadowMount Plus native catalog model.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>

namespace hui::library
{

constexpr std::size_t kMaxGames = 256;
constexpr std::size_t kTitleIdCapacity = 16;
constexpr std::size_t kTitleNameCapacity = 192;
constexpr std::size_t kPathCapacity = 1024;
constexpr std::size_t kSourceTypeCapacity = 16;
constexpr std::size_t kVersionCapacity = 32;

struct Game
{
    char title_id[kTitleIdCapacity]{};
    char name[kTitleNameCapacity]{};
    char path[kPathCapacity]{};
    char source_type[kSourceTypeCapacity]{};
    char version[kVersionCapacity]{};
    std::uint64_t size_bytes = 0;
    bool has_size = false;
    bool installed = false;
    bool installed_pkg = false;
    bool is_ps5 = false;
};

struct Catalog
{
    Game games[kMaxGames]{};
    std::size_t count = 0;
};

enum class ParseError
{
    none,
    input_too_large,
    malformed_json,
    unsuccessful_response,
    missing_games,
    too_many_games,
};

struct ParseResult
{
    ParseError error = ParseError::none;
    std::size_t count = 0;
    explicit operator bool() const { return error == ParseError::none; }
};

// Parses ShadowMount Plus POST /api/v1/games responses. Parsing is bounded,
// rejects malformed JSON, and only commits a catalog after full validation.
ParseResult parse_shadowmount_games(const char *json, std::size_t length, Catalog &out);

bool valid_title_id(const char *text, std::size_t length);

} // namespace hui::library
