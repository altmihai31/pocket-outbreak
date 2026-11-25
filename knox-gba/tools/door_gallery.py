"""Photographs the door gallery bench (make BENCH=18, playtest 7): every kind of door on the fixed seed in every
state, from both sides. Runs the ROM in mGBA's libretro core (apt: libretro-mgba) and saves a shot whenever the bench
writes a new shot number to SRAM, named by the bench, then a contact sheet.

    python tools/door_gallery.py knox_gallery.gba out_folder
"""
import ctypes as C
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from retro_drive import Emu  # noqa: E402


def main():
    rom, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    emu = Emu(rom)
    emu.lib.retro_get_memory_data.restype = C.c_void_p
    emu.lib.retro_get_memory_size.restype = C.c_size_t
    last = 0
    shots = []

    for _ in range(20000):
        emu.run(1)
        address = emu.lib.retro_get_memory_data(0)

        if not address:
            continue

        sram = C.string_at(address, 64)
        number = sram[0]

        if number == 255 and shots:
            break

        if number == 255:
            continue

        if number and number != last:
            last = number
            name = sram[16:].split(b'\0')[0].decode()
            path = os.path.join(out, '%02d.png' % number)
            emu.shot(path)
            shots.append((path, name))
            print(number, name)

    # Contact sheet: two shots a row (outside, inside), labelled, at 2x.
    scale = 2
    width, height, label = 240 * scale, 160 * scale, 14
    rows = (len(shots) + 1) // 2
    sheet = Image.new('RGB', (width * 2 + 8, rows * (height + label + 6)), (24, 24, 28))
    draw = ImageDraw.Draw(sheet)

    for index, (path, name) in enumerate(shots):
        x = (index % 2) * (width + 8)
        y = (index // 2) * (height + label + 6)
        draw.text((x + 2, y), name, fill=(230, 230, 230))
        sheet.paste(Image.open(path).convert('RGB').resize((width, height), Image.NEAREST), (x, y + label))

    sheet.save(os.path.join(out, 'sheet.png'))
    print('%d shots' % len(shots))


if __name__ == '__main__':
    main()
