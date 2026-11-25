#ifndef KX_SAVE_H
#define KX_SAVE_H

#include "bn_assert.h"
#include "bn_common.h"
#include "bn_memory.h"

#include <type_traits>

#include "kx_sandbox.h"

// Save system (spec: Save system and lazy simulation). One world slot in the 32 KB SRAM:
//
//   0x0000  character slot A (2 KB): 64 B header (magic, version, sequence, seed, clock, CRCs) + character block
//   0x0800  character slot B (2 KB), the same; the newer valid one wins, so a cut-off write never loses the
//           character
//   0x1000  world area (28 KB): 16 B world header (magic, sequence, size, CRC) + world block (horde counts,
//           containers, decals...)
//   0x4800  world copy 1 (playtest 5): when a world block fits half the area (14,320 bytes) the two halves take
//           turns, copy 0's header at 0x1000 and copy 1's here; a bigger block fills the area from 0x1000
//
// A save serialises both blocks into EWRAM buffers in one go (a few small copies), then writes them out: the world
// data, the world header, then the character slot not used last. Writing can be spread over frames (autosave while
// walking) or done at once (sleeping, quitting). The world block is checked on its own, and the load takes the
// world copy saved with the survivor's slot: a write cut off leaves the last whole save when the world fits two
// copies; a bigger world's changes are lost (the survivor is always kept). BENCH=17 cuts saves at every point.
namespace kx::save
{
    constexpr int sram_size = 32 * 1024;
    constexpr int slot_size = 2 * 1024;
    constexpr int header_size = 64;
    constexpr int character_capacity = slot_size - header_size;
    constexpr int world_offset = 2 * slot_size;
    constexpr int world_header_size = 16;
    constexpr int world_capacity = sram_size - world_offset - world_header_size;

    // Bytes written to SRAM per frame while a save is spread over frames.
    constexpr int bytes_per_frame = 512;

    // Autosave every in-game hour while walking (spec), at this minute: the hourly horde migration started on the
    // hour is long finished by then, so the save never has to complete it at once.
    constexpr int autosave_minute = 30;

    // Holding Select this long on a Start-menu tab saves and quits to the title.
    constexpr int quit_hold_frames = 60;

    // The world's save lists (containers, item pages, wall changes, base pieces): shown on the pause menu's save
    // line above this, and a toast warns once at the second (spec: caps show in the UI when close to full).
    constexpr int storage_show_percent = 80;
    constexpr int storage_warn_percent = 90;

    // Appends values to a block buffer.
    class writer
    {

    public:
        writer(uint8_t* data, int capacity) :
            _data(data),
            _capacity(capacity)
        {
        }

        void bytes(const void* source, int size)
        {
            BN_ASSERT(_size + size <= _capacity, "Save block full: ", _size + size, " > ", _capacity);
            bn::memory::copy(*static_cast<const uint8_t*>(source), size, _data[_size]);
            _size += size;
        }

        // Plain data (no pointers): copied as it is in memory.
        template<typename Type>
        void put(const Type& value)
        {
            static_assert(std::is_trivially_copyable_v<Type>);
            bytes(&value, sizeof(Type));
        }

        [[nodiscard]] int size() const
        {
            return _size;
        }

        // Where the next bytes go, for writers that fill in place.
        [[nodiscard]] uint8_t* cursor()
        {
            return _data + _size;
        }

    private:
        uint8_t* _data;
        int _capacity;
        int _size = 0;
    };

    // Reads values back in the order they were written.
    class reader
    {

    public:
        reader(const uint8_t* data, int size) :
            _data(data),
            _size(size)
        {
        }

        void bytes(void* destination, int size)
        {
            BN_ASSERT(_offset + size <= _size, "Save block too short: ", _offset + size, " > ", _size);
            bn::memory::copy(_data[_offset], size, *static_cast<uint8_t*>(destination));
            _offset += size;
        }

        template<typename Type>
        void get(Type& value)
        {
            static_assert(std::is_trivially_copyable_v<Type>);
            bytes(&value, sizeof(Type));
        }

        template<typename Type>
        [[nodiscard]] Type get()
        {
            Type value;
            get(value);
            return value;
        }

        // Bytes not read yet (a block saved before a part was added ends early).
        [[nodiscard]] int remaining() const
        {
            return _size - _offset;
        }

    private:
        const uint8_t* _data;
        int _size;
        int _offset = 0;
    };

    // What the title screen shows about the save.
    struct summary
    {
        bool valid = false;             // a character slot checks out
        bool has_survivor = false;      // false after a death (the world stays; a new survivor starts in it)
        uint32_t seed = 0;
        int world_minute = 0;
        int focus_x = 0;                // where the survivor stood, world pixels (0: not known)
        int focus_y = 0;
        sandbox::settings sandbox;      // the world's sandbox settings (milestone 13)
    };

    [[nodiscard]] summary read_summary();

    // Builds a save: fill the two blocks through these writers, then commit.
    [[nodiscard]] writer character_writer();
    [[nodiscard]] writer world_writer();

    // Starts writing the filled blocks (finishing a save still in progress first). At once, or over the next frames
    // through step().
    void commit(const writer& character, const writer& world, const summary& info, bool at_once);

    // Once per frame: writes the next slice of a save in progress.
    void step();

    [[nodiscard]] bool busy();

    // Loads the newest valid save into the block buffers. Returns false if there is none; world_ok tells whether
    // the world block checked out too.
    [[nodiscard]] bool load(summary& info, bool& world_ok);

    [[nodiscard]] reader character_reader();
    [[nodiscard]] reader world_reader();

    // Forgets the save (a new world replaces it).
    void erase();

    // BENCH=17: cuts the power part-way through saves and checks what loads (logs KXCUT lines).
    void power_cut_bench();
}

#endif
