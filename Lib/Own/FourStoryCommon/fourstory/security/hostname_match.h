#pragma once

// RFC 6125 §6.4.3 hostname-pattern matching.
//
// Used by post-handshake identity checks on TLS connections: a peer
// certificate's SubjectAltName DNS entries may legitimately contain
// wildcards like "*.cluster.example.com" to cover N peers without
// reissuing the cert for each one. The matcher answers the question
// "does the operator-configured expected name match this SAN entry
// (whether the entry is literal or a wildcard pattern)?"
//
// Rules implemented (strict subset of RFC 6125 §6.4.3 / §6.4.4):
//   1. Pattern with no '*' → exact match (case-insensitive, per
//      RFC 1035 ASCII rules).
//   2. Wildcard MUST sit alone as the leftmost label. "*x.foo.com"
//      or "x*.foo.com" are rejected — RFC discourages partial
//      wildcards because of homograph attacks, and no in-tree
//      operator format requires them.
//   3. The wildcard label matches exactly one label (no dots) of
//      the candidate name.
//   4. Pattern must have ≥ 3 labels (≥ 2 dots). "*.com" is too
//      broad — covers the whole TLD.
//   5. Comparison is case-insensitive ASCII. Non-ASCII (IDN) is
//      not specially handled — the peer name must already be in
//      ACE form on both sides if it contains internationalized
//      characters.
//
// Out of scope (RFC 6125 leaves these "MAY" / deployment-dependent):
//   * Public suffix list checks — operator is responsible for
//     not issuing certs against actual TLD wildcards.
//   * URI-type / IP-type SAN matching — DNS names only.
//
// Header-only because the function is small and call sites want it
// inlined into the SAN-loop body.

#include <cstddef>
#include <string_view>

namespace fourstory::security {

namespace detail {

inline bool EqualIgnoreCase(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
        if (ca != cb) return false;
    }
    return true;
}

} // namespace detail

// Returns true if `name` matches `pattern` under RFC 6125 hostname
// semantics. Both literal-equal patterns and the restricted wildcard
// form are accepted; everything else returns false.
//
// Examples:
//   HostnameMatch("tlogin-eu-1.cluster.local", "tlogin-eu-1.cluster.local")  // true
//   HostnameMatch("*.cluster.local",            "tlogin-eu-1.cluster.local")  // true
//   HostnameMatch("*.cluster.local",            "deep.tlogin.cluster.local")  // false (2 labels in *)
//   HostnameMatch("*.com",                      "anything.com")               // false (too broad)
//   HostnameMatch("tl*.cluster.local",          "tlogin.cluster.local")       // false (partial wildcard)
//   HostnameMatch("TLOGIN.CLUSTER.LOCAL",       "tlogin.cluster.local")       // true (case insensitive)
inline bool HostnameMatch(std::string_view pattern,
                          std::string_view name) noexcept
{
    if (pattern.empty() || name.empty()) return false;

    // No wildcard → straight literal compare.
    const auto star_pos = pattern.find('*');
    if (star_pos == std::string_view::npos)
        return detail::EqualIgnoreCase(pattern, name);

    // Wildcard present. Validate the pattern shape per RFC 6125.
    const auto pattern_dot = pattern.find('.');
    if (pattern_dot == std::string_view::npos)
        return false;  // "*"-only or "*xyz" with no dot — too broad

    const auto wild_label = pattern.substr(0, pattern_dot);
    const auto pattern_rest = pattern.substr(pattern_dot + 1);

    // Wildcard label MUST be exactly "*". Partial wildcards
    // ("*foo", "foo*", "f*o") are intentionally not supported.
    if (wild_label != "*") return false;

    // The remainder of the pattern must contain at least one more
    // dot, i.e. pattern has ≥ 3 labels total. Rejects "*.tld".
    if (pattern_rest.find('.') == std::string_view::npos) return false;

    // Match `name` against the wildcard label + rest. Name's first
    // label has to be non-empty (a single concrete label) and the
    // remainder must compare equal (case-insensitive).
    const auto name_dot = name.find('.');
    if (name_dot == std::string_view::npos) return false;
    const auto name_label = name.substr(0, name_dot);
    const auto name_rest  = name.substr(name_dot + 1);
    if (name_label.empty()) return false;

    return detail::EqualIgnoreCase(name_rest, pattern_rest);
}

} // namespace fourstory::security
