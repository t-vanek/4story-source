#pragma once

// MapperProfile — group related mappings under a named bundle.
//
// Mirrors the Profile pattern in AutoMapper / Mapster: instead of
// scattering MapperConfig<>() calls across the codebase, declare a
// Profile subclass with Configure() and call ApplyAll() at startup.
//
// Usage:
//
//   class CharMappingProfile : public MapperProfile {
//   public:
//       const char* Name() const override { return "CharMapping"; }
//       void Configure() override {
//           MapperConfig<DbCharRow, CharSnapshot>()
//               .Set(&CharSnapshot::dwCharID, &DbCharRow::id)
//               .Set(&CharSnapshot::szNAME,   &DbCharRow::name);
//
//           MapperConfig<CharSnapshot, CharListEntry>()
//               .Set(&CharListEntry::id,    &CharSnapshot::dwCharID)
//               .Set(&CharListEntry::name,  &CharSnapshot::szNAME)
//               .Set(&CharListEntry::level, &CharSnapshot::bLevel);
//       }
//   };
//
//   // In main():
//   MapperRegistry::Get().Register<CharMappingProfile>();
//   MapperRegistry::Get().Register<EventMappingProfile>();
//   MapperRegistry::Get().ApplyAll();

#include <memory>
#include <string>
#include <vector>

namespace fourstory::mapper {

class MapperProfile
{
public:
    virtual ~MapperProfile() = default;
    virtual void Configure() = 0;
    virtual const char* Name() const = 0;
};

class MapperRegistry
{
public:
    static MapperRegistry& Get()
    {
        static MapperRegistry instance;
        return instance;
    }

    template<typename ProfileT, typename... Args>
    requires std::is_base_of_v<MapperProfile, ProfileT>
    void Register(Args&&... args)
    {
        m_profiles.push_back(
            std::make_unique<ProfileT>(std::forward<Args>(args)...));
    }

    // Run Configure() on every registered profile in registration order.
    // Profiles' configurations don't conflict because each TypeMap<S,D>
    // is a separate singleton.
    void ApplyAll()
    {
        for (auto& p : m_profiles)
            p->Configure();
        m_applied = true;
    }

    bool Applied() const { return m_applied; }
    std::size_t Count() const { return m_profiles.size(); }

    std::vector<std::string> ListNames() const
    {
        std::vector<std::string> out;
        out.reserve(m_profiles.size());
        for (const auto& p : m_profiles)
            out.emplace_back(p->Name());
        return out;
    }

private:
    std::vector<std::unique_ptr<MapperProfile>> m_profiles;
    bool m_applied = false;
};

} // namespace fourstory::mapper
