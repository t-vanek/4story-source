#pragma once

// wire_codec — POD reader/writer + length-prefixed string for the
// legacy CPacket body layout. Shared by handlers, senders, and tests
// so framing logic lives in exactly one place.
//
// Layout follows CPacket::operator<<(LPCTSTR):
//   int32 length followed by raw bytes (no NUL terminator, CP1252).
//
// Reader::ReadString matches CPacket::operator>>(CString&): rejects
// negative lengths and lengths past the packet end, leaving `out`
// empty on framing violation. This is the safe replacement for the
// pre-auth heap-corruption path documented in PATCH_README §X-9.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace tworldsvr::wire {

template <class T>
inline void WritePOD(std::vector<std::byte>& out, T v)
{
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

    bool Eof() const              { return m_off >= m_size; }
    std::size_t Remaining() const { return m_size > m_off ? m_size - m_off : 0; }

    template <class T>
    bool Read(T& out)
    {
        if (m_off + sizeof(T) > m_size) return false;
        std::memcpy(&out, m_data + m_off, sizeof(T));
        m_off += sizeof(T);
        return true;
    }

    bool ReadString(std::string& out)
    {
        out.clear();
        std::int32_t len = 0;
        if (!Read(len)) return false;
        if (len < 0) return false;
        if (static_cast<std::size_t>(len) > Remaining()) return false;
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

} // namespace tworldsvr::wire
