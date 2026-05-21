#pragma once

// MapperProfile bundle for TLoginSvrAsio.
//
// Demonstrates and exercises the fourstory::mapper framework:
//   * CharacterInfo → CharLobbySummary  (compact log entry)
//   * CharacterCreateRequest → CharLobbySummary (new char audit row)
//
// Registered at startup via main.cpp:
//   MapperRegistry::Get().Register<LoginMappingProfile>();
//   MapperRegistry::Get().ApplyAll();

#include "char_service.h"

#include "fourstory/mapper/mapper.h"

#include <cstdint>
#include <string>

namespace tloginsvr::services {

// Compact projection of a character for one-line audit / log entries.
// Lives in the service header so handlers + audit-log builders share
// the type without each rolling its own ad-hoc DTO.
struct CharLobbySummary
{
    std::int32_t  char_id     = 0;
    std::string   name;
    std::uint8_t  level       = 1;
    std::uint8_t  char_class  = 0;
    std::uint8_t  country     = 0;
    std::uint32_t fame        = 0;
};

class LoginMappingProfile : public fourstory::mapper::MapperProfile
{
public:
    const char* Name() const override { return "LoginMappingProfile"; }

    void Configure() override
    {
        using namespace fourstory::mapper;

        // CharacterInfo (full lobby row) → CharLobbySummary (compact).
        // Demonstrates straight member-to-member mapping with implicit
        // numeric narrowing (uint8 ↔ uint32) via the Convert overloads.
        MapperConfig<CharacterInfo, CharLobbySummary>()
            .Set(&CharLobbySummary::char_id,    &CharacterInfo::char_id)
            .Set(&CharLobbySummary::name,       &CharacterInfo::name)
            .Set(&CharLobbySummary::level,      &CharacterInfo::level)
            .Set(&CharLobbySummary::char_class, &CharacterInfo::char_class)
            .Set(&CharLobbySummary::country,    &CharacterInfo::country)
            .Set(&CharLobbySummary::fame,       &CharacterInfo::fame);

        // CharacterCreateRequest → CharLobbySummary (post-create audit).
        // Shows lambda transform for fields not present in the source
        // (level = 1, fame = 0 are defaults; char_id is filled in by
        // the SP and patched on AfterMap by the caller).
        MapperConfig<CharacterCreateRequest, CharLobbySummary>()
            .Set(&CharLobbySummary::name,       &CharacterCreateRequest::name)
            .Set(&CharLobbySummary::char_class, &CharacterCreateRequest::char_class)
            .Set(&CharLobbySummary::country,    &CharacterCreateRequest::country)
            .Default(&CharLobbySummary::level,  std::uint8_t{1})
            .Default(&CharLobbySummary::fame,   std::uint32_t{0});
    }
};

} // namespace tloginsvr::services
