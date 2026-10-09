#pragma once
#include <stdexcept>
#include <string>
#include <string_view>

namespace tlogsvr {
inline std::string AuditHex(std::string_view bytes) {
    constexpr char digits[]="0123456789abcdef";
    std::string out; out.reserve(bytes.size()*2);
    for(unsigned char c:bytes){out+=digits[c>>4];out+=digits[c&15];}
    return out;
}
inline std::string AuditUnhex(std::string_view hex) {
    if(hex.size()%2)throw std::runtime_error("Invalid audit byte representation");
    const auto digit=[](char c)->unsigned {
        if(c>='0'&&c<='9')return c-'0';
        if(c>='a'&&c<='f')return c-'a'+10;
        throw std::runtime_error("Invalid audit byte representation");
    };
    std::string out;out.reserve(hex.size()/2);
    for(std::size_t i=0;i<hex.size();i+=2)out+=static_cast<char>((digit(hex[i])<<4)|digit(hex[i+1]));
    return out;
}
} // namespace tlogsvr
