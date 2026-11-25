"""Milestone 14 mockup: content. Drawn at 240x160 from the game's world graphics (and the new ones in
tools/assets/world14.py), sprites, 5 px font and palette: nine moments in a 3x3 sheet, saved at 3x.

    python tools/mockup_towns.py        -> mockups/14-towns.png

1. The supermarket from inside (the roof cut away): shelves, freezers, the checkouts, a vending machine.
2. A school: two classrooms with blackboards and desks, the corridor, the lockers.
3. A motel from the street: three rooms in a row, roofs on, the parking in front.
4. The village church (a set piece): pews, the altar, the graveyard behind.
5. The town's set piece: a crashed coach across the main road, bodies, zombies.
6. The big town's set piece: a police barricade, road blocks and sandbags across the road.
7. The military base's set piece: the checkpoint, its barrier, sandbags and watchtower.
8. The wild: a hunter stand in the forest, a campsite (a tent, a cold campfire, a crate).
9. The new furniture, and survivors in some of the new clothes.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'assets'))

from mockup_character import ACCENT, QUIET, TEXT, Screen  # noqa: E402
from mockup_doors import GRIDS, caption, hud, sprite  # noqa: E402
import buildings  # noqa: E402
import characters  # noqa: E402
import world  # noqa: E402
import world14  # noqa: E402

ROOT = os.path.join(HERE, '..', 'mockups')
GRAPHICS = os.path.join(HERE, '..', 'graphics')
TILE = 16
PLAYER_FRAMES = {'down': 0, 'up': 6, 'side': 12}
ZOMBIE_FRAMES = {'down': 0, 'up': 5, 'side': 10}

GRIDS.update(dict(world14.WORLD_GFX_14))
PALETTE = world.WORLD_PALETTE
DECAL_PALETTE = world.WORLD_PALETTE_DECALS
DECAL_GRIDS = {name: grid for name, grid, bank in world.world_graphics()[0] if bank == 1}


def paint_palette(kind, variant):
    """The palette a building's walls, roof or floor take (a colour variant from world.py)."""
    swaps = {'WALL': world.WALL_VARIANTS, 'ROOF': world.ROOF_VARIANTS, 'FLOOR': world.FLOOR_VARIANTS}[kind]
    return world.variant_bank(swaps[variant]) if variant is not None else PALETTE


def put(s, name, tx, ty, palette=None):
    if name in DECAL_GRIDS:
        grid, colours = DECAL_GRIDS[name], DECAL_PALETTE
    else:
        grid, colours = GRIDS[name], palette or PALETTE
    for y in range(TILE):
        for x in range(TILE):
            value = grid[y][x]
            if value:
                px, py = tx * TILE + x, ty * TILE + y
                if 0 <= px < 240 and 0 <= py < 160:
                    s.pixels[px, py] = colours[value]


def fill(s, name, tx0, ty0, tx1, ty1):
    for ty in range(ty0, ty1):
        for tx in range(tx0, tx1):
            put(s, name, tx, ty)


def road_across(s, ty, pavement=True):
    """A road running across the screen on rows ty..ty+2, with a centre line, pavements above and below."""
    for tx in range(15):
        if pavement:
            put(s, 'PAVEMENT', tx, ty - 1)
            put(s, 'PAVEMENT', tx, ty + 3)
        put(s, 'ROAD', tx, ty)
        put(s, 'ROAD_LINE_H' if tx % 2 else 'ROAD', tx, ty + 1)
        put(s, 'ROAD', tx, ty + 2)


def building(s, template, left, top, inside, wall=1, roof=0, floor=0):
    """A template drawn at (left, top) in tiles: ground, walls and furniture, and the roof unless inside."""
    name, floor_letter, roof_colour, rows = template
    rows = buildings.with_front_step(rows)
    names = {}

    class Ids(dict):
        def __missing__(self, key):
            names[len(names) + 1] = key
            self[key] = len(names)
            return self[key]

    ids = Ids()
    ground, walls, roofs = buildings.tile_graphics(rows, floor_letter, roof_colour, ids, name)
    width = len(rows[0])
    wall_palette = paint_palette('WALL', wall)
    floor_palette = paint_palette('FLOOR', floor)
    roof_palette = paint_palette('ROOF', roof)
    for y in range(len(rows)):
        for x in range(width):
            index = y * width + x
            tx, ty = left + x, top + y
            if ground[index]:
                gname = names[ground[index]]
                put(s, gname, tx, ty, floor_palette if gname in world.PAINTED['FLOOR'] else None)
            if walls[index]:
                wname = names[walls[index]]
                put(s, wname, tx, ty, wall_palette if wname in world.PAINTED['WALL'] else None)
            if not inside and roofs[index]:
                put(s, names[roofs[index]], tx, ty, roof_palette)


def template(name):
    return next(t for t in buildings.TEMPLATES_14 + buildings.TEMPLATES if t[0] == name)


def tree(s, kind, tx, ty):
    trunk = {'OAK': 'TREE_TRUNK', 'PINE': 'PINE_TRUNK', 'BIRCH': 'BIRCH_TRUNK'}[kind]
    lower = {'OAK': 'CANOPY_BOTTOM', 'PINE': 'PINE_BOTTOM', 'BIRCH': 'BIRCH_BOTTOM'}[kind]
    upper = {'OAK': 'CANOPY_TOP', 'PINE': 'PINE_TOP', 'BIRCH': 'BIRCH_TOP'}[kind]
    put(s, trunk, tx, ty)
    put(s, lower, tx, ty)
    put(s, upper, tx, ty - 1)


def label(s, x, y, text, colour=TEXT):
    width = s.width(text) + 6
    s.panel(x, y, width, 9)
    s.text(x + 3, y + 2, text, colour)


# Clothes: the player sprite's palette areas recoloured (characters.py), as the game does.
OUTFITS = {
    'POLICE': {characters.P_TORSO: '2e3f66', characters.P_TORSO_SHADE: '1f2b47', characters.P_TORSO_LIGHT: '44598c',
               characters.P_LEGS: '1e2433', characters.P_HIPS: '1e2433', characters.P_FEET: '0b0e14',
               characters.P_HAT: '1e2433'},
    'ARMY': {characters.P_TORSO: '5a6640', characters.P_TORSO_SHADE: '3e4a2a', characters.P_TORSO_LIGHT: '76824e',
             characters.P_LEGS: '4a5236', characters.P_HIPS: '4a5236', characters.P_FEET: '2a2418',
             characters.P_HAT: '5a6640', characters.P_BAG: '4a5236'},
    'FIREFIGHTER': {characters.P_TORSO: 'b8962e', characters.P_TORSO_SHADE: '7a6020',
                    characters.P_TORSO_LIGHT: 'e8c547', characters.P_LEGS: '3a3d42', characters.P_HIPS: '3a3d42',
                    characters.P_FEET: '1e2229', characters.P_HAT: 'd0453a'},
    'FARMER': {characters.P_TORSO: 'a8402e', characters.P_TORSO_SHADE: '74291e', characters.P_TORSO_LIGHT: 'c8604a',
               characters.P_LEGS: '3d5a80', characters.P_HIPS: '3d5a80', characters.P_FEET: '3e5a2a',
               characters.P_HAT: 'c8a060'},
}


def survivor(s, outfit, frame, feet_x, feet_y):
    image = Image.open(os.path.join(GRAPHICS, 'player.bmp'))
    colours = image.getpalette()
    swaps = {index: world.hexc(value) for index, value in OUTFITS[outfit].items()}
    tile = image.crop((0, frame * 16, 16, frame * 16 + 16))
    for y in range(16):
        for x in range(16):
            value = tile.getpixel((x, y))
            if value:
                rgb = swaps.get(value, tuple(colours[value * 3:value * 3 + 3]))
                s.pixels[feet_x - 8 + x, feet_y - 16 + y] = rgb


# ---------------------------------------------------------------------------------------------------------------------
# Panels
# ---------------------------------------------------------------------------------------------------------------------

def panel_supermarket():
    s = Screen()
    fill(s, 'PAVEMENT', 0, 0, 15, 10)
    building(s, template('SUPERMARKET'), 1, -1, True, wall=0, floor=1)
    hud(s, '9 JUL ` 11:05')
    sprite(s, 'player', PLAYER_FRAMES['up'], 6 * TILE + 8, 6 * TILE + 12)
    sprite(s, 'zombie', ZOMBIE_FRAMES['side'], 10 * TILE + 8, 3 * TILE + 12, mirror=True)
    return s


def panel_school():
    s = Screen()
    fill(s, 'GRASS_A', 0, 0, 15, 10)
    building(s, template('SCHOOL'), 1, 0, True, wall=2, floor=0)
    road_across(s, 9, pavement=False)
    hud(s, '9 JUL ` 14:30')
    sprite(s, 'player', PLAYER_FRAMES['down'], 5 * TILE + 8, 6 * TILE + 12)
    sprite(s, 'zombie', ZOMBIE_FRAMES['down'], 3 * TILE + 8, 3 * TILE + 12)
    return s


def panel_motel():
    s = Screen()
    fill(s, 'GRASS_A', 0, 0, 15, 10)
    building(s, template('MOTEL'), 1, 1, False, wall=1, roof=1)
    road_across(s, 8)
    put(s, 'PUMP', 13, 6)
    hud(s, '9 JUL ` 19:40')
    sprite(s, 'player', PLAYER_FRAMES['up'], 3 * TILE + 8, 7 * TILE + 12)
    return s


def panel_church():
    s = Screen()
    fill(s, 'GRASS_A', 0, 0, 15, 10)
    # The graveyard behind (top), the nave with its pews; the doors are just below the screen.
    building(s, template('CHURCH'), 1, 0, True, wall=0, floor=0)
    tree(s, 'OAK', 14, 3)
    tree(s, 'BIRCH', 0, 5)
    hud(s, '10 JUL ` 08:15')
    sprite(s, 'player', PLAYER_FRAMES['up'], 6 * TILE + 8, 8 * TILE + 12)
    sprite(s, 'zombie', ZOMBIE_FRAMES['down'], 7 * TILE + 8, 4 * TILE + 12)
    sprite(s, 'zombie', ZOMBIE_FRAMES['side'], 9 * TILE + 8, 1 * TILE + 12, mirror=True)
    return s


def panel_bus():
    s = Screen()
    fill(s, 'GRASS_A', 0, 0, 15, 10)
    road_across(s, 4)
    for column in range(4):
        for row in range(2):
            put(s, 'BUS_%d%d' % (column, row), 5 + column, 4 + row)
    put(s, 'ROAD_CORPSE', 3, 6)
    put(s, 'ROAD_BLOOD_A', 10, 5)
    put(s, 'PAVEMENT_CORPSE', 11, 7)
    tree(s, 'OAK', 1, 1)
    hud(s, '11 JUL ` 16:20')
    sprite(s, 'zombie', ZOMBIE_FRAMES['side'], 2 * TILE + 8, 4 * TILE + 12)
    sprite(s, 'zombie', ZOMBIE_FRAMES['down'], 12 * TILE + 8, 3 * TILE + 12)
    sprite(s, 'player', PLAYER_FRAMES['up'], 7 * TILE + 8, 9 * TILE + 12)
    label(s, 76, 100, '[A] SEARCH THE BUS', ACCENT)
    return s


def panel_barricade():
    s = Screen()
    fill(s, 'GRASS_DRY', 0, 0, 15, 10)
    road_across(s, 4)
    for tx in (4, 6, 8, 10):
        put(s, 'ROADBLOCK', tx, 4 + (tx // 2) % 3)
    for tx in range(3, 12):
        put(s, 'SANDBAGS', tx, 2)
    put(s, 'LOCKER', 12, 2)
    put(s, 'ROAD_CORPSE', 7, 5)
    hud(s, '12 JUL ` 10:00')
    for x, y in ((5, 8), (9, 8), (11, 4)):
        sprite(s, 'zombie', ZOMBIE_FRAMES['up'], x * TILE + 8, y * TILE + 12)
    sprite(s, 'player', PLAYER_FRAMES['down'], 2 * TILE + 8, 1 * TILE + 12)
    return s


def panel_checkpoint():
    s = Screen()
    fill(s, 'GRASS_DRY', 0, 0, 15, 10)
    for ty in range(10):
        put(s, 'ROAD', 6, ty)
        put(s, 'ROAD_LINE_V' if ty % 2 else 'ROAD', 7, ty)
        put(s, 'ROAD', 8, ty)
    put(s, 'BARRIER', 6, 4)
    put(s, 'BARRIER', 7, 4)
    for tx in (3, 4, 5, 9, 10, 11):
        put(s, 'SANDBAGS', tx, 4)
    put(s, 'WATCHTOWER', 10, 2)
    put(s, 'CRATE', 4, 2)
    for tx in range(0, 15):
        if tx not in (6, 7, 8):
            put(s, 'FENCE_H', tx, 6)
    hud(s, '13 JUL ` 06:30')
    sprite(s, 'zombie', ZOMBIE_FRAMES['down'], 5 * TILE + 8, 2 * TILE + 12)
    sprite(s, 'player', PLAYER_FRAMES['up'], 7 * TILE + 8, 8 * TILE + 12)
    return s


def panel_wild():
    s = Screen()
    fill(s, 'FOREST_FLOOR', 0, 0, 15, 10)
    fill(s, 'GRASS_LUSH', 9, 0, 15, 10)
    for tx, ty in ((1, 2), (3, 5), (0, 8), (6, 1), (5, 9), (13, 1)):
        tree(s, 'PINE', tx, ty)
    put(s, 'HUNTER_STAND', 4, 3)
    put(s, 'TENT', 11, 4)
    put(s, 'CAMPFIRE', 12, 6)
    put(s, 'CRATE', 10, 6)
    put(s, 'STUMP', 13, 7)
    hud(s, '14 JUL ` 17:50')
    sprite(s, 'player', PLAYER_FRAMES['up'], 4 * TILE + 8, 5 * TILE + 12)
    label(s, 36, 34, '[A] SEARCH', ACCENT)
    return s


def panel_sheet():
    s = Screen()
    s.rect(0, 0, 240, 160, (0x10, 0x14, 0x1b))
    s.text(6, 4, 'NEW FURNITURE', ACCENT)
    for index, (name, _) in enumerate(world14.FURNITURE_14 + [('HUNTER_STAND', None), ('GRAVE', None),
                                                               ('ROADBLOCK', None)]):
        column = index % 6
        row = index // 6
        put(s, name, column + 0, row + 1)
    s.text(6, 82, 'NEW CLOTHES', ACCENT)
    for index, outfit in enumerate(('POLICE', 'ARMY', 'FIREFIGHTER', 'FARMER')):
        x = 20 + index * 55
        survivor(s, outfit, PLAYER_FRAMES['down'], x, 112)
        survivor(s, outfit, PLAYER_FRAMES['side'], x + 18, 112)
        s.text_centered(x + 9, 116, outfit, QUIET)
    s.text(6, 130, '25 BUILDINGS ` 4 SET PIECES ` ABOUT 70 ITEMS', TEXT)
    return s


def main():
    panels = [
        (panel_supermarket(), [('1 SUPERMARKET (TOWN)', ACCENT), ('FREEZERS, SHELVES, CHECKOUTS', QUIET)]),
        (panel_school(), [('2 SCHOOL (TOWN; SMALL ONE IN VILLAGES)', ACCENT), ('BLACKBOARDS, DESKS, LOCKERS', QUIET)]),
        (panel_motel(), [('3 MOTEL (TOWN)', ACCENT), ('THREE ROOMS, PARKING IN FRONT', QUIET)]),
        (panel_church(), [('4 VILLAGE SET PIECE: THE CHURCH', ACCENT), ('PEWS, ALTAR, GRAVEYARD BEHIND', QUIET)]),
        (panel_bus(), [('5 TOWN SET PIECE: A CRASHED COACH', ACCENT), ('ACROSS THE MAIN ROAD, SEARCHABLE', QUIET)]),
        (panel_barricade(), [('6 BIG TOWN: POLICE BARRICADE', ACCENT), ('ROAD BLOCKS, SANDBAGS, A LOCKER', QUIET)]),
        (panel_checkpoint(), [('7 MILITARY BASE: CHECKPOINT', ACCENT), ('BARRIER, SANDBAGS, WATCHTOWER', QUIET)]),
        (panel_wild(), [('8 THE WILD: HUNTER STAND, CAMPSITE', ACCENT), ('TENT, COLD CAMPFIRE, A CRATE', QUIET)]),
        (panel_sheet(), [('9 NEW FURNITURE AND CLOTHES', ACCENT), ('CLOTHES ARE PALETTE SWAPS', QUIET)]),
    ]
    gap = 4
    width = 240 * 3 + gap * 2
    height = (160 + 22) * 3 + gap * 2
    sheet = Image.new('RGB', (width, height), (0x05, 0x07, 0x0b))

    for index, (screen, lines) in enumerate(panels):
        x = (index % 3) * (240 + gap)
        y = (index // 3) * (160 + 22 + gap)
        sheet.paste(screen.image, (x, y))
        sheet.paste(caption(240, lines), (x, y + 160))

    sheet = sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST)
    sheet.save(os.path.join(ROOT, '14-towns.png'))
    print('wrote mockups/14-towns.png')


if __name__ == '__main__':
    main()
