// Waypoint Engine — UUID implementation.
// SPDX-License-Identifier: Apache-2.0
#include "core/Uuid.hpp"

#include <random>

namespace waypoint {
namespace {

std::uint8_t hexDigit(char c) {
    if (c >= '0' && c <= '9') {
        return static_cast<std::uint8_t>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<std::uint8_t>(c - 'a' + 10);
    }
    if (c >= 'A' && c <= 'F') {
        return static_cast<std::uint8_t>(c - 'A' + 10);
    }
    return 0xFF;
}

constexpr char kHexChars[] = "0123456789abcdef";

}  // namespace

Uuid Uuid::generate() {
    // A thread_local generator avoids locking without rebuilding entropy on
    // every call. random_device seeds it once per thread.
    thread_local std::mt19937_64 engine{std::random_device{}()};
    std::array<std::uint8_t, 16> bytes{};
    for (std::size_t i = 0; i < bytes.size(); i += 8) {
        const std::uint64_t value = engine();
        for (std::size_t b = 0; b < 8; ++b) {
            bytes[i + b] = static_cast<std::uint8_t>((value >> (b * 8)) & 0xFFu);
        }
    }
    // RFC 4122: version 4 (random) and the variant bits.
    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0Fu) | 0x40u);
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3Fu) | 0x80u);
    return Uuid{bytes};
}

std::optional<Uuid> Uuid::fromString(std::string_view text) {
    std::array<std::uint8_t, 16> bytes{};
    std::size_t byteIndex = 0;
    std::size_t digits = 0;
    std::uint8_t pending = 0;

    for (const char c : text) {
        if (c == '-' || c == ' ' || c == '_' || c == '{' || c == '}') {
            continue;
        }
        const std::uint8_t digit = hexDigit(c);
        if (digit == 0xFF) {
            return std::nullopt;
        }
        pending = static_cast<std::uint8_t>((pending << 4) | digit);
        if (++digits % 2 == 0) {
            if (byteIndex >= bytes.size()) {
                return std::nullopt;
            }
            bytes[byteIndex++] = pending;
            pending = 0;
        }
        if (digits > 32) {
            return std::nullopt;
        }
    }

    if (digits != 32 || byteIndex != bytes.size()) {
        return std::nullopt;
    }
    return Uuid{bytes};
}

std::string Uuid::toString() const {
    std::string text;
    text.reserve(kStringLength);
    for (std::size_t i = 0; i < m_bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            text.push_back('-');
        }
        text.push_back(kHexChars[(m_bytes[i] >> 4) & 0x0F]);
        text.push_back(kHexChars[m_bytes[i] & 0x0F]);
    }
    return text;
}

bool Uuid::isNull() const {
    for (const std::uint8_t byte : m_bytes) {
        if (byte != 0) {
            return false;
        }
    }
    return true;
}

std::size_t Uuid::hash() const {
    // FNV-1a over the 16 bytes; cheap and good enough for hash tables.
    std::size_t value = 1469598103934665603ull;
    for (const std::uint8_t byte : m_bytes) {
        value ^= byte;
        value *= 1099511628211ull;
    }
    return value;
}

}  // namespace waypoint
