// ps5-homebrew-ui - ShadowMount catalog parser tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "library/shadowmount_catalog.hpp"

#include <gtest/gtest.h>

#include <cstring>

namespace
{

using hui::library::Catalog;
using hui::library::ParseError;
using hui::library::parse_shadowmount_games;

TEST(ShadowMountCatalog, ParsesTitlesAndOptionalMeasuredSizes)
{
    static constexpr char kResponse[] =
        R"({"status":0,"count":2,"size_included":true,"games":[{"title_id":"PPSA12345","title_name":"A \"Great\" Game","path":"/mnt/usb0/Games/PPSA12345","source_type":"folder","version":"01.002","size_bytes":123456,"installed":true,"installed_pkg":false},{"title_id":"CUSA00001","title_name":"Legacy title","path":"/user/app/CUSA00001/app.pkg","source_type":"pkg","version":"","installed":true,"installed_pkg":true}]})";
    Catalog catalog{};
    const auto result = parse_shadowmount_games(kResponse, std::strlen(kResponse), catalog);

    ASSERT_TRUE(result);
    ASSERT_EQ(catalog.count, 2u);
    EXPECT_STREQ(catalog.games[0].title_id, "PPSA12345");
    EXPECT_STREQ(catalog.games[0].name, "A \"Great\" Game");
    EXPECT_STREQ(catalog.games[0].source_type, "folder");
    EXPECT_TRUE(catalog.games[0].is_ps5);
    EXPECT_TRUE(catalog.games[0].has_size);
    EXPECT_EQ(catalog.games[0].size_bytes, 123456u);
    EXPECT_FALSE(catalog.games[0].installed_pkg);
    EXPECT_FALSE(catalog.games[1].is_ps5);
    EXPECT_FALSE(catalog.games[1].has_size);
    EXPECT_TRUE(catalog.games[1].installed_pkg);
}

TEST(ShadowMountCatalog, DecodesUtf16EscapesAndRejectsInvalidTitleIds)
{
    static constexpr char kResponse[] =
        R"({"status":0,"games":[{"title_id":"PPSA12345","title_name":"Cafe\u0301 \ud83c\udfae","path":"/mnt/usb0/PPSA12345","source_type":"image","version":"","installed":false,"installed_pkg":false}]})";
    Catalog catalog{};
    const auto result = parse_shadowmount_games(kResponse, std::strlen(kResponse), catalog);
    ASSERT_TRUE(result);
    EXPECT_STREQ(catalog.games[0].name, "Cafe\xcc\x81 \xf0\x9f\x8e\xae");

    static constexpr char kInvalid[] =
        R"({"status":0,"games":[{"title_id":"BAD123456","title_name":"Bad","path":"/bad","source_type":"folder"}]})";
    EXPECT_EQ(parse_shadowmount_games(kInvalid, std::strlen(kInvalid), catalog).error,
              ParseError::malformed_json);
}

TEST(ShadowMountCatalog, RejectsBadStatusMalformedJsonAndPartialGameRecords)
{
    Catalog catalog{};
    static constexpr char kError[] = R"({"status":22,"games":[]})";
    EXPECT_EQ(parse_shadowmount_games(kError, std::strlen(kError), catalog).error,
              ParseError::unsuccessful_response);
    static constexpr char kMalformed[] = R"({"status":0,"games":[)";
    EXPECT_EQ(parse_shadowmount_games(kMalformed, std::strlen(kMalformed), catalog).error,
              ParseError::malformed_json);
    static constexpr char kMissingField[] =
        R"({"status":0,"games":[{"title_id":"PPSA12345","title_name":"Missing path","source_type":"folder"}]})";
    EXPECT_EQ(parse_shadowmount_games(kMissingField, std::strlen(kMissingField), catalog).error,
              ParseError::malformed_json);
    static constexpr char kInvalidPrimitive[] = R"({"status":0,"junk":NaN,"games":[]})";
    EXPECT_EQ(parse_shadowmount_games(kInvalidPrimitive, std::strlen(kInvalidPrimitive), catalog).error,
              ParseError::malformed_json);
}

TEST(ShadowMountCatalog, DoesNotReplaceLastCatalogWhenResponseIsInvalid)
{
    static constexpr char kGood[] =
        R"({"status":0,"games":[{"title_id":"PPSA12345","title_name":"Saved","path":"/a","source_type":"folder"}]})";
    Catalog catalog{};
    ASSERT_TRUE(parse_shadowmount_games(kGood, std::strlen(kGood), catalog));
    static constexpr char kBad[] = R"({"status":0,"games":[null]})";
    EXPECT_FALSE(parse_shadowmount_games(kBad, std::strlen(kBad), catalog));
    EXPECT_EQ(catalog.count, 1u);
    EXPECT_STREQ(catalog.games[0].name, "Saved");
}

} // namespace
