"""Milestone 14 world graphics: furniture for the new buildings, the set pieces (a crashed bus, a police barricade,
a military checkpoint, a graveyard) and the wild's hunter stand. Drawn now for mockup 14; they join the tileset once
it has room for 256 graphics (stage 2), and the templates that use them come in stages 3 and 4.

Each graphic is 16 rows of 16 letters (KEYS below), in the world palette (world.WORLD_PALETTE)."""
from world import (W_TRANS, W_GRASS, W_GRASS_L, W_CANOPY, W_ROAD, W_LINE, W_PAVE, W_BRICK, W_MORTAR, W_WOOD,
                   W_WATER, W_WATER_L, W_DEEP, W_FOREST, W_DARK, W_PAVE_D)

KEYS = {'.': W_TRANS, 'o': W_DARK, 'w': W_WOOD, 'm': W_MORTAR, 'g': W_PAVE, 'G': W_PAVE_D, 'r': W_ROAD,
        'd': W_DEEP, 'b': W_WATER, 'l': W_WATER_L, 'y': W_LINE, 'f': W_FOREST, 'n': W_GRASS, 'N': W_GRASS_L,
        'c': W_CANOPY, 'k': W_BRICK}


def art(rows):
    assert len(rows) == 16, len(rows)
    for row in rows:
        assert len(row) == 16, row
    return [[KEYS[c] for c in row] for row in rows]


# ---------------------------------------------------------------------------------------------------------------------
# Furniture (solid, filling most of the tile, in the 3/4 view of the existing pieces)
# ---------------------------------------------------------------------------------------------------------------------

PEW = art([
    '................',
    '................',
    'oooooooooooooooo',
    'owwwwwwwwwwwwwwo',
    'ommmmmmmmmmmmmmo',
    'oooooooooooooooo',
    '................',
    'oooooooooooooooo',
    'owwwwwwwwwwwwwwo',
    'owwwwwwwwwwwwwwo',
    'ommmmmmmmmmmmmmo',
    'oooooooooooooooo',
    'om............mo',
    'om............mo',
    'oo............oo',
    '................',
])

ALTAR = art([
    '.......oo.......',
    '......oyyo......',
    '.......yy.......',
    '.....yyyyyy.....',
    '.......yy.......',
    'oooooooyyooooooo',
    'oggggggggggggggo',
    'oggggggggggggggo',
    'oGGGGGGGGGGGGGGo',
    'ogggggkkkkgggggo',
    'ogggggkyykgggggo',
    'ogggggkkkkgggggo',
    'ogggggggggggggGo',
    'oGGGGGGGGGGGGGGo',
    'oooooooooooooooo',
    '................',
])

SCHOOL_DESK = art([
    '................',
    'oooooooooooooo..',
    'owwwwwwwwwwwwo..',
    'owwwgwwwwwwwwo..',
    'owwwgggwwwwwwo..',
    'ommmmmmmmmmmmo..',
    'oo..........oo..',
    'om..........mo..',
    'om..ooooooo.mo..',
    'oo..oGGGGGo.oo..',
    '....oGGGGGo.....',
    '....ooooooo.....',
    '....om...mo.....',
    '....om...mo.....',
    '....oo...oo.....',
    '................',
])

BLACKBOARD = art([
    'oooooooooooooooo',
    'owwwwwwwwwwwwwwo',
    'owccccccccccccwo',
    'owcgcccgccgcccwo',
    'owccgcgccgcgccwo',
    'owcccgcccccccgwo',
    'owccccccgggcccwo',
    'owccccccccccccwo',
    'owwwwwwwwwwwwwwo',
    'omggmmmmmmmmmmmo',
    'oooooooooooooooo',
    '..om........mo..',
    '..om........mo..',
    '..om........mo..',
    '..oo........oo..',
    '................',
])

BUNK_BED = art([
    'oooooooooooooooo',
    'oGggggggggggggGo',
    'oGbbbbbbbbbbbbGo',
    'oGbbbbbbbbbbbbGo',
    'oGddddddddddddGo',
    'oGGGGGGGGGGGGGGo',
    'oG............Go',
    'oGggggggggggggGo',
    'oGbbbbbbbbbbbbGo',
    'oGbbbbbbbbbbbbGo',
    'oGbbbbbbbbbbbbGo',
    'oGddddddddddddGo',
    'oGGGGGGGGGGGGGGo',
    'oG............Go',
    'oo............oo',
    '................',
])

RACK = art([
    'oooooooooooooooo',
    'oGGGGGGGGGGGGGGo',
    'oGwwwwo.wwwww.Go',
    'oGwmmwo.wmmmw.Go',
    'oGGGGGGGGGGGGGGo',
    'oG.wwwwwo.www.Go',
    'oG.wmmmwo.wmw.Go',
    'oGGGGGGGGGGGGGGo',
    'oGwwww.wwwwwo.Go',
    'oGwmmw.wmmmwo.Go',
    'oGGGGGGGGGGGGGGo',
    'oG............Go',
    'oG............Go',
    'oG............Go',
    'oo............oo',
    '................',
])

GUN_RACK = art([
    'oooooooooooooooo',
    'owwwwwwwwwwwwwwo',
    'owoowoowoowoowwo',
    'owrowrowrowrowwo',
    'owrowrowrowrowwo',
    'owrowrowrowrowwo',
    'owrowrowrowrowwo',
    'owmowmowmowmowwo',
    'owmowmowmowmowwo',
    'owwwwwwwwwwwwwwo',
    'ommmmmmmmmmmmmmo',
    'oooooooooooooooo',
    'om............mo',
    'om............mo',
    'oo............oo',
    '................',
])

TABLE = art([
    '.....oooooo.....',
    '.....owwwwo.....',
    '.....ommmmo.....',
    '.oooooooooooooo.',
    '.owwwwwwwwwwwwo.',
    '.owwgwwwwwwwwwo.',
    '.owwwwwwwwwgwwo.',
    '.owwwwwwwwwwwwo.',
    '.ommmmmmmmmmmmo.',
    '.oooooooooooooo.',
    '.om..........mo.',
    '.om..oooooo..mo.',
    '.oo..owwwwo..oo.',
    '.....ommmmo.....',
    '.....oo..oo.....',
    '................',
])

FILING_CABINET = art([
    'oooooooooooooo..',
    'oGGGGGGGGGGGGo..',
    'ogggggggggggGo..',
    'ooooooooooooGo..',
    'oggggoogggggGo..',
    'ogggggggggggGo..',
    'ooooooooooooGo..',
    'oggggoogggggGo..',
    'ogggggggggggGo..',
    'ooooooooooooGo..',
    'oggggoogggggGo..',
    'ogggggggggggGo..',
    'ooooooooooooGo..',
    'oGGGGGGGGGGGGo..',
    'oooooooooooooo..',
    '................',
])

HAY_BALE = art([
    '................',
    '..oooooooooooo..',
    '.oyyyyyyyyyyyyo.',
    'oyywyyyyyyywyyyo',
    'oyyyyyywyyyyyyyo',
    'owwwwwwwwwwwwwwo',
    'oyyyyyyyyyyyyyyo',
    'oyywyyyyyywyyyyo',
    'omwmmmmmmmmmmwmo',
    'oyyyyyyyyyyyyyyo',
    'oyyyywyyyyyyyyyo',
    'owwwwwwwwwwwwwwo',
    'oyyyyyyyywyyyyyo',
    '.oyyyyyyyyyyyyo.',
    '..oooooooooooo..',
    '................',
])

STOVE = art([
    'oooooooooooooooo',
    'oGGGGGGGGGGGGGGo',
    'oGooGGooGGooGGGo',
    'oGorGGorGGorGGGo',
    'oGooGGooGGooGGGo',
    'oGGGGGGGGGGGGGGo',
    'oooooooooooooooo',
    'ogggggggggggggGo',
    'ogoooooooooooggo',
    'ogorrrrrrrrrogGo',
    'ogorrrrrrrrrogGo',
    'ogoooooooooooggo',
    'ogggggggggggggGo',
    'oyogggggggggoyGo',
    'oooooooooooooooo',
    '................',
])

BATH = art([
    'oooooooooooooooo',
    'oggggggggggggggo',
    'oggooooooooooggo',
    'oggoblllbbbbogGo',
    'oggobbbbbbbbogGo',
    'oggobbbbblbbogGo',
    'oggobbbbbbbbogGo',
    'oggobbbbbbbbogGo',
    'oggobbblbbbbogGo',
    'oggobbbbbbbbogGo',
    'oggoddbbbbbdogGo',
    'oggooooooooooggo',
    'oggggggggggggggo',
    'oGGGGGGGGGGGGGGo',
    'oooooooooooooooo',
    '................',
])

CHECKOUT = art([
    'oooooooooooooooo',
    'orrrrrrrrrrrrrro',
    'orGrrrrGrrrrGrro',
    'orrrrrrrrrrrrrro',
    'ooooooooooooooGo',
    'oggggggggoooogGo',
    'oggggggggoyyogGo',
    'oggggggggoooogGo',
    'oooooooooooooooo',
    'okkkkkkkkkkkkkko',
    'okkkkkkkkkkkkkko',
    'ommmmmmmmmmmmmmo',
    'okkkkkkkkkkkkkko',
    'okkkkkkkkkkkkkko',
    'oooooooooooooooo',
    '................',
])

VENDING = art([
    'oooooooooooooo..',
    'okkkkkkkkkkkko..',
    'okoooooooooGko..',
    'okolbyolbyoGko..',
    'okolbyolbyoGko..',
    'okoooooooooGko..',
    'okolbyolbyoGko..',
    'okolbyolbyoGko..',
    'okoooooooooGko..',
    'okkkkkkkkkkyko..',
    'okooooooooooko..',
    'okoGGGGGGGGoko..',
    'okooooooooooko..',
    'okkkkkkkkkkkko..',
    'oooooooooooooo..',
    '................',
])

TOILET = art([
    '................',
    '....oooooooo....',
    '....oggggggo....',
    '....oggggggo....',
    '....oGGGGGGo....',
    '...oooooooooo...',
    '...oggggggggo...',
    '..oggoooooooggo.',
    '..oggobbbbbbogo.',
    '..oggobbbbbbogo.',
    '..oggoobbbbooggo'[:16],
    '...oggooooooggo.',
    '....oggggggggo..',
    '.....oGGGGGGo...',
    '......oooooo....',
    '................',
])

# ---------------------------------------------------------------------------------------------------------------------
# Set pieces
# ---------------------------------------------------------------------------------------------------------------------

def bus_parts():
    """A crashed coach lying across the road, 4 tiles long and 2 deep: BUS_<column><row>."""
    # 64x32 px drawn as one picture, then cut into 8 graphics. Body yellow-cream with a red stripe, windows dark
    # and smashed, the front (right) crumpled.
    width, height = 64, 32
    pic = [['.'] * width for _ in range(height)]

    def rect(x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if 0 <= x < width and 0 <= y < height:
                    pic[y][x] = c

    rect(1, 3, 61, 29, 'o')
    rect(2, 4, 60, 9, 'g')          # roof
    rect(2, 10, 60, 27, 'y')        # body
    rect(2, 18, 60, 19, 'k')        # stripe
    for x0 in range(6, 54, 8):      # windows
        rect(x0, 12, x0 + 5, 16, 'o')
        rect(x0 + 1, 13, x0 + 2, 14, 'b')
    for x0, y0 in ((15, 13), (31, 14), (44, 12)):   # smashed panes
        rect(x0, y0, x0 + 1, y0, 'l')
    rect(55, 11, 60, 26, 'G')       # crumpled front
    for x, y in ((56, 12), (58, 15), (57, 20), (59, 23), (56, 25)):
        rect(x, y, x, y + 1, 'o')
    rect(3, 21, 8, 26, 'o')         # door
    rect(10, 25, 15, 29, 'r')       # wheels
    rect(44, 25, 49, 29, 'r')
    rect(62, 8, 63, 22, 'r')        # skid marks off the front
    rect(0, 30, 63, 31, '.')
    parts = []
    for row in range(2):
        for column in range(4):
            rows = [''.join(pic[row * 16 + y][column * 16:(column + 1) * 16]) for y in range(16)]
            parts.append(('BUS_%d%d' % (column, row), art(rows)))
    return parts


ROADBLOCK = art([
    '................',
    '................',
    '................',
    'oooooooooooooooo',
    'okkggkkggkkggkko',
    'okkggkkggkkggkko',
    'oooooooooooooooo',
    '.om..........mo.',
    '.om..........mo.',
    'oooooooooooooooo',
    'oggkkggkkggkkggo',
    'oggkkggkkggkkggo',
    'oooooooooooooooo',
    '.om..........mo.',
    '.oo..........oo.',
    '................',
])

SANDBAGS = art([
    '................',
    '................',
    '..oooo.oooo.ooo.',
    '.owwwwowwwwowwwo',
    '.ommmmommmmommmo',
    'ooooooooooooooo.',
    'owwwwowwwwowwwwo',
    'ommmmommmmommmmo',
    'oooooooooooooooo',
    'owwwwowwwwowwwwo',
    'owwwwowwwwowwwwo',
    'ommmmommmmommmmo',
    'oooooooooooooooo',
    '................',
    '................',
    '................',
])

BARRIER = art([
    '................',
    '................',
    '................',
    '................',
    'oooooooooooooooo',
    'okkkggggkkkkgggo',
    'okkkggggkkkkgggo',
    'oooooooooooooooo',
    '.........ooooo..',
    '.........oGGGo..',
    '.........ogggo..',
    '.........ogggo..',
    '.........ogggo..',
    '.........oGGGo..',
    '.........ooooo..',
    '................',
])

WATCHTOWER = art([
    'oooooooooooooooo',
    'oGGGGGGGGGGGGGGo',
    'oGwwwwwwwwwwwwGo',
    'oGwmmmmmmmmmmwGo',
    'oGwmoooooooomwGo',
    'oGwmoGGGGGGomwGo',
    'oGwmoGGGGGGomwGo',
    'oGwmoooooooomwGo',
    'oGwmmmmmmmmmmwGo',
    'oGwwwwwwwwwwwwGo',
    'oGGGGGGGGGGGGGGo',
    'oooooooooooooooo',
    '.om.om....om.om.',
    '.om..om..om..om.',
    '.oo...oooo...oo.',
    '................',
])

GRAVE = art([
    '................',
    '................',
    '.....oooooo.....',
    '....oggggggo....',
    '...oggGggGggo...',
    '...ogggGGgggo...',
    '...oggGggGggo...',
    '...oggggggggo...',
    '...oGGGGGGGGo...',
    '..oooooooooooo..',
    '..ommmmmmmmmmo..',
    '..ommNmmmmNmmo..',
    '..ommmmNmmmmmo..',
    '..oooooooooooo..',
    '................',
    '................',
])

# ---------------------------------------------------------------------------------------------------------------------
# The wild
# ---------------------------------------------------------------------------------------------------------------------

HUNTER_STAND = art([
    '..oooooooooooo..',
    '..owwwwwwwwwwo..',
    '..owmmmmmmmmwo..',
    '..owmoooooomwo..',
    '..owmogGGgomwo..',
    '..owmooooooowo..',
    '..owwwwwwwwwwo..',
    '..oooooooooooo..',
    '..om.om..om.mo..',
    '..om.omwwom.mo..',
    '..om.om..om.mo..',
    '..om.omwwom.mo..',
    '..om.om..om.mo..',
    '..om.omwwom.mo..',
    '..oo.oo..oo.oo..',
    '................',
])

FURNITURE_14 = [
    ('PEW', PEW), ('ALTAR', ALTAR), ('SCHOOL_DESK', SCHOOL_DESK), ('BLACKBOARD', BLACKBOARD), ('BUNK_BED', BUNK_BED),
    ('RACK', RACK), ('GUN_RACK', GUN_RACK), ('TABLE', TABLE), ('FILING_CABINET', FILING_CABINET),
    ('HAY_BALE', HAY_BALE), ('STOVE', STOVE), ('BATH', BATH), ('CHECKOUT', CHECKOUT), ('VENDING', VENDING),
    ('TOILET', TOILET),
]
SET_PIECES_14 = bus_parts() + [('ROADBLOCK', ROADBLOCK), ('SANDBAGS', SANDBAGS), ('BARRIER', BARRIER),
                               ('WATCHTOWER', WATCHTOWER), ('GRAVE', GRAVE)]
WILD_14 = [('HUNTER_STAND', HUNTER_STAND)]
WORLD_GFX_14 = FURNITURE_14 + SET_PIECES_14 + WILD_14
