#pragma once
#include <soci/soci.h>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace tmapsvr::native_wire_text {
inline std::string Hex(const std::string& bytes) {
    constexpr char digits[]="0123456789abcdef";std::string out;
    for(unsigned char b:bytes){out+=digits[b>>4];out+=digits[b&15];}return out;
}
inline std::string Unhex(const std::string& hex) {
    if(hex.size()%2)throw std::runtime_error("Invalid original wire text bytes");
    std::string out;out.reserve(hex.size()/2);
    for(std::size_t n=0;n<hex.size();n+=2)out+=static_cast<char>(std::stoul(hex.substr(n,2),nullptr,16));
    return out;
}
// No inferred ACP for client input. ASCII is invariant across supported ANSI
// profiles; other bytes require the operator's explicit PostgreSQL encoding.
inline std::string Decode(soci::session& sql,const std::string& bytes,const std::string& configured) {
    if(bytes.find('\0')!=std::string::npos)throw std::runtime_error("NUL in original client text");
    const bool ascii=std::all_of(bytes.begin(),bytes.end(),[](unsigned char b){return b<128;});
    if(ascii)return bytes;
    if(configured.empty())throw std::runtime_error("Non-ASCII client text requires native_map.client_text_encoding");
    const auto hex=Hex(bytes);std::string decoded,roundtrip,probe;
    sql << "SELECT convert_from(decode(:raw,'hex'),:encoding),"
        "encode(convert_to(convert_from(decode(:raw,'hex'),:encoding),:encoding),'hex'),"
        "encode(convert_to('Mail',:encoding),'hex')",
        soci::use(hex,"raw"),soci::use(configured,"encoding"),soci::into(decoded),soci::into(roundtrip),soci::into(probe);
    if(!sql.got_data()||roundtrip!=hex||probe!="4d61696c")
        throw std::runtime_error("Client text encoding is not a lossless ANSI profile");
    return decoded;
}
inline std::string Original(const soci::row& row,const char* bytes,const char* decoded) {
    if(row.get_indicator(bytes)!=soci::i_null)return Unhex(row.get<std::string>(bytes));
    const auto text=row.get<std::string>(decoded);
    if(!std::all_of(text.begin(),text.end(),[](unsigned char b){return b>0&&b<128;}))
        throw std::runtime_error("Historical non-ASCII name requires original wire bytes");
    return text;
}
}
