"""Milestone 13 mockup: the wild and loose ends, drawn at 240x160 from the game's own world graphics, sprites, 5 px
font and palette, six moments in a 3x2 sheet, saved at 3x.

    python tools/mockup_wild.py        -> mockups/13-wild.png

1. Swimming across a river: the swim frame and its ripple; two zombies wait at the shore.
2. The wild: a deer grazing, another fleeing, a rabbit, a flock of birds taking off from the road at a shot.
3. Butchering a deer: a knife in hand, hold A, the action bar over the head.
4. A village: a well (A fills a bottle with tainted water) and a snare that caught a rabbit overnight.
5. A kitchen: a chest freezer beside the fridge, a TV in the living room showing a broadcast.
6. World creation's sandbox settings list.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'assets'))

from mockup_character import ACCENT, BORDER, DIM, QUIET, RED, SCREEN, TEXT, Screen  # noqa: E402
from mockup_doors import action_bar, caption, held, hud, prompt, put, sprite  # noqa: E402
import animals  # noqa: E402

ROOT = os.path.join(HERE, '..', 'mockups')
GRAPHICS = os.path.join(HERE, '..', 'graphics')
TILE = 16
PLAYER_FRAMES = {'down': 0, 'up': 6, 'side': 12, 'crouch_side': 15}
ZOMBIE_FRAMES = {'down': 0, 'up': 5, 'side': 10}
SWIM = 31                      # kx::sprite_frames::player_swim: per facing block (down, up, side), two strokes
DEER = {name: i for i, name in enumerate(animals.DEER_FRAMES)}
RABBIT = {name: len(animals.DEER_FRAMES) + i for i, name in enumerate(animals.RABBIT_FRAMES)}


def blit(s, sheet, width, height, frame, left, top, mirror=False):
    """A frame of a sprite sheet with its top-left corner at (left, top)."""
    image = Image.open(os.path.join(GRAPHICS, sheet + '.bmp'))
    colours = image.getpalette()
    tile = image.crop((0, frame * height, width, frame * height + height))
    if mirror:
        tile = tile.transpose(Image.FLIP_LEFT_RIGHT)
    for y in range(height):
        for x in range(width):
            value = tile.getpixel((x, y))
            if value:
                px, py = left + x, top + y
                if 0 <= px < 240 and 0 <= py < 160:
                    s.pixels[px, py] = tuple(colours[value * 3:value * 3 + 3])


def fill(s, name, tx0, ty0, tx1, ty1):
    for ty in range(ty0, ty1):
        for tx in range(tx0, tx1):
            put(s, name, tx, ty)


def tree(s, kind, tx, ty):
    """A tree on tile (tx, ty): its trunk, the canopy's lower half over it and the upper half on the tile above."""
    trunk = {'OAK': 'TREE_TRUNK', 'PINE': 'PINE_TRUNK', 'BIRCH': 'BIRCH_TRUNK'}[kind]
    lower = {'OAK': 'CANOPY_BOTTOM', 'PINE': 'PINE_BOTTOM', 'BIRCH': 'BIRCH_BOTTOM'}[kind]
    upper = {'OAK': 'CANOPY_TOP', 'PINE': 'PINE_TOP', 'BIRCH': 'BIRCH_TOP'}[kind]
    put(s, trunk, tx, ty)
    put(s, lower, tx, ty)
    put(s, upper, tx, ty - 1)


def bar_meter(s, x, y, label, eighths, colour):
    """A small labelled meter (endurance while swimming)."""
    width = s.width(label) + 6 + 34
    s.panel(x, y, width, 9)
    s.text(x + 3, y + 2, label, TEXT)
    left = x + 3 + s.width(label) + 3
    s.rect(left, y + 3, 30, 3, BORDER)
    s.rect(left, y + 3, 30 * eighths // 8, 3, colour)


def toast(s, text, colour=TEXT):
    width = s.width(text) + 8
    x = (240 - width) // 2
    s.panel(x, 130, width, 11)
    s.text(x + 4, 133, text, colour)


def panel_swim():
    s = Screen()
    fill(s, 'GRASS_A', 0, 0, 15, 2)
    fill(s, 'SAND', 0, 2, 15, 3)
    fill(s, 'SHALLOW_WATER', 0, 3, 15, 4)
    fill(s, 'DEEP_WATER', 0, 4, 15, 7)
    fill(s, 'SHALLOW_WATER', 0, 7, 15, 8)
    fill(s, 'GRASS_A', 0, 8, 15, 10)
    for tx in (1, 4, 11, 13):
        tree(s, 'BIRCH' if tx % 2 else 'OAK', tx, 1)
    put(s, 'BUSH', 7, 0)
    tree(s, 'PINE', 0, 9)
    tree(s, 'PINE', 14, 9)

    hud(s, '3 JUL ` 18:40')
    # The player swims north mid-river: the swim frame (stroke 2) cut at the waterline, the ripple over the cut.
    px, top = 118, 66
    blit(s, 'player', 16, 16, SWIM + 2 + 1, px - 8, top)
    blit(s, 'ripple', 16, 8, 0, px - 8, top + 7)
    # Two zombies stopped at the shore behind, staring after you.
    sprite(s, 'zombie', ZOMBIE_FRAMES['up'], 104, 140)
    sprite(s, 'zombie', ZOMBIE_FRAMES['up'], 138, 142)
    sprite(s, 'zombie', ZOMBIE_FRAMES['side'], 62, 146)
    bar_meter(s, 6, 17, 'ENDURANCE', 3, RED)
    return s


def panel_wild():
    s = Screen()
    fill(s, 'FOREST_FLOOR', 0, 0, 6, 10)
    fill(s, 'GRASS_LUSH', 6, 0, 15, 7)
    put(s, 'GRASS_FLOWERS', 10, 2)
    put(s, 'GRASS_FLOWERS', 13, 5)
    fill(s, 'ROAD', 6, 7, 15, 8)
    for tx in range(6, 15):
        put(s, 'ROAD_LINE_H' if tx % 2 else 'ROAD', tx, 8)
    fill(s, 'GRASS_DRY', 6, 9, 15, 10)
    for tx, ty in ((1, 2), (3, 1), (0, 5), (4, 4), (2, 7), (5, 9)):
        tree(s, 'PINE', tx, ty)
    put(s, 'BUSH', 6, 4)

    hud(s, '4 JUL ` 07:15')
    # A deer grazes at the edge of the wood; a second one bolts away from the shot.
    blit(s, 'animals', 16, 16, DEER['side_stand'], 112, 30, mirror=True)
    blit(s, 'animals', 16, 16, DEER['side_step'], 196, 50)
    # A rabbit in the grass, sitting up.
    blit(s, 'animals', 16, 16, RABBIT['sit'], 150, 76)
    # The flock on the road takes off: two still perched, three in the air.
    for x, y, frame in ((152, 118, 0), (164, 121, 0), (172, 104, 1), (186, 98, 2), (200, 92, 1)):
        blit(s, 'birds', 8, 8, frame, x, y)
    # The player crouched in the wood with the rifle up, a shot's smoke at the muzzle.
    sprite(s, 'player', PLAYER_FRAMES['crouch_side'], 72, 92)
    held(s, 'LONG_GUN', 30, 76, 84)
    for i in range(4):
        s.rect(221 + i * 3, 12 - i, 2, i + 1, ACCENT)
    return s


def panel_butcher():
    s = Screen()
    fill(s, 'FOREST_FLOOR', 0, 0, 15, 10)
    fill(s, 'GRASS_LUSH', 9, 0, 15, 10)
    for tx, ty in ((1, 1), (3, 3), (0, 6), (2, 9), (12, 1), (14, 7)):
        tree(s, 'PINE' if tx < 9 else 'BIRCH', tx, ty)

    hud(s, '4 JUL ` 07:22')
    blit(s, 'animals', 16, 16, DEER['carcass'], 112, 72)
    sx, sy = 108, 76
    sprite(s, 'player', PLAYER_FRAMES['side'], sx, sy)
    held(s, 'KNIFE', 8, sx + 4, sy - 6)
    prompt(s, 120, 52, '[A] BUTCHER')
    action_bar(s, sx, sy - 34, 3)
    toast(s, '4 S WITH A KNIFE OR AXE: 4 VENISON, 1 HIDE', QUIET)
    return s


def panel_village():
    s = Screen()
    fill(s, 'GRASS_A', 0, 0, 15, 10)
    fill(s, 'ROAD', 0, 8, 15, 9)
    fill(s, 'PAVEMENT', 0, 7, 15, 8)
    for tx in range(0, 15):
        put(s, 'ROAD_LINE_H' if tx % 2 else 'ROAD', tx, 9)
    for tx in range(9, 15):
        put(s, 'FENCE_H', tx, 1)
    put(s, 'WELL', 6, 4)
    tree(s, 'OAK', 2, 3)
    put(s, 'BUSH', 1, 6)
    put(s, 'GRASS_FLOWERS', 11, 4)
    put(s, 'SNARE', 12, 4)
    put(s, 'SNARE', 9, 3)
    blit(s, 'animals', 16, 16, RABBIT['sit'], 12 * TILE - 2, 4 * TILE)

    hud(s, '5 JUL ` 06:50')
    sx, sy = 6 * TILE + 8, 6 * TILE + 12
    sprite(s, 'player', PLAYER_FRAMES['up'], sx, sy)
    prompt(s, sx, 4 * TILE - 12, '[A] FILL BOTTLE')
    toast(s, 'THE SNARE CAUGHT A RABBIT', ACCENT)
    return s


def panel_house():
    s = Screen()
    fill(s, 'GRASS_A', 0, 0, 15, 10)
    # A house's ground floor with the roof cut away: kitchen on the left, living room on the right.
    for tx in range(1, 14):
        put(s, 'WALL_FACE', tx, 1)
    for ty in range(2, 9):
        put(s, 'WALL_TOP', 1, ty)
        put(s, 'WALL_TOP', 13, ty)
    fill(s, 'TILE_FLOOR', 2, 2, 7, 9)
    fill(s, 'WOOD_FLOOR', 7, 2, 13, 9)
    for ty in (2, 3, 6, 7, 8):
        put(s, 'WALL_TOP', 7, ty)
    for tx in range(1, 14):
        put(s, 'WALL_FACE', tx, 9)
    put(s, 'WINDOW_FACE', 4, 1)
    put(s, 'WINDOW_FACE', 10, 1)
    put(s, 'KITCHEN_CABINET', 2, 2)
    put(s, 'FRIDGE', 3, 2)
    put(s, 'CHEST_FREEZER', 5, 2)
    put(s, 'COUNTER', 2, 5)
    put(s, 'TV', 10, 2)
    put(s, 'DESK', 12, 6)

    hud(s, '6 JUL ` 20:05')
    sx, sy = 10 * TILE + 8, 4 * TILE + 12
    sprite(s, 'player', PLAYER_FRAMES['up'], sx, sy)
    # The TV's light on the player's face is left to the game's night lights.
    # The broadcast, low on the screen so the freezer and the TV stay in view.
    lines = ['TV: ...THE EMERGENCY SERVICES ARE', 'NO LONGER ABLE TO REACH ASHFORD.',
             'STAY INDOORS. LOCK YOUR DOORS...']
    width = max(s.width(line) for line in lines) + 8
    x = (240 - width) // 2
    s.panel(x, 112, width, 4 + len(lines) * 7, ACCENT)
    for index, line in enumerate(lines):
        s.text(x + 4, 115 + index * 7, line, TEXT)
    return s


SETTINGS = [
    ('ZOMBIE SPEED', 'NORMAL', 'FAST SHAMBLERS 18%, SPRINTERS 2%'),
    ('ZOMBIES', 'MANY', ''),
    ('LOOT', 'RARE', ''),
    ('SHUTOFF', 'MONTH 1', ''),
    ('DAY LENGTH', '60 MIN', ''),
]
SELECTED = 2
DETAILS = 'HALF THE LOOT IN EVERY CONTAINER'


def panel_sandbox():
    s = Screen()
    s.rect(0, 0, 240, 160, SCREEN)
    s.text_centered(120, 10, 'KNOX', ACCENT)
    s.panel(40, 22, 160, 110)
    s.text_centered(120, 28, 'NEW WORLD', ACCENT)
    s.text_centered(120, 38, 'SEED  4F0A91C2', TEXT)
    s.rect(44, 47, 152, 1, BORDER)
    s.text_centered(120, 51, 'SANDBOX', QUIET)
    for index, (name, value, _) in enumerate(SETTINGS):
        y = 61 + index * 11
        if index == SELECTED:
            s.rect(44, y - 2, 152, 9, BORDER)
            s.text(48, y, name, TEXT)
            s.text_right(192, y, '< ' + value + ' >', ACCENT)
        else:
            s.text(48, y, name, QUIET)
            s.text_right(192, y, value, TEXT)
    s.text_centered(120, 119, DETAILS, DIM)

    s.rect(0, 144, 240, 16, SCREEN)
    s.hints(147, [('A', 'START'), ('UP/DOWN', 'PICK'), ('<>', 'CHANGE')], [('B', 'BACK')])
    return s


def main():
    panels = [
        (panel_swim(), [('1 SWIMMING: HALF SPEED, ENDURANCE DRAINS', ACCENT), ('ZOMBIES STOP AT THE SHORE', QUIET)]),
        (panel_wild(), [('2 DEER GRAZE, FLEE; A RABBIT; BIRDS', ACCENT),
                        ('A SHOT SCATTERS THE FLOCK (NOISE 6)', QUIET)]),
        (panel_butcher(), [('3 BUTCHERING: HOLD A WITH A BLADE', ACCENT), ('THE CARCASS STAYS UNTIL DONE', QUIET)]),
        (panel_village(), [('4 A WELL: TAINTED WATER, ANY TIME', ACCENT), ('SNARES: ROPE AND A PLANK', QUIET)]),
        (panel_house(), [('5 A CHEST FREEZER AND A TV', ACCENT), ('POWER OFF: TV STATIC, FOOD THAWS', QUIET)]),
        (panel_sandbox(), [('6 SANDBOX SETTINGS ON A NEW WORLD', ACCENT), ('SHOWN READ-ONLY IN THE PAUSE MENU', QUIET)]),
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
    path = os.path.join(ROOT, '13-wild.png')
    sheet.save(path)
    print('wrote mockups/13-wild.png')


if __name__ == '__main__':
    main()
