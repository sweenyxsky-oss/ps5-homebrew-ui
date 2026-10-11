// ps5-homebrew-ui - loopback-only ShadowMount Plus API client.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "library/shadowmount_catalog.hpp"

namespace hui::library
{

enum class FetchError
{
    none,
    socket_unavailable,
    connect_failed,
    request_failed,
    response_too_large,
    invalid_http_response,
    api_error,
};

struct FetchResult
{
    FetchError error = FetchError::none;
    ParseError parse_error = ParseError::none;
    int http_status = 0;
    std::size_t count = 0;
    explicit operator bool() const { return error == FetchError::none; }
};

// Reads the indexed game snapshot from the local ShadowMount Plus service.
// It never accepts a caller-supplied host, so it cannot send credentials or
// catalog requests to a remote machine.
FetchResult fetch_shadowmount_games(Catalog &catalog);

} // namespace hui::library
