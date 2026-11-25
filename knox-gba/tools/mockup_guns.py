"""Milestone 12.6 mockup: aiming a gun and the active reload, drawn on mockup 01's scene at 240x160 with the game's
5 px font and palette, three moments side by side, saved at 3x.

    python tools/mockup_guns.py        -> mockups/11-aiming.png

1. Aiming: R held, standing still, so the cone is narrow (its two dotted edges); the zombie it locks on to is
   bracketed; the loaded rounds and the rounds in the bag sit over the equipped slot.
2. Reloading (B): a bar over the player fills from left to right; A inside the green sweet spot is a fast reload.
3. Jammed (A pressed outside the sweet spot): the gun clicks; B clears it.
"""
import math
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
from mockup_character import ACCENT, BORDER, EMPTY, FILL, GOOD, QUIET, RED, SLOT, TEXT, Screen  # noqa: E402

ROOT = os.path.join(os.path.dirname(__file__), '..', 'mockups')
GUN_DARK = (0x2b, 0x2e, 0x33)
GUN_LIGHT = (0x6f, 0x6d, 0x64)

# Mockup 01's scene: the player on the pavement, a zombie straight below on the far pavement.
PLAYER_HAND = (204, 76)
TARGET_BOX = (192, 111, 16, 18)
EQUIPPED = (218, 135, 18, 18)


def scene():
    s = Screen()
    base = Image.open(os.path.join(ROOT, '01-gameplay-dusk.png')).convert('RGB')
    s.image = base.resize((240, 160), Image.NEAREST)
    s.pixels = s.image.load()

    # The context prompt does not show while aiming: the ground under it is cloned from just above.
    for y in range(34, 46):
        for x in range(158, 204):
            s.pixels[x, y] = s.pixels[x, y - 12]

    # Mockup 01's clock and noise labels were drawn with a browser font; scaled down they blur, so they are
    # drawn again with the game's font.
    box = s.pixels[7, 6]
    s.rect(6, 6, 48, 9, box)
    s.text(9, 8, 'DAY 3 ` 18:40', TEXT)
    s.rect(196, 6, 21, 9, box)
    s.text(197, 8, 'NOISE', TEXT)

    # The pistol in the hand, pointing down at the zombie.
    s.rect(PLAYER_HAND[0] - 1, PLAYER_HAND[1], 3, 7, FILL)
    s.rect(PLAYER_HAND[0], PLAYER_HAND[1] + 1, 1, 5, GUN_LIGHT)

    # The equipped slot shows the pistol instead of the axe.
    x, y, w, h = EQUIPPED
    s.rect(x + 2, y + 2, w - 4, h - 4, SLOT)
    s.rect(x + 4, y + 6, 10, 4, GUN_DARK)
    s.rect(x + 5, y + 9, 3, 5, GUN_DARK)
    s.rect(x + 5, y + 7, 8, 1, GUN_LIGHT)
    s.rect(x, y + h + 1, w, 2, EMPTY)
    return s


def ammo(s, loaded, carried, colour=TEXT):
    # Over the equipped slot: loaded / in the bag.
    text = '%d/%d' % (loaded, carried)
    width = s.width(text) + 6
    x = EQUIPPED[0] + EQUIPPED[2] - width
    s.panel(x, EQUIPPED[1] - 11, width, 9)
    s.text(x + 3, EQUIPPED[1] - 9, text, colour)


def cone(s, half_degrees):
    # Two dotted edges from the muzzle, every 5 px, as far as the target.
    mx, my = PLAYER_HAND[0], PLAYER_HAND[1] + 7
    for side in (-1, 1):
        angle = math.radians(half_degrees) * side
        for step in range(1, 8):
            d = step * 5
            px = round(mx + math.sin(angle) * d)
            py = round(my + math.cos(angle) * d)
            s.rect(px, py, 1, 1, TEXT)


def brackets(s, colour):
    x, y, w, h = TARGET_BOX
    for cx, cy, dx, dy in ((x, y, 1, 1), (x + w - 1, y, -1, 1), (x, y + h - 1, 1, -1), (x + w - 1, y + h - 1, -1, -1)):
        s.rect(min(cx, cx + 2 * dx), cy, 3, 1, colour)
        s.rect(cx, min(cy, cy + 2 * dy), 1, 3, colour)


def reload_bar(s, fill_x, sweet=(28, 36)):
    # Over the player's head: a track, the green sweet spot, what has filled, the moving mark.
    x, y, w = 178, 50, 44
    s.panel(x, y, w, 9)
    track = x + 3
    s.rect(track, y + 3, w - 6, 3, EMPTY)
    s.rect(track, y + 3, fill_x, 3, QUIET)
    s.rect(track + sweet[0], y + 3, sweet[1] - sweet[0], 3, GOOD)
    s.rect(track + fill_x, y + 2, 1, 5, TEXT)


def jammed(s):
    text = 'JAMMED'
    hint = 'B CLEAR'
    width = s.width(text) + s.width(hint) + 12
    x = min(202 - width // 2, 236 - width)
    s.panel(x, 50, width, 9, RED)
    s.text(x + 3, 52, text, RED)
    s.text(x + 3 + s.width(text) + 6, 52, 'B', TEXT)
    s.text(x + 3 + s.width(text) + 6 + s.width('B') + 4, 52, 'CLEAR', QUIET)


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
    aiming = scene()
    cone(aiming, 8)
    brackets(aiming, ACCENT)
    ammo(aiming, 12, 30)

    reloading = scene()
    reload_bar(reloading, 19)
    ammo(reloading, 0, 30, QUIET)

    jam = scene()
    jammed(jam)
    ammo(jam, 6, 24, RED)

    panels = [
        (aiming, [('1 AIMING: R HELD, STANDING STILL', ACCENT), ('NARROW CONE, TARGET BRACKETED, 12/30', QUIET)]),
        (reloading, [('2 RELOADING: B, THEN A IN THE GREEN', ACCENT), ('MISS IT AND THE GUN JAMS', QUIET)]),
        (jam, [('3 JAMMED: A ONLY CLICKS', ACCENT), ('B CLEARS IT (HALF A SECOND)', QUIET)]),
    ]
    gap = 4
    width = 240 * 3 + gap * 2
    sheet = Image.new('RGB', (width, 160 + 22), (0x05, 0x07, 0x0b))

    for index, (screen, lines) in enumerate(panels):
        x = index * (240 + gap)
        sheet.paste(screen.image, (x, 0))
        sheet.paste(caption(240, lines), (x, 160))

    sheet = sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST)
    path = os.path.join(ROOT, '11-aiming.png')
    sheet.save(path)
    print('wrote mockups/11-aiming.png')


if __name__ == '__main__':
    main()
