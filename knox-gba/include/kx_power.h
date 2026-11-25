#ifndef KX_POWER_H
#define KX_POWER_H

#include <cstdint>

namespace kx
{

class game;

namespace save
{
    class reader;
    class writer;
}

// Generators (spec: Power and preservation): after the power shutoff, a generator (rare, heavy, fuel 1 l per ~3
// in-game hours, noise radius 20) powers appliances and lights within 10 tiles. Up to 4 per world. Fuel is lazy:
// a generator stores the fuel it had when last worked out and when that was; fridges near a running one keep food
// longer, credited when the player comes back.
namespace power_tuning
{
    constexpr int max_generators = 4;
    constexpr int minutes_per_litre = 180;          // spec: 1 l per ~3 in-game hours
    constexpr int tank_litres = 10;
    constexpr int tank_minutes = tank_litres * minutes_per_litre;
    constexpr int reach_tiles = 10;                 // spec: powers appliances and lights within 10 tiles
    constexpr int noise_tiles = 20;                 // spec: noise radius 20
    constexpr int noise_frames = 120;               // a running generator is heard this often
    constexpr int update_tiles = 32;                // generators this close to the player are worked out
}

namespace power
{
    void reset();

    // Generators placed (of 4).
    [[nodiscard]] int count();

    // Places a generator, off and empty.
    bool add(int tile_x, int tile_y, int now_minute);

    // The generator on a tile, or -1.
    [[nodiscard]] int at(int tile_x, int tile_y);

    // Fuel left in minutes, and running.
    [[nodiscard]] int fuel(int index, int now_minute);
    [[nodiscard]] bool running(int index, int now_minute);

    // Switches it on (false if it has no fuel) or off.
    bool switch_on(int index, bool on, int now_minute);

    // Pours fuel in; returns the litres it took (up to a full tank).
    int refuel(int index, int litres, int now_minute);

    // A running generator powers this tile.
    [[nodiscard]] bool powered(int tile_x, int tile_y, int now_minute);

    // Once a frame: generators near the player are worked out (fridges around a running one are credited), and
    // running ones are heard.
    void update(game& owner, int frame);

    void save(save::writer& out);
    void load(save::reader& in);
}

}

#endif
