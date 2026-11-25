"""Showcase art for the Miyoo Mini's OnionOS game list, made from the game's own pixels: its font, its sprites
(graphics/*.bmp as the asset build writes them) and two scenes from the game (mockups/frames/street-day.png and a
night screenshot from mGBA at 3x, the night bench's first phase: a campfire, the flashlight, a car's headlights).

    python tools/make_showcase.py <night screenshot 720x480>

Writes, in showcase/:
- pocket-outbreak-boxart.png (250x360): box art for OnionOS (put it in Roms/GBA/Imgs/ named as the ROM, e.g.
  knox.png beside knox.gba).
- pocket-outbreak-showcase.png (640x480, the Miyoo Mini's screen): title, day and night, the cast.
Both are drawn at a low resolution and scaled up whole pixels, so they read as the game does.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, 'assets'))

from characters import ZOMBIE_OUTFITS, hexc  # noqa: E402
from small_font import GLYPHS  # noqa: E402

ROOT = os.path.join(HERE, '..')
OUT = os.path.join(ROOT, 'showcase')

INK = (0x0b, 0x0e, 0x14)
PANEL = (0x10, 0x14, 0x1b)
BORDER = (0x2a, 0x2f, 0x3a)
TEXT = (0xe8, 0xe4, 0xd8)
QUIET = (0xa8, 0xb0, 0xbc)
ACCENT = (0xe8, 0xc5, 0x47)
BLOOD = (0xb0, 0x2a, 0x20)
BLOOD_DARK = (0x5e, 0x14, 0x10)

# Frame indices (include/kx_sprite_frames.h, kx_animal_frames.h, kx_weapon_frames.h).
PLAYER_AIM_SIDE = 37 + 2 * 4            # aiming, side block, standing
ZOMBIE_SIDE = [10, 11, 13]              # side: stand, step, arms out (windup)
ZOMBIE_BODY_FRAMES = 20
WOLF_SIDE_RUN = 15
BOAR_SIDE_STAND = 7
LONG_GUN = 8 * 32                       # weapons.bmp: LONG_GUN's first angle (0 = pointing right)


# ----------------------------------------------------------------------------------------------------------------
# Drawing
# ----------------------------------------------------------------------------------------------------------------

def text_width(text, scale=1):
    width = sum(len(GLYPHS.get(c, GLYPHS['?'])[0]) + 1 for c in text.upper()) - 1
    return max(width, 0) * scale


def draw_text(image, x, y, text, colour, scale=1, outline=None):
    """The game's 5 px capitals, scaled, with an optional 1-scaled-pixel outline (drawn first)."""
    def glyphs(ox, oy, fill):
        cx = ox
        for c in text.upper():
            glyph = GLYPHS.get(c, GLYPHS['?'])
            for row, line in enumerate(glyph):
                for column, cell in enumerate(line):
                    if cell == '#':
                        for py in range(scale):
                            for px in range(scale):
                                image.putpixel((cx + column * scale + px, oy + row * scale + py), fill)
            cx += (len(glyph[0]) + 1) * scale
    if outline:
        for dx in (-scale, 0, scale):
            for dy in (-scale, 0, scale):
                if dx or dy:
                    glyphs(x + dx, y + dy, outline)
    glyphs(x, y, colour)


def centred_text(image, centre_x, y, text, colour, scale=1, outline=None):
    draw_text(image, centre_x - text_width(text, scale) // 2, y, text, colour, scale, outline)


def rect(image, x, y, w, h, colour):
    for py in range(y, y + h):
        for px in range(x, x + w):
            if 0 <= px < image.width and 0 <= py < image.height:
                image.putpixel((px, py), colour)


def frame(image, x, y, w, h, colour):
    rect(image, x, y, w, 1, colour)
    rect(image, x, y + h - 1, w, 1, colour)
    rect(image, x, y, 1, h, colour)
    rect(image, x + w - 1, y, 1, h, colour)


def sheet_frame(path, index, width=16, height=16, palette=None, flip=False):
    """One frame of an indexed sprite sheet as RGBA (index 0 transparent), with an optional palette override."""
    sheet = Image.open(path)
    colours = sheet.getpalette()
    if palette:
        colours = list(colours)
        for slot, value in palette.items():
            colours[slot * 3:slot * 3 + 3] = list(value)
    cell = sheet.crop((0, index * height, width, index * height + height))
    out = Image.new('RGBA', (width, height))
    for y in range(height):
        for x in range(width):
            c = cell.getpixel((x, y))
            if c:
                out.putpixel((x, y), tuple(colours[c * 3:c * 3 + 3]) + (255,))
    return out.transpose(Image.FLIP_LEFT_RIGHT) if flip else out


def shadow(image, centre_x, y, width):
    """A dark ground shadow, an ellipse in whole pixels."""
    half = width // 2
    for dx in range(-half, half + 1):
        rows = 2 if abs(dx) < half - 1 else 1
        for dy in range(rows):
            px, py = centre_x + dx, y + dy
            if 0 <= px < image.width and 0 <= py < image.height:
                r, g, b = image.getpixel((px, py))[:3]
                image.putpixel((px, py), (r // 2, g // 2, b // 2))


def gunner():
    """The survivor aiming the hunting rifle to the right, in a 32x16 cell (body at x 8..24)."""
    cell = Image.new('RGBA', (32, 32))
    body = sheet_frame(os.path.join(ROOT, 'graphics', 'player.bmp'), PLAYER_AIM_SIDE)
    gun = sheet_frame(os.path.join(ROOT, 'graphics', 'weapons.bmp'), LONG_GUN, 32, 32)
    cell.alpha_composite(body, (8, 8))
    cell.alpha_composite(gun, (8 + 8 + 5 - 16, 8 + 8 + 1 - 16))
    return cell.crop((0, 8, 32, 24))


def zombie(body, outfit, pose, facing_left=True):
    palette = {slot: hexc(value) for slot, value in ZOMBIE_OUTFITS[outfit][1].items()}
    index = body * ZOMBIE_BODY_FRAMES + ZOMBIE_SIDE[pose]
    return sheet_frame(os.path.join(ROOT, 'graphics', 'zombie.bmp'), index, palette=palette, flip=facing_left)


def wide_animal(index, facing_left=True):
    return sheet_frame(os.path.join(ROOT, 'graphics', 'animals_wide.bmp'), index, 32, 16, flip=facing_left)


def paste_scaled(image, sprite, x, y, scale):
    big = sprite.resize((sprite.width * scale, sprite.height * scale), Image.NEAREST)
    image.alpha_composite(big, (x, y))


def native(screenshot_path):
    """A 3x mGBA screenshot back to its 240x160 pixels."""
    shot = Image.open(screenshot_path).convert('RGB')
    return shot.resize((240, 160), Image.NEAREST)


def fade(image, box, colour, strength):
    x0, y0, x1, y1 = box
    for y in range(y0, y1):
        t = (y - y0) / max(y1 - y0 - 1, 1)
        a = strength * t
        for x in range(x0, x1):
            r, g, b = image.getpixel((x, y))[:3]
            image.putpixel((x, y), (int(r + (colour[0] - r) * a), int(g + (colour[1] - g) * a),
                                    int(b + (colour[2] - b) * a), 255))


# ----------------------------------------------------------------------------------------------------------------
# The two images
# ----------------------------------------------------------------------------------------------------------------

def title(image, centre_x, y, scale_small, scale_big):
    centred_text(image, centre_x, y, 'POCKET', ACCENT, scale_small, INK)
    centred_text(image, centre_x, y + 7 * scale_small, 'OUTBREAK', BLOOD, scale_big, INK)


def boxart(night):
    """125x180 pixels, shown at 2x (250x360)."""
    image = Image.new('RGBA', (125, 180), INK + (255,))

    # The night: the campfire, the survivor with the flashlight, the car's headlights (HUD left out).
    scene = night.crop((52, 22, 177, 126)).convert('RGBA')
    image.alpha_composite(scene, (0, 38))
    fade(image, (0, 38, 125, 60), INK, 0)
    fade(image, (0, 112, 125, 142), INK, 0.85)
    frame(image, 0, 0, 125, 180, BORDER)

    # Title over the dark sky.
    rect(image, 1, 1, 123, 37, INK)
    title(image, 62, 5, 2, 3)

    # The cast, large: the survivor aiming at a shambling pair.
    ground = 158
    shadow(image, 30, ground - 1, 22)
    paste_scaled(image, gunner(), 2, ground - 48, 3)
    shadow(image, 84, ground - 1, 14)
    paste_scaled(image, zombie(1, 2, 2), 70, ground - 48, 3)
    shadow(image, 108, ground - 3, 12)
    paste_scaled(image, zombie(2, 3, 1), 96, ground - 50, 3)

    # The strip at the bottom.
    rect(image, 1, 162, 123, 17, PANEL)
    rect(image, 1, 161, 123, 1, BORDER)
    centred_text(image, 62, 165, 'SURVIVE THE NIGHT', TEXT)
    centred_text(image, 62, 172, 'GBA HOMEBREW', QUIET)
    return image.resize((250, 360), Image.NEAREST)


def showcase(day, night):
    """320x240 pixels, shown at 2x (640x480, the Miyoo Mini's screen)."""
    image = Image.new('RGBA', (320, 240), INK + (255,))
    frame(image, 0, 0, 320, 240, BORDER)
    title(image, 160, 8, 3, 4)
    centred_text(image, 160, 53, 'A ZOMBIE SURVIVAL SANDBOX', QUIET)

    # Day and night, side by side, framed like screens.
    for index, (scene, label) in enumerate(((day, 'BY DAY'), (night, 'BY NIGHT'))):
        x = 12 + index * 152
        crop = scene.crop((48, 40, 192, 136)) if index == 0 else scene.crop((44, 20, 188, 116))
        rect(image, x - 2, 64, 148, 100, BORDER)
        image.alpha_composite(crop.convert('RGBA'), (x, 66))
        rect(image, x + 2, 152, text_width(label) + 6, 9, INK)
        draw_text(image, x + 5, 154, label, ACCENT)

    # The cast on a strip of ground.
    ground = 214
    rect(image, 1, ground, 318, 1, BORDER)
    cast = [
        (gunner(), 18, 2),
        (zombie(0, 0, 0), 92, 2),
        (zombie(1, 1, 1), 122, 2),
        (zombie(3, 4, 2), 152, 2),
        (zombie(2, 2, 0), 182, 2),
        (wide_animal(WOLF_SIDE_RUN), 220, 2),
        (wide_animal(BOAR_SIDE_STAND), 262, 2),
    ]
    for sprite, x, scale in cast:
        shadow(image, x + sprite.width * scale // 2, ground - 2, sprite.width * scale // 2 if sprite.width > 16 else 14)
        paste_scaled(image, sprite, x, ground - sprite.height * scale, scale)

    centred_text(image, 160, 222, 'SCAVENGE ` BUILD ` HUNT ` SURVIVE THE NIGHT', TEXT)
    centred_text(image, 160, 231, 'FOR GBA, MIYOO MINI AND ANY GBA EMULATOR', QUIET)
    return image.resize((640, 480), Image.NEAREST)


def main():
    night = native(sys.argv[1])
    day = Image.open(os.path.join(ROOT, 'mockups', 'frames', 'street-day.png')).convert('RGB')

    # The frame has the door's "[A] CLOSE" prompt on the roof: painted over with the roof 12 rows up.
    pixels = day.load()
    for y in range(47, 60):
        for x in range(94, 147):
            pixels[x, y] = pixels[x, y - 12]
    os.makedirs(OUT, exist_ok=True)
    boxart(night).convert('RGB').save(os.path.join(OUT, 'pocket-outbreak-boxart.png'))
    showcase(day, night).convert('RGB').save(os.path.join(OUT, 'pocket-outbreak-showcase.png'))
    print('wrote showcase/pocket-outbreak-boxart.png and showcase/pocket-outbreak-showcase.png')


if __name__ == '__main__':
    main()
