#ifndef KX_HASH_H
#define KX_HASH_H

#include <cstdint>

namespace kx
{

// Integer hash (lowbias32): everything generated from the world seed (chunks, loot) goes through it, so the same
// seed always gives the same world.
[[nodiscard]] constexpr uint32_t hash32(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352d;
    value ^= value >> 15;
    value *= 0x846ca68b;
    value ^= value >> 16;
    return value;
}

// Hash of a seed and a 2D integer position (one round: it runs several times per generated tile).
[[nodiscard]] constexpr uint32_t hash_xy(uint32_t seed, int x, int y)
{
    return hash32(seed ^ (uint32_t(x) * 0x9e3779b1u) ^ (uint32_t(y) * 0x85ebca77u));
}

// Deterministic random stream from a seed.
class roll_stream
{

public:
    explicit constexpr roll_stream(uint32_t seed) :
        _state(seed)
    {
    }

    // 0 .. limit - 1.
    [[nodiscard]] constexpr int next(int limit)
    {
        _state = hash32(_state + 0x9e3779b9);
        return limit > 0 ? int(_state % unsigned(limit)) : 0;
    }

private:
    uint32_t _state;
};

}

#endif
