"""Milestone 12.7 mockup: doors, locks, gun safes and notes, drawn at 240x160 from the game's own world graphics,
sprites, 5 px font and palette, four moments in a 2x2 sheet, saved at 3x.

    python tools/mockup_doors.py        -> mockups/12-doors.png

1. Outside a house: its front door closed, the OPEN prompt; a log wall and a stump in the garden.
2. The door is locked: a crowbar in hand forces it (hold A), the action bar over the head; a zombie hears it.
3. Inside (the roof cut away): the front door open, an inner door closed with a zombie banging on it, a broken one,
   a side door open; a gun safe with its prompt, and a note just read.
4. A note read: its box, and its place goes on the map.
5. The gun safe's dial: three digits, the code from the note shown below them.
6. The Map tab: automatic marks for a key's house and a note's safe, next to your own.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'assets'))

from mockup_character import ACCENT, BORDER, DIM, FILL, QUIET, SCREEN, TEXT, Screen  # noqa: E402
import buildings  # noqa: E402
import world  # noqa: E402

ROOT = os.path.join(HERE, '..', 'mockups')
GRAPHICS = os.path.join(HERE, '..', 'graphics')
TILE = 16
HOUSE_X = 3          # where the small house's top-left tile sits on screen, in tiles
HOUSE_Y = -1         # outside; inside, the camera is a tile lower (the house one tile further down)
PLAYER_FRAMES = {'down': 0, 'up': 6, 'side': 12}
ZOMBIE_FRAMES = {'down': 0, 'up': 5, 'side': 10}


def world_grids():
    gfx, _, _ = world.world_graphics()
    grids = {name: grid for name, grid, _ in gfx}
    grids.update({
        'DOOR_FACE_CLOSED': world.gfx_door_face('closed'), 'DOOR_FACE_OPEN': world.gfx_door_face('open'),
        'DOOR_FACE_BROKEN': world.gfx_door_face('broken'), 'DOOR_TOP_CLOSED': world.gfx_door_top('closed'),
        'DOOR_TOP_OPEN': world.gfx_door_top('open'), 'DOOR_TOP_BROKEN': world.gfx_door_top('broken'),
        'LOG_WALL_H': world.gfx_log_wall(False), 'LOG_WALL_V': world.gfx_log_wall(True),
        'STUMP': world.gfx_stump(), 'GUN_SAFE': world.gfx_gun_safe(),
    })
    return grids


GRIDS = world_grids()
PALETTE = world.WORLD_PALETTE
# The house takes a wall colour variant (white siding) as a generated one would; door frames take it with the
# walls (stage 2 adds them to the painted list).
WALL_PALETTE = world.variant_bank(world.WALL_VARIANTS[1])


def painted(name):
    return name in world.PAINTED['WALL'] or name.startswith('DOOR_FACE')


def put(s, name, tx, ty):
    grid = GRIDS[name]
    palette = WALL_PALETTE if painted(name) else PALETTE
    for y in range(TILE):
        for x in range(TILE):
            value = grid[y][x]
            if value:
                px, py = tx * TILE + x, ty * TILE + y
                if 0 <= px < 240 and 0 <= py < 160:
                    s.pixels[px, py] = palette[value]


def sprite(s, sheet, frame, feet_x, feet_y, mirror=False):
    image = Image.open(os.path.join(GRAPHICS, sheet + '.bmp'))
    colours = image.getpalette()
    tile = image.crop((0, frame * 16, 16, frame * 16 + 16))
    if mirror:
        tile = tile.transpose(Image.FLIP_LEFT_RIGHT)
    for y in range(16):
        for x in range(16):
            value = tile.getpixel((x, y))
            if value:
                px, py = feet_x - 8 + x, feet_y - 16 + y
                if 0 <= px < 240 and 0 <= py < 160:
                    s.pixels[px, py] = tuple(colours[value * 3:value * 3 + 3])


def held(s, look, angle, hand_x, hand_y):
    """A held item from the weapons sheet (32x32 frames, the hand at the centre)."""
    import weapons
    image = Image.open(os.path.join(GRAPHICS, 'weapons.bmp'))
    colours = image.getpalette()
    frame = weapons.LOOKS.index(look) * weapons.ANGLES + angle
    for y in range(32):
        for x in range(32):
            value = image.getpixel((x, frame * 32 + y))
            if value:
                px, py = hand_x - 16 + x, hand_y - 16 + y
                if 0 <= px < 240 and 0 <= py < 160:
                    s.pixels[px, py] = tuple(colours[value * 3:value * 3 + 3])


def house_layers():
    rows = buildings.with_front_step(buildings.TEMPLATES[0][3])
    names = {}

    class Ids(dict):
        def __missing__(self, key):
            names[len(names) + 1] = key
            self[key] = len(names)
            return self[key]

    ids = Ids()
    ground, walls, roofs = buildings.tile_graphics(rows, 'o', 'RED', ids, 'SMALL_HOUSE')
    width = len(rows[0])

    def layer(values):
        return [[names.get(values[y * width + x]) for x in range(width)] for y in range(len(rows))]

    return rows, layer(ground), layer(walls), layer(roofs)


def doorways(rows):
    """Doorways: a floor tile with walls either side (front wall, 'FACE') or above and below (side wall, 'TOP')."""
    result = {}
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c not in buildings.DOOR_FLOORS:
                continue
            left = x > 0 and row[x - 1] in buildings.WALLS
            right = x + 1 < len(row) and row[x + 1] in buildings.WALLS
            up = y > 0 and rows[y - 1][x] in buildings.WALLS
            down = y + 1 < len(rows) and rows[y + 1][x] in buildings.WALLS
            if left and right:
                result[(x, y)] = 'FACE'
            elif up and down:
                result[(x, y)] = 'TOP'
    return result


def scene(inside, door_states, safe=False):
    """The small house on a street: grass, a paved step and pavement, the road below. door_states maps a doorway
    (template x, y) to closed, open or broken; the rest are closed."""
    global HOUSE_Y
    HOUSE_Y = 0 if inside else -1
    s = Screen()
    street = HOUSE_Y + 9
    for ty in range(10):
        for tx in range(15):
            put(s, 'GRASS_A' if ty < street else 'PAVEMENT' if ty == street else 'ROAD', tx, ty)
    for tx in range(15):
        if street + 2 <= 9:
            put(s, 'ROAD_LINE_H' if tx % 2 else 'ROAD', tx, street + 2)

    rows, ground, walls, roofs = house_layers()
    doors = doorways(rows)
    for y in range(len(rows)):
        for x in range(len(rows[0])):
            tx, ty = HOUSE_X + x, HOUSE_Y + y
            if ground[y][x]:
                put(s, ground[y][x], tx, ty)
            name = walls[y][x]
            if (x, y) in doors:
                name = 'DOOR_%s_%s' % (doors[(x, y)], door_states.get((x, y), 'closed').upper())
            if safe and rows[y][x] == 'D':
                name = 'GUN_SAFE'
            if name:
                put(s, name, tx, ty)
            if not inside and roofs[y][x]:
                put(s, roofs[y][x], tx, ty)

    # The garden: a log wall along the left of the lot, a stump and a tree to the right.
    for ty in range(1, 6):
        put(s, 'LOG_WALL_V', 1, ty + HOUSE_Y + 1)
    put(s, 'LOG_WALL_H', 0, 7 + HOUSE_Y)
    put(s, 'STUMP', 13, 5 + HOUSE_Y)
    put(s, 'TREE_TRUNK', 13, 2 + HOUSE_Y)
    put(s, 'CANOPY_BOTTOM', 13, 1 + HOUSE_Y)
    return s


def hud(s, clock='1 JUL ` 14:20'):
    width = s.width(clock) + 6
    s.panel(6, 6, width, 9)
    s.text(9, 8, clock, TEXT)
    s.panel(196, 6, 38, 9)
    s.text(199, 8, 'NOISE', TEXT)
    for i in range(4):
        s.rect(221 + i * 3, 12 - i, 2, i + 1, ACCENT if i < 1 else BORDER)


def prompt(s, centre_x, top, text):
    width = s.width(text) + 6
    x = centre_x - width // 2
    s.panel(x, top, width, 9, ACCENT)
    s.text(x + 3, top + 2, text, TEXT)


def action_bar(s, centre_x, top, eighths):
    s.panel(centre_x - 22, top, 44, 9)
    s.rect(centre_x - 19, top + 3, 38, 3, BORDER)
    s.rect(centre_x - 19, top + 3, 38 * eighths // 8, 3, QUIET)


def message(s, lines):
    width = max(s.width(line) for line in lines) + 8
    x = (240 - width) // 2
    s.panel(x, 34, width, 4 + len(lines) * 7, ACCENT)
    for index, line in enumerate(lines):
        s.text(x + 4, 37 + index * 7, line, TEXT)


FRONT_DOOR = (5, 7)          # template tiles of the small house facing south
BATH_DOOR = (2, 4)
BED_DOOR = (6, 4)
SIDE_DOOR = (4, 3)


def door_feet(door):
    return (HOUSE_X + door[0]) * TILE + 8, (HOUSE_Y + door[1] + 1) * TILE + 12


def panel_outside():
    s = scene(False, {})
    hud(s)
    fx, fy = door_feet(FRONT_DOOR)
    sprite(s, 'player', PLAYER_FRAMES['up'], fx, fy)
    prompt(s, fx, (HOUSE_Y + FRONT_DOOR[1]) * TILE - 12, '[A] OPEN')
    return s


def panel_force():
    s = scene(False, {})
    hud(s, '1 JUL ` 14:21')
    fx, fy = door_feet(FRONT_DOOR)
    sprite(s, 'player', PLAYER_FRAMES['up'], fx, fy)
    held(s, 'CROWBAR', 26, fx + 4, fy - 9)
    prompt(s, fx, (HOUSE_Y + FRONT_DOOR[1]) * TILE - 12, '[A] FORCE')
    action_bar(s, fx, fy - 30, 5)
    sprite(s, 'zombie', ZOMBIE_FRAMES['side'], 214, 150, mirror=True)
    # The noise meter is up: forcing a door is loud.
    for i in range(4):
        s.rect(221 + i * 3, 12 - i, 2, i + 1, ACCENT)
    return s


INSIDE_DOORS = {FRONT_DOOR: 'open', BED_DOOR: 'broken', SIDE_DOOR: 'open'}


def panel_inside():
    s = scene(True, INSIDE_DOORS, safe=True)
    hud(s, '1 JUL ` 14:24')
    # A zombie in the bathroom bangs on its closed door (an impact spark on the door).
    zx, zy = door_feet((BATH_DOOR[0], BATH_DOOR[1] + 1))
    sprite(s, 'zombie', ZOMBIE_FRAMES['up'], zx, zy - 2)
    for x, y in ((0, -3), (-2, -1), (2, -1), (0, 1), (0, -1)):
        s.rect(zx + x, zy - 24 + y, 1, 1, TEXT)
    # The player in the bedroom, by the safe (it stands where the wardrobe was).
    sx = (HOUSE_X + 7) * TILE + 8
    sy = (HOUSE_Y + 3) * TILE + 12
    sprite(s, 'player', PLAYER_FRAMES['up'], sx, sy)
    prompt(s, sx, (HOUSE_Y + 2) * TILE - 12, '[A] OPEN SAFE')
    return s


def panel_note():
    s = scene(True, INSIDE_DOORS, safe=True)
    hud(s, '1 JUL ` 14:23')
    sprite(s, 'player', PLAYER_FRAMES['down'], (HOUSE_X + 5) * TILE + 8, (HOUSE_Y + 3) * TILE + 12)
    message(s, ['NOTE: THE SAFE IN THE BEDROOM', 'AT 12 MILL ROAD. CODE 4-1-7.', 'DON\'T LET DAD FIND OUT.'])
    return s


def panel_map():
    s = Screen()
    base = Image.open(os.path.join(ROOT, '05-region-map.png')).convert('RGB').resize((240, 160), Image.NEAREST)
    s.image = base
    s.pixels = base.load()

    # Automatic marks: a key's house (by Breza) and a note's safe (Kamen), in cream so they differ from yours.
    def key_icon(x, y):
        s.rect(x - 1, y - 1, 9, 7, FILL)
        s.rect(x, y, 3, 3, TEXT)
        s.rect(x + 1, y + 1, 1, 1, FILL)
        s.rect(x + 3, y + 1, 4, 1, TEXT)
        s.rect(x + 5, y + 2, 1, 2, TEXT)

    def note_icon(x, y):
        s.rect(x - 1, y - 1, 7, 9, FILL)
        s.rect(x, y, 5, 7, TEXT)
        for row in (2, 4):
            s.rect(x + 1, y + row, 3, 1, DIM)

    # Mockup 05 was drawn at 3x with a browser font that blurs when scaled down: the tab bar and the names are
    # drawn again with the game's font.
    s.rect(0, 0, 240, 20, SCREEN)
    s.tab_bar(4)
    for name, x, y in (('STARA PILA', 127, 38), ('KAMEN', 162, 51), ('BREZA', 64, 65), ('VYSOKO', 169, 108),
                       ('DOLINA', 143, 121), ('PRISTAN', 71, 134)):
        s.rect(x - 2, y - 2, s.width(name) + 4, 9, FILL)
        s.text(x, y, name, TEXT)

    key_icon(92, 61)
    note_icon(148, 58)
    s.rect(0, 144, 240, 16, SCREEN)
    s.hints(147, [('A', 'MARK'), ('B', 'BACK')])
    s.text(150, 147, 'KEY', TEXT)
    key_icon(165, 147)
    s.text(184, 147, 'NOTE', TEXT)
    note_icon(204, 146)
    return s


def panel_dial():
    s = Screen()
    s.rect(0, 0, 240, 160, SCREEN)
    s.panel(60, 24, 120, 100)
    s.text_centered(120, 30, 'GUN SAFE', ACCENT)
    s.text_centered(120, 38, '12 MILL ROAD, ASHFORD', QUIET)
    s.rect(64, 46, 112, 1, BORDER)

    digits = [4, 1, 5]
    selected = 2
    for index, digit in enumerate(digits):
        cx = 92 + index * 28
        s.panel(cx - 10, 56, 21, 38, ACCENT if index == selected else BORDER)
        s.text_centered(cx, 59, str((digit + 9) % 10), DIM)
        big = Screen()
        big.image = Image.new('RGB', (8, 6), FILL)
        big.pixels = big.image.load()
        big.text(1, 0, str(digit), TEXT)
        scaled = big.image.resize((16, 12), Image.NEAREST)
        s.image.paste(scaled, (cx - 8, 69))
        s.text_centered(cx, 86, str((digit + 1) % 10), DIM)
        if index == selected:
            for row in range(3):
                s.rect(cx - row, 49 + row, 1 + row * 2, 1, ACCENT)
    s.text_centered(120, 100, 'FROM A NOTE: 4-1-7', QUIET)
    s.text_centered(120, 110, 'WRONG CODE: IT CLICKS', DIM)

    s.rect(0, 144, 240, 16, SCREEN)
    s.hints(147, [('<>', 'DIGIT'), ('UP/DOWN', 'TURN'), ('A', 'TRY')], [('B', 'LEAVE')])
    return s


def caption(width, lines):
    image = Image.new('RGB', (width, 22), (0x05, 0x07, 0x0b))
    s = Screen()
    s.image = image
    s.pixels = image.load()
    y = 3
    for text, colour in lines:
        s.text(4, y, text, colour)
        y += 8
    return image


def main():
    panels = [
        (panel_outside(), [('1 A CLOSED FRONT DOOR: A OPENS IT', ACCENT), ('LOG WALL LEFT, A STUMP RIGHT', QUIET)]),
        (panel_force(), [('2 LOCKED: A CROWBAR IN HAND FORCES IT', ACCENT), ('2 S, LOUD, THE DOOR BREAKS', QUIET)]),
        (panel_inside(), [('3 INSIDE: OPEN, CLOSED, BROKEN DOORS', ACCENT),
                          ('A ZOMBIE BANGS ON THE BATHROOM DOOR', QUIET)]),
        (panel_note(), [('4 A NOTE READ (GEAR TAB, READ)', ACCENT), ('ITS SAFE GOES ON THE MAP', QUIET)]),
        (panel_dial(), [('5 THE SAFE DIAL: 3 DIGITS', ACCENT), ('THE NOTE\'S CODE SHOWN ONCE READ', QUIET)]),
        (panel_map(), [('6 MAP: AUTOMATIC MARKS IN CREAM', ACCENT), ('A KEY\'S HOUSE, A NOTE\'S SAFE', QUIET)]),
    ]
    gap = 4
    width = 240 * 3 + gap * 2
    height = (160 + 22) * 2 + gap
    sheet = Image.new('RGB', (width, height), (0x05, 0x07, 0x0b))

    for index, (screen, lines) in enumerate(panels):
        x = (index % 3) * (240 + gap)
        y = (index // 3) * (160 + 22 + gap)
        sheet.paste(screen.image, (x, y))
        sheet.paste(caption(240, lines), (x, y + 160))

    sheet = sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST)
    path = os.path.join(ROOT, '12-doors.png')
    sheet.save(path)
    print('wrote mockups/12-doors.png')


if __name__ == '__main__':
    main()
