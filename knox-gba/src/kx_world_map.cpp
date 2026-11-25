#include "kx_world_map.h"

#include "bn_algorithm.h"
#include "bn_display.h"
#include "bn_sprite_items_fx.h"

#include "kx_game.h"
#include "kx_hash.h"
#include "kx_macro_map.h"
#include "kx_world.h"

namespace kx
{

namespace
{
    constexpr int fog_bytes = world_map::chunks * world_map::chunks / 8;
    constexpr int chunk_tiles = 1 << world_map::chunk_shift;

    // EWRAM: IWRAM is kept for code and the stack (memory: big globals go in EWRAM).
    alignas(4) BN_DATA_EWRAM_BSS uint8_t fog[fog_bytes];

    struct mark_list
    {
        uint8_t count = 0;
        uint8_t spare[3] = {};
        int8_t x[world_map::max_marks] = {};
        int8_t y[world_map::max_marks] = {};
    };

    struct auto_mark_list
    {
        uint8_t count = 0;
        uint8_t spare[3] = {};
        uint8_t x[world_map::max_auto_marks] = {};
        uint8_t y[world_map::max_auto_marks] = {};
        uint8_t kind[world_map::max_auto_marks] = {};
        uint16_t id[world_map::max_auto_marks] = {};
    };

    mark_list marks;
    auto_mark_list auto_marks;
    int last_chunk_x = -1;
    int last_chunk_y = -1;

    static_assert(sizeof(fog) + sizeof(mark_list) + sizeof(auto_mark_list) == world_map::save_bytes);

    // Settlement names (mockup 05 shows the first six). Original names in the spirit of the spec's setting.
    constexpr const char* names[] = {
        "STARA PILA", "KAMEN", "BREZA", "VYSOKO", "DOLINA", "PRISTAN", "LIPOVEC", "ZELENA", "BOROVO", "HRAST",
        "JEZERO", "POLJANA", "KRUSEVO", "GORICA", "VRBNO", "SOKOL", "OSTROV", "LUKA", "MLINSKO", "TOPOLA",
        "BELA VODA", "RAVNO", "CRNI VRH", "MOST"
    };

    constexpr int names_count = int(sizeof(names) / sizeof(names[0]));

    static_assert(names_count >= macro::max_settlements);

    void reveal(int chunk_x, int chunk_y)
    {
        if(unsigned(chunk_x) < unsigned(world_map::chunks) && unsigned(chunk_y) < unsigned(world_map::chunks))
        {
            int index = (chunk_y * world_map::chunks) + chunk_x;
            fog[index >> 3] = uint8_t(fog[index >> 3] | (1 << (index & 7)));
        }
    }

    // fx frames (tools/assets/fx.py): the mark arrows, right, up-right, up (flipped for the others).
    constexpr int arrow_right = 20;
    constexpr int arrow_diagonal = 21;
    constexpr int arrow_up = 22;
    constexpr int arrow_bg_priority = 1;
    constexpr int arrow_reach_x = 100;      // inside the horde arrows (110, 50)
    constexpr int arrow_reach_y = 42;

    static_assert(bn::sprite_items::fx.tiles_item().graphics_count() > arrow_up);
}

namespace world_map
{

void reset()
{
    for(uint8_t& value : fog)
    {
        value = 0;
    }

    marks = mark_list();
    auto_marks = auto_mark_list();
    last_chunk_x = -1;
    last_chunk_y = -1;
}

void explore(int tile_x, int tile_y)
{
    int chunk_x = tile_x >> chunk_shift;
    int chunk_y = tile_y >> chunk_shift;

    if(chunk_x == last_chunk_x && chunk_y == last_chunk_y)
    {
        return;
    }

    last_chunk_x = chunk_x;
    last_chunk_y = chunk_y;

    for(int dy = -1; dy <= 1; ++dy)
    {
        for(int dx = -1; dx <= 1; ++dx)
        {
            reveal(chunk_x + dx, chunk_y + dy);
        }
    }
}

void scout(int tile_x, int tile_y, int reach)
{
    int chunk_x = tile_x >> chunk_shift;
    int chunk_y = tile_y >> chunk_shift;

    for(int dy = -reach; dy <= reach; ++dy)
    {
        for(int dx = -reach; dx <= reach; ++dx)
        {
            reveal(chunk_x + dx, chunk_y + dy);
        }
    }
}

void reveal_all()
{
    for(uint8_t& value : fog)
    {
        value = 0xFF;
    }
}

bool explored(int chunk_x, int chunk_y)
{
    if(unsigned(chunk_x) >= unsigned(chunks) || unsigned(chunk_y) >= unsigned(chunks))
    {
        return false;
    }

    int index = (chunk_y * chunks) + chunk_x;
    return fog[index >> 3] & (1 << (index & 7));
}

int mark_count()
{
    return marks.count;
}

void mark(int index, int& chunk_x, int& chunk_y)
{
    chunk_x = uint8_t(marks.x[index]);
    chunk_y = uint8_t(marks.y[index]);
}

bool toggle_mark(int chunk_x, int chunk_y)
{
    for(int index = 0; index < marks.count; ++index)
    {
        if(uint8_t(marks.x[index]) == chunk_x && uint8_t(marks.y[index]) == chunk_y)
        {
            // Cleared: the last mark takes its place.
            --marks.count;
            marks.x[index] = marks.x[marks.count];
            marks.y[index] = marks.y[marks.count];
            return true;
        }
    }

    if(marks.count >= max_marks)
    {
        return false;
    }

    marks.x[marks.count] = int8_t(chunk_x);
    marks.y[marks.count] = int8_t(chunk_y);
    ++marks.count;
    return true;
}

void add_auto_mark(auto_mark kind, uint16_t id, int tile_x, int tile_y)
{
    for(int index = 0; index < auto_marks.count; ++index)
    {
        if(auto_marks.id[index] == id && auto_marks.kind[index] == uint8_t(kind))
        {
            return;
        }
    }

    if(auto_marks.count >= max_auto_marks)
    {
        // The oldest goes; the rest move down.
        for(int index = 1; index < auto_marks.count; ++index)
        {
            auto_marks.x[index - 1] = auto_marks.x[index];
            auto_marks.y[index - 1] = auto_marks.y[index];
            auto_marks.kind[index - 1] = auto_marks.kind[index];
            auto_marks.id[index - 1] = auto_marks.id[index];
        }

        --auto_marks.count;
    }

    int index = auto_marks.count++;
    auto_marks.x[index] = uint8_t(tile_x >> chunk_shift);
    auto_marks.y[index] = uint8_t(tile_y >> chunk_shift);
    auto_marks.kind[index] = uint8_t(kind);
    auto_marks.id[index] = id;
}

bool has_auto_mark(auto_mark kind, uint16_t id)
{
    for(int index = 0; index < auto_marks.count; ++index)
    {
        if(auto_marks.id[index] == id && auto_marks.kind[index] == uint8_t(kind))
        {
            return true;
        }
    }

    return false;
}

void remove_auto_marks(auto_mark kind, uint16_t id)
{
    for(int index = 0; index < auto_marks.count;)
    {
        if(auto_marks.id[index] == id && auto_marks.kind[index] == uint8_t(kind))
        {
            --auto_marks.count;
            auto_marks.x[index] = auto_marks.x[auto_marks.count];
            auto_marks.y[index] = auto_marks.y[auto_marks.count];
            auto_marks.kind[index] = auto_marks.kind[auto_marks.count];
            auto_marks.id[index] = auto_marks.id[auto_marks.count];
        }
        else
        {
            ++index;
        }
    }
}

int auto_mark_count()
{
    return auto_marks.count;
}

void auto_mark_at(int index, auto_mark& kind, uint16_t& id, int& chunk_x, int& chunk_y)
{
    kind = auto_mark(auto_marks.kind[index]);
    id = auto_marks.id[index];
    chunk_x = auto_marks.x[index];
    chunk_y = auto_marks.y[index];
}

bool nearest_mark(int tile_x, int tile_y, int& mark_tile_x, int& mark_tile_y)
{
    int best = -1;

    for(int index = 0; index < marks.count; ++index)
    {
        int x = (int(uint8_t(marks.x[index])) << chunk_shift) + (chunk_tiles / 2);
        int y = (int(uint8_t(marks.y[index])) << chunk_shift) + (chunk_tiles / 2);
        int distance = bn::abs(x - tile_x) + bn::abs(y - tile_y);

        if(best < 0 || distance < best)
        {
            best = distance;
            mark_tile_x = x;
            mark_tile_y = y;
        }
    }

    return best >= 0;
}

const char* settlement_name(int index)
{
    // A shuffle of the names by the seed; settlement i takes the i-th.
    int order[names_count];

    for(int value = 0; value < names_count; ++value)
    {
        order[value] = value;
    }

    roll_stream rolls(hash32(world::seed() ^ 0x6e616d65));

    for(int value = names_count - 1; value > 0; --value)
    {
        int other = rolls.next(value + 1);
        bn::swap(order[value], order[other]);
    }

    return names[order[bn::clamp(index, 0, names_count - 1)]];
}

void save(save::writer& out)
{
    out.put(fog);
    out.put(marks);
    out.put(auto_marks);
}

void load(save::reader& in)
{
    in.get(fog);
    in.get(marks);
    marks.count = uint8_t(bn::min(int(marks.count), max_marks));
    in.get(auto_marks);
    auto_marks.count = uint8_t(bn::min(int(auto_marks.count), max_auto_marks));
    last_chunk_x = -1;
    last_chunk_y = -1;
}

}

void mark_arrow::update(game& owner)
{
    // Most of the time there are no marks: nothing to look up.
    if(world_map::mark_count() == 0)
    {
        hide();
        return;
    }

    const player& hero = owner.hero();
    int tile_x = hero.position().x().integer() >> world::tile_shift;
    int tile_y = hero.position().y().integer() >> world::tile_shift;
    int mark_x;
    int mark_y;

    if(hero.items().find(item_id::COMPASS) < 0 || ! world_map::nearest_mark(tile_x, tile_y, mark_x, mark_y))
    {
        hide();
        return;
    }

    bn::fixed_point target((mark_x * world::tile_size) + (world::tile_size / 2),
                           (mark_y * world::tile_size) + (world::tile_size / 2));
    int dx = owner.screen_x(target);
    int dy = owner.screen_y(target);

    // On screen: no arrow.
    if(bn::abs(dx) <= bn::display::width() / 2 && bn::abs(dy) <= bn::display::height() / 2)
    {
        hide();
        return;
    }

    // Where the line from the screen's middle to the mark meets a rectangle inside the screen edge.
    int x;
    int y;

    if(bn::abs(dx) * arrow_reach_y >= bn::abs(dy) * arrow_reach_x)
    {
        x = dx > 0 ? arrow_reach_x : -arrow_reach_x;
        y = (dy * arrow_reach_x) / bn::max(bn::abs(dx), 1);
    }
    else
    {
        y = dy > 0 ? arrow_reach_y : -arrow_reach_y;
        x = (dx * arrow_reach_y) / bn::max(bn::abs(dy), 1);
    }

    // Eight directions: straight when one axis is more than twice the other, else diagonal.
    int frame = bn::abs(dx) > bn::abs(dy) * 2 ? arrow_right : bn::abs(dy) > bn::abs(dx) * 2 ? arrow_up :
                                                                                            arrow_diagonal;

    if(! _arrow)
    {
        _arrow = bn::sprite_items::fx.create_sprite(x, y, frame);
        _arrow->set_bg_priority(arrow_bg_priority);
        _frame = frame;
    }
    else
    {
        _arrow->set_position(x, y);

        if(frame != _frame)
        {
            _arrow->set_tiles(bn::sprite_items::fx.tiles_item(), frame);
            _frame = frame;
        }
    }

    // The frames point right and up.
    _arrow->set_horizontal_flip(dx < 0 && frame != arrow_up);
    _arrow->set_vertical_flip(dy > 0 && frame != arrow_right);
}

void mark_arrow::hide()
{
    _arrow.reset();
}

}
