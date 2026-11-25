"""Renders the HUD layer dumps of a DUMP=1 bench build to PNGs, to check the UI against the mockups without a
screen. Transparent pixels show as a dark checkerboard.

    mgba-sdl -C logToFile=1 -C logFile=kx.log -l 255 knox_survival_dump.gba
    python tools/render_hud_dump.py kx.log out_folder [scale]
"""
import os
import re
import sys

from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'assets'))

from hud import palette  # noqa: E402

COLUMNS, ROWS = 30, 20


def render(banks, tiles, colours):
    image = Image.new('RGB', (COLUMNS * 8, ROWS * 8))
    pixels = image.load()
    for index, data in tiles.items():
        column, row = index % COLUMNS, index // COLUMNS
        bank = banks[row][column]
        for line in range(8):
            word = int(data[line * 8:(line + 1) * 8], 16)
            for x in range(8):
                value = (word >> (x * 4)) & 15
                px, py = column * 8 + x, row * 8 + line
                if value == 0:
                    pixels[px, py] = (24, 28, 36) if ((px // 4) + (py // 4)) % 2 else (16, 18, 24)
                else:
                    pixels[px, py] = colours[bank * 16 + value]
    return image


def main():
    log, out = sys.argv[1], sys.argv[2]
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    os.makedirs(out, exist_ok=True)
    colours = palette()
    banks, tiles, frame = None, None, None
    count = 0
    for line in open(log, encoding='utf-8', errors='replace'):
        match = re.search(r'KXDUMP (\w+) (\d+)(?: (\S+))?', line)
        if not match:
            continue
        kind, number, data = match.group(1), int(match.group(2)), match.group(3)
        if kind == 'begin':
            banks, tiles, frame = [[0] * COLUMNS for _ in range(ROWS)], {}, number
            colours = palette()
        elif kind == 'p':
            # The palette as the game had it (a screen may swap a bank's colours): 15-bit BGR values.
            for index in range(16):
                value = int(data[index * 4:(index + 1) * 4], 16)
                colours[number * 16 + index] = ((value & 31) << 3, ((value >> 5) & 31) << 3, ((value >> 10) & 31) << 3)
        elif kind == 'b' and banks is not None:
            banks[number] = [int(c, 16) for c in data]
        elif kind == 't' and tiles is not None:
            tiles[number] = data
        elif kind == 'end' and tiles is not None:
            image = render(banks, tiles, colours)
            image = image.resize((image.width * scale, image.height * scale), Image.NEAREST)
            image.save(os.path.join(out, 'hud_%05d.png' % frame))
            count += 1
            banks = tiles = None
    print('rendered', count)


if __name__ == '__main__':
    main()
