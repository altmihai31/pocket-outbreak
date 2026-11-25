#ifndef KX_TEST_MAP_H
#define KX_TEST_MAP_H

namespace kx
{

// Hand-made milestone 1 test map, 64x48 tiles of 16 px. One character per tile:
//   .  grass            #  road              =  road, horizontal centre line   !  road, vertical centre line
//   _  pavement         o  wood floor        f  forest floor                   m  mud
//   ~  shallow water    W  deep water*       B  brick wall*                    F  fence*
//   T  tree* on forest floor                 t  tree* on grass
// Furniture containers (solid, on wood floor):
//   k  kitchen cabinet    R  fridge    D  wardrobe (bedroom)    h  bathroom cabinet    G  tool shelf (garage)
//   Y  bed (sleep)
// Doors (milestone 12.7), on wood floor:
//   d  a door, open (so the other benches walk through): in a front wall, or a side wall when walls are above
//      and below
// (* = solid)
// Layout: road crossing in the middle; NW house with double front door, back door, inner doorway, a
// 1-tile corridor, a kitchen, a dresser, a wardrobe and a bathroom cabinet; shed with one door and two
// tool shelves; NE forest with mud and a cleared path; SW fenced field with 1-tile gaps and a pond; SE mud
// strip, lone trees and puddles; river with a shallow ford along the east edge.
constexpr int test_map_width = 64;
constexpr unsigned test_map_seed = 0x4B4E4F58;     // "KNOX"; loot rolls derive from it
constexpr int test_map_height = 48;

constexpr const char* test_map_rows[test_map_height] = {
    "..........................._#!#_..ffffffffffffffffffffff~~~WWWWW",
    "..........................._###_..fTfTffTfTfTTffTffffffT~~~WWWWW",
    "..t........................_#!#_..ffTTffTffffTTfffffffff~~~WWWWW",
    ".......................t..._###_..fffffffTffTfTfffffffff~~~WWWWW",
    "....BBBdBBBBBBBBBBBBB......_#!#_..ffffTfffffffTfTTTfTfff~~~WWWWW",
    "....BkkokkRooBooooooB......_###_..TffffffffffTTfffffTfff~~~WWWWW",
    "....BooooooooBooBoBoB......_#!#_..fffffTffmmmmmmTTfTfTTT~~~WWWWW",
    "....BooooooooBooBoBoB......_###_..TfTffTffmmmmmmTTfffTTf~~~WWWWW",
    "....BooooooooBhoBoBoB......_#!#_..fTfTffffmmmmmmffffffff~~~WWWWW",
    "....BoooooooodooBoBoBBBBBB._###_..fffTTfffmmmmmmffTfffff~~~WWWWW",
    "....BooooooooBooBoBoBBGoGB._#!#_..ffTfffffmmmmmmfffTTTff~~~WWWWW",
    "....BooooooooBooBoBoBBoooB._###_..TfffffTTffffffffffffff~~~WWWWW",
    "....BooooooooBooBoBoBBoooB._#!#_..ffffffffffffffffffffff~~~WWWWW",
    "....BDoYoooooBooBoBoBBoooB._###_..ffTfffffffffffffffffff~~~WWWWW",
    "....BooooooooBDoooooBBBdBB._#!#_..fffffTTfTfTfffTffTffff~~~WWWWW",
    "....BBBBBBddBBBBBBBBB......_###_..ffffTfffTffTffTfffTfff~~~WWWWW",
    "..........................._#!#_..TfTffTffffTfffTTffTfff~~~WWWWW",
    "..t.....................t.._###_..ffffffffffffffffffffff~~~WWWWW",
    "..........................._#!#_........................~~~~~~~~",
    "____________________________###_________________________~~~~~~~~",
    "########################################################~~~~~~~~",
    "=#=#=#=#=#=#=#=#=#=#=#=#=#=#####=#=#=#=#=#=#=#=#=#=#=#=#~~~~~~~~",
    "########################################################~~~~~~~~",
    "____________________________###_________________________~~~~~~~~",
    "..........................._#!#_........................~~~~~~~~",
    "..........................._###_........................~~~~~~~~",
    "..FFFFFFFFFF.FFFFFFFFFFFF.._#!#_........................~~~~~~~~",
    "..F..........F..........F.._###_........................~~~WWWWW",
    "..F..........F..........F.._#!#_....mmmmmmmmmmmmmmmmm...~~~WWWWW",
    "..F..........F..........F.._###_....mmmmmmmmmmmmmmmmm...~~~WWWWW",
    "..F..........F............._#!#_....mmmmmmmmmmmmmmmmm...~~~WWWWW",
    "..F..........F..........F.._###_........................~~~WWWWW",
    "..F..........F..........F.._#!#_........................~~~WWWWW",
    "..F..........F..~~~~~~~.F.._###_........................~~~WWWWW",
    "..F..........F..~~~~~~~.F.._#!#_........................~~~WWWWW",
    ".............F..~~WWW~~.F.._###_........t...t...........~~~WWWWW",
    "..F..........F..~~WWW~~.F.._#!#_..........t.............~~~WWWWW",
    "..F..........F..~~WWW~~.F.._###_........................~~~WWWWW",
    "..F.............~~WWW~~.F.._#!#_..............t.........~~~WWWWW",
    "..F..........F..~~WWW~~.F.._###_........................~~~WWWWW",
    "..F..........F..~~~~~~~.F.._#!#_......t.................~~~WWWWW",
    "..F..........F..~~~~~~~.F.._###_........................~~~WWWWW",
    "..F..........F..........F.._#!#_................t.......~~~WWWWW",
    "..F..........F..........F.._###_..~~~~~~~...............~~~WWWWW",
    "..FFFF.FFFFFFFFFFFFFFFFFF.._#!#_..~~~~~~~...............~~~WWWWW",
    "..........................._###_..~~~~~~~...............~~~WWWWW",
    "..........................._#!#_..~~~~~~~...............~~~WWWWW",
    "..........................._###_........................~~~WWWWW",
};

}

#endif
