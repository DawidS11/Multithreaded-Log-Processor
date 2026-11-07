#include "LogParser.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <optional>
#include <system_error>

namespace logproc {
namespace {

constexpr std::array<std::string_view, 12> kMonths{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Returns the text before delim and removes that text and the delimiter from s.
// Returns nullopt if the delimiter isn't found.
std::optional<std::string_view> takeUntil(std::string_view& s, char delim) {
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
std::optional<T> parseInteger(std::string_view text) {
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
std::optional<std::chrono::sys_seconds> parseTimestamp(std::string_view s) {
    using namespace std::chrono;

    const auto dayText = takeUntil(s, '/');
    const auto monthText = takeUntil(s, '/');
    const auto yearText = takeUntil(s, ':');
    const auto hourText = takeUntil(s, ':');
    const auto minuteText = takeUntil(s, ':');
    const auto secondText = takeUntil(s, ' ');
    if (!dayText || !monthText || !yearText || !hourText || !minuteText || !secondText) {
        return std::nullopt;
    }
    if (s != "+0000") {
        return std::nullopt;
    }

    const auto monthIt = std::ranges::find(kMonths, *monthText);
    if (monthIt == kMonths.end()) {
        return std::nullopt;
    }

    const auto day = parseInteger<unsigned>(*dayText);
    const auto y = parseInteger<int>(*yearText);
    const auto h = parseInteger<int>(*hourText);
    const auto mi = parseInteger<int>(*minuteText);
    const auto sec = parseInteger<int>(*secondText);
    if (!day || !y || !h || !mi || !sec) {
        return std::nullopt;
    }
    if (*h > 23 || *mi > 59 || *sec > 59) {
        return std::nullopt;
    }

    // Month numbers start at 1, the array index starts at 0.
    const auto monthNumber = static_cast<unsigned>(monthIt - kMonths.begin()) + 1;
    const year_month_day date{year{*y}, month{monthNumber}, std::chrono::day{*day}};
    if (!date.ok()) {  // rejects things like 31 Feb
        return std::nullopt;
    }

    return sys_days{date} + hours{*h} + minutes{*mi} + seconds{*sec};
}

// Skips one referrer or user agent field, which is either a bare "-" or a quoted string.
// Backslash escapes inside the quotes are skipped, not decoded. Consumes the field and
// the space after it, if there is one.
std::expected<void, ParseError> skipQuotedField(std::string_view& s, ParseError onError) {
    if (s.starts_with("- ")) {
        s.remove_prefix(2);
        return {};
    }
    if (!s.starts_with('"')) {
        return std::unexpected(onError);
    }

    std::size_t i = 1;
    while (i < s.size() && s[i] != '"') {
        i += (s[i] == '\\') ? 2 : 1;
    }

    // No closing quote, or a closing quote followed by something other than a space.
    if (i >= s.size() || (i + 1 < s.size() && s[i + 1] != ' ')) {
        return std::unexpected(onError);
    }
    s.remove_prefix(std::min(i + 2, s.size()));
    return {};
}

}  // namespace

std::string_view toString(ParseError error) noexcept {
    switch (error) {
        case ParseError::MissingField:  return "missing field";
        case ParseError::BadTimestamp:  return "bad timestamp";
        case ParseError::BadRequest:    return "bad request";
        case ParseError::BadStatus:     return "bad status";
        case ParseError::BadBytes:      return "bad bytes";
        case ParseError::BadReferrer:   return "bad referrer";
        case ParseError::BadUserAgent:  return "bad user agent";
        case ParseError::BadLatency:    return "bad latency";
    }
    return "unknown";
}

std::expected<LogRecord, ParseError> parseLine(std::string_view line) {
    // Combined format:
    // 10.0.0.5 - - [24/Oct/2025:10:00:04 +0000] "GET /login HTTP/1.1" 500 1024 "https://example.com/" "Mozilla/5.0 (Macintosh)" 1800
    // Generator format, without referrer or user agent:
    // 10.0.0.5 - - [24/Oct/2025:10:00:04 +0000] "GET /login HTTP/1.1" 500 1024 1800

    LogRecord record;

    // Client address. We require it to be present but don't store it.
    const auto client = takeUntil(line, ' ');
    if (!client || client->empty()) {
        return std::unexpected(ParseError::MissingField);
    }

    // Identity fields are always "- -" in this format.
    const auto ident = takeUntil(line, '[');
    if (!ident || *ident != "- - ") {
        return std::unexpected(ParseError::MissingField);
    }

    const auto stamp = takeUntil(line, ']');
    if (!stamp) {
        return std::unexpected(ParseError::MissingField);
    }
    const auto when = parseTimestamp(*stamp);
    if (!when) {
        return std::unexpected(ParseError::BadTimestamp);
    }
    record.timestamp = *when;

    // Request is quoted: "METHOD PATH PROTOCOL"
    if (!line.starts_with(" \"")) {
        return std::unexpected(ParseError::BadRequest);
    }
    line.remove_prefix(2);

    auto request = takeUntil(line, '"');
    if (!request) {
        return std::unexpected(ParseError::BadRequest);
    }
    const auto method = takeUntil(*request, ' ');
    const auto path = takeUntil(*request, ' ');
    if (!method || !path || method->empty() || path->empty() || !request->starts_with("HTTP/")) {
        return std::unexpected(ParseError::BadRequest);
    }
    record.method = std::string{*method};
    record.path = std::string{*path};

    // Remaining fields: " STATUS BYTES ..."
    if (!line.starts_with(' ')) {
        return std::unexpected(ParseError::MissingField);
    }
    line.remove_prefix(1);

    const auto statusText = takeUntil(line, ' ');
    if (!statusText) {
        return std::unexpected(ParseError::MissingField);
    }
    const auto status = parseInteger<std::uint16_t>(*statusText);
    if (!status) {
        return std::unexpected(ParseError::BadStatus);
    }
    record.status = *status;

    const auto bytesText = takeUntil(line, ' ');
    if (!bytesText) {
        return std::unexpected(ParseError::MissingField);
    }
    if (*bytesText == "-") {
        record.bytes = 0;  // Apache writes "-" when no body was sent, such as a 304
    } else {
        const auto bytes = parseInteger<std::uint32_t>(*bytesText);
        if (!bytes) {
            return std::unexpected(ParseError::BadBytes);
        }
        record.bytes = *bytes;
    }

    // Combined format: referrer and user agent come before the latency.
    // They're validated and then skipped, since nothing reads them yet.
    if (line.starts_with('"') || line.starts_with("- ")) {
        if (auto referrer = skipQuotedField(line, ParseError::BadReferrer); !referrer) {
            return std::unexpected(referrer.error());
        }
        if (auto agent = skipQuotedField(line, ParseError::BadUserAgent); !agent) {
            return std::unexpected(agent.error());
        }
    }

    // Latency is the last field, so the rest of the line must be exactly one number.
    const auto latency = parseInteger<std::uint32_t>(line);
    if (!latency) {
        return std::unexpected(ParseError::BadLatency);
    }
    record.latencyUs = *latency;

    return record;
}

}  // namespace logproc