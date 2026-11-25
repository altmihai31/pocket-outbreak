"""Playtest 5 mockup: side doors that show from outside. Drawn at 240x160 from the game's own world graphics, sprites
and palette (the house of mockup 15), cropped round its side door, closed and open, saved at 3x.

    python tools/mockup_side_doors.py        -> mockups/23-side-doors.png

Playtest 1 put outer doors under the roof (mockup 15, look 1): a front door shows under a dip in the eave, a side door
only by the paved step outside it. A tester walking round a house found no side door until the [A] prompt showed.

A. Now: the step only; closed and open look the same from outside.
B. Notch: the roof's edge steps back 6 px over the doorway, and the door frame shows in the gap: the door's wood
   when closed, the dark doorway when open.
C. Door edge: the roof stays whole; the door shows past the wall line as a 4 px wooden slab when closed, and swung
   out along the top of the step when open.
D. Notch and door edge together.
Inside (the roof cut away) nothing changes; front doors keep the eave dip.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'assets'))

from mockup_character import ACCENT, QUIET  # noqa: E402
from mockup_doors import GRIDS, caption  # noqa: E402
import mockup_door_looks as looks  # noqa: E402
import world  # noqa: E402

ROOT = os.path.join(HERE, '..', 'mockups')
T = world.W_TRANS
SIDE = (len(looks.ROWS[0]) - 1, 3)      # the side door's tile in the house (right wall)
NOTCH = 6                               # px the roof steps back over a side doorway
CROP = (104, 24, 224, 104)              # round the side door, 120x80


def notched_roof(name, opened):
    """The roof tile over a doorway in the right wall, its outer 6 px cut back with a dark edge; in the gap the
    door frame: the door's wood when closed, the dark doorway when open."""
    g = [row[:] for row in GRIDS[name]]
    for y in range(16):
        for x in range(16 - NOTCH, 16):
            g[y][x] = T
        g[y][15 - NOTCH] = world.W_DARK
    for y in range(2, 14):
        for x in range(16 - NOTCH, 16):
            edge = y in (2, 13) or x == 15
            g[y][x] = world.W_DARK if edge or opened else world.W_WOOD
    if opened:
        for y in range(3, 13):
            g[y][15] = world.W_WOOD          # the open door's edge against the frame
    return g


def door_slab(opened):
    """The door seen past the wall line, on the tile outside it: a 4 px slab of wood (closed) or the door swung out
    along the top of the step (open)."""
    g = [[T] * 16 for _ in range(16)]
    if opened:
        for x in range(0, 14):
            g[1][x] = world.W_DARK
            g[2][x] = world.W_WOOD
            g[3][x] = world.W_WOOD
            g[4][x] = world.W_DARK
        g[2][13] = g[3][13] = world.W_DARK
    else:
        for y in range(1, 15):
            for x in range(0, 4):
                edge = y in (1, 14) or x == 3
                g[y][x] = world.W_DARK if edge else world.W_WOOD
    return g


def scene(option, opened):
    s = looks.scene(1)          # mockup 15's look 1: what the game draws now
    rows, ground, walls, roofs = looks.layers()
    roofs = looks.roof_parts(rows, True)
    x, y = SIDE
    tx, ty = looks.LEFT + x, looks.TOP + y

    # The roof over the doorway as the option has it.
    if option in ('B', 'D'):
        looks.EXTRA['NOTCHED'] = notched_roof(roofs[y][x], opened)
        looks.put(s, 'NOTCHED', tx, ty)
    else:
        looks.put(s, roofs[y][x], tx, ty)
    if option in ('C', 'D'):
        looks.EXTRA['SLAB'] = door_slab(opened)
        looks.put(s, 'SLAB', tx + 1, ty)
    return s


def main():
    options = [
        ('A', 'A NOW', 'STEP ONLY'),
        ('B', 'B NOTCH', 'ROOF STEPS BACK'),
        ('C', 'C DOOR EDGE', 'DOOR PAST THE WALL'),
        ('D', 'D BOTH', 'NOTCH + DOOR EDGE'),
    ]
    crop_w = CROP[2] - CROP[0]
    crop_h = CROP[3] - CROP[1]
    gap = 4
    label_h = 22
    width = crop_w * 4 + gap * 3
    height = (crop_h + label_h) * 2 + gap
    sheet = Image.new('RGB', (width, height), (0x05, 0x07, 0x0b))

    for column, (option, title, line) in enumerate(options):
        x = column * (crop_w + gap)
        for row, opened in enumerate((False, True)):
            y = row * (crop_h + label_h + gap)
            sheet.paste(scene(option, opened).image.crop(CROP), (x, y))
            sheet.paste(caption(crop_w, [(title + (': OPEN' if opened else ': CLOSED'), ACCENT), (line, QUIET)]),
                        (x, y + crop_h))

    sheet = sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST)
    sheet.save(os.path.join(ROOT, '23-side-doors.png'))
    print('wrote mockups/23-side-doors.png')


if __name__ == '__main__':
    main()
