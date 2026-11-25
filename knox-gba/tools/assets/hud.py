"""HUD palette (BG0) and the item and moodle icon pixels. The HUD has no static tileset: kx_canvas draws text
(small_font.py), panels, bars and icons pixel by pixel into its own BG0 tiles.

Each 8x8 cell has one 16-colour palette bank. Indices 1-7 are the same colour in every bank, so panels,
borders and text mix freely; indices 8-15 belong to the bank, and drawing one of those switches the cell to
that bank. The screens are laid out so that cells needing different banks do not touch (icons, the body
diagram and the Health tab bars each sit in their own cells)."""
import os

from common import GRAPHICS, TRANSPARENT, hexc, save_bmp, write_header
from items import (I_BLUE, I_BORDER, I_BROWN, I_DARK, I_FILL, I_GREEN, I_GREY, I_OLIVE, I_ORANGE, I_OUTLINE,
                   I_RED, I_SKY, I_TRANS, I_WHITE, I_WOOD, I_YELLOW, build_icons, write_icon_header)
from survival import moodle_icons

SHARED = [('fill', '0b0e14'), ('border', '2a2f3a'), ('text', 'e8e4d8'), ('quiet', 'a8b0bc'), ('empty', '262b35'),
          ('accent', 'e8c547'), ('panel', '151a22')]

BANKS = [
    ('ui', [('screen', '10141b'), ('green', '7fd08a'), ('orange', 'e0903a'), ('red', 'e05a3a'),
            ('dark_red', 'b02a20'), ('blood', 'd0453a'), ('water', '4a90d9'), ('cold', '7fb0e0')]),
    # Health tab body diagram (mockup 04).
    ('body', [('screen', '10141b'), ('limb', '6b7a8a'), ('seam', '5d6b7a'), ('wound', 'd0453a'),
              ('pink', 'f08a80'), ('green', '7fd08a'), ('red', 'e05a3a'), ('orange', 'e0903a')]),
    # Item icons (items.py colours).
    ('item', [('orange', 'd98a3a'), ('dark', '6f6d64'), ('red', 'd0453a'), ('blue', '4a90d9'), ('sky', '7fb0e0'),
              ('wood', '8a6a42'), ('brown', '4a2e24'), ('olive', '4d5a3a')]),
    # Moodle icons, with the four level colours for the slot border.
    ('moodle', [('orange', 'e0903a'), ('dark', '6f6d64'), ('red', 'e05a3a'), ('blue', '4a90d9'),
                ('sky', '7fb0e0'), ('brown', '4a2e24'), ('green', '7fd08a'), ('dark_red', 'b02a20')]),
    # Health tab vitals: bar colours and the light value / chip text colours.
    ('stats', [('screen', '10141b'), ('blood', 'd0453a'), ('food', 'd98a3a'), ('water', '4a90d9'),
               ('cold', '7fb0e0'), ('warm_text', 'f0b066'), ('cold_text', 'a8d0f0'), ('pink', 'f08a80')]),
]

# items.py palette index -> canvas index, per icon bank.
ITEM_MAP = {I_TRANS: 0, I_OUTLINE: 1, I_FILL: 7, I_BORDER: 5, I_WHITE: 3, I_GREY: 4, I_YELLOW: 6, I_ORANGE: 8,
            I_DARK: 9, I_RED: 10, I_BLUE: 11, I_SKY: 12, I_WOOD: 13, I_BROWN: 14, I_OLIVE: 15, I_GREEN: 15}
MOODLE_MAP = {I_TRANS: 0, I_OUTLINE: 1, I_FILL: 1, I_BORDER: 5, I_WHITE: 3, I_GREY: 4, I_YELLOW: 6, I_ORANGE: 8,
              I_DARK: 9, I_RED: 10, I_BLUE: 11, I_SKY: 12, I_WOOD: 13, I_BROWN: 13, I_GREEN: 14, I_OLIVE: 9}


def bank_index(name):
    return [bank for bank, _ in BANKS].index(name)


def palette():
    colours = []
    for _, own in BANKS:
        bank = [TRANSPARENT] + [hexc(c) for _, c in SHARED] + [hexc(c) for _, c in own]
        assert len(bank) == 16
        colours += bank
    return colours


def icon_rows(grid, mapping):
    """16x16 grid -> 16 rows of two 4bpp words (low nibble = left pixel)."""
    rows = []
    for row in grid:
        words = []
        for half in range(2):
            value = 0
            for x in range(8):
                value |= mapping[row[(half * 8) + x]] << (x * 4)
            words.append(value)
        rows.append(words)
    return rows


def build_hud():
    # The old static HUD tileset is gone.
    for ext in ('.bmp', '.json'):
        old = os.path.join(GRAPHICS, 'hud_tiles' + ext)
        if os.path.exists(old):
            os.remove(old)

    save_bmp('hud_palette', [[0] * 8 for _ in range(8)], palette(),
             {'type': 'bg_palette', 'bpp_mode': 'bpp_4', 'colors_count': 16 * len(BANKS)})

    icons = [(name, grid, 'item', ITEM_MAP) for name, grid in build_icons()]
    icons += [(name, grid, 'moodle', MOODLE_MAP) for name, grid in moodle_icons()]
    write_icon_header([name for name, _, _, _ in icons])

    lines = ['#include <cstdint>', '', 'namespace kx', '{', '',
             '// Item and moodle icons, 16x16 4bpp in canvas palette indices (kx_hud_palette.h), two words per row.',
             'constexpr uint8_t icon_banks[] = { %s };' % ', '.join(str(bank_index(b)) for _, _, b, _ in icons), '',
             'constexpr uint32_t icon_pixels[][16][2] = {']
    for name, grid, _, mapping in icons:
        rows = icon_rows(grid, mapping)
        lines.append('    {   // ' + name)
        lines += ['        { 0x%08X, 0x%08X },' % (a, b) for a, b in rows]
        lines.append('    },')
    lines += ['};', '', '}']
    write_header('kx_icon_pixels.h', lines)

    lines = ['#include <cstdint>', '', 'namespace kx::colour', '{', '',
             '// Canvas colours: palette bank in the high nibble, index in the low one. Indices 1-7 are shared by',
             '// every bank (drawing them keeps a cell\'s bank); 8-15 switch the cell to their bank.', '']
    for index, (name, _) in enumerate(SHARED):
        lines.append('constexpr uint8_t %s = %d;' % (name, index + 1))
    for bank, (bank_name, own) in enumerate(BANKS):
        lines.append('')
        lines.append('constexpr int %s_bank = %d;' % (bank_name, bank))
        for index, (name, _) in enumerate(own):
            lines.append('constexpr uint8_t %s_%s = 0x%X%X;' % (bank_name, name, bank, index + 8))
    lines += ['', 'constexpr int banks = %d;' % len(BANKS), '', '}']
    write_header('kx_hud_palette.h', lines)

    # Text styles of the hud's cell API (toasts, the gear and search screens).
    lines = ['#include <cstdint>', '', 'namespace kx::hud_tiles', '{', '',
             '// Text styles for hud::text, toasts and boxes.',
             'constexpr int bank_normal = 0;', 'constexpr int bank_accent = 1;', 'constexpr int bank_quiet = 2;',
             'constexpr int bank_danger = 3;', '', '}']
    write_header('kx_hud_tiles.h', lines)
