#pragma once
#include <openssl/rand.h>
#include <cstdint>
#include <stdexcept>

namespace tmapsvr {
// Windows CRT rand() has a 15-bit result. Keep the source modulo bias and
// range, but use independent secure draws rather than shared predictable CRT
// state. Do not replace rand()%N with a uniform distribution over N outcomes.
inline std::uint32_t LegacyRandom15() {
    unsigned char bytes[2];
    if(RAND_bytes(bytes,2)!=1)throw std::runtime_error("Gameplay randomness unavailable");
    return ((std::uint32_t{bytes[0]}<<8)|bytes[1])&32767U;
}
}
