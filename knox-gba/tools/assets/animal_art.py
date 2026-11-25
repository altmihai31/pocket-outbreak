"""Animals plan art (stage 1, agreed 2026-11-24): the boar, wolf, hen and goat, and the deer and rabbit redrawn to
match, in the animal palette (keys as in animals.py). Not built into the ROM yet: stages 2-4 move it into animals.bmp.

Side frames face right (mirror for left). The wolf, boar, goat and deer are 32x16 sprites, the hen and rabbit 16x16;
FRAMES holds every frame padded to its sprite's size, feet on the bottom rows.

Most frames are drawn as colours only and outlined here (`make`): a 1 px outline round the shape, none round blood or
the hen's yellow legs. The wolf, hen and boar standing frames come from the user's reference (native pixels shrunk,
then fixed by hand) and are already outlined; their other side frames keep the standing body and redraw the legs
(`compose`)."""

NO_OUTLINE = 'bBy'


def _grid(rows, width, height=16):
    g = [list(r.ljust(width, '.')) for r in rows] + [['.'] * width for _ in range(height - len(rows))]
    assert all(len(r) == width for r in g) and len(g) == height, (width, [len(r) for r in g])
    return g


def _solid(c):
    return c not in '.o' + NO_OUTLINE


def make(rows, width, height=16, from_row=0):
    """Outlines a colours-only drawing; with from_row, only round what lies on or below that row."""
    g = _grid(rows, width, height)
    out = [r[:] for r in g]
    for y in range(height):
        for x in range(width):
            if g[y][x] != '.':
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < width and 0 <= ny < height and _solid(g[ny][nx]) and max(y, ny) >= from_row:
                    out[y][x] = 'o'
                    break
    return [''.join(r) for r in out]


def compose(base, legs, from_row):
    """The outlined base's rows above from_row, then colours-only rows from there down, outlined."""
    width = len(base[0])
    return make(base[:from_row] + [r.ljust(width, '.') for r in legs], width, from_row=from_row)


def lift(rows, columns):
    """A step for front and back views: each listed column's lowest pixel moves up one."""
    g = [list(r) for r in rows]
    for x in columns:
        y = max(y for y in range(len(g)) if x < len(g[y]) and g[y][x] != '.')
        if x >= len(g[y - 1]):
            g[y - 1] += ['.'] * (x + 1 - len(g[y - 1]))
        g[y - 1][x], g[y][x] = g[y][x], '.'
    return [''.join(r) for r in g]


# ---- The standing frames from the reference.

WOLF_STAND = [
    '..................o.o...',
    '.................oKoKo..',
    '................oKKKKKo.',
    '...............oKKKKeKo.',
    '.............ooKKKllWWWo',
    '......oooooookKKKKlWWoo.',
    '.....oKKKKKKKKKkKKKko...',
    '....oKKKKKllKKKKkllo....',
    '....oKlKKlKlllllllo.....',
    '...oKKllllKoollllo......',
    '..oKKokoKlo..okoKlo.....',
    '.oKKookoolo..okoolo.....',
    'oKlo.okoolo..okoolo.....',
    'olo..okoolo..okoolo.....',
    'oo...okoolo..okoolo.....',
    '.....oooooo..oooooo.....',
]

HEN_STAND = [
    '................',
    '........obobo...',
    '........obbbbo..',
    '........oWWWWo..',
    '........oWWeWo..',
    '..o.....oWWWWyy.',
    '.oWo....oWWWbo..',
    '.oLWo..oWWWbo...',
    '.oWLWooWWWWWo...',
    '.oWWLWWWWLWWWo..',
    '.olWLWLLlWWWWo..',
    '..olWWWLlWWWo...',
    '...olWWWWWWo....',
    '....ooooooo.....',
    '......y..y......',
    '.....yy.yy......',
]

BOAR_STAND = [
    '............oo.o.........',
    '..........ooSSooo........',
    '........ooSSSSSSSoo......',
    '......ooSSSSSSSdSSSo.....',
    '....ooSSSSSSddSdddSo.....',
    '...oSddSdSdddSSddoddo....',
    '..oSddSdddddddSSSddodo...',
    '.odddddddddddddSSdddedo..',
    '.ooddddddddddddSdddLddWo.',
    'odddddddddSSdddSSdLLLWdLo',
    'oSSSdddSSSSSdddSSSoooooo.',
    '.oSoooddoooSoooddo.......',
    '.oSo.oddo.oSo.oddo.......',
    '.oSo.oddo.oSo.oddo.......',
    '.oko.okko.oko.okko.......',
    '.ooo.oooo.ooo.oooo.......',
]

# ---- Goat, deer and rabbit (colours only).

GOAT = {
    'stand': [
        '',
        '...........kk',
        '............kRr',
        '..........RRRrrrr',
        '............RrerWWW',
        '............RrrrlWW',
        '..Rr........Rrrrll',
        '...rRRRRRRRRRrrrl',
        '...rrrrrrrrrrrrrlW',
        '...Rrrrrrrrrrrrrl',
        '....RRllllllllRR',
        '....r.r......r.r',
        '....r.r......r.r',
        '....W.W......W.W',
        '....S.S......S.S',
    ],
    'step': [
        '',
        '...........kk',
        '............kRr',
        '..........RRRrrrr',
        '............RrerWWW',
        '............RrrrlWW',
        '..Rr........Rrrrll',
        '...rRRRRRRRRRrrrl',
        '...rrrrrrrrrrrrrlW',
        '...Rrrrrrrrrrrrrl',
        '....RRllllllllRR',
        '....r.r......r.r',
        '...r...r....r...r',
        '...W...W....W...W',
        '..S.....S..S.....S',
    ],
    'dead': [
        '', '', '', '', '', '', '', '',
        '..............kk',
        '...............kRr',
        '..RRRRRRRRRRRRRRrerWW',
        '..rrrrBBbrrrrrrrrrlWW',
        '..llllllllllllllr.ll',
        '...W.W.bbB...W.W',
        '...S.S.bBbbB.S.S',
        '........bbB',
    ],
}

DEER = {
    'side_stand': [
        '..............S..S',
        '..............dL.Ld',
        '...............dddd',
        '...............dedLL',
        '..............SdddLLe',
        '..............SddWW',
        '.............SddWW',
        '.............SddW',
        '...WSSSSSSSSSSddW',
        '...WddddddddddddL',
        '....dLLLLLLLLLLd',
        '....Sd........Sd',
        '....S.d.......S.d',
        '....S.d.......S.d',
        '....k.k.......k.k',
    ],
    'side_step': [
        '..............S..S',
        '..............dL.Ld',
        '...............dddd',
        '...............dedLL',
        '..............SdddLLe',
        '..............SddWW',
        '.............SddWW',
        '.............SddW',
        '...WSSSSSSSSSSddW',
        '...WddddddddddddL',
        '....dLLLLLLLLLLd',
        '....Sd........Sd',
        '...S..d......S..d',
        '..S....d....S....d',
        '..k....k....k....k',
    ],
    'down_stand': [
        '',
        '..dd......dd',
        '...dLddddLd',
        '.....dddd',
        '.....edde',
        '.....dddd',
        '......LL',
        '......ee',
        '.....WWWW',
        '.....SWWS',
        '....SddddS',
        '....dLddLd',
        '.....d..d',
        '.....d..d',
        '.....k..k',
    ],
    'down_step': [
        '',
        '..dd......dd',
        '...dLddddLd',
        '.....dddd',
        '.....edde',
        '.....dddd',
        '......LL',
        '......ee',
        '.....WWWW',
        '.....SWWS',
        '....SddddS',
        '....dLddLd',
        '.....d..d',
        '.....d..k',
        '.....k',
    ],
    'up_stand': [
        '',
        '..dd......dd',
        '...dSddddSd',
        '.....SddS',
        '.....dddd',
        '.....dddd',
        '......dd',
        '.....SddS',
        '....SddddS',
        '....dddddd',
        '....dLWWLd',
        '....dWWWWd',
        '.....S..S',
        '.....S..S',
        '.....k..k',
    ],
    'up_step': [
        '',
        '..dd......dd',
        '...dSddddSd',
        '.....SddS',
        '.....dddd',
        '.....dddd',
        '......dd',
        '.....SddS',
        '....SddddS',
        '....dddddd',
        '....dLWWLd',
        '....dWWWWd',
        '.....S..S',
        '.....S..k',
        '.....k',
    ],
    'carcass': [
        '', '', '', '', '', '', '', '',
        '.................L.L',
        '................dddd',
        '..WSSSSSSSSSSSSSSdedd',
        '..WddddBBbddddddddLLe',
        '...LLLLLLLLLLLLL..LL',
        '...S.S..bbB...S.S',
        '...k.k.bBbbBb.k.k',
        '........bbB',
    ],
}

RABBIT = {
    'sit': [
        '', '', '', '', '', '',
        '........R.r',
        '........R.r',
        '.........rr',
        '........rrer',
        '...rrrr.rrrr',
        '..Wrrrrrrrrll',
        '..WRrrrrrrrll',
        '...RRRrrrrll',
        '....RRR..rl',
    ],
    'hop': [
        '', '', '', '', '', '',
        '....Rl',
        '.....Rl',
        '......Rrr',
        '...rrr.rrer',
        '..rrrrrrrrrl',
        '.WrrrrrrrrlL',
        '.WRRrrrrll',
        'RR.....l.l',
    ],
}

# ---- Side frames for the wolf, boar and hen, after the standing ones.

WOLF_RUN_LEGS = [           # rows 10-13: the stride stretched, hind legs back, front legs reaching
    '...KK.k.Kl....k.Kl',
    '..KK.k..l......k..l',
    '.Kl.k..l........k..l',
    '.l.k...l.........k..l',
]
WOLF_DEAD = [
    '', '', '', '', '', '', '', '',
    '................K.K',
    '................KKK',
    '.....kkkkkkkkkkKKKeKlW',
    'lKKKKKKKKKKBBbKKKKKlWW',
    '....llllllllllllll..l',
    '.....K.K.bbB.K.K',
    '.....l.lbBbbB.l.l',
    '.........bbB',
]
BOAR_RUN_LEGS = [           # rows 12-13: the charge, legs reaching, hooves off the ground
    '.S...dd.....S...dd',
    '.k..kk.......k...kk',
]
BOAR_DEAD = [
    '', '', '', '', '', '', '', '',
    '..........SSSSSS',
    '.....SSSSSSSSSSSSSSd',
    '...SddddddddddddddddedLL',
    '..SddddBBbdddddddddddLLLW',
    '...SSSSSSSSSSSSSSSSdLW',
    '....S.S.bbB.S.S',
    '....k.kbBbbBk.k',
    '.......bbB',
]
HEN_PECK = [
    '', '', '', '', '', '',
    '..W',
    '..LW',
    '..WLWWWWW',
    '..WWLWWWWLWW',
    '..lWLWLLlWWWW.b',
    '...lWWWLlWWWWbb',
    '....lWWWWWWWWeW',
    '...........bWWy',
    '......y..y',
    '.....yy.yy',
]
HEN_FLAP = [
    '',
    '.........b.b',
    '.........bbbb',
    '...WW....WWWW',
    '...LWW...WWeW',
    '....LWW..WWWWyy',
    '..W..LWW.WWWb',
    '..LW..WWWWWb',
    '..WLW.WWWWWW',
    '..WWLWWWWLWWW',
    '..lWLWWWWWWWW',
    '...lWWWWWWWW',
    '....lWWWWWW',
    '',
    '......y..y',
    '.....yy.yy',
]
HEN_DEAD = [
    '', '', '', '', '', '', '', '', '', '',
    '..W.........b',
    '..LWWWWWWWWbbb',
    '..WLWLLlWWWWeWy',
    '..lWWWWWWWWWWW',
    '....y.y..bbB',
    '...y.y..bBbbB',
]

# ---- Front (down) and back (up) views, colours only, 16 wide; the step lifts one leg.

FRONT = {
    'wolf': [
        '',
        '....K....K',
        '....KK..KK',
        '....KKKKKK',
        '....KeKKeK',
        '....KKllKK',
        '.....lWWl',
        '......ee',
        '.....kWWk',
        '....kKWWKk',
        '....KKllKK',
        '....KllllK',
        '.....l..l',
        '.....l..l',
        '.....K..K',
    ],
    'boar': [
        '',
        '',
        '....S......S',
        '....SS....SS',
        '.....SSSSSS',
        '....SddddddS',
        '....dedddded',
        '....dddddddd',
        '.....WLLLLW',
        '.....WLeeLW',
        '....SddddddS',
        '...SddddddddS',
        '....SSddddSS',
        '.....S....S',
        '.....k....k',
    ],
    'goat': [
        '',
        '....k....k',
        '....kk..kk',
        '.....RrrR',
        '...RRerreRR',
        '.....rWWr',
        '.....rWWr',
        '......WW',
        '......ll',
        '.....rllr',
        '....RrrrrR',
        '....RrllrR',
        '.....r..r',
        '.....W..W',
        '.....S..S',
    ],
    'hen': [
        '',
        '',
        '.......b',
        '......bbb',
        '.....WWWWW',
        '.....eWyWe',
        '......WbW',
        '.....WWWWW',
        '....WWWWWWW',
        '....lWWWWWl',
        '....lWWWWWl',
        '.....lWWWl',
        '......lll',
        '',
        '......y.y',
        '.....yy.yy',
    ],
}
BACK = {
    'wolf': [
        '',
        '....K....K',
        '....kK..Kk',
        '....kkkkkk',
        '....kKKKKk',
        '....KKKKKK',
        '.....kKKk',
        '.....kkkk',
        '....kkkkkk',
        '....KkkkkK',
        '....KKKKKK',
        '....KKKKKK',
        '.....lKKl',
        '.....l..l',
        '.....K..K',
    ],
    'boar': [
        '',
        '',
        '....S......S',
        '....SS....SS',
        '.....SSSSSS',
        '....SSSSSSSS',
        '.....SSSSSS',
        '....SSSSSSSS',
        '...SddSSSSddS',
        '...SdddSSdddS',
        '...SddddddddS',
        '...SdddSSdddS',
        '....SddddddS',
        '.....S....S',
        '.....k....k',
    ],
    'goat': [
        '',
        '....k....k',
        '....kk..kk',
        '.....RRRR',
        '...RRRrrRRR',
        '.....RrrR',
        '.....RrrR',
        '......RR',
        '.....rRRr',
        '....rrRRrr',
        '....rrRRrr',
        '....rrrrrr',
        '.....r..r',
        '.....W..W',
        '.....S..S',
    ],
    'hen': [
        '',
        '',
        '.......b',
        '......bbb',
        '.....WWWWW',
        '.....WWWWW',
        '......WWW',
        '.....WWWWW',
        '....WLWWWLW',
        '....WWLWLWW',
        '....lWWWWWl',
        '.....lWWWl',
        '......lll',
        '',
        '......y.y',
        '.....yy.yy',
    ],
}
STEP_LEG = {'wolf': [8], 'boar': [10], 'goat': [8], 'hen': [8, 9]}


# ---- Crows (stage 5): 8x8, side, facing right, drawn outlined: perched, pecking, wings up, wings down.

CROW = [
    ['........', '....oo..', '...okeoy', '..okkko.', '.okkkko.', 'okkkko..', '..o.o...', '..y.y...'],
    ['........', '........', '........', '...oo...', '.okkkoo.', 'okkkkkeo', '..o.o.oy', '..y.y...'],
    ['o.....o.', 'ok...ko.', '.ok.ko..', '..okkoey', '..okkko.', '...ooo..', '...y.y..', '........'],
    ['........', '........', '...ookey', 'ookkkko.', 'okkkkkko', '.o.oo.o.', '...y.y..', '........'],
]
CROW_FRAMES = ['perched', 'peck', 'wings_up', 'wings_down']


# ---- Every frame, outlined and padded to its sprite (32x16 or 16x16).

def _pad(rows, width):
    left = (width - len(rows[0])) // 2
    return ['.' * left + r + '.' * (width - len(r) - left) for r in rows]


def _views(name, size):
    front = make(FRONT[name], 16)
    back = make(BACK[name], 16)
    return {
        'down_stand': _pad(front, size), 'down_step': _pad(make(lift(FRONT[name], STEP_LEG[name]), 16), size),
        'up_stand': _pad(back, size), 'up_step': _pad(make(lift(BACK[name], STEP_LEG[name]), 16), size),
    }


def frames():
    out = {}
    out['wolf'] = {'side_stand': _pad(WOLF_STAND, 32), 'side_run': _pad(compose(WOLF_STAND, WOLF_RUN_LEGS, 10), 32),
                   **_views('wolf', 32), 'dead': _pad(make(WOLF_DEAD, 24), 32)}
    out['boar'] = {'side_stand': _pad(BOAR_STAND, 32), 'side_run': _pad(compose(BOAR_STAND, BOAR_RUN_LEGS, 12), 32),
                   **_views('boar', 32), 'dead': _pad(make(BOAR_DEAD, 27), 32)}
    out['goat'] = {'side_stand': _pad(make(GOAT['stand'], 26), 32), 'side_step': _pad(make(GOAT['step'], 26), 32),
                   **_views('goat', 32), 'dead': _pad(make(GOAT['dead'], 26), 32)}
    out['hen'] = {'side_stand': HEN_STAND, 'side_peck': make(HEN_PECK, 16), 'side_flap': make(HEN_FLAP, 16),
                  **_views('hen', 16), 'dead': make(HEN_DEAD, 16)}
    out['deer'] = {k: (_pad(make(v, 28), 32) if k.startswith('side') or k == 'carcass' else _pad(make(v, 16), 32))
                   for k, v in DEER.items()}
    out['rabbit'] = {k: make(v, 16) for k, v in RABBIT.items()}
    return out


FRAMES = frames()
