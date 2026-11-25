#include "kx_save.h"

#include "kx_settings.h"

#include "bn_array.h"
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

    void sram_write(const void* source, int size, int offset)
    {
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
    uint32_t job_crc = 0;
    slot_header job_header;

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

        if(slot == 1 && first_ok)
        {
            // The buffer holds the last slot read: read the chosen one again.
            BN_ASSERT(read_slot(1, second), "Save slot changed while reading");
        }
        else if(slot == 0 && second_ok)
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
            int size = bn::min(bytes_per_frame, world_size - job_offset);
            sram_write(world_buffer + job_offset, size, world_offset + world_header_size + job_offset);
            job_crc = crc_update(job_crc, world_buffer + job_offset, size);
            job_offset += size;

            if(job_offset >= world_size)
            {
                job_phase = phase::WORLD_HEADER;
            }
            break;
        }

        case phase::WORLD_HEADER:
        {
            world_header header{ world_magic, job_header.sequence, uint32_t(world_size), ~job_crc };
            sram_write(&header, world_header_size, world_offset);
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
    job_offset = 0;
    job_crc = 0xFFFFFFFFu;

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

    world_header world;
    sram_read(&world, world_header_size, world_offset);

    if(world.magic == world_magic && world.size <= uint32_t(world_capacity))
    {
        sram_read(world_buffer, int(world.size), world_offset + world_header_size);
        world_ok = crc32(world_buffer, int(world.size)) == world.crc;
        world_size = world_ok ? int(world.size) : 0;
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
    bn::sram::set_bytes(0, world_header_size, world_offset);
    last_sequence = 0;
}

}
