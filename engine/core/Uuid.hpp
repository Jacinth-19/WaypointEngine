// Waypoint Engine — universally unique identifiers.
// SPDX-License-Identifier: Apache-2.0
//
// A UUID ("universally unique identifier") is a random 128-bit number written
// as 8-4-4-4-12 hex digits, e.g. 3f2b1c4d-8a9e-4f10-b7c2-5d6e7f809a1b. Entities
// in a scene file carry one so that references survive reordering and editing.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace waypoint {

class Uuid {
public:
    static constexpr std::size_t kStringLength = 36;

    constexpr Uuid() = default;

    // A random version-4 UUID. Thread-friendly: each call uses its own RNG state.
    [[nodiscard]] static Uuid generate();

    // The all-zero UUID, used for "no entity" / "unassigned".
    [[nodiscard]] static constexpr Uuid null() { return Uuid{}; }

    // Accepts upper/lowercase hex with or without hyphens. Returns nullopt for
    // anything that is not exactly 32 hex digits.
    [[nodiscard]] static std::optional<Uuid> fromString(std::string_view text);

    [[nodiscard]] std::string toString() const;
    [[nodiscard]] bool isNull() const;
    [[nodiscard]] const std::array<std::uint8_t, 16>& bytes() const { return m_bytes; }

    friend bool operator==(const Uuid& lhs, const Uuid& rhs) { return lhs.m_bytes == rhs.m_bytes; }
    friend bool operator!=(const Uuid& lhs, const Uuid& rhs) { return !(lhs == rhs); }
    friend bool operator<(const Uuid& lhs, const Uuid& rhs) { return lhs.m_bytes < rhs.m_bytes; }

    [[nodiscard]] std::size_t hash() const;

private:
    explicit Uuid(const std::array<std::uint8_t, 16>& bytes) : m_bytes(bytes) {}

    std::array<std::uint8_t, 16> m_bytes{};
};

}  // namespace waypoint

namespace std {
template <>
struct hash<waypoint::Uuid> {
    std::size_t operator()(const waypoint::Uuid& value) const noexcept { return value.hash(); }
};
}  // namespace std
