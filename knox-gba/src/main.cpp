#include "bn_bg_palettes.h"
#include "bn_core.h"
#include "bn_unique_ptr.h"

#include "kx_bench.h"
#include "kx_creation.h"
#include "kx_game.h"
#include "kx_sandbox.h"
#include "kx_save.h"
#include "kx_title.h"
#include "kx_world.h"

int main()
{
    bn::core::init();

    // The backdrop (shown wherever no layer draws, as while a world is being built or between screens) in the
    // menus' dark background colour (docs/palette.md), instead of the art's transparent magenta.
    bn::bg_palettes::set_transparent_color(bn::color(2, 2, 3));

    #if KX_BENCH == 17
        // Power-cut bench: the save alone, no game.
        kx::save::power_cut_bench();

        while(true)
        {
            bn::core::update();
        }
    #endif

    // The title starts a new world or continues the saved one; quitting comes back here. Tuning edits persist.
    // Bench builds play the hand-made test map so their scripted routes and numbers stay comparable; the travel and
    // save benches use a fixed generated world.
    #if KX_BENCH == 8
        int bench_pass = 0;
    #endif

    // After a death the next survivor starts at once in the same world (spec: Permadeath), without the title.
    #if ! KX_BENCH
        bool next_survivor = false;
        uint32_t world_seed = 0;
    #endif

    while(true)
    {
        bool load = false;
        bool make_character = false;
        [[maybe_unused]] bool replaces_save = false;
        kx::character_choice made;

        #if KX_BENCH == 7 || KX_BENCH == 10 || KX_BENCH == 12 || KX_BENCH == 14 || KX_BENCH == 16
            kx::world::init(0x4B4E4F58);
        #elif KX_BENCH == 8
            // Save bench: a new world saved and quit, then the same save loaded. The first pass plays with
            // sandbox settings that aren't the defaults (milestone 13); the loads must bring them back.
            if(bench_pass == 0)
            {
                kx::sandbox::settings chosen;
                chosen.values[0] = 0;
                chosen.values[1] = 2;
                chosen.values[2] = 2;
                chosen.values[3] = 3;
                chosen.values[4] = 0;
                kx::sandbox::set(chosen);
            }
            else
            {
                kx::sandbox::reset();
            }

            kx::world::init(0x4B4E4F58);
            load = bench_pass++ > 0;

            // The third pass is the new survivor after the death: a strong fire officer.
            if(bench_pass == 3)
            {
                make_character = true;
                made.job = kx::occupation::FIRE_OFFICER;
                made.traits = 1u << int(kx::trait::STRONG);
            }
        #elif KX_BENCH
            kx::world::init_test();
        #else
            if(next_survivor)
            {
                load = true;
                make_character = true;
            }
            else
            {
                kx::title_choice choice = kx::title_screen();
                load = choice.load;
                make_character = ! choice.load || choice.new_survivor;
                replaces_save = choice.replaces;
                world_seed = choice.seed;

                // A new world's sandbox settings (a saved world's are restored when its save loads).
                if(! choice.load)
                {
                    kx::sandbox::set(choice.sandbox);
                }

                // Let the title's background go before the next screen allocates its own.
                bn::core::update();
            }

            // A new world or a new survivor: occupation and traits first. B goes back to the title.
            if(make_character)
            {
                bool made_one = kx::character_creation(made, replaces_save);
                bn::core::update();

                if(! made_one)
                {
                    next_survivor = false;
                    continue;
                }
            }

            kx::world::init(world_seed);
        #endif

        {
            bn::unique_ptr<kx::game> game(new kx::game(load, make_character ? &made : nullptr));
            kx::game_exit exit = game->run();

            #if KX_BENCH
                static_cast<void>(exit);
            #else
                next_survivor = exit == kx::game_exit::DIED;
            #endif
        }

        // Butano releases background VRAM on its next update, so let one frame pass before the next game
        // allocates its maps (otherwise the old maps are still in VRAM and the allocation fails).
        bn::core::update();
    }
}
