"""Milestone 12 mockup: the car screen (5-part checklist and actions), drawn at 240x160 with the game's 5 px font
and palette, saved at 3x.

    python tools/mockup_car.py        -> mockups/10-car.png
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mockup_character import (ACCENT, BORDER, DIM, FILL, GOOD, ORANGE, QUIET, RED, TEXT,  # noqa: E402
                              Screen)


def tick(s, x, y, colour):
    for px, py in ((0, 2), (1, 3), (2, 2), (3, 1), (4, 0)):
        s.rect(x + px, y + py, 1, 2, colour)


def cross(s, x, y, colour):
    for i in range(5):
        s.rect(x + i, y + i, 1, 1, colour)
        s.rect(x + 4 - i, y + i, 1, 1, colour)


def car_screen(path):
    s = Screen()

    # Header, as on the container overlay: what it is, and whether it runs.
    s.panel(12, 4, 216, 14)
    s.text(17, 9, 'CAR', QUIET)
    s.text(35, 9, '`', QUIET)
    s.text(41, 9, 'BLUE SEDAN', ACCENT)
    s.text_right(223, 9, 'WILL NOT START', RED)

    # Left: the 5-part checklist, then what you can do with the car.
    s.panel(12, 22, 120, 120)
    s.text(17, 26, 'PARTS', QUIET)
    s.text_right(127, 26, '3/5', ORANGE)
    s.rect(16, 33, 112, 1, BORDER)
    parts = [('BATTERY', False, 'MISSING'), ('SPARK PLUG', True, ''), ('RADIATOR', True, 'FULL'),
             ('TYRES', True, '3/4'), ('FUEL', False, '0 L')]
    y = 37
    selected = 'BATTERY'
    for name, ok, note in parts:
        chosen = name == selected
        if chosen:
            s.rect(14, y - 1, 116, 7, ACCENT)
        mark = FILL if chosen else (GOOD if ok else RED)
        (tick if ok else cross)(s, 17, y, mark)
        s.text(27, y, name, FILL if chosen else TEXT)
        if note:
            s.text_right(126, y, note, FILL if chosen else (QUIET if ok else RED))
        y += 7

    s.rect(16, y + 2, 112, 1, BORDER)
    y += 7
    s.text(17, y, 'ACTIONS', QUIET)
    y += 8
    actions = [('TRUNK', '12/60 KG', True), ('GLOVE BOX', '', True), ('SIPHON FUEL', 'EMPTY', False),
               ('HOTWIRE', 'NO KEY', True), ('DRIVE', '', False)]
    for name, note, ready in actions:
        s.text(17, y, name, TEXT if ready else DIM)
        if note:
            s.text_right(126, y, note, QUIET if ready else DIM)
        y += 7

    # Right: the selected line.
    s.panel(136, 22, 92, 120)
    s.text(141, 27, 'BATTERY', ACCENT)
    s.text(141, 37, 'MISSING', RED)
    s.rect(140, 46, 84, 1, BORDER)
    s.text(141, 50, 'NEEDS', QUIET)
    needs = [('CAR BATTERY', '1/1', GOOD), ('WRENCH', 'OK', GOOD)]
    y = 58
    for name, have, colour in needs:
        s.text(141, y, name, TEXT)
        s.text_right(223, y, have, colour)
        y += 7
    s.rect(140, y + 1, 84, 1, BORDER)
    s.text(141, y + 5, 'TAKES 20 MIN', QUIET)
    s.text(141, y + 12, 'MECHANICS XP +', QUIET)
    s.text(141, y + 26, 'FOUND IN WRECKS,', DIM)
    s.text(141, y + 33, 'GARAGES AND', DIM)
    s.text(141, y + 40, 'PETROL STATIONS', DIM)

    s.hints(147, [('A', 'FIT'), ('R', 'TAKE OUT'), ('B', 'CLOSE')])
    s.save(path)


if __name__ == '__main__':
    root = os.path.join(os.path.dirname(__file__), '..', 'mockups')
    car_screen(os.path.join(root, '10-car.png'))
    print('wrote mockups/10-car.png')
