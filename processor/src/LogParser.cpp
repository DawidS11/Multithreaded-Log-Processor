#include "LogParser.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <system_error>

namespace logproc {
namespace {

constexpr std::array<std::string_view, 12> kMonths{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Returns the text before delim and removes that text and the delimiter from s.
// Returns nullopt if the delimiter isn't found.
std::optional<std::string_view> take_until(std::string_view& s, char delim) {
    const auto pos = s.find(delim);
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    const auto token = s.substr(0, pos);
    s.remove_prefix(pos + 1);
    return token;
}

// Parses the entire text as an integer. Fails on empty text, extra characters, or overflow.
template <typename T>
std::optional<T> parse_integer(std::string_view text) {
    T value{};
    const char* last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), last, value);
    if (ec != std::errc{} || ptr != last) {
        return std::nullopt;
    }
    return value;
}

// Parses "24/Oct/2026:10:15:32 +0000" into a UTC time point.
// Only +0000 is accepted, since that's what the generator writes.
std::optional<std::chrono::sys_seconds> parse_timestamp(std::string_view s) {
    using namespace std::chrono;

    const auto day_text = take_until(s, '/');
    const auto month_text = take_until(s, '/');
    const auto year_text = take_until(s, ':');
    const auto hour_text = take_until(s, ':');
    const auto minute_text = take_until(s, ':');
    const auto second_text = take_until(s, ' ');
    if (!day_text || !month_text || !year_text || !hour_text || !minute_text || !second_text) {
        return std::nullopt;
    }
    if (s != "+0000") {
        return std::nullopt;
    }

    const auto month_it = std::ranges::find(kMonths, *month_text);
    if (month_it == kMonths.end()) {
        return std::nullopt;
    }

    const auto day = parse_integer<unsigned>(*day_text);
    const auto y = parse_integer<int>(*year_text);
    const auto h = parse_integer<int>(*hour_text);
    const auto mi = parse_integer<int>(*minute_text);
    const auto sec = parse_integer<int>(*second_text);
    if (!day || !y || !h || !mi || !sec) {
        return std::nullopt;
    }
    if (*h > 23 || *mi > 59 || *sec > 59) {
        return std::nullopt;
    }

    // Month numbers start at 1, the array index starts at 0.
    const auto month_number = static_cast<unsigned>(month_it - kMonths.begin()) + 1;
    const year_month_day date{year{*y}, month{month_number}, std::chrono::day{*day}};
    if (!date.ok()) {  // rejects things like 31 Feb
        return std::nullopt;
    }

    return sys_days{date} + hours{*h} + minutes{*mi} + seconds{*sec};
}

}  // namespace

std::string_view to_string(ParseError error) noexcept {
    switch (error) {
        case ParseError::MissingField:  return "missing field";
        case ParseError::BadTimestamp:  return "bad timestamp";
        case ParseError::BadRequest:    return "bad request";
        case ParseError::BadStatus:     return "bad status";
        case ParseError::BadBytes:      return "bad bytes";
        case ParseError::BadLatency:    return "bad latency";
    }
    return "unknown";
}

std::expected<LogRecord, ParseError> parse_line(std::string_view line) {
    // Line example:
    // 10.0.0.5 - - [24/Oct/2025:10:00:04 +0000] "GET /login HTTP/1.1" 500 1024 1800
    
    LogRecord record;

    // Client address. We require it to be present but don't store it.
    const auto client = take_until(line, ' ');
    if (!client || client->empty()) {
        return std::unexpected(ParseError::MissingField);
    }

    // Identity fields are always "- -" in this format.
    const auto ident = take_until(line, '[');
    if (!ident || *ident != "- - ") {
        return std::unexpected(ParseError::MissingField);
    }

    const auto stamp = take_until(line, ']');
    if (!stamp) {
        return std::unexpected(ParseError::MissingField);
    }
    const auto when = parse_timestamp(*stamp);
    if (!when) {
        return std::unexpected(ParseError::BadTimestamp);
    }
    record.timestamp = *when;

    // Request is quoted: "METHOD PATH PROTOCOL"
    if (!line.starts_with(" \"")) {
        return std::unexpected(ParseError::BadRequest);
    }
    line.remove_prefix(2);

    auto request = take_until(line, '"');
    if (!request) {
        return std::unexpected(ParseError::BadRequest);
    }
    const auto method = take_until(*request, ' ');
    const auto path = take_until(*request, ' ');
    if (!method || !path || method->empty() || path->empty() || !request->starts_with("HTTP/")) {
        return std::unexpected(ParseError::BadRequest);
    }
    record.method = std::string{*method};
    record.path = std::string{*path};

    // Remaining fields: " STATUS BYTES LATENCY"
    if (!line.starts_with(' ')) {
        return std::unexpected(ParseError::MissingField);
    }
    line.remove_prefix(1);

    const auto status_text = take_until(line, ' ');
    if (!status_text) {
        return std::unexpected(ParseError::MissingField);
    }
    const auto status = parse_integer<std::uint16_t>(*status_text);
    if (!status) {
        return std::unexpected(ParseError::BadStatus);
    }
    record.status = *status;

    const auto bytes_text = take_until(line, ' ');
    if (!bytes_text) {
        return std::unexpected(ParseError::MissingField);
    }
    const auto bytes = parse_integer<std::uint32_t>(*bytes_text);
    if (!bytes) {
        return std::unexpected(ParseError::BadBytes);
    }
    record.bytes = *bytes;

    // Latency is the last field, so the rest of the line must be exactly one number.
    const auto latency = parse_integer<std::uint32_t>(line);
    if (!latency) {
        return std::unexpected(ParseError::BadLatency);
    }
    record.latency_us = *latency;

    return record;
}

}  // namespace logproc