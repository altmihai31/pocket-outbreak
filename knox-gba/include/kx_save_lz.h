#ifndef KX_SAVE_LZ_H
#define KX_SAVE_LZ_H

#include "bn_common.h"

// The world block's packing (playtest 6): LZ77 as the GBA BIOS has it, so a world fits half the save area and keeps
// two copies (a power cut while one is written leaves the other). A packed block is the unpacked size (4 bytes,
// little-endian), then groups of a flag byte and up to 8 tokens, the flag's bit 7 for the first: 0 a literal byte,
// 1 a match of two bytes, ((length - 3) << 12) | (distance - 1), big-endian, copying `length` (3-18) bytes from
// `distance` (1-4096) back.
namespace kx::save::lz
{
    constexpr int min_length = 3;
    constexpr int max_length = 18;
    constexpr int window = 4096;
    constexpr int hash_bits = 12;
    constexpr int table_size = 1 << hash_bits;

    // A packer working through a block a slice at a time. The open group's flag byte stays in the output until the
    // group is full: bytes before `group` are final.
    struct state
    {
        int16_t* table = nullptr;       // table_size entries: the last position with each 3-byte hash, -1 none
        int group = -1;                 // the open group's flag byte in the output (-1: none open)
        int tokens = 0;                 // tokens in it
    };

    // Packs data[position, end) (matches may reach to `size`), appending to output[output_size]; returns the next
    // position (past `end` when a match ran over it). The output needs room for (end - position) * 9 / 8 + 20
    // bytes.
    BN_CODE_IWRAM int pack(const uint8_t* data, int size, int position, int end, state& packer, uint8_t* output,
                           int& output_size);
}

#endif
