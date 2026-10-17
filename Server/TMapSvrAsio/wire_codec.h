#pragma once

// wire_codec — POD little-endian reader/writer for the legacy CPacket
// body layout. Mirrors tcontrolsvr::wire and tpatchsvr's local helpers
// so handlers, senders, and tests share one set of primitives.
//
// String layout (length-prefixed) matches CPacket::operator<<(LPCTSTR):
// int32 length followed by raw bytes (no NUL, CP1252). The reader
// rejects negative lengths and lengths past the packet end, matching
// the legacy CPacket::operator>>(CString&) "silent empty" recovery.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <bit>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace tmapsvr::wire {

// Explicit scalar little-endian access for newly ported source contracts.
// Keep the older POD API independent; never serialize a native struct here.
template <class T>
inline void WriteLE(std::vector<std::byte>& out, T value)
{
    if constexpr (std::is_same_v<T,float>) {
        static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559);
        WriteLE(out,std::bit_cast<std::uint32_t>(value));
    } else {
        static_assert(std::is_unsigned_v<T>&&std::is_integral_v<T>&&sizeof(T)<=8);
        for(std::size_t i=0;i<sizeof(T);++i)
            out.push_back(static_cast<std::byte>((value>>(8*i))&255U));
    }
}

template <class T>
inline void WritePOD(std::vector<std::byte>& out, T v)
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "WritePOD requires trivially-copyable types");
    const auto* p = reinterpret_cast<const std::byte*>(&v);
    out.insert(out.end(), p, p + sizeof(T));
}

inline void WriteString(std::vector<std::byte>& out, const std::string& s)
{
    const std::int32_t len = static_cast<std::int32_t>(s.size());
    WritePOD<std::int32_t>(out, len);
    if (!s.empty())
    {
        const auto* sp = reinterpret_cast<const std::byte*>(s.data());
        out.insert(out.end(), sp, sp + s.size());
    }
}

class Reader
{
public:
    Reader(const std::byte* data, std::size_t size)
        : m_data(data), m_size(size), m_off(0) {}

    explicit Reader(const std::vector<std::byte>& v)
        : m_data(v.data()), m_size(v.size()), m_off(0) {}

    bool Eof()               const { return m_off >= m_size; }
    std::size_t Remaining()  const { return m_size > m_off ? m_size - m_off : 0; }
    std::size_t Offset()     const { return m_off; }

    template <class T>
    bool ReadLE(T& out)
    {
        if constexpr (std::is_same_v<T,float>) {
            static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559);
            std::uint32_t bits{};
            if(!ReadLE(bits))return false;
            out=std::bit_cast<float>(bits);
        } else {
            static_assert(std::is_unsigned_v<T>&&std::is_integral_v<T>&&sizeof(T)<=8);
            if(sizeof(T)>Remaining())return false;
            std::uint64_t value{};
            for(std::size_t i=0;i<sizeof(T);++i)
                value|=std::uint64_t{std::to_integer<unsigned>(m_data[m_off++])}<<(8*i);
            out=static_cast<T>(value);
        }
        return true;
    }

    template <class T>
    bool Read(T& out)
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Reader::Read requires trivially-copyable types");
        if (m_off + sizeof(T) > m_size) return false;
        std::memcpy(&out, m_data + m_off, sizeof(T));
        m_off += sizeof(T);
        return true;
    }

    bool ReadString(std::string& out)
    {
        out.clear();
        std::int32_t len = 0;
        if (!Read(len))                                       return false;
        if (len < 0)                                          return false;
        if (static_cast<std::size_t>(len) > Remaining())      return false;
        if (len > 0)
        {
            out.assign(reinterpret_cast<const char*>(m_data + m_off),
                       static_cast<std::size_t>(len));
            m_off += static_cast<std::size_t>(len);
        }
        return true;
    }

private:
    const std::byte* m_data;
    std::size_t      m_size;
    std::size_t      m_off;
};

} // namespace tmapsvr::wire
