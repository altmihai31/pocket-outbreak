#include "kx_input.h"

#include "bn_keypad.h"

namespace kx
{

#ifndef KX_BENCH
    #define KX_BENCH 0
#endif

namespace
{
    constexpr unsigned button_a = 1;
    constexpr unsigned button_b = 2;
    constexpr unsigned button_l = 4;
    constexpr unsigned button_select = 8;
    constexpr unsigned button_start = 16;
    constexpr unsigned button_r = 32;

    [[nodiscard]] input_frame make_frame(int dx, int dy, unsigned buttons, unsigned previous, int previous_dx,
                                         int previous_dy)
    {
        unsigned pressed = buttons & ~previous;
        unsigned released = previous & ~buttons;
        input_frame result;
        result.dx = dx;
        result.dy = dy;
        result.up_pressed = dy < 0 && previous_dy >= 0;
        result.down_pressed = dy > 0 && previous_dy <= 0;
        result.left_pressed = dx < 0 && previous_dx >= 0;
        result.right_pressed = dx > 0 && previous_dx <= 0;
        result.a_held = buttons & button_a;
        result.a_pressed = pressed & button_a;
        result.b_held = buttons & button_b;
        result.b_pressed = pressed & button_b;
        result.b_released = released & button_b;
        result.l_held = buttons & button_l;
        result.l_pressed = pressed & button_l;
        result.l_released = released & button_l;
        result.select_pressed = pressed & button_select;
        result.select_held = buttons & button_select;
        result.start_pressed = pressed & button_start;
        result.r_pressed = pressed & button_r;
        result.r_held = buttons & button_r;
        return result;
    }
}

#if KX_BENCH

namespace
{
    struct bench_segment
    {
        int frames;
        int dx;
        int dy;
        unsigned held;
        unsigned mashed;        // pressed for the first quarter of every mash period
        int mash_period;
    };

#if KX_BENCH == 6
    // Survival (make BENCH=6, 1 in-game minute per frame): bleed, bandage in the Health tab, eat from the Gear
    // tab, sleep in the bed, then die of a zombie bite's infection.
    constexpr bench_segment bench_route[] = {
        { 240, 0, 0, 0, 0, 0 },                     // 4 h bleeding from the laceration
        { 2, 0, 0, button_start, 0, 0 },            // Start menu (Gear)
        { 4, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_r, 0, 0 },                // Health tab
        { 4, 0, 0, 0, 0, 0 },
        { 2, 0, 1, 0, 0, 0 },                       // down: torso
        { 2, 0, 0, 0, 0, 0 },
        { 2, 0, 1, 0, 0, 0 },                       // down: left arm
        { 2, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // bandage it
        { 4, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_b, 0, 0 },
        { 30, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_start, 0, 0 },            // Gear tab: carried slot 2 holds the chips
        { 4, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },
        { 2, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },
        { 2, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // actions
        { 4, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // eat (closes the menu)
        { 600, 0, 0, 0, 0, 0 },                     // 10 h
        { 3, 0, 1, 0, 0, 0 },                       // face the bed
        { 4, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // sleep box
        { 4, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // sleep 8 h
        { 120, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_start, 0, 0 },            // Gear: the cursor is still on slot 2; slot 1 is the water
        { 4, 0, 0, 0, 0, 0 },
        { 2, -1, 0, 0, 0, 0 },
        { 2, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },
        { 4, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // drink
        { 600, 0, 0, 0, 0, 0 },                     // bitten after waking; the infection kills in ~3 h
    };
#elif KX_BENCH == 7
    // Travel (make BENCH=7): sprint a loop through a generated world (collision off for the player), crossing
    // several chunks on both axes while the interim population spawns and despawns zombies. It starts through the
    // starting village's south-east lots, so roofs fade.
    constexpr bench_segment bench_route[] = {
        { 30, 0, 0, 0, 0, 0 },
        { 170, 1, 1, 0, 0, 0 },                     // walk south-east off the crossroads, through the lots
        { 30, 0, 0, 0, 0, 0 },
        { 700, 1, 0, button_b, 0, 0 },              // east
        { 500, 0, 1, button_b, 0, 0 },              // south
        { 700, -1, 0, button_b, 0, 0 },             // west
        { 400, 1, -1, button_b, 0, 0 },             // north-east
        { 60, 0, 0, 0, 0, 0 },
    };
#elif KX_BENCH == 14
    // Towns (make BENCH=14): the census runs at the start, then the tour teleports to each template and set piece;
    // the survivor stands still, long enough for the music director's first tune (40 s).
    constexpr bench_segment bench_route[] = {
        { 2700, 0, 0, 0, 0, 0 },
    };
#elif KX_BENCH == 8
    // Save (make BENCH=8), played twice: off the crossroads and east at a sprint. The first time the game saves
    // over frames on the way, then drops an apple, saves at once and quits; the second time it loads that save.
    constexpr bench_segment bench_route[] = {
        { 30, 0, 0, 0, 0, 0 },
        { 170, 1, 1, 0, 0, 0 },
        { 30, 0, 0, 0, 0, 0 },
        { 600, 1, 0, button_b, 0, 0 },
        { 60, 0, 0, 0, 0, 0 },
    };
#elif KX_BENCH == 9
    // Vehicles (make BENCH=9): the game's own script fixes the car west of the start while this waits, then this
    // drives it east along the pavement through three zombies, turns south and runs into the field's fence; gets
    // out; then rides the bicycle out of the shed (the script puts the player on it) into a zombie.
    constexpr bench_segment bench_route[] = {
        { 12, 0, 0, 0, 0, 0 },
        { 6, 0, -1, 0, 0, 0 },                      // step up to the car
        { 402, 0, 0, 0, 0, 0 },                     // the car screen, three jobs on the car, getting in
        { 110, 0, 0, button_a, 0, 0 },              // east, up to top speed, through three zombies
        { 33, 1, 0, button_a, 0, 0 },               // turn right, to the south
        { 90, 0, 0, button_a, 0, 0 },               // into the fence
        { 40, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_r, 0, 0 },                // headlights
        { 10, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_l, 0, 0 },                // get out
        { 140, 0, 0, 0, 0, 0 },                     // the script moves the player to the bicycle and mounts it
        { 70, 0, 1, 0, 0, 0 },                      // ride south out of the shed door, into the zombie
        { 120, 1, 0, 0, 0, 0 },                     // east along the pavement
        { 30, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // get off (if still on)
        { 320, 0, 0, 0, 0, 0 },                     // the script takes a tyre off the wreck and siphons it
    };
#elif KX_BENCH == 10
    // Night (make BENCH=10): the game's own script moves the player and the clock; the player turns round (a
    // two-frame tap each time) so the flashlight's beam sweeps, outdoors and in the dark building.
    constexpr bench_segment bench_route[] = {
        { 150, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },                       // face east
        { 148, 0, 0, 0, 0, 0 },
        { 2, 0, -1, 0, 0, 0 },                      // north
        { 148, 0, 0, 0, 0, 0 },
        { 2, -1, -1, 0, 0, 0 },                     // north-west (towards the campfire)
        { 748, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },                       // in the dark building: east
        { 298, 0, 0, 0, 0, 0 },
        { 2, 0, 1, 0, 0, 0 },                       // south
        { 1798, 0, 0, 0, 0, 0 },
        // The Map tab (opened by the script at frame 3000, 300 frames before this): the cursor right three
        // chunks, a mark, down one, a mark, a look, then back to the game, where the arrow points at a mark.
        { 20, 1, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },
        { 60, 0, 0, 0, 0, 0 },
        { 2, 0, 1, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },
        { 300, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_b, 0, 0 },
        { 900, 0, 0, 0, 0, 0 },                     // in a battered car while zombies come (the game's script)
    };
#elif KX_BENCH == 12
    // Doors and locks (make BENCH=12, milestone 12.7) on the generated world: the game's script puts the survivor
    // at a house's door every 400 frames from frame 60 and hands out what each test needs; this presses A 40
    // frames in and holds it (forcing and picking need it held). Phases: open a door; a locked door rattles; its
    // key; a crowbar; a screwdriver; the Burglar's pick; unlocking from inside; a zombie breaking a door.
    #define KX_DOOR_PHASE { 40, 0, 0, 0, 0, 0 }, { 340, 0, 0, button_a, 0, 0 }, { 20, 0, 0, 0, 0, 0 }
    constexpr bench_segment bench_route[] = {
        { 60, 0, 0, 0, 0, 0 },
        KX_DOOR_PHASE, KX_DOOR_PHASE, KX_DOOR_PHASE, KX_DOOR_PHASE, KX_DOOR_PHASE, KX_DOOR_PHASE, KX_DOOR_PHASE,
        { 40, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // phase 8: close the door between it and the zombie
        { 800, 0, 0, 0, 0, 0 },
        // Stage 4 (from 3720): a safe's note read, the dial opened, a wrong code, the right one, the safe searched;
        // then (from 4120) a stash's note read and the stash searched; the Map tab at 4520.
        { 18, 0, 0, 0, 0, 0 },
        { 40, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // the dial
        { 38, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // a wrong code
        { 38, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // the right one: the search opens
        { 240, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_b, 0, 0 },
        { 36, 0, 0, 0, 0, 0 },
        { 40, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // the stash
        { 300, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_b, 0, 0 },
        { 56, 0, 0, 0, 0, 0 },
        // Stage 5 (from 4520): a tree chopped (A held 4 s with the axe); from 4920 the game's script builds a
        // wooden door and a log wall and has zombies break them; the Map tab at 5320.
        { 40, 0, 0, 0, 0, 0 },
        { 300, 0, 0, button_a, 0, 0 },
        { 60, 0, 0, 0, 0, 0 },
        { 400, 0, 0, 0, 0, 0 },
        { 200, 0, 0, 0, 0, 0 },
    };
    #undef KX_DOOR_PHASE
#elif KX_BENCH == 11
    // Guns (make BENCH=11): a shooting range on the test map. The game's own script hands over each gun and puts
    // zombies down the road to the east; this aims (R held) and fires. Pistol: walking (a wide cone), then
    // standing (it narrows), a reload (B), more shots. Then the shotgun, the hunting rifle, and the assault
    // rifle with A held, each from frame 560, 920 and 1280.
    constexpr bench_segment bench_route[] = {
        { 30, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },                       // face east
        { 60, 0, 0, 0, 0, 0 },
        { 60, 1, 0, button_r, button_a, 20 },       // pistol, walking at half speed
        { 270, 0, 0, button_r, button_a, 24 },      // standing still
        { 2, 0, 0, button_r | button_b, 0, 0 },     // reload
        { 70, 0, 0, button_r, 0, 0 },
        { 60, 0, 0, button_r, button_a, 20 },
        { 6, 0, 0, 0, 0, 0 },
        { 300, 0, 0, button_r, button_a, 45 },      // shotgun
        { 60, 0, 0, 0, 0, 0 },
        { 300, 0, 0, button_r, button_a, 45 },      // hunting rifle
        { 60, 0, 0, 0, 0, 0 },
        { 200, 0, 0, button_r | button_a, 0, 0 },   // assault rifle, A held
        { 2, 0, 0, button_r | button_b, 0, 0 },
        { 70, 0, 0, button_r, 0, 0 },
        { 60, 0, 0, 0, 0, 0 },
        // Active reload (from frame 1607, a pistol with 5 rounds loaded): A pressed 3 frames into the bar jams it;
        // A on the jammed gun clicks; B clears it; then a reload the game's script presses at the sweet spot.
        { 2, 0, 0, button_r | button_b, 0, 0 },
        { 3, 0, 0, button_r, 0, 0 },
        { 2, 0, 0, button_r | button_a, 0, 0 },
        { 30, 0, 0, button_r, button_a, 10 },
        { 2, 0, 0, button_r | button_b, 0, 0 },
        { 40, 0, 0, button_r, 0, 0 },
        { 2, 0, 0, button_r | button_b, 0, 0 },
        { 80, 0, 0, button_r, 0, 0 },
        { 2, 0, 0, button_r, button_a, 1 },         // the reloaded pistol fires
        { 40, 0, 0, 0, 0, 0 },
        // Throwables at night (milestone 12.6 stage 5; the game hands each one over): an alarm clock thrown east at
        // 1852, a bottle at 1910, a Molotov at the nearest of four zombies at 1960, then a walk west away from them.
        { 30, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },                       // face east
        { 10, 0, 0, button_r, 0, 0 },
        { 2, 0, 0, button_r | button_a, 0, 0 },     // the alarm clock
        { 46, 0, 0, 0, 0, 0 },
        { 10, 0, 0, button_r, 0, 0 },
        { 2, 0, 0, button_r | button_a, 0, 0 },     // a bottle
        { 38, 0, 0, 0, 0, 0 },
        { 10, 0, 0, button_r, 0, 0 },
        { 2, 0, 0, button_r | button_a, 0, 0 },     // the Molotov
        { 120, -1, 0, 0, 0, 0 },
        // The game stands the survivor in the fire at 2400 and back out at 2490; then the spear from 2510 against
        // two zombies, swung every 30 frames; then the Craft tab makes a Molotov and a spear.
        { 438, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },
        { 220, 0, 0, 0, button_a, 30 },
        { 718, 0, 0, 0, 0, 0 },
    };
#elif KX_BENCH == 16
    // Animal census (make BENCH=16, animals plan stage 7) on the fixed seed: the game's script moves the survivor and
    // the clock; the survivor stands still.
    constexpr bench_segment bench_route[] = {
        { 6150, 0, 0, 0, 0, 0 },
    };
#elif KX_BENCH == 15
    // Animals (make BENCH=15, animals plan) on the test map; the game's script places the survivor and the boar for
    // each phase: one walks off (30), one charges from close (200), one shot charges and is dodged (500), shot dead
    // (800); then it is butchered with a knife (1000): face it, hold A. Wolves at night (stage 3, from 1400): a pack
    // left alone, then come upon (1500); one shot dead (2000); a pack by a campfire (2300); a wolf killed with an axe
    // and butchered (2750).
    constexpr bench_segment bench_route[] = {
        { 1002, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },                       // 1002: face the carcass
        { 270, 0, 0, button_a, 0, 0 },              // 1004: hold A to butcher
        { 120, 0, 0, 0, 0, 0 },
        { 356, 0, 0, 0, 0, 0 },                     // 1394: the wolves come and circle
        { 120, 1, 0, 0, 0, 0 },                     // 1750: walking east while they circle (playtest 3)
        { 130, 0, 1, 0, 0, 0 },                     // 1870: then south
        { 752, 0, 0, 0, 0, 0 },                     // 2000: the wolves go
        { 2, 1, 0, 0, 0, 0 },                       // 2752: face the dead wolf
        { 270, 0, 0, button_a, 0, 0 },              // 2754: hold A to butcher
        { 80, 0, 0, 0, 0, 0 },
        // Farm animals (stage 4, from 3110): hens and goats at a farm; a hen caught, a goat milked on two days,
        // then killed (3398) and butchered; a hen killed; foraging by the yard.
        { 296, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },                       // 3400: face the dead goat
        { 270, 0, 0, button_a, 0, 0 },              // 3402: hold A to butcher
        { 130, 0, 0, 0, 0, 0 },
        // Crows (stage 5, from 3810): two corpses, crows come down; the survivor comes close at 3990.
        { 400, 0, 0, 0, 0, 0 },
    };
#elif KX_BENCH == 13
    // The wild (make BENCH=13, milestone 13) on the test map; the game's script places the survivor for each phase.
    // Swimming: east across the field's pond with a zombie behind, waiting mid-pond; drowning: exhausted in the
    // pond, then out; a heavy load at the east river (kept out), then without it (swims to the map's edge).
    constexpr bench_segment bench_route[] = {
        { 60, 0, 0, 0, 0, 0 },
        { 120, 1, 0, 0, 0, 0 },                     // 60: into the pond
        { 400, 0, 0, 0, 0, 0 },                     // 180: treading water while the zombie comes
        { 220, 1, 0, 0, 0, 0 },                     // 580: out on the far side
        { 20, 0, 0, 0, 0, 0 },
        { 600, 0, 0, 0, 0, 0 },                     // 820: in the pond, tiring fast, then drowning
        { 200, 1, 0, 0, 0, 0 },                     // 1420: swim out
        { 200, 0, 0, 0, 0, 0 },                     // 1620: at the river with 8 logs (Heavy load shows)
        { 200, 1, 0, 0, 0, 0 },                     // 1820: east: kept out of deep water
        { 200, 0, 0, 0, 0, 0 },                     // 2020: the logs dropped
        { 400, 1, 0, 0, 0, 0 },                     // 2220: east into the river, to the map's edge
        { 30, 0, 0, 0, 0, 0 },
        // Stage 4 (from 2650): a deer 6 tiles east, shot twice with the hunting rifle from a crouch; butchered with a
        // knife; a flock walked into; a rabbit shot with the pistol; another speared from a crouch.
        { 20, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_l, 0, 0 },                // 2670: crouch
        { 10, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },                       // 2682: face east
        { 40, 0, 0, button_r, 0, 0 },               // 2684: aim, the cone narrowing
        { 2, 0, 0, button_r | button_a, 0, 0 },     // 2724: fire
        { 44, 0, 0, button_r, 0, 0 },
        { 2, 0, 0, button_r | button_a, 0, 0 },     // 2770: fire again at the fleeing deer
        { 20, 0, 0, button_r, 0, 0 },
        { 86, 0, 0, 0, 0, 0 },                      // 2792: the fight lock runs out; the carcass (2869)
        { 2, 1, 0, 0, 0, 0 },                       // 2878: face it
        { 270, 0, 0, button_a, 0, 0 },              // 2880: hold A to butcher
        { 20, 0, 0, 0, 0, 0 },                      // 3150: the flock (3169)
        { 2, 0, 0, button_l, 0, 0 },                // 3170: stand up
        { 10, 0, 0, 0, 0, 0 },
        { 150, 1, 0, 0, 0, 0 },                     // 3182: walk east towards the birds
        { 60, 0, 0, 0, 0, 0 },                      // 3332: a rabbit and the pistol (3391)
        { 2, 1, 0, 0, 0, 0 },                       // 3392: face east
        { 30, 0, 0, button_r, 0, 0 },
        { 2, 0, 0, button_r | button_a, 0, 0 },     // 3424: fire
        { 40, 0, 0, button_r, 0, 0 },
        { 28, 0, 0, 0, 0, 0 },                      // 3466: the spear (3493)
        { 2, 0, 0, button_l, 0, 0 },                // 3494: crouch; the rabbit comes at 3505
        { 12, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },                       // 3508: face east
        { 2, 0, 0, button_a, 0, 0 },                // 3510: thrust
        { 60, 0, 0, 0, 0, 0 },
        { 30, 0, 0, 0, 0, 0 },
        // Stage 5 (from 3600): the game sets a snare and a box trap and checks them after nights (3619), a freezer
        // through the power shutoff (3640), and puts the survivor under a well (3660); A drinks from it.
        { 80, 0, 0, 0, 0, 0 },
        { 2, 0, -1, 0, 0, 0 },                      // 3680: face the well
        { 10, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // 3692: drink
        { 40, 0, 0, 0, 0, 0 },
        { 30, 0, 0, 0, 0, 0 },
        // Stage 6 (from 3764): the game sets the sandbox, makes the survivor a craving Smoker who lights up (3780)
        // and lights a campfire with the lighter (4000).
        { 256, 0, 0, 0, 0, 0 },
        // Stage 7 (from 4020): a flock 8 tiles east, scattered by a pistol shot; then (4098) Tired 4: the screen
        // dims and the survivor sleeps 4 hours where they stand (the bench's frame count stops while they sleep);
        // last, the Map tab with the sandbox settings (bench frame 4400).
        { 2, 1, 0, 0, 0, 0 },                       // 4020: face east
        { 24, 0, 0, button_r, 0, 0 },
        { 2, 0, 0, button_r | button_a, 0, 0 },     // 4046: fire
        { 50, 0, 0, 0, 0, 0 },
        { 900, 0, 0, 0, 0, 0 },                     // 4098: forced sleep, then the Map tab
    };
#elif KX_BENCH == 5
    // Crawler (make BENCH=5): face east and swing every 30 frames while the crawler comes in.
    constexpr bench_segment bench_route[] = {
        { 2, 1, 0, 0, 0, 0 },                       // face east
        { 20, 0, 0, 0, 0, 0 },
        { 400, 0, 0, 0, button_a, 30 },
        { 30, 0, 0, 0, 0, 0 },
    };
#elif KX_BENCH == 4
    // Restart (make BENCH=4): the game kills the player at frame 30; press A on the death screen to start the
    // next game. After the last forced death the route runs out and the bench reports.
    constexpr bench_segment bench_route[] = {
        { 200, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },
        { 40, 0, 0, 0, 0, 0 },
    };
#elif KX_BENCH == 3
    // Loot (make BENCH=3): start in the kitchen facing a cabinet. Search it, take everything, search the next
    // cabinet, then open the gear menu, use the first carried item and drop the second; at the end move an item.
    constexpr bench_segment bench_route[] = {
        { 20, 0, 0, 0, 0, 0 },
        { 3, 0, -1, 0, 0, 0 },                      // face the cabinet above
        { 2, 0, 0, button_a, 0, 0 },                // search
        { 200, 0, 0, 0, 0, 0 },                     // items pop in
        { 2, 0, 0, button_r, 0, 0 },                // take all
        { 10, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_b, 0, 0 },                // close
        { 32, 1, 0, 0, 0, 0 },                      // walk right two tiles
        { 3, 0, -1, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },
        { 200, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_r, 0, 0 },
        { 10, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_b, 0, 0 },
        { 10, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_start, 0, 0 },            // gear menu
        { 10, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },                       // cursor to carried slot 1 (water bottle)
        { 2, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // actions
        { 4, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // drink (closes the menu)
        { 80, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_start, 0, 0 },            // back to Gear
        { 4, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },                       // cursor to carried slot 2 (chips)
        { 2, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // actions
        { 4, 0, 0, 0, 0, 0 },
        { 2, -1, 0, 0, 0, 0 },                      // wrap to DROP
        { 2, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // drop them at your feet
        { 10, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_b, 0, 0 },
        { 10, 0, 0, 0, 0, 0 },
        { 3, 0, 1, 0, 0, 0 },                       // face down: the dropped chips are under the feet
        { 2, 0, 0, button_a, 0, 0 },                // pick them up again
        { 60, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_r, 0, 0 },
        { 2, 0, 0, button_b, 0, 0 },
        { 30, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_start, 0, 0 },            // Gear: move the chips (carried slot 1) two slots right
        { 4, 0, 0, 0, 0, 0 },
        { 2, -1, 0, 0, 0, 0 },                      // the cursor is still on the emptied slot 2
        { 2, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // actions
        { 4, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },                       // MOVE
        { 2, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },
        { 4, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },
        { 2, 0, 0, 0, 0, 0 },
        { 2, 1, 0, 0, 0, 0 },
        { 24, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_a, 0, 0 },                // put it there (swap)
        { 24, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_b, 0, 0 },
        { 20, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_start, 0, 0 },            // Start menu, then R1 twice: the Skills tab
        { 4, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_r, 0, 0 },
        { 4, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_r, 0, 0 },
        { 24, 0, 0, 0, 0, 0 },
        { 2, 0, 1, 0, 0, 0 },                       // down to Blunt (the bat swings in this bench)
        { 2, 0, 0, 0, 0, 0 },
        { 2, 0, 1, 0, 0, 0 },
        { 24, 0, 0, 0, 0, 0 },
        { 2, 0, 0, button_b, 0, 0 },
        { 20, 0, 0, 0, 0, 0 },
    };
#elif KX_BENCH == 2
    // Duel (make BENCH=2): one zombie east of the spawn. Walk towards it until it hears the footsteps, then hold
    // ground facing east and swing whenever it comes into reach; then stomp what is left on the ground.
    constexpr bench_segment bench_route[] = {
        { 30, 0, 0, 0, 0, 0 },
        { 30, 1, 0, 0, 0, 0 },                      // walk east (noise 2 tiles)
        { 900, 0, 0, 0, button_a, 26 },             // swing every 26 frames (bat cycle is 28); stomps crawlers
        { 160, 1, 0, 0, button_a, 20 },             // step onto anything lying down and stomp
        { 60, 0, 0, 0, 0, 0 },
    };
#else
    // Route from the spawn on the east-west road with a 40-zombie horde on the map (god mode): covers BG
    // streaming on both axes, every terrain, collisions, falls, then sustained melee with grabs and shoves.
    constexpr bench_segment bench_route[] = {
        { 30, 0, 0, 0, 0, 0 },
        { 150, 1, 0, button_b, 0, 0 },              // sprint east along the road
        { 90, 1, -1, 0, 0, 0 },                     // walk north-east into the forest edge
        { 120, 0, -1, button_b, 0, 0 },             // sprint north through the trees
        { 240, -1, 0, button_b, 0, 0 },             // sprint west back over the vertical road
        { 6, 0, 1, 0, 0, 0 },                       // double-tap down...
        { 6, 0, 0, 0, 0, 0 },
        { 188, 0, 1, 0, 0, 0 },                     // ...and jog south over the road
        { 160, -1, 1, button_b, 0, 0 },             // sprint south-west through the field
        { 2, 0, 0, button_l, 0, 0 },                // crouch
        { 120, 1, 0, 0, 0, 0 },                     // crouch-walk east to the pond
        { 2, 0, 0, button_l, 0, 0 },                // stand
        { 180, 1, -1, button_b, 0, 0 },             // sprint north-east back towards the crossing
        { 300, 0, 0, 0, button_a, 12 },             // stand and fight: repeated swings, mash out of grabs
        { 60, 0, 0, button_a, 0, 0 },               // charged swing
        { 4, 0, 0, 0, 0, 0 },
        { 120, 0, 0, 0, button_b, 20 },             // shoves
        { 2, 0, 0, button_select, 0, 0 },           // swap weapon
        { 200, -1, 0, 0, button_a, 10 },            // advance west swinging
        { 60, 0, 0, 0, 0, 0 },
    };
#endif
}

input_frame input_source::read()
{
    if(_finished)
    {
        return input_frame();
    }

    const bench_segment& segment = bench_route[_segment];
    unsigned buttons = segment.held;

    if(segment.mash_period > 0 && (_segment_frame % segment.mash_period) < (segment.mash_period + 3) / 4)
    {
        buttons |= segment.mashed;
    }

    input_frame result = make_frame(segment.dx, segment.dy, buttons, _previous_buttons, _previous_dx, _previous_dy);
    _previous_buttons = buttons;
    _previous_dx = segment.dx;
    _previous_dy = segment.dy;

    ++_route_frame;

    if(++_segment_frame >= segment.frames)
    {
        _segment_frame = 0;

        if(++_segment >= int(sizeof(bench_route) / sizeof(bench_segment)))
        {
            _finished = true;
        }
    }

    return result;
}

#else

input_frame input_source::read()
{
    unsigned buttons = (bn::keypad::a_held() ? button_a : 0) | (bn::keypad::b_held() ? button_b : 0) |
                       (bn::keypad::l_held() ? button_l : 0) | (bn::keypad::select_held() ? button_select : 0) |
                       (bn::keypad::start_held() ? button_start : 0) | (bn::keypad::r_held() ? button_r : 0);
    int dx = int(bn::keypad::right_held()) - int(bn::keypad::left_held());
    int dy = int(bn::keypad::down_held()) - int(bn::keypad::up_held());

    // Playtest 2: a button still down from the screen before (Start on the title or the character screen) is not a
    // new press: it used to open the Start menu as the game began.
    if(! _primed)
    {
        _primed = true;
        _previous_buttons = buttons;
        _previous_dx = dx;
        _previous_dy = dy;
    }

    input_frame result = make_frame(dx, dy, buttons, _previous_buttons, _previous_dx, _previous_dy);
    _previous_buttons = buttons;
    _previous_dx = dx;
    _previous_dy = dy;
    return result;
}

#endif

}
