#include "kx_save.h"

#include "kx_bench.h"
#include "kx_save_lz.h"
#include "kx_settings.h"

#include "bn_array.h"
#include "bn_log.h"
#include "bn_span.h"
#include "bn_sram.h"

namespace kx::save
{

namespace
{
    constexpr uint32_t slot_magic = 0x584F4E4B;       // "KNOX"
    constexpr uint32_t world_magic = 0x444C5257;      // "WRLD"

    // Bumped whenever a block's layout changes: older saves then read as no save.
    constexpr uint16_t format_version = 17;

    struct slot_header
    {
        uint32_t magic;
        uint16_t version;
        uint8_t has_survivor;
        uint8_t unused;
        uint32_t sequence;
        uint32_t seed;
        int32_t world_minute;
        uint32_t character_size;
        uint32_t character_crc;
        uint32_t header_crc;            // of the fields above
        int32_t focus_x;                // where the survivor stood (title backdrop; 0 in older saves)
        int32_t focus_y;
        uint8_t sandbox[sandbox::setting_count];    // milestone 13: the sandbox settings (in the header CRC)
        uint8_t reserved[header_size - 40 - sandbox::setting_count];   // playtest 2: the game's settings first
    };

    static_assert(settings::save_bytes <= header_size - 40 - sandbox::setting_count);

    static_assert(sizeof(slot_header) == header_size);

    struct world_header
    {
        uint32_t magic;
        uint32_t sequence;
        uint32_t size;
        uint32_t crc;
    };

    static_assert(sizeof(world_header) == world_header_size);

    // CRC-32 (IEEE), table driven.
    struct crc_table
    {
        uint32_t values[256];

        constexpr crc_table() :
            values()
        {
            for(uint32_t index = 0; index < 256; ++index)
            {
                uint32_t value = index;

                for(int bit = 0; bit < 8; ++bit)
                {
                    value = (value & 1) ? (value >> 1) ^ 0xEDB88320u : value >> 1;
                }

                values[index] = value;
            }
        }
    };

    constexpr crc_table crc_values;

    // Running CRC: start with ~0, finish with ~.
    [[nodiscard]] uint32_t crc_update(uint32_t crc, const uint8_t* data, int size)
    {
        for(int index = 0; index < size; ++index)
        {
            crc = crc_values.values[(crc ^ data[index]) & 0xFF] ^ (crc >> 8);
        }

        return crc;
    }

    [[nodiscard]] uint32_t crc32(const void* data, int size)
    {
        return ~crc_update(0xFFFFFFFFu, static_cast<const uint8_t*>(data), size);
    }

    [[nodiscard]] uint32_t header_crc(const slot_header& header)
    {
        uint32_t crc = crc_update(0xFFFFFFFFu, reinterpret_cast<const uint8_t*>(&header), 28);
        return ~crc_update(crc, header.sandbox, sandbox::setting_count);
    }

    void sram_read(void* destination, int size, int offset)
    {
        bn::span<uint8_t> bytes(static_cast<uint8_t*>(destination), size);
        bn::sram::read_span_offset(bytes, offset);
    }

    #if KX_BENCH == 17
        // Power-cut bench: the bytes SRAM still takes before the power goes (-1: no cut).
        int power_budget = -1;
    #endif

    void sram_write(const void* source, int size, int offset)
    {
        #if KX_BENCH == 17
            if(power_budget >= 0)
            {
                size = bn::min(size, power_budget);
                power_budget -= size;

                if(size <= 0)
                {
                    return;
                }
            }
        #endif

        bn::sram::write_span_offset(bn::span<const uint8_t>(static_cast<const uint8_t*>(source), size), offset);
    }

    alignas(4) BN_DATA_EWRAM_BSS uint8_t character_buffer[character_capacity];
    alignas(4) BN_DATA_EWRAM_BSS uint8_t world_buffer[world_capacity];
    int character_size = 0;
    int world_size = 0;

    // The newest save in SRAM (0 if none); the next goes to the other character slot.
    uint32_t last_sequence = 0;

    // The save being written.
    enum class phase : uint8_t
    {
        IDLE,
        WORLD_DATA,
        WORLD_HEADER,
        CHARACTER
    };

    phase job_phase = phase::IDLE;
    int job_offset = 0;
    int job_world_copy = 0;         // the world copy being written

    // Playtest 5: the world area holds two copies when the world block fits half of it, written in turn, so a cut
    // while one is written leaves the other (the one the survivor's slot was saved with). A bigger block takes the
    // whole area from copy 0, as before. Playtest 6: "fits" is the packed size, so in practice every world does.
    constexpr int world_copy_offsets[2] = { world_offset, world_offset + ((sram_size - world_offset) / 2) };
    constexpr int world_half_capacity = world_copy_offsets[1] - world_copy_offsets[0] - world_header_size;

    [[nodiscard]] constexpr int world_copy_capacity(int copy)
    {
        return copy == 0 ? world_capacity : sram_size - world_copy_offsets[1] - world_header_size;
    }

    static_assert(world_copy_capacity(1) >= world_half_capacity);

    // The copy holding the newest world (-1: not known, as after erasing).
    int last_world_copy = -1;
    uint32_t job_crc = 0;
    slot_header job_header;

    // Playtest 6: the world block is stored packed (kx_save_lz.h), so a big world fits half the area and keeps two
    // copies. It is packed a slice a frame into a small stage and each finished part goes straight to SRAM. The world
    // header's size has the top bit set for a packed block; a block that won't pack into its space is stored as is.
    constexpr uint32_t packed_flag = 0x80000000u;
    // Packing costs more than writing: about a quarter of a frame for 512 bytes, so half that a frame.
    constexpr int pack_bytes_per_frame = bytes_per_frame / 2;
    constexpr int stage_capacity = ((pack_bytes_per_frame * 9) / 8) + 64;

    BN_DATA_EWRAM_BSS int16_t lz_table[lz::table_size];
    alignas(4) BN_DATA_EWRAM_BSS uint8_t stage[stage_capacity];
    int stage_size = 0;
    lz::state packer;
    bool job_packed = false;
    int job_out = 0;                // bytes of the block stored so far
    int job_capacity = 0;           // the space the copy being written has

    #if KX_BENCH == 17
        int bench_stored = 0;       // the last world block's stored bytes
    #endif

    void start_world(int copy, bool packed)
    {
        job_world_copy = copy;
        job_packed = packed;
        job_offset = 0;
        job_out = 0;
        // Packed on copy 0, a block that would fit half the area as is gets only that half: past it, it is stored as
        // is (noise packs bigger than it is) and keeps copy 1.
        job_capacity = packed && copy == 0 && world_size <= world_half_capacity ? world_half_capacity :
                                                                                  world_copy_capacity(copy);
        job_crc = 0xFFFFFFFFu;
        last_world_copy = copy;

        if(packed)
        {
            for(int16_t& entry : lz_table)
            {
                entry = -1;
            }

            packer.table = lz_table;
            packer.group = -1;
            packer.tokens = 0;
            stage[0] = uint8_t(world_size);
            stage[1] = uint8_t(world_size >> 8);
            stage[2] = uint8_t(world_size >> 16);
            stage[3] = uint8_t(world_size >> 24);
            stage_size = 4;
        }
    }

    // A slice of the packed block: packs the next bytes and stores what is final.
    void write_packed_slice()
    {
        int end = bn::min(job_offset + pack_bytes_per_frame, world_size);
        job_offset = lz::pack(world_buffer, world_size, job_offset, end, packer, stage, stage_size);
        BN_BASIC_ASSERT(stage_size <= stage_capacity, "Save stage overflow");

        bool done = job_offset >= world_size;
        int ready = done || packer.group < 0 ? stage_size : packer.group;

        if(job_out + ready > job_capacity)
        {
            // Too big packed for this copy's space: stored as is if that fits, otherwise copy 1 goes back to copy 0
            // (the whole area).
            #if KX_BENCH
                BN_LOG("KXSAVE packed block over ", job_capacity, " bytes on copy ", job_world_copy);
            #endif

            if(job_world_copy == 1 && world_size > world_half_capacity)
            {
                start_world(0, true);
            }
            else
            {
                start_world(job_world_copy, false);
            }

            return;
        }

        sram_write(stage, ready, world_copy_offsets[job_world_copy] + world_header_size + job_out);
        job_crc = crc_update(job_crc, stage, ready);
        job_out += ready;

        for(int index = ready; index < stage_size; ++index)
        {
            stage[index - ready] = stage[index];
        }

        stage_size -= ready;
        packer.group -= packer.group >= 0 ? ready : 0;

        if(done)
        {
            job_phase = phase::WORLD_HEADER;
        }
    }

    // The CRC of `size` bytes of SRAM.
    [[nodiscard]] uint32_t sram_crc(int offset, int size)
    {
        alignas(4) uint8_t chunk[256];
        uint32_t crc = 0xFFFFFFFFu;

        while(size > 0)
        {
            int part = bn::min(size, int(sizeof(chunk)));
            sram_read(chunk, part, offset);
            crc = crc_update(crc, chunk, part);
            offset += part;
            size -= part;
        }

        return ~crc;
    }

    // Unpacks a packed block of `size` bytes in SRAM into the world buffer; false if it doesn't make sense.
    [[nodiscard]] bool unpack(int offset, int size)
    {
        alignas(4) uint8_t chunk[256];
        int position = 0;
        int available = 0;
        bool short_input = false;

        auto next = [&]() -> uint8_t
        {
            if(position == available)
            {
                int part = bn::min(size, int(sizeof(chunk)));

                if(part <= 0)
                {
                    short_input = true;
                    return 0;
                }

                sram_read(chunk, part, offset);
                offset += part;
                size -= part;
                position = 0;
                available = part;
            }

            return chunk[position++];
        };

        int unpacked = next();
        unpacked |= next() << 8;
        unpacked |= next() << 16;
        unpacked |= next() << 24;

        if(short_input || unpacked < 0 || unpacked > world_capacity)
        {
            return false;
        }

        int out = 0;

        while(out < unpacked)
        {
            int flags = next();

            for(int bit = 0; bit < 8 && out < unpacked; ++bit)
            {
                if(flags & (0x80 >> bit))
                {
                    int value = next() << 8;
                    value |= next();
                    int length = (value >> 12) + lz::min_length;
                    int distance = (value & 0xFFF) + 1;

                    if(distance > out || out + length > unpacked)
                    {
                        return false;
                    }

                    for(int index = 0; index < length; ++index, ++out)
                    {
                        world_buffer[out] = world_buffer[out - distance];
                    }
                }
                else
                {
                    world_buffer[out++] = next();
                }

                if(short_input)
                {
                    return false;
                }
            }
        }

        world_size = unpacked;
        return true;
    }

    // Reads a character slot into the character buffer if it checks out.
    [[nodiscard]] bool read_slot(int slot, slot_header& header)
    {
        sram_read(&header, header_size, slot * slot_size);

        if(header.magic != slot_magic || header.version != format_version || header.header_crc != header_crc(header) ||
           header.character_size > uint32_t(character_capacity))
        {
            return false;
        }

        sram_read(character_buffer, int(header.character_size), (slot * slot_size) + header_size);
        return crc32(character_buffer, int(header.character_size)) == header.character_crc;
    }

    // The newest valid character slot, left in the character buffer; -1 if none.
    [[nodiscard]] int read_newest(slot_header& header)
    {
        slot_header first;
        slot_header second;
        bool first_ok = read_slot(0, first);
        bool second_ok = read_slot(1, second);

        if(! first_ok && ! second_ok)
        {
            return -1;
        }

        int slot = ! second_ok || (first_ok && first.sequence > second.sequence) ? 0 : 1;

        // The buffer holds what the last read left in it, slot 1's, even when that slot failed its check (its
        // block is read before the check): slot 0 is read again (playtest 5's power-cut bench: a cut while slot 1
        // was written loaded slot 0's header with slot 1's broken block).
        if(slot == 0)
        {
            BN_ASSERT(read_slot(0, first), "Save slot changed while reading");
        }

        header = slot == 0 ? first : second;
        return slot;
    }

    void write_slice()
    {
        switch(job_phase)
        {

        case phase::WORLD_DATA:
        {
            if(job_packed)
            {
                write_packed_slice();
                break;
            }

            int size = bn::min(bytes_per_frame, world_size - job_offset);
            sram_write(world_buffer + job_offset, size,
                       world_copy_offsets[job_world_copy] + world_header_size + job_offset);
            job_crc = crc_update(job_crc, world_buffer + job_offset, size);
            job_offset += size;
            job_out = job_offset;

            if(job_offset >= world_size)
            {
                job_phase = phase::WORLD_HEADER;
            }
            break;
        }

        case phase::WORLD_HEADER:
        {
            world_header header{ world_magic, job_header.sequence,
                                 uint32_t(job_out) | (job_packed ? packed_flag : 0), ~job_crc };

            #if KX_BENCH == 17
                bench_stored = job_out;
            #endif

            #if KX_BENCH
                BN_LOG("KXSAVE stored ", job_out, " bytes", job_packed ? " packed" : "", " on copy ", job_world_copy);
            #endif
            sram_write(&header, world_header_size, world_copy_offsets[job_world_copy]);
            job_phase = phase::CHARACTER;
            break;
        }

        case phase::CHARACTER:
        {
            int slot = int(job_header.sequence & 1);
            sram_write(character_buffer, character_size, (slot * slot_size) + header_size);
            sram_write(&job_header, header_size, slot * slot_size);
            last_sequence = job_header.sequence;
            job_phase = phase::IDLE;
            break;
        }

        default:
            break;
        }
    }

    void finish()
    {
        while(job_phase != phase::IDLE)
        {
            write_slice();
        }
    }
}

summary read_summary()
{
    summary result;
    slot_header header;

    if(read_newest(header) >= 0)
    {
        result.valid = true;
        result.has_survivor = header.has_survivor;
        result.seed = header.seed;
        result.world_minute = header.world_minute;
        result.focus_x = header.focus_x;
        result.focus_y = header.focus_y;
        bn::memory::copy(header.sandbox[0], sandbox::setting_count, result.sandbox.values[0]);
        last_sequence = header.sequence;
        settings::unpack(header.reserved);
    }

    return result;
}

writer character_writer()
{
    finish();
    return writer(character_buffer, character_capacity);
}

writer world_writer()
{
    finish();
    return writer(world_buffer, world_capacity);
}

void commit(const writer& character, const writer& world, const summary& info, bool at_once)
{
    finish();
    character_size = character.size();
    world_size = world.size();

    job_header = slot_header();
    job_header.magic = slot_magic;
    job_header.version = format_version;
    job_header.has_survivor = info.has_survivor;
    job_header.sequence = last_sequence + 1;
    job_header.seed = info.seed;
    job_header.world_minute = info.world_minute;
    job_header.focus_x = info.focus_x;
    job_header.focus_y = info.focus_y;
    bn::memory::copy(info.sandbox.values[0], sandbox::setting_count, job_header.sandbox[0]);
    job_header.character_size = uint32_t(character_size);
    job_header.character_crc = crc32(character_buffer, character_size);
    job_header.header_crc = header_crc(job_header);
    settings::pack(job_header.reserved);

    job_phase = phase::WORLD_DATA;
    start_world(last_world_copy == 0 ? 1 : 0, true);

    #if KX_BENCH
        BN_LOG("KXSAVE world block ", world_size, " bytes, copy ", job_world_copy);
    #endif

    if(at_once)
    {
        finish();
    }
}

void step()
{
    if(job_phase != phase::IDLE)
    {
        write_slice();
    }
}

bool busy()
{
    return job_phase != phase::IDLE;
}

bool load(summary& info, bool& world_ok)
{
    finish();

    slot_header header;
    world_ok = false;

    if(read_newest(header) < 0)
    {
        return false;
    }

    info.valid = true;
    info.has_survivor = header.has_survivor;
    info.seed = header.seed;
    info.world_minute = header.world_minute;
    bn::memory::copy(header.sandbox[0], sandbox::setting_count, info.sandbox.values[0]);
    character_size = int(header.character_size);
    last_sequence = header.sequence;

    // The world copy saved with this survivor; if it doesn't check out, the newest older one that does.
    int chosen = -1;
    uint32_t chosen_sequence = 0;
    uint32_t chosen_size = 0;

    for(int copy = 0; copy < 2; ++copy)
    {
        world_header world;
        sram_read(&world, world_header_size, world_copy_offsets[copy]);
        uint32_t stored = world.size & ~packed_flag;

        // Older than the survivor's slot or the same save first; a newer one (a cut while the survivor's slot was
        // written after a world too big for two copies) only if nothing else checks out.
        bool newer = world.sequence > header.sequence;
        bool chosen_newer = chosen >= 0 && chosen_sequence > header.sequence;

        if(world.magic != world_magic || stored > uint32_t(world_copy_capacity(copy)) ||
           (chosen >= 0 && (newer ? ! chosen_newer || world.sequence <= chosen_sequence :
                                    ! chosen_newer && world.sequence <= chosen_sequence)))
        {
            continue;
        }

        if(sram_crc(world_copy_offsets[copy] + world_header_size, int(stored)) == world.crc)
        {
            chosen = copy;
            chosen_sequence = world.sequence;
            chosen_size = world.size;
        }
    }

    if(chosen >= 0)
    {
        int offset = world_copy_offsets[chosen] + world_header_size;
        int stored = int(chosen_size & ~packed_flag);

        if(chosen_size & packed_flag)
        {
            if(! unpack(offset, stored))
            {
                chosen = -1;
            }
        }
        else
        {
            sram_read(world_buffer, stored, offset);
            world_size = stored;
        }
    }

    world_ok = chosen >= 0;
    last_world_copy = chosen;

    if(! world_ok)
    {
        world_size = 0;
    }

    return true;
}

reader character_reader()
{
    return reader(character_buffer, character_size);
}

reader world_reader()
{
    return reader(world_buffer, world_size);
}

void erase()
{
    finish();
    bn::sram::set_bytes(0, header_size, 0);
    bn::sram::set_bytes(0, header_size, slot_size);
    bn::sram::set_bytes(0, world_header_size, world_copy_offsets[0]);
    bn::sram::set_bytes(0, world_header_size, world_copy_offsets[1]);
    last_sequence = 0;
    last_world_copy = -1;
}


#if KX_BENCH == 17
namespace
{
    // Playtest 6: world blocks are packed, so the bench fills them two ways: a pattern that packs well, and noise
    // that doesn't (stored as is when it won't fit packed).
    bool noise_blocks = false;

    [[nodiscard]] uint8_t block_byte(int index, uint32_t tag)
    {
        if(! noise_blocks)
        {
            return uint8_t((index * 31) + (tag * 7));
        }

        uint32_t value = (uint32_t(index) * 2654435761u) ^ (tag * 40503u);
        value ^= value >> 15;
        value *= 2246822519u;
        return uint8_t(value >> 13);
    }

    // A block of `size` bytes tagged with its save's number: the tag, then bytes worked out from it.
    void fill_block(writer& output, int size, uint32_t tag)
    {
        output.put(tag);

        for(int index = 4; index < size; ++index)
        {
            output.put(block_byte(index, tag));
        }
    }

    // The block's tag if every byte matches it, 0 if not.
    [[nodiscard]] uint32_t check_block(reader input, int size)
    {
        if(input.remaining() != size)
        {
            return 0;
        }

        uint32_t tag = input.get<uint32_t>();

        for(int index = 4; index < size; ++index)
        {
            if(input.get<uint8_t>() != block_byte(index, tag))
            {
                return 0;
            }
        }

        return tag;
    }

    void write_save(int character_size, int world_size, uint32_t tag)
    {
        writer character = character_writer();
        fill_block(character, character_size, tag);
        writer world = world_writer();
        fill_block(world, world_size, tag);
        summary info;
        info.valid = true;
        info.has_survivor = true;
        info.seed = tag;
        commit(character, world, info, true);
    }

    alignas(4) BN_DATA_EWRAM_BSS uint8_t sram_copy[sram_size];
}

void power_cut_bench()
{
    // Two complete saves (2 and 3), then a third (4) cut off after every `step` bytes of its writing, from none to
    // all of them; after each cut the save is loaded as at power on. The survivor must come back from save 3 or 4,
    // and the world from the same save.
    constexpr int character_size = 1200;
    struct bench_case
    {
        int world_size;
        bool noise;
    };

    constexpr bench_case cases[] = {
        { 4096 + 600, false }, { 9000, false }, { 14000, false }, { 20000, false }, { world_capacity, false },
        { 9000, true }, { 14000, true }, { 20000, true }, { world_capacity, true }
    };
    constexpr int step = 61;

    for(bench_case test : cases)
    {
        int world_size = test.world_size;
        noise_blocks = test.noise;
        erase();
        write_save(character_size, world_size, 2);
        write_save(character_size, world_size, 3);
        sram_read(sram_copy, sram_size, 0);

        // The bytes the third save writes, counted on a run with no cut (a packed block too big for its copy is
        // started again, so this can be more than the block).
        constexpr int unlimited = 1 << 30;
        last_sequence = 2;
        power_budget = unlimited;
        write_save(character_size, world_size, 4);
        int total = unlimited - power_budget;
        int stored = bench_stored;
        power_budget = -1;
        bn::sram::write_span_offset(bn::span<const uint8_t>(sram_copy, sram_size), 0);
        last_world_copy = 1;
        int same = 0;
        int other_world = 0;
        int fresh_world = 0;
        int lost = 0;
        int wrong = 0;

        for(int cut = 0; cut <= total + step; cut += step)
        {
            bn::sram::write_span_offset(bn::span<const uint8_t>(sram_copy, sram_size), 0);
            last_sequence = 2;          // as after the two saves (each load sets it again)
            last_world_copy = 1;
            power_budget = cut;
            write_save(character_size, world_size, 4);
            power_budget = -1;
            job_phase = phase::IDLE;

            summary info;
            bool world_ok = false;

            if(! load(info, world_ok))
            {
                ++lost;
                continue;
            }

            uint32_t survivor = check_block(character_reader(), character_size);
            uint32_t world = world_ok ? check_block(world_reader(), world_size) : 0;

            if(survivor != info.seed || (survivor != 3 && survivor != 4))
            {
                ++wrong;
                BN_LOG("KXCUT wrong at ", cut, " of ", total, ": survivor ", survivor, " header ", info.seed);
            }
            else if(world == survivor)
            {
                ++same;
            }
            else if(world != 0)
            {
                ++other_world;
            }
            else
            {
                ++fresh_world;
            }
        }

        BN_LOG("KXCUT world=", world_size, noise_blocks ? " noise" : " pattern", " stored=", stored,
               " cuts=", (total / step) + 2, " same=", same);
        BN_LOG("KXCUT other_world=", other_world, " world_lost=", fresh_world, " lost=", lost, " wrong=", wrong);
    }

    erase();
    BN_LOG("KXBENCH done");
}
#endif

}
