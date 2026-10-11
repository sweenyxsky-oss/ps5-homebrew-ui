// ps5-homebrew-ui - loopback-only ShadowMount Plus API client.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "library/shadowmount_client.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace hui::library
{
namespace
{

constexpr std::uint16_t kApiPort = 10101;
constexpr std::size_t kMaxBodyBytes = 2u * 1024u * 1024u;
constexpr std::size_t kMaxHeaderBytes = 8192;

struct Socket
{
    int fd = -1;
    ~Socket() { if (fd >= 0) ::close(fd); }
};

char lower_ascii(char ch)
{
    return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : ch;
}

bool equals_ascii(const char *left, std::size_t length, const char *right)
{
    if (std::strlen(right) != length) return false;
    for (std::size_t i = 0; i < length; ++i)
        if (lower_ascii(left[i]) != lower_ascii(right[i])) return false;
    return true;
}

bool parse_content_length(const std::string &headers, std::size_t &body_offset,
                          std::size_t &body_length, int &status)
{
    const std::size_t divider = headers.find("\r\n\r\n");
    if (divider == std::string::npos || divider > kMaxHeaderBytes)
        return false;
    const std::size_t first_line_end = headers.find("\r\n");
    if (first_line_end == std::string::npos || first_line_end > divider)
        return false;
    const char *line = headers.data();
    if (first_line_end < 12 || std::memcmp(line, "HTTP/1.", 7) != 0 ||
        line[8] != ' ' || line[9] < '0' || line[9] > '9' ||
        line[10] < '0' || line[10] > '9' || line[11] < '0' || line[11] > '9')
        return false;
    status = (line[9] - '0') * 100 + (line[10] - '0') * 10 + line[11] - '0';

    bool has_length = false;
    std::size_t cursor = first_line_end + 2;
    while (cursor < divider)
    {
        const std::size_t end = headers.find("\r\n", cursor);
        if (end == std::string::npos || end > divider) return false;
        const std::size_t colon = headers.find(':', cursor);
        if (colon == std::string::npos || colon >= end) return false;
        if (equals_ascii(headers.data() + cursor, colon - cursor, "content-length"))
        {
            std::size_t value = colon + 1;
            while (value < end && (headers[value] == ' ' || headers[value] == '\t')) ++value;
            if (value == end) return false;
            std::size_t parsed = 0;
            for (; value < end; ++value)
            {
                const char digit = headers[value];
                if (digit < '0' || digit > '9' || parsed > (kMaxBodyBytes - (digit - '0')) / 10)
                    return false;
                parsed = parsed * 10 + static_cast<unsigned>(digit - '0');
            }
            body_length = parsed;
            has_length = true;
        }
        cursor = end + 2;
    }
    body_offset = divider + 4;
    return has_length && body_offset <= headers.size();
}

FetchResult fail(FetchError error, int http_status = 0)
{
    FetchResult result{};
    result.error = error;
    result.http_status = http_status;
    return result;
}

} // namespace

FetchResult fetch_shadowmount_games(Catalog &catalog, std::string *snapshot_json)
{
    Socket socket;
    socket.fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket.fd < 0)
        return fail(FetchError::socket_unavailable);

    timeval timeout{};
    timeout.tv_sec = 2;
    if (::setsockopt(socket.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        ::setsockopt(socket.fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0)
        return fail(FetchError::socket_unavailable);

    sockaddr_in address{};
#if defined(__FreeBSD__)
    address.sin_len = sizeof(address);
#endif
    address.sin_family = AF_INET;
    address.sin_port = htons(kApiPort);
    if (::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1 ||
        ::connect(socket.fd, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0)
        return fail(FetchError::connect_failed);

    static constexpr char request[] =
        "POST /api/v1/games HTTP/1.1\r\n"
        "Host: 127.0.0.1:10101\r\n"
        "Content-Type: application/json\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n"
        "Content-Length: 21\r\n\r\n"
        "{\"include_size\":true}";
    std::size_t sent = 0;
    while (sent < sizeof(request) - 1)
    {
        const ssize_t written = ::send(socket.fd, request + sent, sizeof(request) - 1 - sent, 0);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return fail(FetchError::request_failed);
        sent += static_cast<std::size_t>(written);
    }

    std::string response;
    response.reserve(16u * 1024u);
    std::size_t body_offset = 0;
    std::size_t body_length = 0;
    int http_status = 0;
    bool headers_ready = false;
    char chunk[4096];
    while (response.size() < kMaxHeaderBytes + kMaxBodyBytes)
    {
        const ssize_t received = ::recv(socket.fd, chunk, sizeof(chunk), 0);
        if (received < 0 && errno == EINTR) continue;
        if (received <= 0) break;
        response.append(chunk, static_cast<std::size_t>(received));
        if (response.size() > kMaxHeaderBytes + kMaxBodyBytes)
            return fail(FetchError::response_too_large);
        if (!headers_ready)
        {
            const std::size_t divider = response.find("\r\n\r\n");
            if (divider == std::string::npos)
            {
                if (response.size() > kMaxHeaderBytes) return fail(FetchError::invalid_http_response);
                continue;
            }
            if (!parse_content_length(response, body_offset, body_length, http_status))
                return fail(FetchError::invalid_http_response);
            headers_ready = true;
        }
        if (headers_ready && response.size() >= body_offset + body_length)
            break;
    }
    if (!headers_ready)
        return fail(FetchError::invalid_http_response);
    if (response.size() < body_offset + body_length)
        return fail(FetchError::invalid_http_response, http_status);
    if (http_status != 200)
        return fail(FetchError::api_error, http_status);

    const ParseResult parsed = parse_shadowmount_games(response.data() + body_offset,
                                                       body_length, catalog);
    if (!parsed)
    {
        FetchResult result = fail(FetchError::api_error, http_status);
        result.parse_error = parsed.error;
        return result;
    }
    if (snapshot_json)
        snapshot_json->assign(response.data() + body_offset, body_length);
    FetchResult result{};
    result.count = parsed.count;
    result.http_status = http_status;
    return result;
}

} // namespace hui::library

