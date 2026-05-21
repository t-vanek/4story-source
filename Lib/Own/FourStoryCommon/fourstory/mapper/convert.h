#pragma once

// Convert(src, dst) — built-in conversions used by TypeMap::Set.
//
// Provides reasonable defaults for cross-type assignments:
//   numeric → numeric    : static_cast (uint8 ↔ uint32 etc.)
//   char[N] → std::string: copy with NUL termination
//   std::string → char[N]: bounded copy + NUL terminate
//   T → std::optional<T> : wrap
//   std::optional<T> → T : unwrap with default
//   bool ↔ integer       : explicit
//
// Users can override behavior by providing a custom lambda in
// TypeMap::Set(&Dst::field, [](const Src& s) { return ... });

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace fourstory::mapper {

// ── Generic assignable / convertible ────────────────────────────────
// Handles same-type assignment AND arithmetic narrowing (uint8 ↔ uint32)
// via static_cast. The char[N] / std::string / optional overloads below
// are more-specific and win overload resolution for those types.
template<typename From, typename To>
requires std::is_assignable_v<To&, const From&>
        && (!std::is_array_v<std::remove_reference_t<To>>)
inline void Convert(const From& src, To& dst)
{
    dst = static_cast<To>(src);
}

// ── char[N] → std::string ───────────────────────────────────────────
template<std::size_t N>
inline void Convert(const char (&src)[N], std::string& dst)
{
    // Stop at first NUL or N; legacy buffers are NUL-terminated but
    // may also be NUL-padded — strnlen handles both.
    const auto len = ::strnlen(src, N);
    dst.assign(src, len);
}

// ── std::string → char[N] (bounded, NUL-terminated) ─────────────────
template<std::size_t N>
inline void Convert(const std::string& src, char (&dst)[N])
{
    const auto n = std::min(src.size(), N - 1);
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

// ── std::string_view → std::string ──────────────────────────────────
inline void Convert(std::string_view src, std::string& dst)
{
    dst.assign(src);
}

// ── T → std::optional<T> (wrap) ─────────────────────────────────────
template<typename T>
inline void Convert(const T& src, std::optional<T>& dst)
{
    dst = src;
}

// ── std::optional<T> → T (unwrap; empty → value-initialized) ────────
template<typename T>
inline void Convert(const std::optional<T>& src, T& dst)
{
    dst = src.value_or(T{});
}

// ── std::optional<From> → std::optional<To> (chain) ─────────────────
template<typename From, typename To>
requires (!std::is_same_v<From, To>)
inline void Convert(const std::optional<From>& src, std::optional<To>& dst)
{
    if (src) { To tmp{}; Convert(*src, tmp); dst = tmp; }
    else      dst.reset();
}

// ── enum class ↔ underlying integer ─────────────────────────────────
template<typename EnumT, typename IntT>
requires std::is_enum_v<EnumT> && std::is_integral_v<IntT>
inline void Convert(EnumT src, IntT& dst)
{
    dst = static_cast<IntT>(static_cast<std::underlying_type_t<EnumT>>(src));
}

template<typename IntT, typename EnumT>
requires std::is_integral_v<IntT> && std::is_enum_v<EnumT>
inline void Convert(IntT src, EnumT& dst)
{
    dst = static_cast<EnumT>(src);
}

} // namespace fourstory::mapper
