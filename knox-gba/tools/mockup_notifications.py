"""Playtest 4 mockup: notifications in one place. Drawn at 240x160 over screens from the game (a mGBA screenshot of
the street, bench 3's Gear tab and guide dumps), with the game's 5 px font and colours, saved at 3x.

    python tools/mockup_notifications.py <street screenshot> <gear dump> <guide dump>  -> mockups/22-notifications.png

Every notification shows in the same place, in the world and over every menu: centred, just above the hotbar
(the newest box's bottom at y 116), each box as wide as its text with a border in its kind's colour (normal light
grey, good news yellow, danger red). Up to 3 stack, the newest at the bottom, older ones above, each on its own timer.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from mockup_character import ACCENT, FILL, QUIET, RED, TEXT, Screen  # noqa: E402
from mockup_doors import caption  # noqa: E402

ROOT = os.path.join(HERE, '..', 'mockups')
STACK_BOTTOM = 116
BOX_HEIGHT = 11
BOX_GAP = 2
PADDING = 3
STYLES = {'normal': (QUIET, TEXT), 'accent': (ACCENT, ACCENT), 'danger': (RED, RED)}


def background(path):
    s = Screen()
    image = Image.open(path).convert('RGB').resize((240, 160), Image.NEAREST)
    s.image = image
    s.pixels = image.load()
    return s


def stack(s, toasts):
    """toasts: oldest first, (text, kind)."""
    for index, (text, kind) in enumerate(reversed(toasts)):
        border, colour = STYLES[kind]
        width = s.width(text) + (1 + PADDING) * 2
        y = STACK_BOTTOM - BOX_HEIGHT - index * (BOX_HEIGHT + BOX_GAP)
        x = (240 - width) // 2
        s.rect(x, y, width, BOX_HEIGHT, border)
        s.rect(x + 1, y + 1, width - 2, BOX_HEIGHT - 2, FILL)
        s.text(x + 1 + PADDING, y + 3, text, colour)


def main():
    street, gear, guide = sys.argv[1:4]
    panels = []

    s = background(street)
    stack(s, [('ATE CANNED BEANS', 'normal')])
    panels.append((s, [('1 IN THE WORLD', ACCENT), ('ABOVE THE HOTBAR, NOT AT THE TOP', QUIET)]))

    s = background(street)
    stack(s, [('LACERATION: LEFT ARM', 'danger'), ('BLADE LEVEL 2', 'accent'), ('TOOK VITAMINS', 'normal')])
    panels.append((s, [('2 THREE AT ONCE', ACCENT), ('THE NEWEST AT THE BOTTOM', QUIET)]))

    s = background(gear)
    stack(s, [('TOOK VITAMINS', 'normal')])
    panels.append((s, [('3 OVER THE GEAR TAB', ACCENT), ('THE SAME PLACE, NOT ON THE TAB BAR', QUIET)]))

    s = background(guide)
    stack(s, [('WORLD STORAGE NEARLY FULL', 'danger'), ('ATE AN APPLE', 'normal')])
    panels.append((s, [('4 OVER ANY MENU', ACCENT), ('TWO AT ONCE', QUIET)]))

    gap = 4
    sheet = Image.new('RGB', (240 * 2 + gap, (160 + 22) * 2 + gap), (0x05, 0x07, 0x0b))
    for index, (screen, lines) in enumerate(panels):
        x = (index % 2) * (240 + gap)
        y = (index // 2) * (160 + 22 + gap)
        sheet.paste(screen.image, (x, y))
        sheet.paste(caption(240, lines), (x, y + 160))
    sheet = sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST)
    sheet.save(os.path.join(ROOT, '22-notifications.png'))
    print('wrote mockups/22-notifications.png')


if __name__ == '__main__':
    main()
