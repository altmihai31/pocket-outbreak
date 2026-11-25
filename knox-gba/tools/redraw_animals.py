"""Goat, deer and rabbit redrawn in the style of the reference animals: colours drawn by hand, outline added around."""
import sys

from PIL import Image

sys.path.insert(0, sys.argv[1] + '/tools/assets')
sys.path.insert(0, sys.argv[2])
import animals  # noqa: E402
import ref_animals as ref  # noqa: E402

BLOOD = 'bB'


def make(rows, width, height=16):
    g = [list(r.ljust(width, '.')) for r in rows] + [['.'] * width for _ in range(height - len(rows))]
    assert all(len(r) == width for r in g), [len(r) for r in g]
    out = [r[:] for r in g]
    for y in range(height):
        for x in range(width):
            if g[y][x] != '.':
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < width and 0 <= ny < height and g[ny][nx] not in '.' + BLOOD:
                    out[y][x] = 'o'
                    break
    return [''.join(r) for r in out]


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


def put(img, rows, x0, y0, scale):
    pal = animals.ANIMAL_PALETTE
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c != '.':
                col = tuple(pal[animals.ANIMAL_KEYS[c]][:3])
                for dy in range(scale):
                    for dx in range(scale):
                        img.putpixel((x0 + x * scale + dx, y0 + y * scale + dy), col)


def main():
    goat = {k: make(v, 26) for k, v in GOAT.items()}
    deer = {k: make(v, 28 if k.startswith('side') or k == 'carcass' else 16) for k, v in DEER.items()}
    rabbit = {k: make(v, 16) for k, v in RABBIT.items()}
    for name, d in (('goat', goat), ('deer', deer), ('rabbit', rabbit)):
        for k, rows in d.items():
            print(name, k)
            for r in rows:
                print("    '%s'," % r)
    S = 5
    img = Image.new('RGB', (1320, 330), (0x3f, 0x6b, 0x34))
    # Row 1: everything for these three, large.
    x = 6
    for rows in [*goat.values(), *deer.values(), *rabbit.values()]:
        put(img, rows, x, 6, 3)
        x += len(rows[0]) * 3 + 6
    # Row 2: the full lineup, standing, large; row 3: the same at 1x.
    lineup = [ref.WOLF, ref.HEN, ref.BOAR, goat['stand'], deer['side_stand'], rabbit['sit'], rabbit['hop']]
    x = 6
    for rows in lineup:
        put(img, rows, x, 70, S)
        put(img, rows, 40 + x // S * 2, 240, 2)
        put(img, rows, 40 + x // S, 300, 1)
        x += len(rows[0]) * S + 12
    img.save(sys.argv[3])


if __name__ == '__main__':
    main()
