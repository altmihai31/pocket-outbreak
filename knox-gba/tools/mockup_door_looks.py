"""Playtest 1 mockup: doors that stay inside the building's outline. Drawn at 240x160 from the game's own world
graphics, sprites and palette, a house from outside in four versions, saved at 3x.

    python tools/mockup_door_looks.py        -> mockups/15-door-looks.png

Today every doorway in an outer wall is left out of the roof, so the whole 16x16 door tile shows in a notch: a front
door stands 11 px above the strip of wall under the eave, and a side door makes a box in the roof's side edge.

0. Now.
1. Under the eave: the eave dips over a front door (a roof graphic with its edge 3 px higher), so the door shows a
   little taller than the wall strip; side doors go under the roof, a paved step outside shows where they are.
2. In the wall strip: front doors get the plain eave, so only their foot shows, level with the wall strip; side
   doors as in 1.
3. Porch: the notch stays, with a small porch roof over the step in front of each door, so the door sits in an
   entrance rather than poking out.
In 1-3 the roof's slopes and edges are worked out as if the doorways were roofed: today the tile behind a front door
turns into an eave, which shows a bar of floor above the door.
Inside (the roof cut away) every door looks as it does now.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'assets'))

from mockup_character import ACCENT, QUIET, Screen  # noqa: E402
from mockup_doors import GRIDS, caption, sprite  # noqa: E402
import buildings  # noqa: E402
import world  # noqa: E402

ROOT = os.path.join(HERE, '..', 'mockups')
TILE = 16
PALETTE = world.WORLD_PALETTE
WALL_PALETTE = world.variant_bank(world.WALL_VARIANTS[1])        # white siding
T = world.W_TRANS

# The small house with a side door in its right wall (row 3).
ROWS = [
    'BBwBBBwBB',
    'BkkRBYooB',
    'BoooBooDB',
    'Boooooooo',
    'BBoBBBoBB',
    'BxxBoooTB',
    'BhxBooooB',
    'BBBBBoBBB',
]
LEFT, TOP = 2, 1


def eave_dip():
    """The eave over a front door: its dark edge 3 px higher, open below."""
    g = world.gfx_roof_slope(world.W_BRICK, world.W_MORTAR, 'LOWER')
    for x in range(16):
        g[7][x] = world.W_DARK
        for y in range(8, 16):
            g[y][x] = T
    return g


def porch_front():
    """A small porch roof over the step in front of a door: a lip of shingles with a dark edge and two posts."""
    g = [[T] * 16 for _ in range(16)]
    for y in range(0, 5):
        for x in range(1, 15):
            g[y][x] = world.W_MORTAR if y % 2 else world.W_BRICK
    for x in range(1, 15):
        g[5][x] = world.W_DARK
    for y in range(6, 12):
        g[y][2] = g[y][13] = world.W_DARK
    return g


def porch_side():
    """The same porch beside a side door (to its right)."""
    g = [[T] * 16 for _ in range(16)]
    for y in range(1, 15):
        for x in range(0, 5):
            g[y][x] = world.W_MORTAR if x % 2 else world.W_BRICK
        g[y][5] = world.W_DARK
    for x in range(6, 12):
        g[2][x] = g[13][x] = world.W_DARK
    return g


EXTRA = {'EAVE_DIP': eave_dip(), 'PORCH_FRONT': porch_front(), 'PORCH_SIDE': porch_side()}


def put(s, name, tx, ty, palette=None, offset=(0, 0)):
    grid = EXTRA.get(name) or GRIDS[name]
    colours = palette or PALETTE
    for y in range(TILE):
        for x in range(TILE):
            value = grid[y][x]
            if value:
                px, py = tx * TILE + x + offset[0], ty * TILE + y + offset[1]
                if 0 <= px < 240 and 0 <= py < 160:
                    s.pixels[px, py] = colours[value]


def layers():
    rows = buildings.with_front_step(ROWS)
    names = {}

    class Ids(dict):
        def __missing__(self, key):
            names[len(names) + 1] = key
            self[key] = len(names)
            return self[key]

    ids = Ids()
    ground, walls, roofs = buildings.tile_graphics(rows, 'o', 'RED', ids, 'SMALL_HOUSE')
    width = len(rows[0])

    def grid(values):
        return [[names.get(values[y * width + x]) for x in range(width)] for y in range(len(rows))]

    return rows, grid(ground), grid(walls), grid(roofs)


def roof_parts(rows, door_tiles_roofed):
    """Roof graphic names per tile. The slopes and edges are worked out as if outer doorways were roofed (so the
    tile behind a door keeps its slope instead of turning into an eave over the floor); the doorways themselves
    get a roof only if door_tiles_roofed."""
    roofed = [[c not in buildings.UNROOFED for c in row] for row in rows]
    ridge_rows = [y for y, row in enumerate(roofed) if any(row)]
    ridge = (ridge_rows[0] + ridge_rows[-1]) // 2
    out = []
    for y, row in enumerate(rows):
        line = []
        for x, c in enumerate(row):
            if not roofed[y][x] or (buildings.outside_door(rows, x, y) and not door_tiles_roofed):
                line.append(None)
            else:
                line.append('ROOF_RED_' + buildings.roof_part(roofed, ridge, x, y, 'SMALL_HOUSE'))
        out.append(line)
    return out


def scene(option):
    s = Screen()
    rows, ground, walls, roofs = layers()
    if option:
        roofs = roof_parts(rows, option in (1, 2))
    steps = []
    overhead = []          # (name, tx, ty) drawn over everything, like the canopy layer

    front = (5, len(ROWS) - 1)
    side = (len(ROWS[0]) - 1, 3)

    if option in (1, 2):
        if option == 1:
            roofs[front[1]][front[0]] = 'EAVE_DIP'
        steps.append((side[0] + 1, side[1]))
    elif option == 3:
        overhead.append(('PORCH_FRONT', front[0], front[1] + 1))
        overhead.append(('PORCH_SIDE', side[0] + 1, side[1]))
        steps.append((side[0] + 1, side[1]))

    # Grass, a path from the front step, the house.
    for ty in range(10):
        for tx in range(15):
            put(s, 'GRASS_A' if (tx * 7 + ty * 3) % 5 else 'GRASS_B', tx, ty)
    for ty in range(len(ROWS) + 1, 10):
        put(s, 'PAVEMENT', LEFT + front[0], TOP + ty)
    for x, y in steps:
        put(s, 'PAVEMENT', LEFT + x, TOP + y)
    for y, row in enumerate(rows):
        for x in range(len(row)):
            if ground[y][x]:
                put(s, ground[y][x], LEFT + x, TOP + y)
            if walls[y][x]:
                name = walls[y][x]
                painted = name in world.PAINTED['WALL'] or name.startswith('DOOR_FACE')
                put(s, name, LEFT + x, TOP + y, WALL_PALETTE if painted else None)
    sprite(s, 'player', 12, (LEFT + side[0] + 2) * TILE + 6, (TOP + side[1]) * TILE + 14, mirror=True)
    for y, row in enumerate(roofs):
        for x, name in enumerate(row):
            if name:
                put(s, name, LEFT + x, TOP + y)
    for name, x, y in overhead:
        put(s, name, LEFT + x, TOP + y)
    # A tree for scale.
    put(s, 'TREE_TRUNK', 13, 6)
    put(s, 'CANOPY_BOTTOM', 13, 6)
    put(s, 'CANOPY_TOP', 13, 5)
    return s


def main():
    panels = [
        (0, [('0 NOW: DOORS IN A ROOF NOTCH', ACCENT), ('FRONT DOOR STANDS ABOVE THE WALL', QUIET)]),
        (1, [('1 UNDER THE EAVE (RECOMMENDED)', ACCENT), ('EAVE DIPS OVER IT; SIDE DOOR: A STEP', QUIET)]),
        (2, [('2 IN THE WALL STRIP', ACCENT), ('ONLY THE DOOR\'S FOOT; SIDE: A STEP', QUIET)]),
        (3, [('3 PORCH', ACCENT), ('NOTCH KEPT, A PORCH ROOF IN FRONT', QUIET)]),
    ]
    gap = 4
    width = 240 * 2 + gap
    height = (160 + 22) * 2 + gap
    sheet = Image.new('RGB', (width, height), (0x05, 0x07, 0x0b))
    for index, (option, lines) in enumerate(panels):
        screen = scene(option)
        x = (index % 2) * (240 + gap)
        y = (index // 2) * (160 + 22 + gap)
        sheet.paste(screen.image, (x, y))
        sheet.paste(caption(240, lines), (x, y + 160))
    sheet = sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST)
    sheet.save(os.path.join(ROOT, '15-door-looks.png'))
    print('wrote mockups/15-door-looks.png')


if __name__ == '__main__':
    main()
