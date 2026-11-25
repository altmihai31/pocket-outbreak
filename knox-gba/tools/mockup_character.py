"""Milestone 8 mockups: the Skills tab and the character creation screen, drawn at 240x160 with the game's own
5 px font and the docs/palette.md colours, saved at 3x like the other mockups.

    python tools/mockup_character.py        -> mockups/07-skills.png, mockups/08-character-creation.png
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'assets'))
from small_font import GLYPHS  # noqa: E402

SCREEN = (0x10, 0x14, 0x1b)
FILL = (0x0b, 0x0e, 0x14)
BORDER = (0x2a, 0x2f, 0x3a)
EMPTY = (0x26, 0x2b, 0x35)
SLOT = (0x15, 0x1a, 0x22)
TEXT = (0xe8, 0xe4, 0xd8)
QUIET = (0xa8, 0xb0, 0xbc)
DIM = (0x6b, 0x72, 0x7e)
ACCENT = (0xe8, 0xc5, 0x47)
ORANGE = (0xe0, 0x90, 0x3a)
GOOD = (0x7f, 0xd0, 0x8a)
RED = (0xe0, 0x5a, 0x3a)


class Screen:
    def __init__(self):
        self.image = Image.new('RGB', (240, 160), SCREEN)
        self.pixels = self.image.load()

    def rect(self, x, y, w, h, colour):
        for py in range(y, y + h):
            for px in range(x, x + w):
                if 0 <= px < 240 and 0 <= py < 160:
                    self.pixels[px, py] = colour

    def panel(self, x, y, w, h, border=BORDER):
        self.rect(x, y, w, h, border)
        self.rect(x + 1, y + 1, w - 2, h - 2, FILL)

    @staticmethod
    def width(text):
        return sum(len(GLYPHS[c][0]) + 1 for c in text) - 1 if text else 0

    def text(self, x, y, text, colour):
        for c in text:
            rows = GLYPHS[c]
            for row, line in enumerate(rows):
                for column, bit in enumerate(line):
                    if bit == '#':
                        self.pixels[x + column, y + row] = colour
            x += len(rows[0]) + 1
        return x

    def text_right(self, right, y, text, colour):
        self.text(right - self.width(text), y, text, colour)

    def text_centered(self, centre, y, text, colour):
        self.text(centre - self.width(text) // 2, y, text, colour)

    def hints(self, y, items, right_items=()):
        x = 12
        for button, action in items:
            x = self.text(x, y, button, TEXT) + 4
            x = self.text(x, y, action, QUIET) + 10
        right = 228
        for button, action in reversed(right_items):
            width = self.width(button) + 4 + self.width(action)
            self.text(right - width, y, button, TEXT)
            self.text(right - self.width(action), y, action, QUIET)
            right -= width + 10

    def tab_bar(self, selected):
        tabs = ['GEAR', 'HEALTH', 'CRAFT', 'SKILLS', 'MAP']
        self.panel(12, 4, 216, 14)
        self.text(17, 9, 'L1', QUIET)
        self.text_right(222, 9, 'R1', QUIET)
        total = 6 + 6 * 4 + sum(self.width(t) for t in tabs)
        x = (240 - total) // 2
        for index, name in enumerate(tabs):
            if index == selected:
                self.rect(x, 8, self.width(name) + 6, 7, ACCENT)
                self.text(x + 3, 9, name, FILL)
                x += self.width(name) + 6 + 6
            else:
                self.text(x, 9, name, QUIET)
                x += self.width(name) + 6

    def save(self, path):
        self.image.resize((720, 480), Image.NEAREST).save(path)


def skills_tab(path):
    s = Screen()
    s.tab_bar(3)

    # Left: the two body stats, then the 14 skills, each with 5 level pips (the next one fills with XP).
    s.panel(12, 22, 120, 120)
    rows = [('FITNESS', 3, 40, True), ('STRENGTH', 4, 10, True), None,
            ('BLUNT', 1, 60, True), ('BLADE', 0, 30, True), ('AXE', 2, 60, True), ('SPEAR', 0, 0, False),
            ('AIMING', 0, 0, False), ('RELOADING', 0, 0, False), ('FIRST AID', 1, 20, True),
            ('SNEAKING', 0, 50, True), ('LIGHTFOOT', 0, 70, True), ('FORAGING', 0, 0, False),
            ('COOKING', 0, 0, False), ('FARMING', 0, 0, False), ('CARPENTRY', 0, 0, False),
            ('MECHANICS', 0, 0, False)]
    y = 25
    selected = 'AXE'
    for row in rows:
        if row is None:
            s.rect(16, y, 112, 1, BORDER)
            y += 2
            continue
        name, level, progress, trains = row
        if name == selected:
            s.rect(14, y - 1, 116, 7, ACCENT)
            name_colour, pip_on, pip_off = FILL, FILL, (0xb8, 0x9a, 0x30)
        else:
            name_colour, pip_on, pip_off = (TEXT if trains else DIM), ACCENT, EMPTY
        s.text(17, y, name, name_colour)
        for pip in range(5):
            px = 102 + pip * 5
            s.rect(px, y, 4, 5, pip_on if pip < level else pip_off)
            if pip == level and progress:
                height = max(1, (5 * progress) // 100)
                s.rect(px, y + 5 - height, 4, height, pip_on)
        y += 7

    # Right: the selected skill.
    s.panel(136, 22, 92, 120)
    s.text(141, 27, 'AXE', ACCENT)
    s.text_right(223, 27, 'LEVEL 2', TEXT)
    s.rect(141, 36, 82, 5, BORDER)
    s.rect(142, 37, 80, 3, SLOT)
    s.rect(142, 37, 48, 3, ACCENT)
    s.text(141, 44, '150 / 250 XP', QUIET)
    s.text(141, 54, 'FIRE OFFICER +1', GOOD)
    s.text(141, 64, 'BOOK VOL 2', TEXT)
    s.text(141, 71, 'READ ` X3 XP', GOOD)
    s.text(141, 81, 'AXE HITS: +10%', QUIET)
    s.text(141, 88, 'DAMAGE AND 5%', QUIET)
    s.text(141, 95, 'FASTER SWINGS', QUIET)
    s.text(141, 102, 'PER LEVEL.', QUIET)
    s.text(141, 114, 'TRAINS BY', DIM)
    s.text(141, 121, 'HITTING WITH', DIM)
    s.text(141, 128, 'AXES.', DIM)

    s.hints(147, [('D-PAD', 'CHOOSE'), ('B', 'CLOSE')], [('SELECT', 'SAVE+QUIT')])
    s.save(path)


def character_creation(path):
    s = Screen()

    # Header: what this is, and the points left to spend.
    s.panel(12, 4, 216, 14)
    s.text(17, 9, 'NEW SURVIVOR', ACCENT)
    x = s.text(170, 9, 'POINTS', QUIET) + 4
    s.text_right(222, 9, '1', GOOD)

    # Occupation: one row, Left/Right to change.
    s.panel(12, 22, 216, 24)
    s.text_centered(120, 27, '<  FIRE OFFICER  >', TEXT)
    s.text_centered(120, 36, 'AXE 1 ` FITNESS 3 ` STRENGTH 3', QUIET)

    # Traits: good ones cost points, bad ones give them back.
    s.panel(12, 50, 216, 70)
    s.text(17, 54, 'GOOD TRAITS', QUIET)
    s.text(122, 54, 'BAD TRAITS', QUIET)
    good = [('STRONG', 4, True), ('ATHLETIC', 4, False), ('FAST LEARNER', 6, False), ('LIGHT EATER', 2, False),
            ('FAST HEALER', 3, False), ('GRACEFUL', 2, False), ('UNNOTICED', 3, False), ('BRAVE', 3, False)]
    bad = [('WEAK', 4, False), ('OUT OF SHAPE', 4, False), ('SLOW LEARNER', 4, False), ('BIG APPETITE', 2, False),
           ('SLOW HEALER', 3, True), ('CLUMSY', 2, True), ('NOTICEABLE', 3, False), ('COWARDLY', 3, False)]
    cursor = ('good', 0)

    def column(items, left, right, kind):
        y = 63
        for index, (name, cost, chosen) in enumerate(items):
            focused = cursor == (kind, index)
            if focused:
                s.rect(left - 2, y - 1, right - left + 4, 7, ACCENT)
            if chosen:
                s.rect(left, y, 5, 5, FILL if focused else ACCENT)
            else:
                s.rect(left, y, 5, 5, (0xb8, 0x9a, 0x30) if focused else EMPTY)
            colour = FILL if focused else (TEXT if chosen else QUIET)
            s.text(left + 8, y, name, colour)
            label = ('-%d' if kind == 'good' else '+%d') % cost
            s.text_right(right, y, label, FILL if focused else (ORANGE if kind == 'good' else GOOD))
            y += 7

    column(good, 17, 116, 'good')
    s.rect(119, 54, 1, 62, BORDER)
    column(bad, 123, 223, 'bad')

    # What the focused choice does.
    s.panel(12, 124, 216, 19, ACCENT)
    s.text(17, 128, 'STRONG', ACCENT)
    s.text_right(223, 128, 'COSTS 4', ORANGE)
    s.text(17, 135, 'STRENGTH +2: CARRY MORE, HIT HARDER', QUIET)

    s.hints(147, [('A', 'PICK'), ('D-PAD', 'MOVE'), ('START', 'BEGIN')], [('B', 'BACK')])
    s.save(path)


if __name__ == '__main__':
    root = os.path.join(os.path.dirname(__file__), '..', 'mockups')
    skills_tab(os.path.join(root, '07-skills.png'))
    character_creation(os.path.join(root, '08-character-creation.png'))
    print('wrote mockups/07-skills.png, mockups/08-character-creation.png')
