#pragma once
#include "domain/character.h"
#include "wire_codec.h"
#include <algorithm>
#include <span>
#include <stdexcept>

namespace tmapsvr {
constexpr std::uint8_t TitleItemKind = 103; // NetCode.h: IK_TITLE
inline auto OrderedTitles(std::vector<CharacterTitle> titles) {
    std::sort(titles.begin(),titles.end(),[](auto a,auto b){return a.id<b.id;});
    if(titles.size()>255)throw std::runtime_error("Title count exceeds original client width");
    for(std::size_t i=1;i<titles.size();++i)if(titles[i-1].id==titles[i].id)
        throw std::runtime_error("Duplicate character title");
    return titles;
}
inline std::string TitleJson(const std::vector<CharacterTitle>& titles) {
    std::string out="[";
    for(auto t:OrderedTitles(titles)){if(out.size()>1)out+=',';out+='['+std::to_string(t.id)+','+(t.selected?"1":"0")+']';}
    return out+']';
}
inline std::vector<std::byte> EncodeTitleList(const std::vector<CharacterTitle>& input) {
    const auto titles=OrderedTitles(input);std::vector<std::byte> out;
    wire::WritePOD(out,static_cast<std::uint8_t>(titles.size()));
    for(auto t:titles){wire::WritePOD(out,t.id);wire::WritePOD(out,static_cast<std::uint8_t>(t.selected));}
    return out;
}
inline std::uint16_t LoadedTitle(const std::vector<CharacterTitle>& titles) {
    // Original LOADCHAR assigns each selected row in turn. RELEASEMAIN emits
    // ascending IDs; use the same deterministic order for native SQL loads.
    std::uint16_t selected=0;for(auto t:OrderedTitles(titles))if(t.selected)selected=t.id;return selected;
}
inline std::uint16_t DecodeTitleChange(std::span<const std::byte> bytes) {
    wire::Reader r(bytes.data(),bytes.size());std::uint16_t id{};
    if(!r.Read(id)||!r.Eof())throw std::runtime_error("Invalid CHANGETITLE request length");
    return id;
}
struct TitleChangeCommit {
    std::shared_ptr<const CharSnapshot> snapshot;
    bool changed{};
};
inline std::vector<std::byte> EncodeTitleRelay(std::uint32_t id,std::uint32_t key,
    std::uint16_t title,const std::string& name) {
    std::vector<std::byte> out;wire::WritePOD(out,id);wire::WritePOD(out,key);
    wire::WritePOD(out,TitleItemKind);wire::WritePOD(out,std::uint8_t{0});
    wire::WritePOD(out,title);wire::WriteString(out,name);return out;
}
inline std::vector<std::byte> EncodeTitleChanged(std::uint32_t id,std::uint16_t title,const std::string& name) {
    std::vector<std::byte> out;wire::WritePOD(out,std::uint8_t{0});wire::WritePOD(out,id);
    wire::WritePOD(out,TitleItemKind);wire::WritePOD(out,std::uint8_t{0});
    wire::WriteString(out,name);wire::WritePOD(out,title);wire::WritePOD(out,std::uint32_t{0});return out;
}
}
