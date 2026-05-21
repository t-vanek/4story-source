#pragma once

// TypeMap<Src, Dst> — fluent object-to-object mapping configuration.
//
// Inspired by AutoMapper / Mapster. Configure once, reuse across the
// codebase. No runtime reflection — uses C++ member pointers + lambdas
// for type-safe field access.
//
// Each .Set() / .Default() / .Ignore() call registers one mapping
// action. Apply()/Map() runs them in registration order on a source
// instance to populate a destination instance.
//
// Usage:
//
//   TypeMap<Source, Dest> m;
//   m.Set(&Dest::id,    &Source::dwID)          // direct member copy + narrow
//    .Set(&Dest::name,  &Source::szName)        // char[N] → std::string handled
//    .Set(&Dest::full,  [](const Source& s) {   // lambda transform
//             return s.first + " " + s.last;
//         })
//    .Default(&Dest::version, 1)                // constant default
//    .Ignore(&Dest::computed_at_runtime);       // explicit skip
//
//   Dest d = m.Map(src);                         // create + apply
//   m.Apply(src, existing_dest);                 // populate existing
//   auto vec = m.MapAll(src_vector);             // vector → vector

#include "convert.h"

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace fourstory::mapper {

template<typename Src, typename Dst>
class TypeMap
{
public:
    using Action = std::function<void(const Src&, Dst&)>;

    // ── Member-to-member ────────────────────────────────────────────
    // Direct field copy. If the source and destination types differ,
    // Convert<From, To> handles the conversion (numeric narrowing,
    // char[N] → std::string, std::string → char[N], etc.).
    template<typename SrcT, typename DstT>
    TypeMap& Set(DstT Dst::*dst_mem, SrcT Src::*src_mem)
    {
        m_actions.emplace_back(
            [dst_mem, src_mem](const Src& s, Dst& d)
            {
                Convert(s.*src_mem, d.*dst_mem);
            });
        return *this;
    }

    // ── Lambda transform ─────────────────────────────────────────────
    // The lambda receives the Source instance and returns a value
    // convertible to the destination member's type.
    template<typename DstT, typename Fn>
    requires std::invocable<Fn&, const Src&>
    TypeMap& Set(DstT Dst::*dst_mem, Fn fn)
    {
        m_actions.emplace_back(
            [dst_mem, fn = std::move(fn)](const Src& s, Dst& d)
            {
                Convert(fn(s), d.*dst_mem);
            });
        return *this;
    }

    // ── Constant default ─────────────────────────────────────────────
    template<typename DstT, typename V>
    TypeMap& Default(DstT Dst::*dst_mem, V value)
    {
        m_actions.emplace_back(
            [dst_mem, v = std::move(value)](const Src&, Dst& d)
            {
                Convert(v, d.*dst_mem);
            });
        return *this;
    }

    // ── Ignore ───────────────────────────────────────────────────────
    // Documentation marker; runtime no-op. Useful for explicit "this
    // field is computed elsewhere, leave it alone" intent and for
    // future static analysis that warns on unmapped destination fields.
    template<typename DstT>
    TypeMap& Ignore(DstT Dst::* /*dst_mem*/) { return *this; }

    // ── Conditional ─────────────────────────────────────────────────
    // Only applies the rule if the predicate returns true.
    template<typename DstT, typename Fn, typename Pred>
    TypeMap& SetIf(DstT Dst::*dst_mem, Fn fn, Pred pred)
    {
        m_actions.emplace_back(
            [dst_mem, fn = std::move(fn), pred = std::move(pred)]
                (const Src& s, Dst& d)
            {
                if (pred(s))
                    Convert(fn(s), d.*dst_mem);
            });
        return *this;
    }

    // ── BeforeMap / AfterMap hooks ──────────────────────────────────
    // Run arbitrary code before / after the field actions. Useful for
    // cross-field invariants or post-processing.
    TypeMap& BeforeMap(std::function<void(const Src&, Dst&)> fn)
    {
        m_before.push_back(std::move(fn));
        return *this;
    }
    TypeMap& AfterMap(std::function<void(const Src&, Dst&)> fn)
    {
        m_after.push_back(std::move(fn));
        return *this;
    }

    // ── Application ─────────────────────────────────────────────────

    void Apply(const Src& src, Dst& dst) const
    {
        for (const auto& f : m_before) f(src, dst);
        for (const auto& a : m_actions) a(src, dst);
        for (const auto& f : m_after) f(src, dst);
    }

    Dst Map(const Src& src) const
    {
        Dst d{};
        Apply(src, d);
        return d;
    }

    template<typename SrcRange>
    std::vector<Dst> MapAll(const SrcRange& src) const
    {
        std::vector<Dst> out;
        if constexpr (requires { src.size(); })
            out.reserve(src.size());
        for (const auto& s : src)
            out.push_back(Map(s));
        return out;
    }

    std::size_t RuleCount() const { return m_actions.size(); }

private:
    std::vector<Action> m_actions;
    std::vector<Action> m_before;
    std::vector<Action> m_after;
};

} // namespace fourstory::mapper
