"""Character sprites (16x16): the player and the zombie, plus the generated frame-index header.

The player's clothes are palette areas (spec cut list: clothing shown through palette swaps, not sprite layers):
torso (3 shades), hips, legs, feet, hands, hat and bag each have their own colour indices, which kx_outfit.h
recolours from what is worn, or to skin and underwear when nothing is."""
from PIL import Image

from common import TRANSPARENT, hexc, save_bmp, write_header

PLAYER_PALETTE = [TRANSPARENT, hexc('151a22'), hexc('d9a77c'), hexc('a87a58'), hexc('4a2e24'),
                  hexc('4d5a3a'), hexc('36402a'), hexc('657552'), hexc('3d5a80'), hexc('1e2229'),
                  hexc('0b0e14'), hexc('4a90d9'), hexc('3d5a80'), hexc('4a2e24'), hexc('d9a77c'),
                  (0, 0, 0)]
# 12-15 (playtest 1): a second hair colour and a second shirt (base, shade, light), worn by bodies B and D.
ZOMBIE_PALETTE = [TRANSPARENT, hexc('151a22'), hexc('8aa07a'), hexc('6b7d5e'), hexc('3e4a36'),
                  hexc('5a4a5e'), hexc('3e3342'), hexc('6e5c72'), hexc('3a3d42'), hexc('1e2229'),
                  hexc('b02a20'), hexc('d0453a'), hexc('c8b878'), hexc('8a6a3a'), hexc('5e4828'),
                  hexc('a8885a')]
KEYS = {'.': 0, 'o': 1, 's': 2, 'S': 3, 'h': 4, 'j': 5, 'd': 6, 'l': 7, 'p': 8, 'b': 9, 'e': 10, 'k': 11,
        'u': 12, 'c': 13, 'g': 14}
# Zombies have no clothing areas: hips are trousers, the hat area is hair, hands are skin. H, J, Q and L are the
# second hair colour and the second shirt (base, shade, light).
ZOMBIE_KEYS = dict(KEYS, u=KEYS['p'], c=KEYS['h'], g=KEYS['s'], H=12, J=13, Q=14, L=15)

# Player palette indices of the clothing areas (kx_outfit.h).
P_SKIN, P_SKIN_SHADE, P_HAIR, P_TORSO, P_TORSO_SHADE, P_TORSO_LIGHT, P_LEGS, P_FEET, P_BAG, P_HIPS, P_HAT, \
    P_HANDS = 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14

# Without clothes: skin, with light grey underwear on the hips.
BARE = {P_TORSO: 'd9a77c', P_TORSO_SHADE: 'a87a58', P_TORSO_LIGHT: 'e6b892', P_LEGS: 'd9a77c', P_HIPS: 'b8bcc4',
        P_FEET: 'a87a58', P_HANDS: 'd9a77c', P_HAT: '4a2e24'}

# Worn items and the areas they colour (torso items: base, shade, light). No bag: the bag area is the torso's.
OUTFIT = [
    ('HOODIE', [(P_TORSO, '4d5a3a'), (P_TORSO_SHADE, '36402a'), (P_TORSO_LIGHT, '657552')]),
    ('LEATHER_JACKET', [(P_TORSO, '6b4a2e'), (P_TORSO_SHADE, '4a2e24'), (P_TORSO_LIGHT, '8a6a42')]),
    ('RAINCOAT', [(P_TORSO, 'e8c547'), (P_TORSO_SHADE, 'b8962e'), (P_TORSO_LIGHT, 'f2dc7a')]),
    ('JEANS', [(P_LEGS, '3d5a80'), (P_HIPS, '3d5a80')]),
    ('BOOTS', [(P_FEET, '1e2229')]),
    ('GLOVES', [(P_HANDS, '3a3d42')]),
    ('BEANIE', [(P_HAT, 'b02a20')]),
    ('SCHOOL_BAG', [(P_BAG, '4a90d9')]),
    ('HIKING_BAG', [(P_BAG, 'e0903a')]),
    ('MILITARY_BAG', [(P_BAG, '5a6640')]),
    # Milestone 14 (the police, army and firefighter colours are the mockup 14 survivors').
    ('T_SHIRT', [(P_TORSO, 'c8ccc4'), (P_TORSO_SHADE, '8f968e'), (P_TORSO_LIGHT, 'e6e9e2')]),
    ('SHIRT', [(P_TORSO, '7aa6c8'), (P_TORSO_SHADE, '4f7896'), (P_TORSO_LIGHT, 'a8c8e0')]),
    ('SWEATER', [(P_TORSO, '8a2e2e'), (P_TORSO_SHADE, '5e1f1f'), (P_TORSO_LIGHT, 'b04848')]),
    ('POLICE_JACKET', [(P_TORSO, '2e3f66'), (P_TORSO_SHADE, '1f2b47'), (P_TORSO_LIGHT, '44598c')]),
    ('ARMY_JACKET', [(P_TORSO, '5a6640'), (P_TORSO_SHADE, '3e4a2a'), (P_TORSO_LIGHT, '76824e')]),
    ('FIREFIGHTER_JACKET', [(P_TORSO, 'b8962e'), (P_TORSO_SHADE, '7a6020'), (P_TORSO_LIGHT, 'e8c547')]),
    ('PADDED_JACKET', [(P_TORSO, 'd0702e'), (P_TORSO_SHADE, '96501f'), (P_TORSO_LIGHT, 'e8964e')]),
    ('CARGO_PANTS', [(P_LEGS, '4a5236'), (P_HIPS, '4a5236')]),
    ('SUIT_TROUSERS', [(P_LEGS, '2a2d33'), (P_HIPS, '2a2d33')]),
    ('OVERALLS', [(P_LEGS, '3d5a80'), (P_HIPS, '3d5a80'), (P_TORSO_LIGHT, '3d5a80')]),
    ('SNEAKERS', [(P_FEET, 'd8dce0')]),
    ('WELLIES', [(P_FEET, '3e5a2a')]),
    ('ARMY_BOOTS', [(P_FEET, '2a2418')]),
    ('WORK_GLOVES', [(P_HANDS, 'b08a5a')]),
    ('CAP', [(P_HAT, 'b02a20')]),
    ('HELMET', [(P_HAT, '5a6640')]),
    ('BANDANA', [(P_HAT, 'd0453a')]),
    ('DUFFEL_BAG', [(P_BAG, '1e2433')]),
    ('SATCHEL', [(P_BAG, '6b4a2e')]),
    ('LARGE_BACKPACK', [(P_BAG, 'b02a20')]),
]

# Zombie outfits by zone (spec: Zombies > Outfit): palette swaps of the zombie's clothing indices (torso base,
# shade, light; trousers; feet). The order matches kx::zombie_outfit.
Z_TORSO, Z_TORSO_SHADE, Z_TORSO_LIGHT, Z_LEGS, Z_FEET = 5, 6, 7, 8, 9
Z_HAIR2, Z_SHIRT2, Z_SHIRT2_SHADE, Z_SHIRT2_LIGHT = 12, 13, 14, 15
ZOMBIE_OUTFITS = [
    ('CIVILIAN', {}),
    ('POLICE', {Z_TORSO: '2e3f66', Z_TORSO_SHADE: '1f2b47', Z_TORSO_LIGHT: '44598c', Z_LEGS: '1e2433',
                Z_FEET: '0b0e14', Z_HAIR2: '9a9a96', Z_SHIRT2: '7aa6c8', Z_SHIRT2_SHADE: '4f7896',
                Z_SHIRT2_LIGHT: 'a8c8e0'}),
    ('SOLDIER', {Z_TORSO: '5a6640', Z_TORSO_SHADE: '3e4a2a', Z_TORSO_LIGHT: '76824e', Z_LEGS: '4a5236',
                 Z_FEET: '2a2418', Z_HAIR2: '6b4a2e', Z_SHIRT2: '8a7a52', Z_SHIRT2_SHADE: '5e5236',
                 Z_SHIRT2_LIGHT: 'a89a6a'}),
    ('DOCTOR', {Z_TORSO: 'c8ccc4', Z_TORSO_SHADE: '8f968e', Z_TORSO_LIGHT: 'e6e9e2', Z_LEGS: '3f7a80',
                Z_FEET: '3a3d42', Z_HAIR2: 'c8c8c0', Z_SHIRT2: '5f8a78', Z_SHIRT2_SHADE: '3f6a5a',
                Z_SHIRT2_LIGHT: '7fa898'}),
    ('FARMER', {Z_TORSO: 'a8402e', Z_TORSO_SHADE: '74291e', Z_TORSO_LIGHT: 'c8604a', Z_LEGS: '3d5a80',
                Z_FEET: '4a2e24', Z_HAIR2: 'a07040', Z_SHIRT2: '3d5a80', Z_SHIRT2_SHADE: '2a3f5e',
                Z_SHIRT2_LIGHT: '5a7aa0'}),
]

# Zombie bodies (playtest 1, mockup 16), each a full set of frames in the sheet: A as first drawn; B long hair in
# the second hair colour and the second shirt; C bald with the shirt torn open and a bare arm; D a beanie in the
# second shirt colour.
ZOMBIE_BODIES = 'ABCD'

DOWN_TOP = [
    '................',
    '......oooo......',
    '.....occcco.....',
    '....occcccco....',
    '....ohssssho....',
    '....osesseso....',
    '....osssssso....',
    '.....oSSSSo.....',
    '....ojjjjjjo....',
    '...ojljjjjdjo...',
    '...ojljjjjdjo...',
    '...ogjjjjjjgo...',
]
UP_TOP = [
    '................',
    '......oooo......',
    '.....occcco.....',
    '....occcccco....',
    '....ohhhhhho....',
    '....ohhhhhho....',
    '....oshhhhso....',
    '.....oSSSSo.....',
    '....ojkkkkjo....',
    '...ojjkkkkjjo...',
    '...ojjkkkkjjo...',
    '...ogjjjjjjgo...',
]
SIDE_TOP = [
    '................',
    '......oooo......',
    '.....occcco.....',
    '.....occccso....',
    '.....ohhsseo....',
    '.....ohsssso....',
    '......osssso....',
    '......oSSSo.....',
    '.....ojjjjo.....',
    '....okjjjljo....',
    '....okjjjljo....',
    '....okjjgjo.....',
]
FRONT_LEGS = {
    'stand': ['....ouuuuuuo....', '....oppooppo....', '....obboobbo....', '.....oo..oo.....'],
    'step_a': ['....ouuuuuuo....', '....oppooppo....', '....obbooppo....', '.....oo..bbo....'],
    'step_b': ['....ouuuuuuo....', '....oppooppo....', '....oppoobbo....', '....obb..oo.....'],
}
SIDE_LEGS = {
    'stand': ['.....ouuuuo.....', '.....oppppo.....', '.....obbbbbo....', '......ooooo.....'],
    'step_a': ['.....ouuuuo.....', '....oppooppo....', '...obbo..obbo...', '...ooo....ooo...'],
    'step_b': ['.....ouuuuo.....', '.....opppo......', '.....obbbbo.....', '.....ooooo......'],
}

# Poses per facing block (down, up, side); side frames face right and are mirrored for left.
PLAYER_POSES = ['stand', 'step_a', 'step_b', 'crouch', 'windup', 'strike']
ZOMBIE_POSES = ['stand', 'step_a', 'step_b', 'windup', 'lunge']


def put(rows, x, y, text):
    """Writes text into rows at (x, y); spaces leave pixels untouched."""
    row = list(rows[y])
    for i, c in enumerate(text):
        if c != ' ':
            row[x + i] = c
    rows[y] = ''.join(row)


def parse_frame(rows, keys=KEYS):
    assert len(rows) == 16, len(rows)
    out = []
    for r in rows:
        assert len(r) == 16, r
        out.append([keys[c] for c in r])
    return out


def player_arms(facing, frame, pose):
    """Weapon-hand poses on a full frame. The weapon hand is screen-right for down and side, screen-left for up."""
    rows = list(frame)
    if facing == 'down':
        put(rows, 3, 11, 'ogjjjjjjjjo')
        if pose == 'windup':
            put(rows, 12, 6, 'o')
            put(rows, 12, 7, 'go')
            put(rows, 12, 8, 'o')
        else:
            put(rows, 11, 12, 'go')
    elif facing == 'up':
        put(rows, 3, 11, 'ojjjjjjjjgo')
        if pose == 'windup':
            put(rows, 3, 6, 'o')
            put(rows, 2, 7, 'og')
            put(rows, 3, 8, 'o')
        else:
            put(rows, 3, 12, 'og')
    else:
        put(rows, 4, 11, 'okjjjjo')
        if pose == 'windup':
            put(rows, 3, 6, 'og')
            put(rows, 3, 7, 'oo')
        else:
            put(rows, 11, 9, 'ggo')
            put(rows, 11, 10, 'oo')
    return rows


def player_carry(facing, frame, drop=0):
    """Carrying a long item on the shoulder: the weapon arm bends up so its hand holds the handle at the shoulder
    (screen-right for down and side, screen-left for up). drop lowers the arm with a crouched body."""
    rows = list(frame)
    if facing == 'down':
        put(rows, 3, 11 + drop, 'ogjjjjjjjjo')
        put(rows, 12, 7 + drop, 'o')
        put(rows, 12, 8 + drop, 'go')
        put(rows, 12, 9 + drop, 'o')
    elif facing == 'up':
        put(rows, 3, 11 + drop, 'ojjjjjjjjgo')
        put(rows, 3, 7 + drop, 'o')
        put(rows, 2, 8 + drop, 'og')
        put(rows, 3, 9 + drop, 'o')
    else:
        put(rows, 4, 11 + drop, 'okjjjjo')
        put(rows, 9, 8 + drop, 'g')
        put(rows, 9, 9 + drop, 'l')
    return rows


# Carry frames (after the fallen frame): per facing block (down, up, side), the poses below.
CARRY_POSES = ['stand', 'step_a', 'step_b', 'crouch']


def player_aim(facing, frame, drop=0):
    """Aiming a gun (playtest 3, mockup 21): the arms held out at chest height towards the target, the gun at the
    hands (its own picture draws the gripping hand). Down: the hands together at the right of the chest. Side: the
    near arm straight out, the hand 5 px in front of the body. Up: the arms out of sight in front of the body.
    drop lowers the arms with a crouched body."""
    rows = list(frame)
    if facing == 'down':
        put(rows, 3, 11 + drop, 'ojjjjogggo')
    elif facing == 'up':
        put(rows, 3, 9 + drop, '.ojkkkkjo.')
        put(rows, 3, 10 + drop, '.ojkkkkjo.')
        put(rows, 3, 11 + drop, '.ojjjjjjo.')
    else:
        put(rows, 5, 8 + drop, 'ojjjjooo')
        put(rows, 4, 9 + drop, 'okjjjllllgo')
        put(rows, 4, 10 + drop, 'okjjjjoooo')
        put(rows, 4, 11 + drop, 'okjjjjo')
    return rows


def player_ready(facing, frame, drop=0):
    """A long gun at low ready (playtest 3, user's choice): held across the body in both hands, the muzzle down
    and forward. The gun's picture draws both hands, so the body's hands go from its sides."""
    rows = list(frame)
    if facing == 'down':
        put(rows, 3, 11 + drop, 'ojjjjjjjjo')
    elif facing == 'up':
        put(rows, 3, 11 + drop, 'ojjjjjjjjo')
    else:
        put(rows, 4, 11 + drop, 'okjjjjo')
    return rows


# Gun frames (after the swimming frames): per facing block (down, up, side), aiming then at low ready, each in
# the carry poses (stand, step_a, step_b, crouch).


def lying(frame_rows, clockwise):
    image = Image.new('P', (16, 16))
    image.putdata([p for row in frame_rows for p in row])
    rotated = image.rotate(-90 if clockwise else 90, expand=False)
    rows = [[rotated.getpixel((x, y)) for x in range(16)] for y in range(16)]
    return rows[4:] + [[0] * 16 for _ in range(4)]


def build_player():
    frames = []
    for facing, top, legs, crouch_boots in (('down', DOWN_TOP, FRONT_LEGS, '....obboobbo....'),
                                            ('up', UP_TOP, FRONT_LEGS, '....obboobbo....'),
                                            ('side', SIDE_TOP, SIDE_LEGS, '.....obbbbbo....')):
        for pose in ('stand', 'step_a', 'step_b'):
            frames.append(parse_frame(top + legs[pose]))
        # Crouch: upper body drops 3 px onto bent legs.
        frames.append(parse_frame(['................'] * 3 + top[1:12] + ['....ouuuuuuo....', crouch_boots]))
        frames.append(parse_frame(player_arms(facing, top + legs['stand'], 'windup')))
        frames.append(parse_frame(player_arms(facing, top + legs['step_a'], 'strike')))

    frames.append(lying(frames[0], False))   # fallen

    for facing, top, legs, crouch_boots in (('down', DOWN_TOP, FRONT_LEGS, '....obboobbo....'),
                                            ('up', UP_TOP, FRONT_LEGS, '....obboobbo....'),
                                            ('side', SIDE_TOP, SIDE_LEGS, '.....obbbbbo....')):
        for pose in ('stand', 'step_a', 'step_b'):
            frames.append(parse_frame(player_carry(facing, top + legs[pose])))
        crouched = ['................'] * 3 + top[1:12] + ['....ouuuuuuo....', crouch_boots]
        frames.append(parse_frame(player_carry(facing, crouched, 2)))

    # Milestone 13: swimming, per facing block, two strokes; the body stops at the waterline (a ripple sprite
    # covers the cut).
    for facing, top in (('down', DOWN_TOP), ('up', UP_TOP), ('side', SIDE_TOP)):
        for stroke in range(2):
            rows = top[:10] + ['................'] * 6
            if facing == 'side':
                put(rows, 11, 8 if stroke == 0 else 5, 'ggo')
            elif stroke == 0:
                put(rows, 1, 8, 'ogg')
                put(rows, 12, 8, 'ggo')
            else:
                put(rows, 2, 5, 'og')
                put(rows, 12, 5, 'go')
            frames.append(parse_frame(rows))

    # Playtest 3: holding a gun, aimed and at low ready.
    for shape in (player_aim, player_ready):
        for facing, top, legs, crouch_boots in (('down', DOWN_TOP, FRONT_LEGS, '....obboobbo....'),
                                                ('up', UP_TOP, FRONT_LEGS, '....obboobbo....'),
                                                ('side', SIDE_TOP, SIDE_LEGS, '.....obbbbbo....')):
            for pose in ('stand', 'step_a', 'step_b'):
                frames.append(parse_frame(shape(facing, top + legs[pose])))
            crouched = ['................'] * 3 + top[1:12] + ['....ouuuuuuo....', crouch_boots]
            frames.append(parse_frame(shape(facing, crouched, 2)))

    save_bmp('player', [r for f in frames for r in f], PLAYER_PALETTE, {'type': 'sprite', 'height': 16})
    return len(frames)


def zombify(top, facing):
    rows = [r.replace('l', 'd') for r in top]
    if facing == 'up':
        rows = [r.replace('k', 'd') for r in rows]
        put(rows, 6, 3, 'ss')                   # bald patch
    else:
        rows = [r.replace('k', 'j') for r in rows]
        put(rows, 7, 9, 'k')                    # blood stains
        put(rows, 9, 10, 'k')
        if facing == 'down':
            put(rows, 6, 4, 'ss')
    return rows


def zombie_body(top, facing, body):
    """The zombified upper body (12 rows) for a body (ZOMBIE_BODIES)."""
    rows = zombify(top, facing)

    def swap(table, y0, y1):
        return [r.translate(str.maketrans(table)) if y0 <= y < y1 else r for y, r in enumerate(rows)]

    if body == 'B':
        rows = swap({'c': 'H', 'h': 'H'}, 0, 7)
        rows = swap({'j': 'J', 'd': 'Q'}, 7, 12)
        if facing == 'up':
            rows = swap({'s': 'H'}, 3, 7)
        else:
            # Hair falling past the face.
            for y in (4, 5, 6):
                row = list(rows[y])
                left = row.index('o')
                right = len(row) - 1 - row[::-1].index('o')
                if facing == 'down':
                    row[left] = 'H'
                    row[left - 1] = 'o'
                row[right] = 'H'
                if right + 1 < 16:
                    row[right + 1] = 'o'
                rows[y] = ''.join(row)
    elif body == 'C':
        rows = swap({'c': 's', 'h': 'S'}, 0, 7)
        torn = {'down': [(5, 8), (6, 8), (6, 9), (7, 9), (8, 9), (6, 10), (7, 10), (9, 10), (10, 9), (10, 10)],
                'up': [(6, 9), (7, 9), (8, 10), (9, 10), (9, 9)],
                'side': [(6, 8), (7, 9), (8, 9), (6, 10), (7, 10), (8, 10)]}[facing]
        for x, y in torn:
            row = list(rows[y])
            if row[x] in 'jdk':
                row[x] = 'S' if (x + y) % 2 else 's'
            rows[y] = ''.join(row)
        row = list(rows[9])
        row[min(i for i, c in enumerate(row) if c in 'jd')] = 's'     # a bare arm
        rows[9] = ''.join(row)
    elif body == 'D':
        rows = swap({'c': 'J'}, 0, 3)
        rows[3] = ''.join('Q' if c in 'hsc' else c for c in rows[3])  # the turned-up brim
    return rows


def zombie_arms(facing, frame, pose):
    rows = list(frame)
    if facing in ('down', 'up'):
        put(rows, 3, 11, 'ojjjjjjjjo')
        if pose == 'windup':
            for x in (2, 11):
                put(rows, x + 1, 2, 'o')
                put(rows, x, 3, 'oso')
                put(rows, x, 4, 'oso')
        elif pose == 'lunge':
            put(rows, 2, 11, 'os')
            put(rows, 2, 12, 'os')
            put(rows, 12, 11, 'so')
            put(rows, 12, 12, 'so')
        else:
            put(rows, 3, 11, 'os')
            put(rows, 11, 11, 'so')
    else:
        put(rows, 4, 11, 'odjjjjo')
        if pose == 'windup':
            put(rows, 10, 4, 'oso')
            put(rows, 10, 5, 'oso')
        elif pose == 'lunge':
            put(rows, 10, 9, 'sssso')
            put(rows, 10, 10, 'ooooo')
        else:
            put(rows, 10, 9, 'sso')
    return rows


# A dead zombie (playtest 2): the lying frame in a pool of blood, so a corpse doesn't read as a zombie knocked down.
# Three sizes: the dying body splashes into the first two over its last frames, the corpse lies in the third. The
# pool stays in the sprite's top 13 rows (the corpse's tile holds rows 0-12 of it: kx_zombies draws it 3 px down).
DEAD_STAGES = 3
DEAD_ROWS = 13
BLOOD_DARK, BLOOD_LIGHT = 10, 11


def dead_frame(lying_rows, stage):
    """The pool: an ellipse under and around the body, darker at its rim, a few lighter wet pixels inside."""
    scale = (0.55, 0.8, 1.0)[stage]
    centre_x, centre_y = 7.5, 7.0
    radius_x, radius_y = 8.6 * scale, 5.6 * scale
    out = [list(row) for row in lying_rows]
    for y in range(DEAD_ROWS):
        for x in range(16):
            if out[y][x]:
                continue
            d = ((x - centre_x) / radius_x) ** 2 + ((y - centre_y) / radius_y) ** 2
            # A ragged edge: some rim pixels left out, by position.
            if d < 1.0 and not (d > 0.75 and (x * 7 + y * 3) % 5 == 0):
                out[y][x] = BLOOD_DARK if d > 0.5 or (x * 5 + y * 3) % 7 else BLOOD_LIGHT
    return out


def build_zombie():
    """Per body: the 3 facing blocks of poses, then the two crawl frames."""
    frames = []
    for body in ZOMBIE_BODIES:
        first = len(frames)
        for facing, top, legs in (('down', DOWN_TOP, FRONT_LEGS), ('up', UP_TOP, FRONT_LEGS),
                                  ('side', SIDE_TOP, SIDE_LEGS)):
            ztop = zombie_body(top, facing, body)
            for pose in ZOMBIE_POSES:
                leg_pose = pose if pose in legs else ('step_a' if pose == 'lunge' else 'stand')
                frames.append(parse_frame(zombie_arms(facing, ztop + legs[leg_pose], pose), ZOMBIE_KEYS))

        crawl_a = lying(frames[first + 2 * len(ZOMBIE_POSES)], True)
        crawl_b = [([0] + row[:15]) if y < 8 else list(row) for y, row in enumerate(crawl_a)]   # reaching
        frames += [crawl_a, crawl_b]
        frames += [dead_frame(crawl_a, stage) for stage in range(DEAD_STAGES)]
    save_bmp('zombie', [r for f in frames for r in f], ZOMBIE_PALETTE, {'type': 'sprite', 'height': 16})
    return len(frames)


def gba_color(value):
    r, g, b = hexc(value)
    return 'bn::color(%d, %d, %d)' % (r >> 3, g >> 3, b >> 3)


def gba_rgb(rgb):
    return 'bn::color(%d, %d, %d)' % (rgb[0] >> 3, rgb[1] >> 3, rgb[2] >> 3)


def write_outfit_header():
    lines = ['#include "bn_color.h"', '', '#include "kx_items.h"', '', 'namespace kx::outfit', '{', '',
             '// Player palette indices of the clothing areas (tools/assets/characters.py).',
             'constexpr int torso = %d;' % P_TORSO, 'constexpr int torso_shade = %d;' % P_TORSO_SHADE,
             'constexpr int torso_light = %d;' % P_TORSO_LIGHT, 'constexpr int legs = %d;' % P_LEGS,
             'constexpr int feet = %d;' % P_FEET, 'constexpr int bag = %d;' % P_BAG,
             'constexpr int hips = %d;' % P_HIPS, 'constexpr int hat = %d;' % P_HAT,
             'constexpr int hands = %d;' % P_HANDS, '',
             'struct area_colour', '{', '    uint8_t index;', '    bn::color colour;', '};', '',
             '// Without clothes.',
             'constexpr area_colour bare[] = {']
    lines += ['    { %d, %s },' % (index, gba_color(value)) for index, value in BARE.items()]
    lines += ['};', '', '// Colours each worn item puts on its areas.', 'struct look', '{', '    item_id item;',
              '    uint8_t count;', '    area_colour colours[3];', '};', '', 'constexpr look looks[] = {']
    for item, areas in OUTFIT:
        colours = ', '.join('{ %d, %s }' % (index, gba_color(value)) for index, value in areas)
        lines.append('    { item_id::%s, %d, { %s } },' % (item, len(areas), colours))
    lines += ['};', '',
              '// Zombie outfits by zone (palette swaps): %s.' % ', '.join(n.lower() for n, _ in ZOMBIE_OUTFITS),
              'constexpr int zombie_outfits = %d;' % len(ZOMBIE_OUTFITS), '',
              'constexpr bn::color zombie_palettes[zombie_outfits][16] = {']
    for name, areas in ZOMBIE_OUTFITS:
        colours = [gba_rgb(hexc(areas[i]) if i in areas else c) for i, c in enumerate(ZOMBIE_PALETTE)]
        lines.append('    {   // %s' % name)
        for row in range(0, 16, 4):
            lines.append('        ' + ', '.join(colours[row:row + 4]) + ',')
        lines.append('    },')
    lines += ['};', '', '}']
    write_header('kx_outfit.h', lines)


def build_characters():
    write_outfit_header()
    player_count = build_player()
    zombie_count = build_zombie()
    lines = ['namespace kx::sprite_frames', '{', '',
             '// Facing blocks: down, up, side (side faces right; mirror for left).',
             'constexpr int player_poses = %d;' % len(PLAYER_POSES)]
    lines += ['constexpr int player_%s = %d;' % (pose, i) for i, pose in enumerate(PLAYER_POSES)]
    lines += ['constexpr int player_fallen = %d;' % (3 * len(PLAYER_POSES)),
              '// Carrying a long item on the shoulder: per facing block, %s.' % ', '.join(CARRY_POSES),
              'constexpr int player_carry = %d;' % (3 * len(PLAYER_POSES) + 1),
              'constexpr int player_carry_poses = %d;' % len(CARRY_POSES),
              '// Swimming (milestone 13): per facing block, two strokes.',
              'constexpr int player_swim = %d;' % (3 * len(PLAYER_POSES) + 1 + 3 * len(CARRY_POSES)),
              'constexpr int player_swim_strokes = 2;',
              '// Holding a gun (playtest 3): per facing block, the carry poses aiming, then at low ready.',
              'constexpr int player_aim = %d;' % (3 * len(PLAYER_POSES) + 1 + 3 * len(CARRY_POSES) + 3 * 2),
              'constexpr int player_ready = %d;' % (3 * len(PLAYER_POSES) + 1 + 6 * len(CARRY_POSES) + 3 * 2),
              'constexpr int player_count = %d;' % player_count, '',
              'constexpr int zombie_poses = %d;' % len(ZOMBIE_POSES)]
    lines += ['constexpr int zombie_%s = %d;' % (pose, i) for i, pose in enumerate(ZOMBIE_POSES)]
    lines += ['constexpr int zombie_crawl = %d;           // two frames, lying, facing right' % (3 * len(ZOMBIE_POSES)),
              '// Playtest 2: lying dead in a pool of blood, small to full (the corpse lies in the last).',
              'constexpr int zombie_dead = %d;' % (3 * len(ZOMBIE_POSES) + 2),
              'constexpr int zombie_dead_frames = %d;' % DEAD_STAGES,
              '// Bodies (playtest 1): each a block of zombie_body_frames frames laid out as above.',
              'constexpr int zombie_bodies = %d;' % len(ZOMBIE_BODIES),
              'constexpr int zombie_body_frames = %d;' % (zombie_count // len(ZOMBIE_BODIES)),
              'constexpr int zombie_count = %d;' % zombie_count, '', '}']
    write_header('kx_sprite_frames.h', lines)
