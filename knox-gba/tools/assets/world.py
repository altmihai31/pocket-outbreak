"""World tileset (BG1-BG3): terrain, walls, trees, canopies and ground decals."""
import random

from common import TRANSPARENT, blank, hexc, overlay, save_bmp, split_16, stack_tiles_8x8, write_header

from buildings import write_buildings_header

# ---------------------------------------------------------------------------------------------
# World tiles (BG1-BG3 share one tileset; bank 0 terrain, bank 1 terrain + decals)
# ---------------------------------------------------------------------------------------------

W_TRANS, W_GRASS, W_GRASS_L, W_CANOPY, W_ROAD, W_LINE, W_PAVE, W_BRICK, W_MORTAR, W_WOOD, \
    W_WATER, W_WATER_L, W_DEEP, W_FOREST, W_DARK, W_PAVE_D = range(16)

WORLD_PALETTE = [TRANSPARENT, hexc('3f6b34'), hexc('557f42'), hexc('24451e'), hexc('3a3d42'),
                 hexc('c9a33a'), hexc('8f8d82'), hexc('7d4b3a'), hexc('4a2e24'), hexc('8a6a42'),
                 hexc('4a90d9'), hexc('7fb0e0'), hexc('2a5a9a'), hexc('2f4f28'), hexc('151a22'),
                 hexc('6f6d64')]


def speckle(grid, rng, colour, chance):
    for y in range(16):
        for x in range(16):
            if rng.random() < chance:
                grid[y][x] = colour


def gfx_grass(variant):
    rng = random.Random(100 + variant)
    g = blank(16, 16, W_GRASS)
    speckle(g, rng, W_GRASS_L, 0.07)
    for _ in range(2 + variant * 2):
        x, y = rng.randrange(1, 15), rng.randrange(2, 15)
        g[y][x] = W_GRASS_L
        g[y - 1][x - 1] = W_GRASS_L
        g[y - 1][x + 1] = W_GRASS_L
    return g


def gfx_grass_lush():
    """Lush grass (wetter ground): darker, with long blades."""
    rng = random.Random(110)
    g = blank(16, 16, W_GRASS)
    speckle(g, rng, W_CANOPY, 0.10)
    for _ in range(7):
        x, y = rng.randrange(1, 15), rng.randrange(3, 15)
        for i in range(3):
            g[y - i][x] = W_GRASS_L if i == 2 else W_CANOPY
    return g


def gfx_grass_flowers():
    """Grass with wild flowers (yellow and white) and clover."""
    rng = random.Random(120)
    g = gfx_grass(0)
    for _ in range(5):
        x, y = rng.randrange(1, 15), rng.randrange(1, 15)
        petal = W_LINE if rng.random() < 0.6 else W_PAVE
        g[y][x] = petal
        g[y - 1][x] = petal
        g[y][x - 1] = petal
        g[y + 1][x] = W_CANOPY
    return g


def gfx_grass_dry():
    """Dry, thin grass on higher ground: lighter, with bare earth showing."""
    rng = random.Random(130)
    g = blank(16, 16, W_GRASS)
    speckle(g, rng, W_GRASS_L, 0.22)
    speckle(g, rng, W_WOOD, 0.05)
    speckle(g, rng, W_LINE, 0.02)
    return g


def gfx_road(line):
    rng = random.Random(200)
    g = blank(16, 16, W_ROAD)
    speckle(g, rng, W_DARK, 0.05)
    speckle(g, rng, W_PAVE_D, 0.015)
    if line == 'h':
        for x in range(3, 13):
            g[7][x] = g[8][x] = W_LINE
    elif line == 'v':
        for y in range(3, 13):
            g[y][7] = g[y][8] = W_LINE
    return g


def gfx_pavement():
    rng = random.Random(300)
    g = blank(16, 16, W_PAVE)
    speckle(g, rng, W_PAVE_D, 0.04)
    for i in range(16):
        g[15][i] = W_PAVE_D
        g[i][15] = W_PAVE_D
    return g


def gfx_forest_floor():
    rng = random.Random(400)
    g = blank(16, 16, W_FOREST)
    speckle(g, rng, W_CANOPY, 0.15)
    speckle(g, rng, W_GRASS, 0.07)
    speckle(g, rng, W_MORTAR, 0.03)
    return g


def gfx_mud():
    rng = random.Random(500)
    g = blank(16, 16, W_MORTAR)
    speckle(g, rng, W_WOOD, 0.10)
    speckle(g, rng, W_DARK, 0.05)
    return g


def gfx_water(deep):
    rng = random.Random(600 + deep)
    g = blank(16, 16, W_DEEP if deep else W_WATER)
    ripple = W_WATER if deep else W_WATER_L
    for _ in range(3 if deep else 4):
        x, y = rng.randrange(0, 13), rng.randrange(0, 16)
        for i in range(3):
            g[y][x + i] = ripple
    return g


def gfx_wood_floor():
    rng = random.Random(700)
    g = blank(16, 16, W_WOOD)
    for y in (3, 7, 11, 15):
        for x in range(16):
            g[y][x] = W_MORTAR
    for band in range(4):
        x = rng.randrange(0, 16)
        for y in range(band * 4, band * 4 + 3):
            g[y][x] = W_MORTAR
    return g


def gfx_wall_face():
    g = blank(16, 16, W_BRICK)
    for y in (3, 7, 11, 15):
        for x in range(16):
            g[y][x] = W_MORTAR
    for band in range(4):
        offset = 0 if band % 2 == 0 else 4
        for x in (offset, offset + 8):
            for y in range(band * 4, band * 4 + 3):
                g[y][x % 16] = W_MORTAR
    for x in range(16):
        g[0][x] = W_DARK
    return g


def gfx_wall_top():
    g = blank(16, 16, W_MORTAR)
    for i in range(16):
        g[0][i] = g[15][i] = W_DARK
        g[i][0] = g[i][15] = W_DARK
    for i in range(1, 15):
        g[1][i] = W_BRICK
    return g


def gfx_trunk():
    g = blank(16, 16, W_TRANS)
    for y in range(13, 16):
        for x in range(3, 13):
            if ((x - 7.5) / 5.5) ** 2 + ((y - 14) / 2) ** 2 <= 1:
                g[y][x] = W_DARK
    for y in range(6, 15):
        for x in range(6, 10):
            g[y][x] = W_MORTAR
        g[y][5] = g[y][10] = W_DARK
    for y in range(6, 14):
        g[y][7] = W_WOOD
    return g


def gfx_fence(horizontal):
    g = blank(16, 16, W_TRANS)
    if horizontal:
        for x in range(16):
            for y in (6, 11):
                g[y][x] = W_WOOD
                g[y + 1][x] = W_MORTAR
        for px in (2, 13):
            for y in range(3, 15):
                g[y][px] = W_MORTAR
                g[y][px - 1] = W_DARK
    else:
        for y in range(16):
            g[y][6] = W_DARK
            g[y][7] = W_WOOD
            g[y][8] = W_WOOD
            g[y][9] = W_MORTAR
        for py in (2, 13):
            for x in range(5, 11):
                g[py][x] = W_MORTAR
            g[py + 1][7] = W_DARK
    return g


def canopy_pixels(dy):
    """Canopy ellipse for a tree whose trunk tile is at dy=+1 (top part) or dy=0 (bottom part)."""
    g = blank(16, 16, W_TRANS)
    cx, cy, rx, ry = 7.5, 13.0, 8.0, 10.0
    for y in range(16):
        for x in range(16):
            yy = y + (0 if dy == 1 else 16)
            d = ((x - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2
            if d <= 1:
                if d > 0.82:
                    c = W_DARK
                elif (x - cx) + (yy - cy) < -8:
                    c = W_GRASS_L
                elif (x * 7 + yy * 13) % 11 == 0:
                    c = W_GRASS
                else:
                    c = W_CANOPY
                g[y][x] = c
    return g


def gfx_pine_trunk():
    """Pine: a short dark trunk under low branches."""
    g = blank(16, 16, W_TRANS)
    for y in range(14, 16):
        for x in range(4, 12):
            if ((x - 7.5) / 4.5) ** 2 + ((y - 14.5) / 1.5) ** 2 <= 1:
                g[y][x] = W_DARK
    for y in range(9, 15):
        g[y][7] = W_MORTAR
        g[y][8] = W_MORTAR
        g[y][6] = g[y][9] = W_DARK
    return g


def gfx_birch_trunk():
    """Birch: a thin pale trunk with dark marks."""
    g = blank(16, 16, W_TRANS)
    for y in range(14, 16):
        for x in range(4, 12):
            if ((x - 7.5) / 4.0) ** 2 + ((y - 14.5) / 1.5) ** 2 <= 1:
                g[y][x] = W_DARK
    for y in range(5, 15):
        g[y][7] = W_PAVE
        g[y][8] = W_PAVE_D
        g[y][6] = g[y][9] = W_DARK
    for y in (6, 9, 12):
        g[y][7] = W_DARK
    return g


def pine_pixels(dy):
    """Pine canopy: three stacked tiers narrowing to a point, dark and cool."""
    g = blank(16, 16, W_TRANS)
    for y in range(16):
        yy = y + (0 if dy == 1 else 16)
        if not 1 <= yy <= 26:
            continue
        tier = (yy - 1) % 8
        half = 1.5 + (yy - 1) * 0.16 + tier * 0.55
        for x in range(16):
            d = abs(x - 7.5)
            if d <= half:
                if d > half - 1:
                    c = W_DARK
                elif x > 8 and (yy + x) % 3:
                    c = W_CANOPY
                elif x < 6 and tier < 2:
                    c = W_GRASS_L
                else:
                    c = W_GRASS
                g[y][x] = c
    return g


def birch_pixels(dy):
    """Birch canopy: smaller and lighter than the oak, loosely leaved."""
    g = blank(16, 16, W_TRANS)
    cx, cy, rx, ry = 7.5, 12.0, 6.5, 8.0
    for y in range(16):
        for x in range(16):
            yy = y + (0 if dy == 1 else 16)
            d = ((x - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2
            if d <= 1:
                if d > 0.8:
                    c = W_CANOPY
                elif (x * 5 + yy * 3) % 7 == 0:
                    c = W_GRASS
                elif (x - cx) + (yy - cy) < -5:
                    c = W_LINE if (x + yy) % 9 == 0 else W_GRASS_L
                else:
                    c = W_GRASS_L
                g[y][x] = c
    return g


def gfx_bush():
    """Bush (walls layer): a low shrub, solid but see-through."""
    g = blank(16, 16, W_TRANS)
    cx, cy, rx, ry = 7.5, 10.5, 6.5, 4.8
    for y in range(16):
        for x in range(16):
            d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
            if d <= 1:
                if d > 0.75:
                    c = W_DARK
                elif (x - cx) + (y - cy) < -3:
                    c = W_GRASS
                elif (x * 3 + y * 5) % 6 == 0:
                    c = W_GRASS_L
                else:
                    c = W_CANOPY
                g[y][x] = c
    for x, y in ((5, 9), (10, 11), (8, 8)):
        g[y][x] = W_BRICK
    return g


def gfx_tile_floor():
    """Kitchen, shop and clinic floor: pale square tiles."""
    g = blank(16, 16, W_PAVE)
    for i in range(16):
        g[7][i] = g[15][i] = W_PAVE_D
        g[i][7] = g[i][15] = W_PAVE_D
    for x, y in ((3, 3), (11, 11)):
        g[y][x] = W_LINE
    return g


def gfx_concrete():
    """Garage, barn and petrol station floor: plain concrete with stains."""
    rng = random.Random(900)
    g = blank(16, 16, W_PAVE_D)
    speckle(g, rng, W_PAVE, 0.12)
    speckle(g, rng, W_ROAD, 0.04)
    return g


def gfx_window_face():
    """Window in a wall seen from the front (solid, but the player sees through it)."""
    g = gfx_wall_face()
    for y in range(3, 12):
        for x in range(3, 13):
            g[y][x] = W_WATER_L if (x + y) % 5 else W_WATER
    for x in range(2, 14):
        g[2][x] = g[12][x] = W_DARK
    for y in range(2, 13):
        g[y][2] = g[y][13] = W_DARK
        g[y][8] = W_DARK
    return g


def gfx_window_top():
    """Window in a side wall seen from above: a glass strip along the wall top."""
    g = gfx_wall_top()
    for y in range(3, 13):
        g[y][7] = W_WATER_L
        g[y][8] = W_WATER
    return g


def boards(g, vertical):
    """Planks nailed across an opening (barricades), in wood the wall colours never repaint."""
    if vertical:
        for x0 in (4, 9):
            for y in range(1, 15):
                for x in range(x0, x0 + 3):
                    g[y][x] = W_WOOD
                g[y][x0 + 3] = W_DARK
            g[3][x0 + 1] = g[12][x0 + 1] = W_DARK
    else:
        for y0 in (3, 9):
            for x in range(1, 15):
                for y in range(y0, y0 + 3):
                    g[y][x] = W_WOOD
                g[y0 + 3][x] = W_DARK
            g[y0 + 1][3] = g[y0 + 1][12] = W_DARK
    return g


def gfx_barricade_face():
    """A window or doorway in a front wall, boarded up (solid, blocks sight; zombies attack it)."""
    g = gfx_window_face()
    for y in range(3, 12):
        for x in range(3, 13):
            g[y][x] = W_DARK
    return boards(g, False)


def gfx_barricade_top():
    """A window or doorway in a side wall seen from above, boarded up."""
    g = gfx_window_top()
    for y in range(3, 13):
        g[y][7] = g[y][8] = W_DARK
    return boards(g, True)


def barricade_damage(g, opening, stage, put):
    """Playtest 8: a barricade wearing down, drawn on two planks along u (0-15) at v rows 3-5 and 9-11 (dark under
    at 6 and 12). Stage 1 (worn): the first plank split near the middle, its far half sagging 1 px, a nail of the
    second gone. Stage 2 (failing): the first plank gone but a stub hanging from its nail, the second split and
    sagging. put(u, v, value) writes in tile space; opening(u, v) is what shows where a plank is gone."""
    if stage >= 1:
        for u in range(9, 15):
            put(u, 3, opening(u, 3))
            for v in (4, 5, 6):
                put(u, v, W_WOOD)
            put(u, 7, W_DARK)
        for u, v in ((8, 3), (8, 4), (9, 5), (9, 6)):
            put(u, v, W_DARK)
        put(12, 10, W_WOOD)
    if stage >= 2:
        for u in range(1, 15):
            for v in range(3, 8):
                put(u, v, opening(u, v))
        for u, v in ((1, 3), (2, 3), (3, 3), (1, 4), (2, 4), (3, 4), (4, 4), (2, 5), (3, 5), (4, 5), (5, 5),
                     (3, 6), (4, 6), (5, 6)):
            put(u, v, W_WOOD)
        for u, v in ((3, 4), (1, 5), (2, 6), (4, 7), (5, 7), (6, 6)):
            put(u, v, W_DARK)
        for u in range(9, 15):
            put(u, 9, opening(u, 9))
            for v in (10, 11, 12):
                put(u, v, W_WOOD)
            put(u, 13, W_DARK)
        for u, v in ((8, 9), (8, 10), (9, 11), (9, 12)):
            put(u, v, W_DARK)
    return g


def gfx_barricade_face_damaged(stage):
    g = gfx_barricade_face()
    base = gfx_window_face()
    for y in range(3, 12):
        for x in range(3, 13):
            base[y][x] = W_DARK

    def put(u, v, value):
        g[v][u] = value
    return barricade_damage(g, lambda u, v: base[v][u], stage, put)


def gfx_barricade_top_damaged(stage):
    """The same in a side wall: the planks run down columns 4-6 and 9-11."""
    g = gfx_barricade_top()
    base = gfx_window_top()
    for y in range(3, 13):
        base[y][7] = base[y][8] = W_DARK

    def put(u, v, value):
        if v + 1 < 16:
            g[u][v + 1] = value
    return barricade_damage(g, lambda u, v: base[u][v + 1] if v + 1 < 16 else W_DARK, stage, put)


def gfx_broken_face():
    """A smashed window: an empty frame with glass shards in the corners (zombies climb through)."""
    g = gfx_window_face()
    for y in range(3, 12):
        for x in range(3, 13):
            g[y][x] = W_DARK
    for x, y in ((3, 3), (4, 3), (3, 4), (12, 3), (12, 4), (11, 11), (12, 11), (3, 11), (9, 3), (10, 4)):
        g[y][x] = W_WATER_L
    for y in range(2, 13):
        g[y][8] = W_DARK
    return g


def gfx_broken_top():
    g = gfx_wall_top()
    for y in range(3, 13):
        g[y][7] = g[y][8] = W_DARK
    g[3][7] = g[12][8] = g[6][8] = W_WATER_L
    return g


# Milestone 12.7 (mockup 12; added to the tileset in stage 2): doors, the log wall, a stump and the gun safe. Doors
# use the wood and dark colours the wall paints never repaint, and leave the doorway transparent where it is open
# (the floor shows through from the ground layer).
def door_leaf_face(g, x0, x1, y0, y1):
    """A closed wooden door filling a frame: an outline, two sunk panels, a brass knob."""
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            g[y][x] = W_WOOD
        g[y][x0] = g[y][x1] = W_DARK
    for x in range(x0, x1 + 1):
        g[y0][x] = W_DARK
    for y0p, y1p in ((y0 + 2, y0 + 6), (y0 + 8, y1 - 1)):
        for y in range(y0p, y1p + 1):
            g[y][x0 + 2] = g[y][x1 - 2] = W_DARK
        for x in range(x0 + 2, x1 - 1):
            g[y0p][x] = g[y1p][x] = W_DARK
    g[y0 + 7][x1 - 2] = W_LINE
    return g


def gfx_door_face(state):
    """A door in a front wall: 'closed', 'open' (swung in, edge-on by its hinge) or 'broken' (splinters)."""
    g = blank(16, 16, W_TRANS)
    # The frame: brick jambs and a lintel.
    wall = gfx_wall_face()
    for y in range(16):
        for x in (0, 1, 14, 15):
            g[y][x] = wall[y][x]
    for x in range(16):
        g[0][x] = W_DARK
        g[1][x] = W_DARK
    for y in range(1, 16):
        g[y][2] = g[y][13] = W_DARK
    if state == 'closed':
        door_leaf_face(g, 3, 12, 2, 15)
    elif state == 'open':
        for y in range(2, 16):
            g[y][3] = g[y][5] = W_DARK
            g[y][4] = W_WOOD
    else:
        for x, y in ((3, 2), (4, 2), (3, 3), (3, 4), (3, 5), (4, 4), (3, 9), (3, 10), (12, 3), (11, 2), (12, 2),
                     (5, 14), (6, 15), (9, 14), (10, 15), (11, 13), (12, 14)):
            g[y][x] = W_WOOD
        for x, y in ((4, 3), (4, 5), (4, 10), (11, 3), (6, 14), (10, 14), (12, 13)):
            g[y][x] = W_DARK
    return g


def gfx_door_top(state):
    """A door in a side wall seen from above: closed, a plank along the wall line; open, swung flat against the
    jamb; broken, splinters."""
    g = blank(16, 16, W_TRANS)
    for x in range(4, 12):
        g[0][x] = g[15][x] = W_DARK
    if state == 'closed':
        for y in range(1, 15):
            g[y][6] = g[y][9] = W_DARK
            g[y][7] = g[y][8] = W_WOOD
        g[1][7] = g[1][8] = g[14][7] = g[14][8] = W_DARK
        g[9][9] = W_LINE
    elif state == 'open':
        for x in range(6, 15):
            g[1][x] = g[4][x] = W_DARK
            g[2][x] = g[3][x] = W_WOOD
        g[2][14] = g[3][14] = W_DARK
    else:
        for x, y in ((7, 1), (8, 1), (7, 2), (7, 3), (8, 13), (8, 14), (7, 14), (10, 6), (11, 7), (4, 10)):
            g[y][x] = W_WOOD
        for x, y in ((8, 2), (8, 3), (7, 13), (11, 6), (5, 10)):
            g[y][x] = W_DARK
    return g


def gfx_log_wall(vertical):
    """Logs stacked into a wall (built by the player): three logs across, or along for a wall running north."""
    g = blank(16, 16, W_TRANS)
    for index in range(3):
        a0 = 1 + index * 5
        for a in range(a0, a0 + 5):
            for b in range(0, 16):
                x, y = (a, b) if vertical else (b, a)
                edge = a in (a0, a0 + 4)
                g[y][x] = W_DARK if edge else W_WOOD
        for b in (0, 15):
            for a in range(a0 + 1, a0 + 4):
                x, y = (a, b) if vertical else (b, a)
                g[y][x] = W_LINE
        knot = 4 + index * 5
        x, y = (a0 + 2, knot) if vertical else (knot, a0 + 2)
        g[y][x] = W_MORTAR
    return g


def gfx_stump():
    """What is left of a chopped tree: a short trunk with its pale cut face."""
    g = blank(16, 16, W_TRANS)
    for y in range(12, 16):
        for x in range(3, 13):
            if ((x - 7.5) / 5.5) ** 2 + ((y - 14) / 2) ** 2 <= 1:
                g[y][x] = W_DARK
    for y in range(10, 15):
        for x in range(5, 11):
            g[y][x] = W_MORTAR
        g[y][4] = g[y][11] = W_DARK
    for x in range(5, 11):
        g[9][x] = W_DARK
        g[10][x] = W_LINE
        g[11][x] = W_LINE if x in (5, 10) else W_WOOD
    g[11][7] = g[11][8] = W_LINE
    return g


def gfx_gun_safe():
    """A squat steel safe: a dial and a handle on its door."""
    g = furniture(W_PAVE, W_PAVE_D, W_DARK, [], top_rows=4)
    for y in range(6, 14):
        g[y][2] = g[y][13] = W_DARK
    for x in range(2, 14):
        g[6][x] = g[14][x] = W_DARK
    for y in range(7, 13):
        for x in range(5, 9):
            if (x - 6.5) ** 2 + (y - 9.5) ** 2 <= 4.5:
                g[y][x] = W_PAVE
    g[9][6] = g[9][7] = g[10][6] = g[10][7] = W_DARK
    for y in range(8, 12):
        g[y][11] = W_LINE
    return g


def gfx_stash():
    """A stash left in the woods (a note says where): a green canvas bag with straps, on a tarp."""
    g = blank(16, 16, W_TRANS)
    for y in range(9, 15):
        for x in range(1, 15):
            if ((x - 7.5) / 7.0) ** 2 + ((y - 12) / 3.0) ** 2 <= 1:
                g[y][x] = W_PAVE_D
    for y in range(4, 13):
        for x in range(3, 13):
            if ((x - 7.5) / 5.0) ** 2 + ((y - 8.5) / 4.2) ** 2 <= 1:
                g[y][x] = W_FOREST
    for y in range(5, 12):
        g[y][5] = g[y][10] = W_CANOPY
    for x in range(4, 12):
        g[8][x] = W_CANOPY
    g[4][7] = g[4][8] = W_DARK
    return g


def gfx_rain_collector():
    """A blue barrel on a plank stand, open at the top (solid)."""
    g = blank(16, 16, W_TRANS)
    for y in range(11, 15):
        for x in (3, 4, 11, 12):
            g[y][x] = W_WOOD
    for x in range(2, 14):
        g[11][x] = W_WOOD
        g[12][x] = W_MORTAR
    for y in range(2, 11):
        for x in range(4, 12):
            g[y][x] = W_WATER if 5 <= x <= 9 else W_DEEP
    for x in range(4, 12):
        g[2][x] = W_DARK
        g[5][x] = g[8][x] = W_DEEP
    for x in range(5, 11):
        g[3][x] = W_WATER_L
    for y in range(2, 11):
        g[y][4] = g[y][11] = W_DARK
    return g


def gfx_campfire():
    """A ring of stones round crossed logs on ash (solid; a flame sprite shows on it while it burns)."""
    g = blank(16, 16, W_TRANS)
    for y in range(5, 15):
        for x in range(2, 14):
            if ((x - 7.5) / 5.6) ** 2 + ((y - 9.8) / 4.4) ** 2 <= 1.0:
                g[y][x] = W_DARK
    for cx, cy in ((2, 9), (4, 6), (7, 5), (10, 5), (12, 7), (13, 10), (11, 13), (8, 14), (5, 13), (2, 12)):
        for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
            if 0 <= cx + dx < 16 and 0 <= cy + dy < 16:
                g[cy + dy][cx + dx] = W_PAVE if dy == 0 else W_PAVE_D
    for i in range(6):
        g[7 + i][5 + i] = W_WOOD
        g[8 + i][5 + i] = W_MORTAR
        g[7 + i][10 - i] = W_WOOD
        g[8 + i][10 - i] = W_MORTAR
    return g


def gfx_plot(stage):
    """A farm plot (walls layer, over the ground): furrows, then sprouts, then ripe plants with their crop showing."""
    g = blank(16, 16, W_MORTAR)
    for y in range(16):
        for x in range(16):
            if y % 4 == 1:
                g[y][x] = W_WOOD
            elif y % 4 == 3:
                g[y][x] = W_DARK if x % 5 else W_MORTAR
    if stage >= 1:
        for row in (1, 5, 9, 13):
            for x in (2, 6, 10, 14):
                g[row][x] = W_GRASS_L
                g[row - 1][x] = W_GRASS_L if stage == 1 else W_GRASS
    if stage >= 2:
        for row in (1, 5, 9, 13):
            for x in (1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14):
                g[row][x] = W_GRASS
            for x in (2, 6, 10, 14):
                g[row - 1][x] = W_CANOPY
                g[row][x] = W_LINE
    return g


def gfx_generator():
    """A petrol generator (solid): a grey box on a frame with a yellow panel."""
    g = blank(16, 16, W_TRANS)
    for y in range(5, 14):
        for x in range(2, 14):
            g[y][x] = W_PAVE if y < 8 else W_PAVE_D
    for x in range(2, 14):
        g[5][x] = g[13][x] = W_DARK
    for y in range(5, 14):
        g[y][2] = g[y][13] = W_DARK
    for y in range(9, 12):
        for x in range(4, 8):
            g[y][x] = W_LINE
    g[10][10] = g[10][11] = W_DARK
    g[3][11] = g[4][11] = W_DARK
    return g


def gfx_trap():
    """A box trap: a small wooden crate on a stick (solid)."""
    g = blank(16, 16, W_TRANS)
    for y in range(7, 14):
        for x in range(3, 13):
            g[y][x] = W_WOOD
    for x in range(3, 13):
        g[7][x] = g[13][x] = W_MORTAR
    for y in range(7, 14):
        g[y][3] = g[y][12] = W_MORTAR
    for y in range(9, 13):
        for x in range(6, 10):
            g[y][x] = W_DARK
    for y in range(3, 8):
        g[y][13] = W_MORTAR
    return g


def gfx_can_line():
    """A bottle-can alarm line (walls layer, passable): a string across with cans hanging from it."""
    g = blank(16, 16, W_TRANS)
    for x in range(16):
        g[7][x] = W_PAVE
    for x0 in (2, 7, 12):
        for y in range(8, 12):
            for x in range(x0, x0 + 3):
                g[y][x] = W_PAVE_D if x == x0 else W_PAVE
        g[11][x0 + 1] = W_DARK
    return g


def gfx_shop_shelf():
    g = furniture(W_PAVE, W_PAVE_D, W_DARK, [], top_rows=3)
    for y in (7, 11):
        for x in range(1, 15):
            g[y][x] = W_DARK
    for x, y, c in ((2, 6, W_BRICK), (3, 6, W_BRICK), (5, 6, W_LINE), (6, 6, W_LINE), (9, 5, W_WATER),
                    (9, 6, W_WATER), (12, 6, W_GRASS_L), (2, 10, W_LINE), (4, 10, W_LINE), (7, 9, W_BRICK),
                    (7, 10, W_BRICK), (11, 10, W_WATER_L), (12, 10, W_WATER_L)):
        g[y][x] = c
    return g


def gfx_counter():
    g = furniture(W_WOOD, W_MORTAR, W_DARK, [], top_rows=6)
    for x in range(9, 14):
        for y in range(2, 5):
            g[y][x] = W_PAVE_D
    g[3][10] = g[3][12] = W_LINE
    return g


def gfx_locker():
    g = furniture(W_PAVE_D, W_PAVE, W_DARK, [(5, 8), (10, 8)], top_rows=2)
    for y in range(3, 15):
        g[y][7] = g[y][8] = W_DARK
    for x in (3, 4, 11, 12):
        g[4][x] = W_DARK
    return g


def gfx_med_cabinet():
    g = furniture(W_PAVE, W_PAVE, W_PAVE_D, [(7, 12), (8, 12)], top_rows=3)
    for i in range(5, 10):
        g[i][7] = g[i][8] = W_BRICK
        g[7][i - 2] = g[8][i - 2] = W_BRICK
    for y in range(4, 15):
        g[y][2] = g[y][13] = W_PAVE_D
    return g


def gfx_crate():
    g = furniture(W_WOOD, W_WOOD, W_MORTAR, [], top_rows=4)
    for x in range(1, 15):
        g[9][x] = W_MORTAR
    for i in range(5, 15):
        g[i][i] = W_MORTAR
        g[i][2] = g[i][13] = W_MORTAR
    return g


def gfx_desk():
    g = furniture(W_WOOD, W_MORTAR, W_DARK, [(4, 10), (11, 10)], top_rows=7)
    for x, y, c in ((3, 3, W_PAVE), (4, 3, W_PAVE), (5, 3, W_PAVE), (4, 4, W_PAVE), (10, 2, W_DARK),
                    (11, 2, W_DARK), (10, 3, W_WATER_L), (11, 3, W_WATER_L)):
        g[y][x] = c
    return g


def gfx_pump():
    """Fuel pump on the forecourt (solid)."""
    g = blank(16, 16, W_TRANS)
    for y in range(2, 15):
        for x in range(4, 12):
            g[y][x] = W_BRICK if y > 5 else W_PAVE
    for x in range(4, 12):
        g[2][x] = g[14][x] = W_DARK
    for y in range(2, 15):
        g[y][4] = g[y][11] = W_DARK
    for x in range(6, 10):
        g[4][x] = W_LINE
    for y in range(7, 12):
        g[y][12] = W_DARK
    g[12][13] = W_DARK
    return g


ART_KEYS = {'.': W_TRANS, 'o': W_DARK, 'w': W_WOOD, 'm': W_MORTAR, 'g': W_PAVE, 'G': W_PAVE_D, 'r': W_ROAD,
            'd': W_DEEP, 'b': W_WATER, 'l': W_WATER_L, 'y': W_LINE, 'f': W_FOREST, 'n': W_GRASS}


def art(rows):
    """A 16x16 graphic from 16 rows of ART_KEYS letters."""
    assert len(rows) == 16 and all(len(r) == 16 for r in rows)
    return [[ART_KEYS[c] for c in r] for r in rows]


def gfx_snare():
    """Milestone 13: a snare (not solid): a peg in the ground and a wire noose tied to it."""
    return art([
        '................',
        '................',
        '................',
        '.....o..........',
        '....owo.........',
        '....owo.........',
        '....owo..ggg....',
        '....owo.g...g...',
        '....owoggG...g..',
        '....owo.g...g...',
        '....owo..ggg....',
        '...oomoo........',
        '..f.ooo.f.......',
        '................',
        '................',
        '................',
    ])


def gfx_well():
    """Milestone 13: a village well (solid): a stone ring round dark water, a beam on two posts, a bucket's rope."""
    return art([
        '................',
        '.oooooooooooooo.',
        '.owwwwwwwwwwwwo.',
        '.ommmmmmmmmmmmo.',
        '..ow..ooy..wo...',
        '..ow....y..wo...',
        '..ow....y..wo...',
        '..ogggGgggGggo..',
        '.ogGoooooooogGo.',
        '.oGoddddddddoGo.',
        '.ogoddbddddoogo.',
        '.oGgoooooooogGo.',
        '.oggGgggGgggGgo.',
        '.oGGGGGGGGGGGGo.',
        '..oooooooooooo..',
        '................',
    ])


def gfx_chest_freezer():
    """Milestone 13: a chest freezer (solid): a long grey lid with a handle, a vent grille on the front."""
    return art([
        'oooooooooooooooo',
        'oggggggggggggggo',
        'oggggggggggggggo',
        'oggggggggggggggo',
        'oggggggggggggggo',
        'oggggggggggggggo',
        'ogggggggoooggggo',
        'oGGGGGGGGGGGGGGo',
        'olllllllllllllllo'[:16],
        'oGGGGGGGGGGGGGGo',
        'oGGGGGGGGGGGGGGo',
        'oGGrGrGrGGGGGGGo',
        'oGGrGrGrGGGGGGGo',
        'oGGGGGGGGGGGGGGo',
        'oGGGGGGGGGGGGGGo',
        'oooooooooooooooo',
    ])


def gfx_tv():
    """Milestone 13: a television (solid) on a low wooden stand: a dark screen in a grey case."""
    return art([
        '................',
        '..oooooooooooo..',
        '..oGGGGGGGGGGo..',
        '..oGoooooooooGo.'[:16],
        '..oGorrrrrrroGo.'[:16],
        '..oGorrrrrrroGo.'[:16],
        '..oGorrrrrrroGo.'[:16],
        '..oGorrrrrrroGo.'[:16],
        '..oGoooooooooGo.'[:16],
        '..oGGGGGGGGGyGo.'[:16],
        'oooooooooooooooo',
        'owwwwwwwwwwwwwwo',
        'ommmmmmmmmmmmmmo',
        'owwwwwwoowwwwwwo',
        'ommmmmmoommmmmmo',
        'oooooooooooooooo',
    ])


def gfx_tent():
    """Abandoned camp: a small ridge tent with a dark opening (solid; searchable)."""
    g = blank(16, 16, W_TRANS)
    for y in range(3, 15):
        half = (y - 2) * 0.62
        for x in range(16):
            d = abs(x - 7.5)
            if d <= half:
                if d > half - 1 or y == 14:
                    c = W_DARK
                elif x < 7.5:
                    c = W_GRASS_L
                else:
                    c = W_GRASS
                g[y][x] = c
    for y in range(9, 14):
        for x in range(7, 9 + (y - 9) // 3):
            g[y][x] = W_DARK
    for y in range(2, 4):
        g[y][7] = g[y][8] = W_MORTAR
    return g


# Roof parts: the slopes and the ridge between them, the eave over the front wall, the gable edges on either side,
# and details (a chimney, a vent or skylight, moss or rust stains). kx::world::detail::is_roof() relies on this order
# (ROOF_RED_UPPER first, ROOF_GREY_WEATHER last). Milestone 13 dropped the stains to free two graphics; milestone 14
# (a tileset twice the size) brought them back.
ROOF_PARTS = ['UPPER', 'RIDGE', 'LOWER', 'EAVE', 'UPPER_L', 'UPPER_R', 'LOWER_L', 'LOWER_R', 'EAVE_L', 'EAVE_R',
              'CHIMNEY', 'VENT', 'WEATHER']


def gfx_roof(colour, shade, part):
    """Roof (canopy layer, over a building): rows of shingles, lit on the upper slope and denser (darker) on the
    lower one, with a ridge between them. The eave row ends in a dark edge with the bottom of the tile left open, so
    the front wall shows beneath it."""
    base, _, side = part.partition('_')
    if base in ('CHIMNEY', 'WEATHER'):
        base = 'UPPER'
    elif base == 'VENT':
        base = 'LOWER'
    g = gfx_roof_slope(colour, shade, base)
    if side == 'L':
        for y in range(16):
            if g[y][0] != W_TRANS:
                g[y][0] = W_DARK
                g[y][1] = shade
    elif side == 'R':
        for y in range(16):
            if g[y][15] != W_TRANS:
                g[y][15] = W_DARK
                g[y][14] = shade
    if part == 'CHIMNEY':
        # A stack in a colour that stands out from the roof (stone on red roofs, brick on grey), a dark cap and a
        # shadow falling to the right.
        body, joint = (W_PAVE, W_PAVE_D) if colour == W_BRICK else (W_BRICK, W_MORTAR)
        for y in range(3, 13):
            g[y][11] = W_DARK
            g[y][12] = shade
            for x in range(5, 11):
                g[y][x] = joint if y % 3 == 1 or x in (5, 10) else body
        for x in range(4, 12):
            g[1][x] = g[2][x] = W_DARK
        for x in range(5, 11):
            g[2][x] = W_ROAD
    elif part == 'VENT':
        # Red roofs: a vent pipe; grey roofs: a skylight.
        if colour == W_BRICK:
            for y in range(4, 10):
                for x in range(6, 10):
                    g[y][x] = W_PAVE_D if x < 8 else W_ROAD
            for x in range(5, 11):
                g[4][x] = W_DARK
            g[10][6] = g[10][9] = W_DARK
        else:
            for y in range(3, 12):
                for x in range(3, 13):
                    g[y][x] = W_WATER_L if (x + y) % 6 else W_WATER
            for i in range(3, 13):
                g[3][i] = g[11][i] = W_DARK
            for y in range(3, 12):
                g[y][3] = g[y][12] = W_DARK
    elif part == 'WEATHER':
        # Red roofs: moss; grey roofs: rust stains.
        stain = W_GRASS if colour == W_BRICK else W_WOOD
        light = W_GRASS_L if colour == W_BRICK else W_MORTAR
        for x, y in ((3, 4), (4, 4), (4, 5), (5, 5), (5, 6), (10, 9), (11, 9), (11, 10), (12, 10), (7, 12),
                     (8, 12), (8, 13)):
            g[y][x] = stain
        for x, y in ((4, 3), (11, 8), (8, 11)):
            g[y][x] = light
    return g


def gfx_roof_eave_door(colour, shade):
    """The eave over a doorway in the front wall (playtest 1): its dark edge 3 px higher than the eave beside it and
    open below, so the door shows a little taller than the strip of wall, inside the building's outline."""
    g = gfx_roof_slope(colour, shade, 'LOWER')
    for x in range(16):
        g[7][x] = W_DARK
        for y in range(8, 16):
            g[y][x] = W_TRANS
    return g


def gfx_roof_slope(colour, shade, part):
    g = blank(16, 16, colour)
    lower = part in ('LOWER', 'EAVE')
    row_step = 2 if lower else 4
    for y in range(16):
        offset = 0 if (y // row_step) % 2 == 0 else 2
        for x in range(16):
            if y % row_step == row_step - 1:
                g[y][x] = shade
            elif (x + offset) % 4 == 0:
                g[y][x] = shade
    if part == 'RIDGE':
        for x in range(16):
            g[6][x] = W_DARK
            g[7][x] = colour
            g[8][x] = shade
    if part == 'EAVE':
        for x in range(16):
            g[10][x] = W_DARK
            for y in range(11, 16):
                g[y][x] = W_TRANS
    return g


# Tree kinds: trunk graphic and the canopy halves (dy=1: the tile above the trunk, dy=0: the trunk tile).
TREES = [('OAK', 'TREE_TRUNK', canopy_pixels), ('PINE', 'PINE_TRUNK', pine_pixels),
         ('BIRCH', 'BIRCH_TRUNK', birch_pixels)]


def canopy_name(kind, part):
    return 'CANOPY_' + part if kind == 'OAK' else kind + '_' + part


def tree_graphics():
    """Trunks and canopies of every tree kind; a tile under two trees (this one's lower half, the upper half of the
    tree below in front) gets a combined graphic per pair."""
    out = []
    for kind, trunk, canopy in TREES:
        if kind != 'OAK':
            out.append((trunk, gfx_pine_trunk() if kind == 'PINE' else gfx_birch_trunk()))
            out.append((canopy_name(kind, 'TOP'), canopy(1)))
            out.append((canopy_name(kind, 'BOTTOM'), canopy(0)))
    # Only same-kind stacks get their own combined graphic (pine and birch; oak over oak is CANOPY_FULL); where two
    # kinds meet, the lower tree's stack stands in, which saves 6 world graphics for base pieces and crops.
    for kind, _, kind_canopy in TREES:
        if kind != 'OAK':
            out.append(('CANOPY_%s_%s' % (kind, kind), overlay(kind_canopy(0), kind_canopy(1), W_TRANS)))
    return out


def full_canopy_name(upper, lower):
    return 'CANOPY_FULL' if lower == 'OAK' else 'CANOPY_%s_%s' % (lower, lower)


def furniture(top, front, trim, handles, top_rows=5):
    """3/4-view furniture: a top surface over a front face, outlined, filling the tile."""
    g = blank(16, 16, front)
    for y in range(1, top_rows):
        for x in range(1, 15):
            g[y][x] = top
    for i in range(16):
        g[0][i] = g[15][i] = W_DARK
        g[i][0] = g[i][15] = W_DARK
    for x in range(1, 15):
        g[top_rows][x] = trim
    for x, y in handles:
        g[y][x] = W_DARK
    return g


def gfx_kitchen_cabinet():
    g = furniture(W_PAVE, W_WOOD, W_MORTAR, [(5, 9), (10, 9)])
    for y in range(6, 15):
        g[y][7] = g[y][8] = W_MORTAR
    return g


def gfx_fridge():
    g = furniture(W_PAVE, W_PAVE, W_PAVE_D, [(13, 4), (13, 8), (13, 9), (13, 10)], top_rows=2)
    for x in range(1, 15):
        g[6][x] = W_PAVE_D
    return g


def gfx_wardrobe():
    g = furniture(W_MORTAR, W_WOOD, W_MORTAR, [(6, 9), (9, 9)], top_rows=3)
    for y in range(4, 15):
        g[y][7] = g[y][8] = W_MORTAR
    return g


def gfx_bath_cabinet():
    g = furniture(W_PAVE, W_PAVE, W_PAVE_D, [(7, 10), (8, 10)], top_rows=4)
    for y in range(5, 15):
        g[y][2] = g[y][13] = W_PAVE_D
    return g


def gfx_bed():
    g = furniture(W_PAVE, W_WATER, W_PAVE_D, [], top_rows=6)
    for y in range(7, 15):
        for x in range(1, 15):
            g[y][x] = W_WATER if y < 13 else W_DEEP
    for x in range(2, 14):
        g[2][x] = g[3][x] = W_PAVE
    for x in range(1, 15):
        g[6][x] = W_PAVE_D
    return g


def gfx_tool_shelf():
    g = furniture(W_PAVE_D, W_MORTAR, W_DARK, [], top_rows=3)
    for y in (8, 12):
        for x in range(1, 15):
            g[y][x] = W_PAVE_D
    for x, y, c in ((3, 6, W_LINE), (4, 6, W_LINE), (8, 7, W_PAVE), (9, 7, W_PAVE), (11, 10, W_WOOD),
                    (12, 10, W_WOOD), (13, 10, W_WOOD), (4, 11, W_WATER), (5, 11, W_WATER)):
        g[y][x] = c
    return g


def gfx_sand():
    """Beach and shore: golden sand with darker grains and a few pebbles."""
    rng = random.Random(800)
    g = blank(16, 16, W_LINE)
    speckle(g, rng, W_WOOD, 0.10)
    speckle(g, rng, W_PAVE, 0.03)
    return g


def gfx_soil():
    """Plowed field: brown earth in furrows."""
    rng = random.Random(810)
    g = blank(16, 16, W_WOOD)
    for y in (1, 5, 9, 13):
        for x in range(16):
            g[y][x] = W_MORTAR
            if rng.random() < 0.4:
                g[y + 1][x] = W_MORTAR
    speckle(g, rng, W_DARK, 0.02)
    return g


def gfx_crops():
    """Crop rows on plowed earth: green plants, some ripening."""
    rng = random.Random(820)
    g = gfx_soil()
    for row_y in (3, 7, 11, 15):
        for x in range(0, 16, 2):
            ripe = rng.random() < 0.25
            leaf_x = min(x + rng.randrange(2), 15)
            g[row_y][x] = W_GRASS
            g[row_y - 1][x] = W_LINE if ripe else W_GRASS_L
            g[row_y - 2][leaf_x] = W_GRASS
    return g


def gfx_rock():
    """Boulder (walls layer, solid but low enough to see past)."""
    g = blank(16, 16, W_TRANS)
    cx, cy, rx, ry = 7.5, 10.0, 6.5, 5.0
    for y in range(16):
        for x in range(16):
            d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
            if d <= 1:
                if d > 0.72:
                    c = W_DARK
                elif (x - cx) + (y - cy) > 2.5:
                    c = W_PAVE_D
                else:
                    c = W_PAVE
                g[y][x] = c
    for x, y in ((5, 8), (6, 8), (9, 11), (10, 12)):
        g[y][x] = W_PAVE_D
    return g


def gfx_bridge(horizontal):
    """Bridge deck over water: planks across the direction of travel, dark seams."""
    g = blank(16, 16, W_WOOD)
    for a in range(16):
        for b in (3, 7, 11, 15):
            if horizontal:
                g[a][b] = W_MORTAR
            else:
                g[b][a] = W_MORTAR
    for a, b in ((1, 1), (9, 5), (13, 9), (5, 13)):
        if horizontal:
            g[a][b] = W_DARK
        else:
            g[b][a] = W_DARK
    return g


from world14 import WORLD_GFX_14  # noqa: E402


WORLD_GFX = [
    ('EMPTY', blank(16, 16, W_TRANS)),
    ('GRASS_A', gfx_grass(0)),
    ('GRASS_B', gfx_grass(1)),
    ('GRASS_LUSH', gfx_grass_lush()),
    ('GRASS_FLOWERS', gfx_grass_flowers()),
    ('GRASS_DRY', gfx_grass_dry()),
    ('ROAD', gfx_road(None)),
    ('ROAD_LINE_H', gfx_road('h')),
    ('ROAD_LINE_V', gfx_road('v')),
    ('PAVEMENT', gfx_pavement()),
    ('FOREST_FLOOR', gfx_forest_floor()),
    ('MUD', gfx_mud()),
    ('SHALLOW_WATER', gfx_water(False)),
    ('DEEP_WATER', gfx_water(True)),
    ('WOOD_FLOOR', gfx_wood_floor()),
    ('WALL_FACE', gfx_wall_face()),
    ('WALL_TOP', gfx_wall_top()),
    ('TREE_TRUNK', gfx_trunk()),
    ('FENCE_H', gfx_fence(True)),
    ('FENCE_V', gfx_fence(False)),
    ('CANOPY_TOP', canopy_pixels(1)),
    ('CANOPY_BOTTOM', canopy_pixels(0)),
    ('CANOPY_FULL', overlay(canopy_pixels(0), canopy_pixels(1), W_TRANS)),
    ('KITCHEN_CABINET', gfx_kitchen_cabinet()),
    ('FRIDGE', gfx_fridge()),
    ('WARDROBE', gfx_wardrobe()),
    ('BATH_CABINET', gfx_bath_cabinet()),
    ('TOOL_SHELF', gfx_tool_shelf()),
    ('BED', gfx_bed()),
    ('SAND', gfx_sand()),
    ('SOIL', gfx_soil()),
    ('CROPS', gfx_crops()),
    ('ROCK', gfx_rock()),
    ('BRIDGE_H', gfx_bridge(True)),
    ('BRIDGE_V', gfx_bridge(False)),
    ('BUSH', gfx_bush()),
    ('TILE_FLOOR', gfx_tile_floor()),
    ('CONCRETE', gfx_concrete()),
    ('WINDOW_FACE', gfx_window_face()),
    ('WINDOW_TOP', gfx_window_top()),
    ('SHOP_SHELF', gfx_shop_shelf()),
    ('COUNTER', gfx_counter()),
    ('LOCKER', gfx_locker()),
    ('MED_CABINET', gfx_med_cabinet()),
    ('CRATE', gfx_crate()),
    ('DESK', gfx_desk()),
    ('PUMP', gfx_pump()),
    ('TENT', gfx_tent()),
] + [('ROOF_%s_%s' % (name, part), gfx_roof(colour, shade, part))
      for name, colour, shade in (('RED', W_BRICK, W_MORTAR), ('GREY', W_PAVE_D, W_ROAD))
      for part in ROOF_PARTS] + tree_graphics() + [
    ('BARRICADE_FACE', gfx_barricade_face()),
    ('BARRICADE_TOP', gfx_barricade_top()),
    ('BROKEN_FACE', gfx_broken_face()),
    ('BROKEN_TOP', gfx_broken_top()),
    ('RAIN_COLLECTOR', gfx_rain_collector()),
    ('CAMPFIRE', gfx_campfire()),
    ('PLOT_SOIL', gfx_plot(0)),
    ('PLOT_GROWING', gfx_plot(1)),
    ('PLOT_RIPE', gfx_plot(2)),
    ('GENERATOR', gfx_generator()),
    ('TRAP', gfx_trap()),
    ('CAN_LINE', gfx_can_line()),
    # Milestone 12.7: doors in front walls (FACE) and side walls (TOP).
    ('DOOR_FACE_CLOSED', gfx_door_face('closed')),
    ('DOOR_FACE_OPEN', gfx_door_face('open')),
    ('DOOR_FACE_BROKEN', gfx_door_face('broken')),
    ('DOOR_TOP_CLOSED', gfx_door_top('closed')),
    ('DOOR_TOP_OPEN', gfx_door_top('open')),
    ('DOOR_TOP_BROKEN', gfx_door_top('broken')),
    # Stage 4: a gun safe (police stations, some houses' bedrooms), a stash a note tells of.
    ('GUN_SAFE', gfx_gun_safe()),
    ('STASH', gfx_stash()),
    # Stage 5: the log wall (across, and running north) and a chopped tree's stump.
    ('LOG_WALL_H', gfx_log_wall(False)),
    ('LOG_WALL_V', gfx_log_wall(True)),
    ('STUMP', gfx_stump()),
    # Milestone 13 (room made by dropping the second grass and the weathered roofs): a snare, a village well, a
    # chest freezer and a television.
    ('SNARE', gfx_snare()),
    ('WELL', gfx_well()),
    ('CHEST_FREEZER', gfx_chest_freezer()),
    ('TV', gfx_tv()),
] + WORLD_GFX_14       # milestone 14: furniture, set pieces and the hunter stand (tools/assets/world14.py)

# Grain (playtest 1, mockup 17, look 2): a few pixels of the ground, floor, wall and roof textures a shade darker or
# lighter, drawn into the tiles so it stays put as the world scrolls (the GBA has no post-processing and no spare
# layer). A shade is a neighbouring colour of the same material in the palette; the decal graphics are built from
# these grids, so corpses and splats carry the same grain.
GRAIN_EVERY = 9            # about one pixel in this many
GRAIN_EVERY_FAR = {        # colours whose nearest shade is a long way off: rarer, or they read as dirt
    W_WOOD: 20, W_ROAD: 18, W_BRICK: 14,
}
GRAIN_SHADES = {           # colour: (darker, lighter); None where the palette has no close shade
    W_GRASS: (W_FOREST, W_GRASS_L), W_GRASS_L: (W_GRASS, None), W_FOREST: (W_CANOPY, W_GRASS),
    W_PAVE: (W_PAVE_D, None), W_PAVE_D: (W_ROAD, W_PAVE), W_ROAD: (W_DARK, None),
    W_WOOD: (W_MORTAR, None), W_BRICK: (W_MORTAR, None), W_LINE: (W_WOOD, None),
}
GRAIN_GRAPHICS = ['GRASS_A', 'GRASS_B', 'GRASS_LUSH', 'GRASS_FLOWERS', 'GRASS_DRY', 'FOREST_FLOOR', 'MUD', 'SAND',
                  'SOIL', 'PAVEMENT', 'CONCRETE', 'ROAD', 'ROAD_LINE_H', 'ROAD_LINE_V', 'WOOD_FLOOR', 'TILE_FLOOR',
                  'WALL_FACE', 'WALL_TOP']


# Painted graphics take a building's colours on some indices only (ROOF_, WALL_ and FLOOR_VARIANTS): their grain
# stays among those, or a speck would keep the drawn colour on a repainted roof, wall or floor.
GRAIN_KEEP_PAINTED = {
    'ROOF': {W_BRICK, W_MORTAR, W_PAVE_D, W_ROAD},
    'WALL': {W_BRICK, W_MORTAR},
    'FLOOR': {W_WOOD, W_MORTAR, W_PAVE, W_PAVE_D},
}


def grained(name, grid):
    if name not in GRAIN_GRAPHICS and not name.startswith('ROOF_'):
        return grid
    kind = 'ROOF' if name.startswith('ROOF_') else 'WALL' if name.startswith('WALL_') else         'FLOOR' if name in ('WOOD_FLOOR', 'TILE_FLOOR') else None
    keep = GRAIN_KEEP_PAINTED.get(kind)
    out = [list(row) for row in grid]
    seed = sum(ord(c) * (i + 1) for i, c in enumerate(name))
    for y in range(16):
        for x in range(16):
            value = ((x + 1) * 0x9E3779B1 ^ (y + 1) * 0x85EBCA77 ^ seed * 0x27D4EB2F) & 0xFFFFFFFF
            value ^= value >> 15
            value = (value * 0x2C1B3C6D) & 0xFFFFFFFF
            value ^= value >> 12
            if out[y][x] not in GRAIN_SHADES or value % GRAIN_EVERY_FAR.get(out[y][x], GRAIN_EVERY):
                continue
            darker, lighter = GRAIN_SHADES[out[y][x]]
            if keep is not None:
                if out[y][x] not in keep:
                    continue
                darker = darker if darker in keep else None
                lighter = lighter if lighter in keep else None
            pick = darker if ((value >> 8) & 1 or lighter is None) else lighter
            if pick is not None:
                out[y][x] = pick
    return out


WORLD_GFX = [(name, grained(name, grid)) for name, grid in WORLD_GFX]

def gfx_door_edge(right, opened):
    """Playtest 5 (mockup 23, look C): a door in a side wall seen past the wall line, on the canopy layer of the tile
    outside it, so a side door shows from outside under its roof: a 4 px slab of wood when closed, the door swung
    out along the top of the step when open. right: the door is in a right-hand wall (the tile is to its right)."""
    g = blank(16, 16, W_TRANS)
    if opened:
        for x in range(0, 14):
            g[1][x] = W_DARK
            g[2][x] = W_WOOD
            g[3][x] = W_WOOD
            g[4][x] = W_DARK
        g[2][13] = g[3][13] = W_DARK
    else:
        for y in range(1, 15):
            for x in range(0, 4):
                g[y][x] = W_DARK if y in (1, 14) or x == 3 else W_WOOD
    if not right:
        g = [row[::-1] for row in g]
    return g


def gfx_door_edge_top(opened):
    """Playtest 7: a door in a back wall (the top wall, outside to the north) seen past the roof's edge, on the
    canopy of the step outside it: a 4 px slab of wood along the bottom of the tile when closed, the door swung out
    up the left side of the step when open."""
    g = blank(16, 16, W_TRANS)
    if opened:
        for y in range(2, 16):
            g[y][1] = W_DARK
            g[y][2] = W_WOOD
            g[y][3] = W_WOOD
            g[y][4] = W_DARK
        g[2][2] = g[2][3] = W_DARK
    else:
        for y in range(12, 16):
            for x in range(1, 15):
                g[y][x] = W_DARK if x in (1, 14) or y == 12 else W_WOOD
    return g


def gfx_door_edge_boarded(where):
    """Playtest 7: a barricade in a side or back wall seen past the wall line: the ends of two planks, 6 px out.
    where: 'R' or 'L' (a side wall, the tile beside it), 'T' (a back wall, the step above it)."""
    g = blank(16, 16, W_TRANS)
    for first in (2, 10):
        for a in range(first, first + 4):
            for b in range(0, 6):
                edge = a in (first, first + 3) or b == 5
                if where == 'T':
                    g[15 - b][a] = W_DARK if edge else W_WOOD
                else:
                    g[a][b] = W_DARK if edge else W_WOOD
    if where == 'L':
        g = [row[::-1] for row in g]
    return g


# Playtest 1: doorways in outer walls are roofed; one in the front wall gets an eave that dips over it.
WORLD_GFX_LATE = [('ROOF_%s_EAVE_DOOR' % name, grained('ROOF_%s_EAVE_DOOR' % name, gfx_roof_eave_door(colour, shade)))
                  for name, colour, shade in (('RED', W_BRICK, W_MORTAR), ('GREY', W_PAVE_D, W_ROAD))]

# Playtest 5: side doors' edges past the wall line (after the eave doors, so saved ids stay put).
WORLD_GFX_LATE += [('DOOR_EDGE_%s_%s' % (side, state), gfx_door_edge(side == 'R', state == 'OPEN'))
                   for side in ('R', 'L') for state in ('CLOSED', 'OPEN')]

# Playtest 7: back doors' edges past the roof line, the same way (after the side doors', so saved ids stay put).
WORLD_GFX_LATE += [('DOOR_EDGE_T_%s' % state, gfx_door_edge_top(state == 'OPEN')) for state in ('CLOSED', 'OPEN')]
WORLD_GFX_LATE += [('DOOR_EDGE_%s_BOARDED' % where, gfx_door_edge_boarded(where)) for where in ('R', 'L', 'T')]
# Playtest 8: barricades show their wear (worn under 67%, failing under 34%).
WORLD_GFX_LATE += [('BARRICADE_%s_%s' % (where, stage_name),
                    (gfx_barricade_face_damaged if where == 'FACE' else gfx_barricade_top_damaged)(stage))
                   for where in ('FACE', 'TOP') for stage, stage_name in ((1, 'WORN'), (2, 'FAILING'))]

# Walls graphics drawn with the decal bank's colours (bank 1): a dead survivor lying where they fell.
WORLD_GFX_DECAL_BANK = [('BODY', None)]


# Decal palette bank: same as bank 0 except for colours no decal-capable ground uses.
WORLD_PALETTE_DECALS = list(WORLD_PALETTE)
W_Z_CLOTHES, W_Z_SKIN, W_BLOOD, W_BLOOD_D = W_BRICK, W_WATER, W_WATER_L, W_DEEP
WORLD_PALETTE_DECALS[W_Z_CLOTHES] = hexc('5a4a5e')
WORLD_PALETTE_DECALS[W_Z_SKIN] = hexc('8aa07a')
WORLD_PALETTE_DECALS[W_BLOOD] = hexc('d0453a')
WORLD_PALETTE_DECALS[W_BLOOD_D] = hexc('7a2a22')


def decal_blood(seed):
    rng = random.Random(seed)
    g = blank(16, 16, W_TRANS)
    cx, cy = 7.5 + rng.uniform(-1, 1), 8 + rng.uniform(-1, 1)
    for y in range(16):
        for x in range(16):
            d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
            wobble = 3.2 + 1.4 * rng.random()
            if d < wobble:
                g[y][x] = W_BLOOD_D if d > wobble - 1.2 or rng.random() < 0.25 else W_BLOOD
    for _ in range(6):
        x, y = rng.randrange(1, 15), rng.randrange(1, 15)
        g[y][x] = W_BLOOD if rng.random() < 0.5 else W_BLOOD_D
    return g


def decal_corpse():
    """A corpse (playtest 2): the zombie's own dead frame (body A in its pool of blood, as kx_zombies draws a corpse),
    where the corpse sprite lies in its tile (the feet 10 px down: the sprite's top 3 px down). On screen the corpse sprite, in
    the zombie's outfit and body, covers it exactly (every body's outline holds body A's); this shows only when more
    corpses are on screen than there are corpse sprites, in the decal bank's colours."""
    import characters as ch
    upper = ch.zombie_body(ch.SIDE_TOP, 'side', 'A')
    frame = ch.parse_frame(ch.zombie_arms('side', upper + ch.SIDE_LEGS['stand'], 'stand'), ch.ZOMBIE_KEYS)
    lying = ch.dead_frame(ch.lying(frame, True), ch.DEAD_STAGES - 1)     # in its full pool of blood
    colours = {0: W_TRANS, 1: W_DARK, 2: W_Z_SKIN, 3: W_Z_SKIN, 4: W_FOREST, 5: W_Z_CLOTHES, 6: W_Z_CLOTHES,
               7: W_Z_CLOTHES, 8: W_ROAD, 9: W_DARK, 10: W_BLOOD_D, 11: W_BLOOD, 12: W_FOREST, 13: W_Z_CLOTHES,
               14: W_Z_CLOTHES, 15: W_Z_CLOTHES}
    g = blank(16, 16, W_TRANS)
    for y in range(16 - CORPSE_TOP):
        for x in range(16):
            g[y + CORPSE_TOP][x] = colours[lying[y][x]]
    return g


# The corpse sprite's top row in its tile: the sprite is drawn 1 px below the feet (kx_zombies lying_feet_offset),
# whose spot is 10 px down the tile, so its 16 rows start 3 px down.
CORPSE_TOP = 3


def decal_items():
    """Dropped items: a tied sack with a can beside it."""
    rows = [
        '................',
        '................',
        '................',
        '................',
        '.......oo.......',
        '......oyyo......',
        '.....ooyyoo.....',
        '....owwwwwwo....',
        '...owwwwwwwwo...',
        '...owwwmwwwwo...',
        '...owwwwwwwwoo..',
        '...owwwwwwwoggo.',
        '....owwwwwwoggo.',
        '.....oooooo.oo..',
        '................',
        '................',
    ]
    keys = {'.': W_TRANS, 'o': W_DARK, 'w': W_WOOD, 'm': W_MORTAR, 'y': W_LINE, 'g': W_PAVE}
    return [[keys[c] for c in r] for r in rows]


DECALS = [('BLOOD_A', decal_blood(11)), ('BLOOD_B', decal_blood(23)), ('CORPSE', decal_corpse()),
          ('ITEMS', decal_items())]
# Milestone 12.7 dropped the decals on sand, soil (and crops) and bridges to free graphics for doors; milestone 14 (a
# tileset twice the size) brought them back. Playtest 1: every ground that takes decals has its own, so a corpse or
# a splat never brings a square of another ground (the grass looks, concrete, crops and road lines used to borrow
# grass A's, pavement's, soil's and the plain road's). New grounds go at the end: saved decals hold graphic ids.
DECAL_GROUNDS = ['GRASS_A', 'ROAD', 'PAVEMENT', 'FOREST_FLOOR', 'MUD', 'WOOD_FLOOR', 'TILE_FLOOR', 'SAND', 'SOIL',
                 'BRIDGE_H', 'BRIDGE_V',
                 'GRASS_B', 'GRASS_LUSH', 'GRASS_FLOWERS', 'GRASS_DRY', 'CONCRETE', 'CROPS', 'ROAD_LINE_H',
                 'ROAD_LINE_V']
# Grounds that would share another's decal graphics (a decal on them shows over the other one), saving VRAM.
DECAL_ALIASES = {}
# Decals drawn with another one's graphics (the second blood splat reuses the first).
SHARED_DECALS = {'BLOOD_B': 'BLOOD_A'}

# The world tileset fills two 16 KB charblocks (1,024 4bpp tiles, the most a regular background can use; milestone 14
# shrank the world maps to 32x32 cells to make room). world_gfx is a uint8_t enum whose COUNT must fit too: 255.
MAX_WORLD_GRAPHICS = 255


def world_graphics():
    """Every world graphic in world_gfx order as (name, 16x16 grid, palette bank), plus the decal ids
    {(ground, decal): id} and {decal graphic name: its ground}."""
    gfx = [(name, grid, 0) for name, grid in WORLD_GFX]
    gfx += [(name, decal_corpse(), 1) for name, _ in WORLD_GFX_DECAL_BANK]
    grids = dict(WORLD_GFX)
    decal_ids = {}
    base_of = {}

    for ground in DECAL_GROUNDS:
        for decal_name, decal_grid in DECALS:
            if decal_name in SHARED_DECALS:
                continue
            name = ground + '_' + decal_name
            decal_ids[(ground, decal_name)] = len(gfx)
            base_of[name] = ground
            gfx.append((name, overlay(grids[ground], decal_grid, W_TRANS), 1))

    for ground in DECAL_GROUNDS:
        for decal_name, shared in SHARED_DECALS.items():
            decal_ids[(ground, decal_name)] = decal_ids[(ground, shared)]

    for ground, shared in DECAL_ALIASES.items():
        for decal_name, _ in DECALS:
            decal_ids[(ground, decal_name)] = decal_ids[(shared, decal_name)]

    # Graphics added after the decals go last, so the ids saved games hold (decals, wall changes) stay put.
    gfx += [(name, grid, 0) for name, grid in WORLD_GFX_LATE]

    assert len(gfx) <= MAX_WORLD_GRAPHICS, 'world tileset too big for VRAM: %d graphics' % len(gfx)
    return gfx, decal_ids, base_of


# Building colour variants (palette banks 2-10, after the normal and decal banks): each building picks a roof, a
# wall and a floor variant; variant 0 is the colours as drawn. A bank only recolours the indices its kind uses.
ROOF_VARIANTS = [   # brown, slate, moss green (red and grey roofs alike)
    {W_BRICK: '6b4a2e', W_MORTAR: '3a2a1a', W_PAVE_D: '6b4a2e', W_ROAD: '3a2a1a'},
    {W_BRICK: '4a5f7a', W_MORTAR: '2a3548', W_PAVE_D: '4a5f7a', W_ROAD: '2a3548'},
    {W_BRICK: '4d6a3a', W_MORTAR: '2f4028', W_PAVE_D: '4d6a3a', W_ROAD: '2f4028'},
]
WALL_VARIANTS = [   # grey brick, white siding, yellow siding
    {W_BRICK: '6f6d64', W_MORTAR: '3a3d42'},
    {W_BRICK: 'c8c2b0', W_MORTAR: '8f8d82'},
    {W_BRICK: 'c9a33a', W_MORTAR: '8a6a42'},
]
FLOOR_VARIANTS = [  # light pine (white tiles), dark walnut (blue tiles), red carpet (green tiles)
    {W_WOOD: 'b08a5a', W_MORTAR: '6b4a2e', W_PAVE: 'c8c2b0', W_PAVE_D: '8f8d82'},
    {W_WOOD: '5a3a24', W_MORTAR: '2e1e14', W_PAVE: '8aa0b8', W_PAVE_D: '5a6a80'},
    {W_WOOD: '7a3a3a', W_MORTAR: '4a2020', W_PAVE: '8fa88a', W_PAVE_D: '5a7058'},
]
PAINT_KINDS = ['ROOF', 'WALL', 'FLOOR']
PAINTED = {
    'WALL': ['WALL_FACE', 'WALL_TOP', 'WINDOW_FACE', 'WINDOW_TOP', 'BARRICADE_FACE', 'BARRICADE_TOP',
             'BARRICADE_FACE_WORN', 'BARRICADE_FACE_FAILING', 'BARRICADE_TOP_WORN', 'BARRICADE_TOP_FAILING',
             'BROKEN_FACE', 'BROKEN_TOP', 'DOOR_FACE_CLOSED', 'DOOR_FACE_OPEN', 'DOOR_FACE_BROKEN'],
    'FLOOR': ['WOOD_FLOOR', 'TILE_FLOOR'],
}

# Random hardware flips (8x8 pieces) that break up repeats without new tiles: both ways for noisy textures, sideways
# only for rows (planks, furrows, shingles), and a whole-tile mirror for props and trees (decided per column, so a
# tree's trunk and canopy agree).
FLIP_BOTH = ['GRASS_A', 'GRASS_B', 'GRASS_LUSH', 'GRASS_FLOWERS', 'GRASS_DRY', 'SAND', 'FOREST_FLOOR', 'MUD',
             'CONCRETE', 'SHALLOW_WATER', 'DEEP_WATER', 'ROAD']
FLIP_SIDEWAYS = ['WOOD_FLOOR', 'SOIL', 'CROPS']
FLIP_NONE, FLIP_PIECES_SIDEWAYS, FLIP_PIECES_BOTH, FLIP_WHOLE, FLIP_WHOLE_TILE = range(5)


def flip_kind(name):
    if name.endswith('_CORPSE'):
        return FLIP_WHOLE_TILE      # mirrored per tile (world::decal_mirrored), like the dying zombie
    if name in FLIP_BOTH:
        return FLIP_PIECES_BOTH
    if name in FLIP_SIDEWAYS:
        return FLIP_PIECES_SIDEWAYS
    if name.startswith('ROOF_'):
        part = name.split('_', 2)[2]
        return FLIP_NONE if part.endswith(('_L', '_R')) or part in ('CHIMNEY', 'VENT') else FLIP_PIECES_SIDEWAYS
    if name.endswith('TRUNK') or name in ('BUSH', 'ROCK') or name.startswith(('CANOPY_', 'PINE_', 'BIRCH_')):
        return FLIP_WHOLE
    return FLIP_NONE


def paint_kind(name):
    if name.startswith('ROOF_'):
        return 1
    for index, kind in enumerate(PAINT_KINDS):
        if name in PAINTED.get(kind, []):
            return index + 1
    return 0


def variant_bank(swaps, base=None):
    bank = list(base or WORLD_PALETTE)
    for index, value in swaps.items():
        bank[index] = hexc(value)
    return bank


def build_world():
    gfx, decal_ids, base_of = world_graphics()

    tiles = []
    for _, grid, _ in gfx:
        tiles.extend(split_16(grid))
    save_bmp('world_tiles', stack_tiles_8x8(tiles), WORLD_PALETTE,
             {'type': 'regular_bg_tiles', 'bpp_mode': 'bpp_4'})
    banks = [WORLD_PALETTE, WORLD_PALETTE_DECALS]
    banks += [variant_bank(swaps) for swaps in ROOF_VARIANTS + WALL_VARIANTS]
    # Floor colours start from the decal bank: floors use none of the indices it gives the blood and corpse colours,
    # so a decal on a painted floor takes the floor's colours too (playtest 1: it used to show the drawn floor).
    decal_indices = {W_Z_CLOTHES, W_Z_SKIN, W_BLOOD, W_BLOOD_D}
    grids = dict(WORLD_GFX)
    for floor in PAINTED['FLOOR']:
        assert not decal_indices & {v for row in grids[floor] for v in row}, floor + ' uses a decal colour index'
    banks += [variant_bank(swaps, WORLD_PALETTE_DECALS) for swaps in FLOOR_VARIANTS]
    save_bmp('world_palette', blank(8, 8), [colour for bank in banks for colour in bank],
             {'type': 'bg_palette', 'bpp_mode': 'bpp_4', 'colors_count': 16 * len(banks)})

    names = [name for name, _, _ in gfx]
    lines = ['#include <cstdint>', '', 'namespace kx', '{', '',
             '// 16x16 world graphics; graphic N uses hardware tiles 4N..4N+3 (TL, TR, BL, BR).',
             'enum class world_gfx : uint8_t', '{']
    lines += ['    ' + n + ',' for n in names]
    lines += ['    COUNT', '};', '',
              'enum class world_decal : uint8_t', '{']
    lines += ['    ' + n + ',' for n, _ in DECALS]
    lines += ['    COUNT', '};', '',
              '// Palette bank of each graphic (bank 1 holds the blood and corpse colours).',
              'constexpr uint8_t world_gfx_banks[] = {']
    lines += ['    ' + ', '.join(str(b) for _, _, b in gfx[i:i + 16]) + ',' for i in range(0, len(gfx), 16)]
    lines += ['};', '',
              '// Hardware flips per graphic: 0 none, 1 8x8 pieces sideways, 2 pieces both ways, 3 whole tile sideways (per',
              '// column), 4 whole tile sideways per tile (corpses, world::decal_mirrored).',
              'constexpr uint8_t world_gfx_flips[] = {']
    flips = [flip_kind(n) for n in names]
    lines += ['    ' + ', '.join(str(v) for v in flips[i:i + 16]) + ',' for i in range(0, len(gfx), 16)]
    lines += ['};', '',
              '// Which building colour a graphic takes: 0 none, 1 roof, 2 wall, 3 floor.',
              'constexpr uint8_t world_gfx_paints[] = {']
    paints = [paint_kind(n) if b == 0 else paint_kind(base_of[n]) if n in base_of else 0 for n, _, b in gfx]
    lines += ['    ' + ', '.join(str(v) for v in paints[i:i + 16]) + ',' for i in range(0, len(gfx), 16)]
    first = 2
    lines += ['};', '',
              '// Palette bank per building colour variant (0: as drawn), per paint kind (roof, wall, floor).',
              'constexpr int world_paint_variants = %d;' % (len(ROOF_VARIANTS) + 1),
              'constexpr uint8_t world_paint_banks[][%d] = {' % (len(ROOF_VARIANTS) + 1)]
    for kind, variants in zip(PAINT_KINDS, (ROOF_VARIANTS, WALL_VARIANTS, FLOOR_VARIANTS)):
        lines.append('    { 0, %s },   // %s' % (', '.join(str(first + i) for i in range(len(variants))), kind))
        first += len(variants)
    lines += ['};', '',
              '// Ground graphic without its decal (itself for plain graphics).',
              'constexpr uint8_t world_gfx_base[] = {']
    base_ids = [names.index(base_of.get(n, n)) for n in names]
    lines += ['    ' + ', '.join(str(b) for b in base_ids[i:i + 16]) + ',' for i in range(0, len(gfx), 16)]
    lines += ['};', '',
              '// Decal version of each ground graphic, per world_decal; 0 where decals are not drawn (water, walls).',
              'constexpr uint8_t world_gfx_decals[][int(world_decal::COUNT)] = {']
    for n in names:
        row = [decal_ids.get((n, d), 0) for d, _ in DECALS]
        lines.append('    { ' + ', '.join(str(v) for v in row) + ' },   // ' + n)
    lines += ['};', '',
              '// Tree kinds (index 0 is no tree).',
              'enum class tree_kind : uint8_t', '{', '    NONE,']
    lines += ['    %s,' % kind for kind, _, _ in TREES]
    lines += ['    COUNT', '};', '',
              '// Trunk graphic per tree kind.',
              'constexpr uint8_t world_tree_trunks[] = { 0, %s };' % ', '.join(
                  str(names.index(trunk)) for _, trunk, _ in TREES), '',
              '// Canopy of a tile from the tree on it and the tree below it: [this tile][tile below].',
              'constexpr uint8_t world_tree_canopies[][int(tree_kind::COUNT)] = {']
    kinds = [None] + [kind for kind, _, _ in TREES]
    for upper in kinds:
        row = []
        for lower in kinds:
            if upper is None and lower is None:
                row.append(0)
            elif upper is None:
                row.append(names.index(canopy_name(lower, 'TOP')))
            elif lower is None:
                row.append(names.index(canopy_name(upper, 'BOTTOM')))
            else:
                row.append(names.index(full_canopy_name(upper, lower)))
        lines.append('    { ' + ', '.join(str(v) for v in row) + ' },   // ' + (upper or 'NONE'))
    lines += ['};', '', '}']
    write_header('kx_world_gfx_ids.h', lines)
    write_buildings_header(names)
