"""Held-item sprites: 32x32 frames, the hand at the centre, 32 angles per item (0 = right, 8 = down). Each item has
its own picture, with the gripping hand drawn over the handle (palette index P_HAND, which the game recolours to
match gloves)."""
import math

from common import TRANSPARENT, blank, hexc, save_bmp, write_header

ANGLES = 32
LOOKS = ['KNIFE', 'HAMMER', 'BAT', 'PIPE', 'CROWBAR', 'AXE', 'FLASHLIGHT', 'PISTOL', 'LONG_GUN', 'SPEAR', 'BOTTLE',
         'MOLOTOV', 'CLOCK', 'MACHETE', 'HATCHET', 'GOLF_CLUB', 'SLEDGEHAMMER', 'PITCHFORK', 'CLEAVER', 'HOCKEY_STICK',
         'BATON', 'REVOLVER']

P_OUTLINE, P_BLADE_L, P_BLADE, P_HANDLE, P_WOOD, P_WOOD_L, P_METAL, P_HAND, P_GLASS, P_GLASS_L, P_RAG = range(1, 12)
WEAPON_PALETTE = [TRANSPARENT, hexc('151a22'), hexc('e8e4d8'), hexc('a8b0bc'), hexc('4a2e24'),
                  hexc('8a6a42'), hexc('b08a5a'), hexc('6f6d64'), hexc('d9a77c'), hexc('3f7a52'),
                  hexc('8cc49a'), hexc('d0453a')] + [(0, 0, 0)] * 4

# Segments along each item: (start, end, half-width, colour), distances in px from the hand. Sized against the
# 16 px player (about 9 px per metre): a bat reaches the hip, a knife is a short glint, readable but not oversized.
SHAPES = {
    'KNIFE': [(-1, 2, 0.6, P_HANDLE), (2, 2.8, 1.1, P_METAL), (2.8, 7, 0.6, P_BLADE), (5.5, 7, 0.35, P_BLADE_L)],
    'HAMMER': [(-1, 5.5, 0.5, P_WOOD), (5.4, 7.6, 2.0, P_METAL)],
    'BAT': [(-1, 3, 0.55, P_HANDLE), (3, 6, 0.75, P_WOOD), (6, 10, 1.05, P_WOOD), (6.5, 9.5, 0.35, P_WOOD_L)],
    'PIPE': [(-1, 9.5, 0.6, P_BLADE), (2, 9, 0.22, P_BLADE_L)],
    'CROWBAR': [(-1, 9, 0.5, P_METAL)],
    'AXE': [(-1, 10, 0.55, P_WOOD)],
    # Off-hand items (milestone 12.6): a stubby dark barrel, a wider head, a pale lens at its end.
    'FLASHLIGHT': [(-1, 3, 0.7, P_METAL), (3, 5, 1.2, P_METAL), (4.4, 5.2, 1.2, P_BLADE)],
    # Guns: a short dark slide; a long barrel over a wooden stock held at its middle.
    'PISTOL': [(0, 5, 0.9, P_METAL), (0.5, 4.5, 0.3, P_BLADE)],
    'LONG_GUN': [(-5, 0, 1.0, P_WOOD), (0, 3, 0.9, P_HANDLE), (3, 11, 0.55, P_METAL)],
    # Milestone 12.6 stage 5: a long shaft with the knife lashed to its tip; throwables held by the neck.
    'SPEAR': [(-5, 10, 0.5, P_WOOD_L), (9, 10.5, 0.8, P_HANDLE), (10.5, 13.5, 0.7, P_BLADE)],
    'BOTTLE': [(-0.5, 1.5, 0.55, P_GLASS), (1.5, 5.5, 1.3, P_GLASS), (2, 5, 0.35, P_GLASS_L)],
    'MOLOTOV': [(-3, -0.5, 0.7, P_RAG), (-0.5, 1.5, 0.55, P_GLASS), (1.5, 5.5, 1.3, P_GLASS),
                (2, 5, 0.35, P_GLASS_L)],
    'CLOCK': [(0.5, 4.5, 2.0, P_RAG), (1.5, 3.5, 1.0, P_BLADE)],
    # Milestone 14: a long dark-handled blade; a short axe; a thin shaft with a club head (off the line, below);
    # a long handle with a heavy block; a shaft with three tines; a square cleaver; a stick with its blade; a baton;
    # a revolver (a pistol with a cylinder).
    'MACHETE': [(-1, 2.5, 0.6, P_HANDLE), (2.5, 9, 0.85, P_BLADE), (6, 9, 0.35, P_BLADE_L)],
    'HATCHET': [(-1, 7, 0.5, P_WOOD)],
    'GOLF_CLUB': [(-1, 2, 0.55, P_HANDLE), (2, 10, 0.35, P_BLADE)],
    'SLEDGEHAMMER': [(-1, 9, 0.55, P_WOOD)],
    'PITCHFORK': [(-4, 9, 0.5, P_WOOD_L)],
    'CLEAVER': [(-1, 2, 0.6, P_HANDLE)],
    'HOCKEY_STICK': [(-1, 9.5, 0.55, P_WOOD)],
    'BATON': [(-1, 7.5, 0.65, P_METAL)],
    'REVOLVER': [(0, 5.5, 0.6, P_METAL), (0.8, 2.6, 1.2, P_METAL), (0.5, 5, 0.25, P_BLADE)],
}
# Parts off the centre line, as (along, across) rectangles: the axe head, the crowbar's hook and claw.
EXTRA = {
    'AXE': [((7.5, 10.5), (0.4, 3.4), P_METAL), ((7.5, 10.5), (2.6, 3.6), P_BLADE_L), ((8, 10), (-1.2, 0.4), P_METAL)],
    'CROWBAR': [((8.2, 9.6), (-2.2, 0.5), P_METAL), ((-1.6, -0.6), (-0.5, 1.4), P_METAL)],
    'HATCHET': [((5, 7.5), (0.4, 2.8), P_METAL), ((5, 7.5), (2.0, 3.0), P_BLADE_L)],
    'GOLF_CLUB': [((9.2, 11), (-0.4, 2.2), P_METAL)],
    'SLEDGEHAMMER': [((8, 11), (-2.4, 2.4), P_METAL)],
    'PITCHFORK': [((8.6, 9.4), (-2.2, 2.2), P_METAL), ((9, 12.5), (-2.2, -1.5), P_BLADE),
                  ((9, 12.5), (-0.35, 0.35), P_BLADE), ((9, 12.5), (1.5, 2.2), P_BLADE)],
    'CLEAVER': [((2, 6.5), (-0.6, 2.4), P_BLADE), ((2, 6.5), (1.8, 2.4), P_BLADE_L)],
    'HOCKEY_STICK': [((8, 10.5), (0.3, 2.6), P_WOOD)],
    'BATON': [((1.5, 2.5), (0.6, 2.6), P_METAL)],
    # Playtest 3: a long gun is held in both hands, the other one under the barrel's front.
    'LONG_GUN': [((4.8, 6.4), (-0.3, 1.3), P_HAND)],
}
OUTLINE = 0.6     # outline width around every part
# The gripping hand over the handle, in screen pixels from the centre (it does not turn with the item).
GRIP = [(0, 0, P_HAND), (1, 0, P_HAND), (0, 1, P_HAND), (1, 1, P_OUTLINE), (-1, 0, P_OUTLINE), (-1, 1, P_OUTLINE),
        (2, 0, P_OUTLINE)]


def paint(grid, angle, along, across, colour, grow=0.0):
    ux, uy = math.cos(angle), math.sin(angle)
    vx, vy = -uy, ux
    a0, a1 = along
    c0, c1 = across
    for y in range(32):
        for x in range(32):
            px, py = x + 0.5 - 16, y + 0.5 - 16
            a = px * ux + py * uy
            c = px * vx + py * vy
            if a0 - grow <= a <= a1 + grow and c0 - grow <= c <= c1 + grow:
                grid[y][x] = colour


def weapon_frame(name, index):
    angle = index * 2 * math.pi / ANGLES
    grid = blank(32, 32)
    parts = [((s, e), (-w, w), col) for s, e, w, col in SHAPES[name]] + EXTRA.get(name, [])
    for along, across, _ in parts:
        paint(grid, angle, along, across, P_OUTLINE, grow=OUTLINE)
    for along, across, colour in parts:
        paint(grid, angle, along, across, colour)
    for dx, dy, colour in GRIP:
        grid[16 + dy][16 + dx] = colour
    return grid


def build_weapons():
    rows = []
    for name in LOOKS:
        for index in range(ANGLES):
            rows.extend(weapon_frame(name, index))
    save_bmp('weapons', rows, WEAPON_PALETTE, {'type': 'sprite', 'height': 32})

    lines = ['namespace kx::weapon_frames', '{', '',
             'constexpr int angles = %d;              // 0 = right, angles / 4 = down' % ANGLES,
             'constexpr int hand_colour = %d;          // palette index of the gripping hand' % P_HAND]
    lines += ['constexpr int %s = %d;' % (name.lower(), i * ANGLES) for i, name in enumerate(LOOKS)]
    lines += ['', '}']
    write_header('kx_weapon_frames.h', lines)
