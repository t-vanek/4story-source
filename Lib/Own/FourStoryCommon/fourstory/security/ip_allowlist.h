#pragma once

// IpAllowlist — IPv4 CIDR-aware allowlist for server-to-server traffic.
//
// Accepts entries in three forms:
//   "127.0.0.1"          → exact /32 match
//   "10.0.0.0/8"         → CIDR block
//   "192.168.1.0/24"     → CIDR block
//
// IPv6 not yet supported — the 4Story wire layer carries IPv4 only.
// Empty allowlist means "deny everything" by default; pass
// `default_allow = true` when constructing to flip the default to allow
// (useful for dev runs without [security] config).
//
// Header-only. Used by every server's accept loop to reject inbound
// connections from non-whitelisted IPs before any wire parsing runs.

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fourstory::security {

class IpAllowlist
{
public:
    // Empty allowlist + default_allow=true → no filtering.
    // Empty allowlist + default_allow=false → reject everything.
    explicit IpAllowlist(bool default_allow = false)
        : m_default_allow(default_allow)
    {}

    // Parse and add one CIDR entry. Returns false on malformed input;
    // caller is responsible for logging.
    bool Add(std::string_view entry)
    {
        Rule r{};
        if (!Parse(entry, r)) return false;
        m_rules.push_back(r);
        return true;
    }

    // Bulk add from a list of strings. Returns the count of successful
    // entries (rejected entries are silently dropped — caller filters
    // beforehand via ValidateEntries() if it wants per-line errors).
    std::size_t AddAll(const std::vector<std::string>& entries)
    {
        std::size_t n = 0;
        for (const auto& e : entries) if (Add(e)) ++n;
        return n;
    }

    // Return the first malformed entry, or nullopt when all parse OK.
    static std::optional<std::string>
    FirstInvalid(const std::vector<std::string>& entries)
    {
        Rule tmp{};
        for (const auto& e : entries)
            if (!Parse(e, tmp)) return e;
        return std::nullopt;
    }

    // Match an IP. Returns true when allowed, false when denied.
    // Empty rule set defers to default_allow.
    bool Allows(std::string_view ipv4) const
    {
        std::uint32_t addr = 0;
        if (!ParseIPv4(ipv4, addr)) return false;
        return AllowsRaw(addr);
    }

    bool AllowsRaw(std::uint32_t ipv4_be) const
    {
        if (m_rules.empty()) return m_default_allow;
        for (const auto& r : m_rules)
        {
            const std::uint32_t mask = MaskFor(r.prefix);
            if ((ipv4_be & mask) == (r.base & mask)) return true;
        }
        return false;
    }

    std::size_t Size() const { return m_rules.size(); }
    bool        Empty() const { return m_rules.empty(); }
    bool        DefaultAllow() const { return m_default_allow; }

private:
    struct Rule
    {
        std::uint32_t base   = 0;   // network base, big-endian-style host order
        std::uint8_t  prefix = 32;
    };

    static std::uint32_t MaskFor(std::uint8_t prefix)
    {
        if (prefix == 0)  return 0u;
        if (prefix >= 32) return 0xFFFFFFFFu;
        return 0xFFFFFFFFu << (32 - prefix);
    }

    // Strict IPv4 parser: "a.b.c.d" with each octet 0..255. Returns
    // host-order representation (top byte = a).
    static bool ParseIPv4(std::string_view s, std::uint32_t& out)
    {
        std::array<unsigned, 4> oct{};
        std::size_t idx = 0;
        unsigned cur = 0;
        bool have_digit = false;
        for (char c : s)
        {
            if (c == '.')
            {
                if (!have_digit || idx >= 3 || cur > 255) return false;
                oct[idx++] = cur;
                cur = 0;
                have_digit = false;
            }
            else if (c >= '0' && c <= '9')
            {
                cur = cur * 10 + static_cast<unsigned>(c - '0');
                if (cur > 255) return false;
                have_digit = true;
            }
            else
            {
                return false;
            }
        }
        if (!have_digit || idx != 3 || cur > 255) return false;
        oct[3] = cur;
        out = (oct[0] << 24) | (oct[1] << 16) | (oct[2] << 8) | oct[3];
        return true;
    }

    static bool Parse(std::string_view entry, Rule& out)
    {
        auto slash = entry.find('/');
        std::string_view ip_part = (slash == std::string_view::npos)
            ? entry : entry.substr(0, slash);
        std::string_view prefix_part = (slash == std::string_view::npos)
            ? std::string_view{}
            : entry.substr(slash + 1);

        if (!ParseIPv4(ip_part, out.base)) return false;

        if (prefix_part.empty())
        {
            out.prefix = 32;
        }
        else
        {
            unsigned p = 0;
            for (char c : prefix_part)
            {
                if (c < '0' || c > '9') return false;
                p = p * 10 + static_cast<unsigned>(c - '0');
                if (p > 32) return false;
            }
            out.prefix = static_cast<std::uint8_t>(p);
        }
        return true;
    }

    std::vector<Rule> m_rules;
    bool              m_default_allow;
};

} // namespace fourstory::security
