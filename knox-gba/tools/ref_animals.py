"""Hand-drawn wolf, hen and boar after the user's reference, previewed large and at 1x next to the reference shrunk."""
import sys

from PIL import Image

sys.path.insert(0, sys.argv[1] + '/tools/assets')
import animals  # noqa: E402

WOLF = [
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
HEN = [
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
BOAR = [
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


def check(rows):
    assert all(len(r) == len(rows[0]) for r in rows), [(i, len(r)) for i, r in enumerate(rows)]
    return rows


def main():
    pal = animals.ANIMAL_PALETTE
    ref = Image.open(sys.argv[2]).convert('RGBA')
    scale = 8
    img = Image.new('RGB', (900, 330), (0x3f, 0x6b, 0x34))
    small_ref = ref.resize((ref.width * 140 // ref.height, 140), Image.LANCZOS)
    img.paste(small_ref, (10, 180), small_ref)
    x0 = 10
    for rows in (check(WOLF), check(HEN), check(BOAR)):
        for y, r in enumerate(rows):
            for x, c in enumerate(r):
                if c != '.':
                    col = tuple(pal[animals.ANIMAL_KEYS[c]][:3])
                    for dy in range(scale):
                        for dx in range(scale):
                            img.putpixel((x0 + x * scale + dx, 10 + y * scale + dy), col)
                    img.putpixel((x0 + 40 + x, 150 + y), col)
        x0 += len(rows[0]) * scale + 20
    img.save(sys.argv[3])


if __name__ == '__main__':
    main()
