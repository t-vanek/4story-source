#pragma once

// MapperProfile bundle for TPatchSvrAsio.
//
// Bridges PatchFile (DB row) to PatchManifestEntry (compact log /
// admin DTO). Keeps the field-by-field copy out of handlers.

#include "patch_repository.h"

#include "fourstory/mapper/mapper.h"

#include <cstdint>
#include <string>

namespace tpatchsvr {

// Compact patch row for boot-time inventory log + admin tooling.
// Same fields as PatchFile but exposes a unified path/name slug used
// by the audit log: "path/name".
struct PatchManifestEntry
{
    std::uint32_t version  = 0;
    std::uint32_t beta_ver = 0;
    std::string   slug;          // "<path>/<name>"
    std::uint32_t size     = 0;
};

class PatchMappingProfile : public fourstory::mapper::MapperProfile
{
public:
    const char* Name() const override { return "PatchMappingProfile"; }

    void Configure() override
    {
        using namespace fourstory::mapper;

        // PatchFile → PatchManifestEntry. The slug field is built via
        // a lambda transform that joins path + name with '/'.
        MapperConfig<PatchFile, PatchManifestEntry>()
            .Set(&PatchManifestEntry::version,  &PatchFile::version)
            .Set(&PatchManifestEntry::beta_ver, &PatchFile::beta_ver)
            .Set(&PatchManifestEntry::size,     &PatchFile::size)
            .Set(&PatchManifestEntry::slug,
                 [](const PatchFile& p) {
                     std::string s;
                     s.reserve(p.path.size() + 1 + p.name.size());
                     s += p.path;
                     if (!p.path.empty() && p.path.back() != '/' &&
                         p.path.back() != '\\')
                         s += '/';
                     s += p.name;
                     return s;
                 });
    }
};

} // namespace tpatchsvr
