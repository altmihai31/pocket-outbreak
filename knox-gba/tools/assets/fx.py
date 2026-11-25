"""FX sprite: 8x8 particles, three frames per kind (order matches kx::fx_kind)."""
from common import TRANSPARENT, hexc, save_bmp

FX_PALETTE = [TRANSPARENT, hexc('a8b0bc'), hexc('8f8d82'), hexc('7fb0e0'), hexc('e8e4d8'),
              hexc('4a90d9'), hexc('d0453a'), hexc('7a2a22'), hexc('e8c547'), hexc('3f7a52'),
              hexc('8cc49a'), hexc('151a22')] + [(0, 0, 0)] * 4

FX_FRAMES = [
    # dust
    ['........', '........', '........', '...12...', '..1221..', '...11...', '........', '........'],
    ['........', '........', '..1..1..', '.12..21.', '..2112..', '.1....1.', '........', '........'],
    ['........', '.1....1.', '........', '2......2', '........', '........', '.2....2.', '........'],
    # splash
    ['........', '........', '...4....', '..343...', '.35553..', '..333...', '........', '........'],
    ['........', '..4..4..', '.3....3.', '4......4', '.3....3.', '..5..5..', '........', '........'],
    ['.4....4.', '........', '3......3', '........', '........', '3......3', '........', '.5....5.'],
    # impact spark
    ['........', '...4....', '...4....', '.448844.', '...4....', '...4....', '........', '........'],
    ['.4.....4', '..4...4.', '........', '4..88..4', '...88...', '........', '..4...4.', '.4.....4'],
    ['4......4', '........', '........', '........', '........', '........', '........', '4......4'],
    # blood spray
    ['........', '........', '...6....', '..676...', '...66...', '........', '........', '........'],
    ['........', '..6..6..', '.6....7.', '...7....', '.7...6..', '...6....', '........', '........'],
    ['.6....6.', '........', '7......6', '........', '........', '6.....7.', '........', '..7..7..'],
    # horde edge arrows (not an fx kind): right, up-right, up; flipped for the other directions
    ['.77.....', '.7677...', '.766677.', '.7666667', '.7666667', '.766677.', '.7677...', '.77.....'],
    ['77777777', '.7666667', '..766667', '...76667', '....7667', '.....767', '......77', '.......7'],
    ['...77...', '..7667..', '..7667..', '.766667.', '.766667.', '76666667', '77777777', '........'],
    # campfire flames (not an fx kind): two frames
    ['...6....', '..686...', '..6886..', '.68886..', '.68486.6', '668848668'[0:8], '.666666.', '........'],
    ['....6...', '.6.686..', '..6886..', '.688886.', '.684486.', '66884866', '.666666.', '........'],
    # build cursor corner (top-left; flipped for the others): yellow, red
    ['8888....', '8888....', '88......', '88......', '........', '........', '........', '........'],
    ['6666....', '6666....', '66......', '66......', '........', '........', '........', '........'],
    # rain streak (weather)
    ['......4.', '......3.', '.....3..', '.....3..', '....3...', '....3...', '........', '........'],
    # map mark arrows (not an fx kind, kx_world_map): right, up-right, up, in the mark's yellow
    ['.22.....', '.2822...', '.288822.', '.2888882', '.2888882', '.288822.', '.2822...', '.22.....'],
    ['22222222', '.2888882', '..288882', '...28882', '....2882', '.....282', '......22', '.......2'],
    ['...22...', '..2882..', '..2882..', '.288882.', '.288882.', '28888882', '22222222', '........'],
    # thrown items in flight (kx_throwables): a bottle, a Molotov with its rag alight, an alarm clock, and the
    # shadow under them
    ['...99...', '...99...', '..9AA9..', '..9A99..', '..9A99..', '..9999..', '........', '........'],
    ['...86...', '...99...', '..9AA9..', '..9A99..', '..9A99..', '..9999..', '........', '........'],
    ['.2....2.', '..6666..', '.644446.', '.644746.', '.644446.', '..6666..', '.2....2.', '........'],
    ['........', '........', '........', '..BBBB..', '.BBBBBB.', '..BBBB..', '........', '........'],
]


def build_event_sprites():
    """World events: a helicopter's shadow passing over (32x16) and a crash site's smoke column (16x32, 2 frames)."""
    import math
    shadow = [[0] * 32 for _ in range(16)]
    for y in range(16):
        for x in range(32):
            body = ((x - 13) / 9.0) ** 2 + ((y - 8) / 4.0) ** 2 <= 1.0
            tail = 20 <= x <= 30 and 7 <= y <= 9
            rotor = abs((x - 13) - (y - 8) * 2.2) < 1.2 or abs((x - 13) + (y - 8) * 2.2) < 1.2
            if body or tail or (rotor and ((x - 13) ** 2 + (y - 8) ** 2) < 120):
                shadow[y][x] = 1
    save_bmp('heli', shadow, [TRANSPARENT, hexc('0b0e14')] + [(0, 0, 0)] * 14, {'type': 'sprite', 'height': 16})

    rows = []
    for frame in range(2):
        for y in range(32):
            row = []
            for x in range(16):
                width = 2.0 + (32 - y) * 0.16
                wobble = math.sin((y + frame * 5) * 0.45) * 1.6
                d = abs(x - 7.5 - wobble)
                if d <= width:
                    row.append(2 if d < width * 0.45 and (y + frame) % 3 else 1)
                else:
                    row.append(0)
            rows.append(row)
    save_bmp('smoke', rows, [TRANSPARENT, hexc('6f6d64'), hexc('8f8d82')] + [(0, 0, 0)] * 13,
             {'type': 'sprite', 'height': 32})


def build_light():
    """Night light windows (64x64 window sprites, never drawn: their opaque pixels mark where the light falls).
    Soft edges are a checkerboard. Frame 0: a round glow 60 px across (campfires). Frame 1: a beam, its apex at the
    left edge's middle, 50 degrees wide, cut by the circle inscribed in the frame so that, drawn at double size and
    turned by the hardware, it never leaves its 128x128 area (flashlight and headlights: 128 px long). Frame 2: a
    small glow 30 px across (around a player holding a lit flashlight)."""
    import math
    size = 64
    centre = (size - 1) / 2.0

    def frame(inside):
        rows = []
        for y in range(size):
            row = []
            for x in range(size):
                value = inside(x, y)
                row.append(1 if value == 2 or (value == 1 and (x + y) % 2 == 0) else 0)
            rows.append(row)
        return rows

    def disc(radius, soft):
        def inside(x, y):
            d = math.hypot(x - centre, y - centre)
            return 2 if d <= radius - soft else 1 if d <= radius else 0
        return inside

    def beam(x, y):
        dx = x + 0.5
        dy = y - centre
        angle = abs(math.degrees(math.atan2(dy, dx)))
        d = math.hypot(x - centre, y - centre)
        if angle > 25 or d > 31.5:
            return 0
        return 2 if angle <= 21 and d <= 28.5 else 1

    rows = frame(disc(30, 4)) + frame(beam) + frame(disc(15, 3))
    save_bmp('light', rows, [TRANSPARENT, hexc('e8e4d8')] + [(0, 0, 0)] * 14, {'type': 'sprite', 'height': 64})


def build_aim():
    """Aiming sprites (32x8, turned and stretched by the hardware). Frame 0: a shot's streak, bright in the
    middle. Frame 1: a dotted line, an edge of the aim cone."""
    rows = []
    for y in range(8):
        rows.append([1 if y in (3, 4) and 2 <= x < 30 else 0 for x in range(32)])
    for y in range(8):
        rows[y] = [2 if rows[y][x] and y == 3 else rows[y][x] for x in range(32)]
    for y in range(8):
        rows.append([2 if y == 4 and x % 4 == 1 else 0 for x in range(32)])
    save_bmp('aim', rows, [TRANSPARENT, hexc('e8c547'), hexc('e8e4d8')] + [(0, 0, 0)] * 13,
             {'type': 'sprite', 'height': 8})


def build_fire():
    """A fire patch's flames (kx_throwables): 16x16, three frames that flicker in turn."""
    import math
    import random
    rng = random.Random(12)
    rows = []
    for frame in range(3):
        heights = []
        for x in range(16):
            wave = 0.5 + 0.5 * math.cos((x - 7.5) * 0.75 + frame * 2.1)
            edge = max(0.0, 1.0 - abs(x - 7.5) / 8.5)
            heights.append(max(0.0, min(15.0, 3 + 11 * wave * edge + rng.uniform(-1.0, 1.0))))
        for y in range(16):
            row = []
            for x in range(16):
                top = 15.5 - heights[x]
                if y < top or abs(x - 7.5) > 7.4:
                    row.append(0)
                    continue
                depth = y - top
                core = (1.0 - abs(x - 7.5) / 8.0) * (0.4 + y / 26.0)
                if y >= 14:
                    colour = 1 if (x + frame) % 3 else 2
                elif depth < 1.2:
                    colour = 2
                elif core < 0.45:
                    colour = 3
                elif core < 0.62:
                    colour = 4
                else:
                    colour = 5
                row.append(colour)
            rows.append(row)
    save_bmp('fire', rows, [TRANSPARENT, hexc('7a2a22'), hexc('d0453a'), hexc('e07a2a'), hexc('e8c547'),
                            hexc('f4f0d0')] + [(0, 0, 0)] * 10, {'type': 'sprite', 'height': 16})


def build_fx():
    rows = []
    for f in FX_FRAMES:
        for r in f:
            assert len(r) == 8
            rows.append([int(c, 16) if c != '.' else 0 for c in r])
    save_bmp('fx', rows, FX_PALETTE, {'type': 'sprite', 'height': 8})
    build_event_sprites()
    build_light()
    build_aim()
    build_fire()
