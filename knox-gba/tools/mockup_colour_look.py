"""Playtest 1 mockup: a grainier, more filmic colour look. Two frames captured from the game in mGBA
(mockups/frames/), each in four looks, saved at 3x.

    python tools/mockup_colour_look.py       -> mockups/17-colour-look.png

The GBA has no post-processing, and all four background layers are taken (HUD, roofs, walls, ground), so there is
no spare layer for a film-grain overlay. A look has to live where the colours do:

0. Now.
1. Faded: every world and sprite palette graded once when loaded (a third less saturation, blacks lifted, whites
   pulled down, shadows a little cool and lights a little warm). Costs nothing per frame; the HUD keeps its colours.
2. Grain: about one pixel in seven of the world's textures a shade darker or lighter, drawn into the tiles
   (tools/assets), so it sits still on the ground as you move, like a texture, not a flicker.
3. Both.

The mockup applies the grade to every pixel outside the HUD boxes and the grain to the same pixels; in the game the
grain would be drawn into ground, floor and wall tiles only, not sprites.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'assets'))

from mockup_character import ACCENT, QUIET  # noqa: E402
from mockup_doors import caption  # noqa: E402
from hud import palette as hud_palette  # noqa: E402

ROOT = os.path.join(HERE, '..', 'mockups')
FRAMES = [('street-day', 'A STREET BY DAY'), ('house-inside', 'INSIDE A HOUSE (ROOF CUT AWAY)')]
HUD_FILL = (8, 8, 16)           # the HUD boxes' fill, as captured
HUD_RECTS = [(148, 136, 240, 160), (0, 136, 40, 160)]     # the hotbar and the status icons (x0, y0, x1, y1)
# The HUD's colours as mGBA shows them (5 bits a channel, widened as v << 3 | v >> 2): in HUD_RECTS only these are
# HUD.
HUD_COLOURS = {tuple(((c >> 3) << 3) | (c >> 5) for c in colour) for colour in hud_palette()}


def hud_mask(image):
    """Pixels of the HUD: every box filled with the HUD's fill colour, with its border."""
    w, h = image.size
    px = image.load()
    mask = [[False] * w for _ in range(h)]
    for x0, y0, x1, y1 in HUD_RECTS:
        for my in range(y0, y1):
            for mx in range(x0, x1):
                mask[my][mx] = px[mx, my] in HUD_COLOURS
    seen = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if seen[y][x] or px[x, y] != HUD_FILL:
                continue
            stack, cells = [(x, y)], []
            seen[y][x] = True
            while stack:
                cx, cy = stack.pop()
                cells.append((cx, cy))
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if 0 <= nx < w and 0 <= ny < h and not seen[ny][nx] and px[nx, ny] == HUD_FILL:
                        seen[ny][nx] = True
                        stack.append((nx, ny))
            if len(cells) < 20:
                continue
            x0 = min(c[0] for c in cells) - 2
            x1 = max(c[0] for c in cells) + 2
            y0 = min(c[1] for c in cells) - 2
            y1 = max(c[1] for c in cells) + 2
            for my in range(max(0, y0), min(h, y1 + 1)):
                for mx in range(max(0, x0), min(w, x1 + 1)):
                    mask[my][mx] = True
    return mask


def gba(value):
    """To the GBA's 5 bits a channel and back."""
    return max(0, min(255, int(round(value / 8.0)) * 8))


def faded(rgb):
    r, g, b = (c / 255.0 for c in rgb)
    lum = 0.3 * r + 0.59 * g + 0.11 * b
    r, g, b = (lum + (c - lum) * 0.66 for c in (r, g, b))           # a third less saturation
    r, g, b = (0.07 + c * 0.86 for c in (r, g, b))                   # blacks lifted, whites pulled down
    r += 0.035 * lum - 0.01 * (1 - lum)                              # lights warm, shadows cool
    b += 0.03 * (1 - lum) - 0.03 * lum
    return tuple(gba(c * 255) for c in (r, g, b))


def grain(rgb, x, y):
    value = ((x * 0x9E3779B1) ^ (y * 0x85EBCA77)) & 0xFFFFFFFF
    value ^= value >> 15
    pick = value % 14
    if pick == 0:
        scale = 0.84
    elif pick == 1:
        scale = 1.12
    else:
        return rgb
    return tuple(gba(c * scale) for c in rgb)


def look(image, mask, grade, grained):
    out = image.copy()
    src = image.load()
    dst = out.load()
    w, h = image.size
    for y in range(h):
        for x in range(w):
            if mask[y][x]:
                continue
            value = src[x, y]
            if grained:
                value = grain(value, x, y)
            if grade:
                value = faded(value)
            dst[x, y] = value
    return out


def main():
    looks = [(False, False, '0 NOW'), (True, False, '1 FADED GRADE'), (False, True, '2 GRAIN IN THE TILES'),
             (True, True, '3 BOTH')]
    gap = 4
    width = 240 * 4 + gap * 3
    height = (160 + 22) * len(FRAMES) + gap
    sheet = Image.new('RGB', (width, height), (0x05, 0x07, 0x0b))
    for row, (name, where) in enumerate(FRAMES):
        frame = Image.open(os.path.join(ROOT, 'frames', name + '.png')).convert('RGB')
        mask = hud_mask(frame)
        for column, (grade, grained, title) in enumerate(looks):
            x = column * (240 + gap)
            y = row * (160 + 22 + gap)
            sheet.paste(look(frame, mask, grade, grained), (x, y))
            sheet.paste(caption(240, [(title, ACCENT), (where, QUIET)]), (x, y + 160))
    sheet = sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST)
    sheet.save(os.path.join(ROOT, '17-colour-look.png'))
    print('wrote mockups/17-colour-look.png')


if __name__ == '__main__':
    main()
