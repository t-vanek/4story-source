#pragma once

// Stat chart — the three read-only tables the max-HP/MP formula needs,
// loaded once at boot:
//   TFORMULACHART  bID → {dwInit, fRateX, fRateY}   (formula coefficients)
//   TCLASSCHART    bClassID → StatBlock              (per-class base stats)
//   TRACECHART     bRaceID  → StatBlock              (per-race  base stats)
//
// One interface so the engine + handlers depend on an abstraction (the
// in-memory impl doubles as the test fake; the SOCI impl loads from the
// DB). Lookups are O(1); a missing row returns a documented default so a
// data hole degrades to a small/zero contribution rather than a crash.

#include "domain/stat.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace tmapsvr {

class IStatChart
{
public:
    virtual ~IStatChart() = default;

    // Formula row for an FTYPE_ id (FormulaRow{} of zeros when absent —
    // a zero rate yields a zero contribution, which is the safe default).
    virtual FormulaRow Formula(std::uint8_t ftype) const = 0;

    // Per-class base stats (StatBlock{} of zeros when the class id is
    // unknown).
    virtual StatBlock ClassStats(std::uint8_t class_id) const = 0;

    // Per-race base stats (StatBlock{} of zeros when the race id is
    // unknown).
    virtual StatBlock RaceStats(std::uint8_t race_id) const = 0;

    // Row counts loaded (boot log / validation).
    virtual std::size_t FormulaCount() const = 0;
    virtual std::size_t ClassCount()   const = 0;
    virtual std::size_t RaceCount()    const = 0;
};

// Header-only in-memory impl — also the test fake.
class InMemoryStatChart final : public IStatChart
{
public:
    void AddFormula(std::uint8_t ftype, const FormulaRow& r) { m_formula[ftype] = r; }
    void AddClass(std::uint8_t id, const StatBlock& s)       { m_class[id] = s; }
    void AddRace(std::uint8_t id, const StatBlock& s)        { m_race[id] = s; }

    FormulaRow Formula(std::uint8_t ftype) const override
    {
        const auto it = m_formula.find(ftype);
        return it == m_formula.end() ? FormulaRow{} : it->second;
    }
    StatBlock ClassStats(std::uint8_t class_id) const override
    {
        const auto it = m_class.find(class_id);
        return it == m_class.end() ? StatBlock{} : it->second;
    }
    StatBlock RaceStats(std::uint8_t race_id) const override
    {
        const auto it = m_race.find(race_id);
        return it == m_race.end() ? StatBlock{} : it->second;
    }
    std::size_t FormulaCount() const override { return m_formula.size(); }
    std::size_t ClassCount()   const override { return m_class.size(); }
    std::size_t RaceCount()    const override { return m_race.size(); }

private:
    std::unordered_map<std::uint8_t, FormulaRow> m_formula;
    std::unordered_map<std::uint8_t, StatBlock>  m_class;
    std::unordered_map<std::uint8_t, StatBlock>  m_race;
};

} // namespace tmapsvr
