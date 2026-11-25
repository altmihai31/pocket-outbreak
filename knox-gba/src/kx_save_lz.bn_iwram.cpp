#include "kx_save_lz.h"

namespace kx::save::lz
{

namespace
{
    [[nodiscard]] inline unsigned hash(const uint8_t* at)
    {
        uint32_t value = (uint32_t(at[0]) << 16) | (uint32_t(at[1]) << 8) | at[2];
        return (value * 2654435761u) >> (32 - hash_bits);
    }
}

int pack(const uint8_t* data, int size, int position, int end, state& packer, uint8_t* output, int& output_size)
{
    int16_t* table = packer.table;
    int out = output_size;

    while(position < end)
    {
        if(packer.group < 0 || packer.tokens == 8)
        {
            packer.group = out;
            packer.tokens = 0;
            output[out++] = 0;
        }

        int length = 0;
        int distance = 0;

        // One candidate: the last position with the same 3-byte hash.
        if(position + min_length <= size)
        {
            unsigned key = hash(data + position);
            int previous = table[key];
            table[key] = int16_t(position);

            if(previous >= 0 && position - previous <= window)
            {
                const uint8_t* from = data + previous;
                const uint8_t* to = data + position;
                int limit = size - position < max_length ? size - position : max_length;

                while(length < limit && from[length] == to[length])
                {
                    ++length;
                }

                if(length >= min_length)
                {
                    distance = position - previous;
                }
                else
                {
                    length = 0;
                }
            }
        }

        if(length)
        {
            output[packer.group] |= uint8_t(0x80 >> packer.tokens);
            int value = ((length - min_length) << 12) | (distance - 1);
            output[out++] = uint8_t(value >> 8);
            output[out++] = uint8_t(value);

            // The positions inside the match go in the table too.
            int stop = position + length;

            if(stop > size - min_length + 1)
            {
                stop = size - min_length + 1;
            }

            for(int next = position + 1; next < stop; ++next)
            {
                table[hash(data + next)] = int16_t(next);
            }

            position += length;
        }
        else
        {
            output[out++] = data[position++];
        }

        ++packer.tokens;
    }

    output_size = out;
    return position;
}

}
