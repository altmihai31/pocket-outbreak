"""Milestone 13 sprites: deer, rabbits and birds (the wild), and the ripple round a swimmer.

animals.bmp (16x16): deer side (stand, step), down (stand, step), up (stand, step), the deer carcass, then the rabbit
side (sit, hop). Side frames face right and are mirrored for left. birds.bmp (8x8): perched, wings up, wings down.
ripple.bmp (16x8): two frames of the ring of water round a swimmer's waist."""
from common import TRANSPARENT, hexc, save_bmp, write_header

ANIMAL_PALETTE = [TRANSPARENT, hexc('151a22'), hexc('8a5a34'), hexc('5e3c22'), hexc('b88458'), hexc('e8e4d8'),
                  hexc('0b0e14'), hexc('8f7e68'), hexc('5f5244'), hexc('b8a890'), hexc('d0453a'), hexc('7a2a22'),
                  hexc('3a3d42'), hexc('5a6270'), hexc('e8c547')] + [(0, 0, 0)]
ANIMAL_KEYS = {'.': 0, 'o': 1, 'd': 2, 'S': 3, 'L': 4, 'W': 5, 'e': 6, 'r': 7, 'R': 8, 'l': 9, 'b': 10, 'B': 11,
               'k': 12, 'K': 13, 'y': 14}

DEER_SIDE_TOP = [
    '................',
    '...........o.o..',
    '..........oSoSo.',
    '..........odddo.',
    '..........oddeo.',
    '..........odddLo',
    '..oo......oddoo.',
    '.oWWoooooooddo..',
    '.oWdLLLLLLLddo..',
    '..odddddddddddo.',
    '..oSddddddddSo..',
    '...oWWWWWWWWo...',
]
DEER_SIDE_LEGS = {
    'stand': ['...oSo....oSo...', '...oSo....oSo...', '...oSo....oSo...', '...oo.....oo....'],
    'step': ['..oSo......oSo..', '..oSo.....oSo...', '.oSo.....oSo....', '.oo......oo.....'],
}
DEER_DOWN = {
    'stand': [
        '................',
        '....o......o....',
        '....oSo..oSo....',
        '.....oooooo.....',
        '.....oddddo.....',
        '.....oeddeo.....',
        '.....oddddo.....',
        '......oLLo......',
        '.....oddddo.....',
        '....oddWWddo....',
        '....odLWWLdo....',
        '....oddddddo....',
        '....oSo..oSo....',
        '....oSo..oSo....',
        '....oSo..oSo....',
        '....oo....oo....',
    ],
    'step': [
        '................',
        '....o......o....',
        '....oSo..oSo....',
        '.....oooooo.....',
        '.....oddddo.....',
        '.....oeddeo.....',
        '.....oddddo.....',
        '......oLLo......',
        '.....oddddo.....',
        '....oddWWddo....',
        '....odLWWLdo....',
        '....oddddddo....',
        '....oSo..oSo....',
        '....oSo..oo.....',
        '....oSo.........',
        '....oo..........',
    ],
}
DEER_UP = {
    'stand': [
        '................',
        '....o......o....',
        '....oSo..oSo....',
        '.....oooooo.....',
        '.....oddddo.....',
        '.....oddddo.....',
        '.....oSddSo.....',
        '......oddo......',
        '.....oddddo.....',
        '....oLddddLo....',
        '....oLddddLo....',
        '....odoWWodo....',
        '....oSoWWoSo....',
        '....oSo..oSo....',
        '....oSo..oSo....',
        '....oo....oo....',
    ],
    'step': [
        '................',
        '....o......o....',
        '....oSo..oSo....',
        '.....oooooo.....',
        '.....oddddo.....',
        '.....oddddo.....',
        '.....oSddSo.....',
        '......oddo......',
        '.....oddddo.....',
        '....oLddddLo....',
        '....oLddddLo....',
        '....odoWWodo....',
        '....oSoWWoSo....',
        '....oo...oSo....',
        '.........oSo....',
        '..........oo....',
    ],
}
DEER_CARCASS = [
    '................',
    '................',
    '................',
    '................',
    '................',
    '................',
    '...........o.o..',
    '..........oSoSo.',
    '.ooooooooooddddo',
    'oWddddddddddeddo',
    'oWdddBBbdddddddo',
    '.oWWWWWWWWoooLo.',
    '.oSSoSSo.oSSoSSo',
    '..oobBoo..oo.oo.',
    '....bbBb........',
    '................',
]
# Playtest 2: redrawn, it read as a rat or a shoe (stub ears, a flat wedge): tall ears (the far one in shade), a round
# back over the haunch, a light chest, the white tail; hopping, stretched out with the ears laid back.
RABBIT = {
    'sit': [
        '................',
        '..........o..oo.',
        '.........oRoollo',
        '.........oRoollo',
        '.........oRoollo',
        '..........oRoro.',
        '.........orrrro.',
        '........orrrrrro',
        '....oooo.orrrero',
        '...orrrrRrrrrrro',
        '..orrrrrrrrrrRo.',
        '.oWWrrrrrrrrllo.',
        '.oWWRrrrrrrrllo.',
        '..oRRRrrrrrrllo.',
        '...oRRRrrorlloo.',
        '....oooooo.ooo..',
    ],
    'hop': [
        '................',
        '........oo......',
        '.......oRlo.....',
        '........oRllo...',
        '.........oRlro..',
        '..........orrrro',
        '....ooo..orrrero',
        '..oorrrooorrrrro',
        '.oWrrrrrrrrrrrRo',
        '.oWWrrrrrrrrrllo',
        '..oRRrrrrrrrllo.',
        '..oRRRRrrrrllo..',
        '.oRRoo.....olo..',
        'oRRo........oo..',
        'ooo.............',
        '................',
    ],
}
BIRDS = [
    ['........', '...oo...', '..oKKo..', '.oKkkKo.', '.oKkkKo.', '..oKKo..', '...yy...', '........'],
    ['o......o', 'oK....Ko', '.oKooKo.', '..okko..', '..oKKo..', '...oo...', '...yy...', '........'],
    ['........', '........', '...oo...', 'ooKkkKoo', 'oKKkkKKo', '.o.oo.o.', '...yy...', '........'],
]

RIPPLE_PALETTE = [TRANSPARENT, hexc('7fb0e0'), hexc('e8e4d8'), hexc('2a5a9a')] + [(0, 0, 0)] * 12
RIPPLES = [
    ['................', '................', '.11..........11.', '1.............21', '.12..........21.',
     '...2222222222...', '................', '................'],
    ['................', '.1............1.', '1..............1', '.2............2.', '..2..........2..',
     '...1111111111...', '.....333333.....', '................'],
]

DEER_FRAMES = ['side_stand', 'side_step', 'down_stand', 'down_step', 'up_stand', 'up_step', 'carcass']
RABBIT_FRAMES = ['sit', 'hop']


def parse(rows, keys):
    for r in rows:
        assert len(r) == len(rows[0]), r
    return [[keys[c] for c in r] for r in rows]


def animal_frames():
    frames = [DEER_SIDE_TOP + DEER_SIDE_LEGS['stand'], DEER_SIDE_TOP + DEER_SIDE_LEGS['step'],
              DEER_DOWN['stand'], DEER_DOWN['step'], DEER_UP['stand'], DEER_UP['step'], DEER_CARCASS,
              RABBIT['sit'], RABBIT['hop']]
    for f in frames:
        assert len(f) == 16 and all(len(r) == 16 for r in f), f
    return [parse(f, ANIMAL_KEYS) for f in frames]


# Animals plan (2026-11-24): the sheets come from animal_art.py. animals.bmp holds the 16x16 animals, animals_wide.bmp
# the 32x16 ones; the deer and rabbit rows above are the milestone 13 originals, kept for mockup 15.
NARROW = [('rabbit', ['sit', 'hop']),
          ('hen', ['side_stand', 'side_peck', 'side_flap', 'down_stand', 'down_step', 'up_stand', 'up_step', 'dead'])]
WIDE = [('deer', ['side_stand', 'side_step', 'down_stand', 'down_step', 'up_stand', 'up_step', 'carcass']),
        ('boar', ['side_stand', 'side_run', 'down_stand', 'down_step', 'up_stand', 'up_step', 'dead']),
        ('wolf', ['side_stand', 'side_run', 'down_stand', 'down_step', 'up_stand', 'up_step', 'dead']),
        ('goat', ['side_stand', 'side_step', 'down_stand', 'down_step', 'up_stand', 'up_step', 'dead'])]


def sheet_frames(groups, width):
    from animal_art import FRAMES
    frames, names = [], []
    for animal, keys in groups:
        for key in keys:
            rows = FRAMES[animal][key]
            assert len(rows) == 16 and all(len(r) == width for r in rows), (animal, key)
            frames.append(parse(rows, ANIMAL_KEYS))
            names.append('%s_%s' % (animal, key))
    return frames, names


def build_animals():
    narrow, narrow_names = sheet_frames(NARROW, 16)
    wide, wide_names = sheet_frames(WIDE, 32)
    save_bmp('animals', [r for f in narrow for r in f], ANIMAL_PALETTE, {'type': 'sprite', 'height': 16})
    save_bmp('animals_wide', [r for f in wide for r in f], ANIMAL_PALETTE, {'type': 'sprite', 'height': 16})
    from animal_art import CROW
    birds = [parse(f, ANIMAL_KEYS) for f in BIRDS + CROW]
    save_bmp('birds', [r for f in birds for r in f], ANIMAL_PALETTE, {'type': 'sprite', 'height': 8})
    ripples = [[[int(c) if c != '.' else 0 for c in r] for r in f] for f in RIPPLES]
    save_bmp('ripple', [r for f in ripples for r in f], RIPPLE_PALETTE, {'type': 'sprite', 'height': 8})

    lines = ['namespace kx::animal_frames', '{', '',
             '// animals.bmp (16x16) and animals_wide.bmp (32x16); side frames face right (mirror for left).']
    lines += ['constexpr int %s = %d;' % (name, i) for i, name in enumerate(narrow_names)]
    lines += ['']
    lines += ['constexpr int %s = %d;' % (name, i) for i, name in enumerate(wide_names)]
    lines += ['', '// birds.bmp (8x8).', 'constexpr int bird_perched = 0;', 'constexpr int bird_wings_up = 1;',
              'constexpr int bird_wings_down = 2;', 'constexpr int crow_perched = 3;', 'constexpr int crow_peck = 4;',
              'constexpr int crow_wings_up = 5;', 'constexpr int crow_wings_down = 6;', '', '}']
    write_header('kx_animal_frames.h', lines)
