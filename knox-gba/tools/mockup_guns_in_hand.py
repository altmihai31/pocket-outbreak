"""Playtest 3 mockup: guns held in the hand. The game's own sprites (graphics/player.bmp, graphics/weapons.bmp, as
the asset build writes them) put together at the hand positions the game uses (kx_player_combat.cpp's gun poses),
for every gun, the four facings, at rest and aimed, on grass, saved at 5x.

    python tools/mockup_guns_in_hand.py        -> mockups/21-guns-in-hand.png

At rest a pistol or revolver hangs in the hand, pointing down and forward; a long gun (shotgun, hunting rifle,
assault rifle: one picture) is held at low ready, across the body in both hands, the muzzle down and forward.
Aiming, the arms come up to chest height and the gun points along the aim. docs/playtest-3-guns-now.png is how they
looked before.
"""
import os

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
LOOKS = ['KNIFE', 'HAMMER', 'BAT', 'PIPE', 'CROWBAR', 'AXE', 'FLASHLIGHT', 'PISTOL', 'LONG_GUN', 'SPEAR', 'BOTTLE',
         'MOLOTOV', 'CLOCK', 'MACHETE', 'HATCHET', 'GOLF_CLUB', 'SLEDGEHAMMER', 'PITCHFORK', 'CLEAVER', 'HOCKEY_STICK',
         'BATON', 'REVOLVER']

# The game's frame indices (include/kx_sprite_frames.h) and gun poses (kx_player_combat.cpp): per pose block
# (down, side, up), the hand from the body's centre, the angle (32 steps, 0 right, 8 down) and whether the gun is
# behind the body. Aimed guns take the aim's angle.
PLAYER_AIM, PLAYER_READY, HOLD_POSES = 37, 49, 4
HAND_REST = [(3, 3, 8, False), (0, 3, 3, False), (-4, 3, 8, True)]
READY = [(2, 2, 10, False), (0, 2, 2, False), (-2, 2, 12, True)]
AIM = [(3, 3, False), (5, 1, False), (3, -2, True)]

FACINGS = [('down', 0, 1), ('right', 1, 0), ('left', -1, 0), ('up', 0, -1)]
FACE_ANGLE = {(0, 1): 8, (1, 0): 0, (-1, 0): 16, (0, -1): 24}
FLOOR = (85, 127, 66)
FLOOR_DARK = (63, 107, 52)


def keyed(image):
    rgba = image.convert('RGBA')
    px = rgba.load()
    for y in range(rgba.height):
        for x in range(rgba.width):
            if px[x, y][:3] == (255, 0, 255):
                px[x, y] = (0, 0, 0, 0)
    return rgba


def weapon_frame(sheet, palette, index):
    frame = sheet.crop((0, index * 32, 32, index * 32 + 32))
    rgba = Image.new('RGBA', (32, 32))
    for y in range(32):
        for x in range(32):
            c = frame.getpixel((x, y))
            if c:
                rgba.putpixel((x, y), tuple(palette[c * 3:c * 3 + 3]) + (255,))
    return rgba


def floor_tile(size):
    tile = Image.new('RGBA', (size, size), FLOOR + (255,))
    px = tile.load()
    for y in range(size):
        for x in range(size):
            if (x * 7 + y * 13) % 11 == 0:
                px[x, y] = FLOOR_DARK + (255,)
    return tile


def scene(player, weapons, palette, gun, fx, fy, aiming):
    long_gun = gun == 'LONG_GUN'
    sheet_block = 2 if fx else (1 if fy < 0 else 0)     # body sheet: down, up, side
    pose_block = 1 if fx else (2 if fy < 0 else 0)      # poses: down, side, up
    mirrored = fx < 0
    if aiming:
        hx, hy, behind = AIM[pose_block]
        angle = FACE_ANGLE[(fx, fy)]
        index = PLAYER_AIM + sheet_block * HOLD_POSES
    else:
        hx, hy, angle, behind = (READY if long_gun else HAND_REST)[pose_block]
        if mirrored:
            angle = 16 - angle
        index = (PLAYER_READY + sheet_block * HOLD_POSES) if long_gun else sheet_block * 6
    angle &= 31
    if mirrored:
        hx = -hx
    body = player.crop((0, index * 16, 16, index * 16 + 16))
    if mirrored:
        body = body.transpose(Image.FLIP_LEFT_RIGHT)
    canvas = floor_tile(48)
    weapon = weapon_frame(weapons, palette, LOOKS.index(gun) * 32 + angle)
    centre = 24
    weapon_at = (centre + hx - 16, centre + hy - 16)
    body_at = (centre - 8, centre - 8)
    for image, at in ((weapon, weapon_at), (body, body_at)) if behind else ((body, body_at), (weapon, weapon_at)):
        canvas.alpha_composite(image, at)
    return canvas


def main():
    player = keyed(Image.open(os.path.join(ROOT, 'graphics', 'player.bmp')))
    weapons = Image.open(os.path.join(ROOT, 'graphics', 'weapons.bmp'))
    palette = weapons.getpalette()
    scale = 5
    rows = [(gun, aiming) for gun in ('PISTOL', 'REVOLVER', 'LONG_GUN') for aiming in (False, True)]
    label_width = 150
    after = Image.new('RGBA', (label_width + len(FACINGS) * 48 * scale, len(rows) * 48 * scale), (30, 30, 30, 255))
    draw = ImageDraw.Draw(after)
    for r, (gun, aiming) in enumerate(rows):
        name = 'LONG GUN' if gun == 'LONG_GUN' else gun
        draw.text((6, r * 48 * scale + 110), '%s\n%s' % (name, 'aiming' if aiming else 'at rest'),
                  fill=(255, 255, 255))
        for c, (_, fx, fy) in enumerate(FACINGS):
            tile = scene(player, weapons, palette, gun, fx, fy, aiming).resize((48 * scale, 48 * scale),
                                                                               Image.NEAREST)
            after.alpha_composite(tile, (label_width + c * 48 * scale, r * 48 * scale))

    before_path = os.path.join(ROOT, 'docs', 'playtest-3-guns-now.png')
    before = Image.open(before_path).convert('RGBA') if os.path.exists(before_path) else None
    gap = 24
    width = after.width + (before.width + gap if before else 0)
    sheet = Image.new('RGBA', (width, after.height + 40), (20, 20, 20, 255))
    draw = ImageDraw.Draw(sheet)
    x = 0
    if before:
        draw.text((10, 12), 'BEFORE', fill=(232, 197, 71))
        sheet.alpha_composite(before, (0, 40))
        x = before.width + gap
    draw.text((x + 10, 12), 'AFTER (playtest 3)', fill=(232, 197, 71))
    sheet.alpha_composite(after, (x, 40))
    sheet.convert('RGB').save(os.path.join(ROOT, 'mockups', '21-guns-in-hand.png'))
    print('wrote mockups/21-guns-in-hand.png')


if __name__ == '__main__':
    main()
