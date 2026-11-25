"""Playtest 2 mockup: the settings menu. Drawn at 240x160 with the game's 5 px font and colours, saved at 3x.

    python tools/mockup_settings.py        -> mockups/18-settings.png

1. The Start menu's SETTINGS tab (after MAP), over the dimmed world like the other tabs: the four settings on the
   left, the chosen one's levels and what it does on the right. Up/Down choose, Left/Right change, at once.
2. The title with a save: CONTINUE, NEW WORLD, SETTINGS.
3. The same settings from the title.
4. The title without a save (the new world screen): SELECT opens the settings.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'assets'))

from mockup_character import ACCENT, BORDER, DIM, EMPTY, FILL, QUIET, TEXT, Screen  # noqa: E402
from mockup_doors import caption  # noqa: E402

ROOT = os.path.join(HERE, '..', 'mockups')
TABS = ['GEAR', 'HEALTH', 'CRAFT', 'SKILLS', 'MAP', 'SETTINGS']
LEVELS = ['OFF', 'LOW', 'MEDIUM', 'FULL']
SETTINGS = [   # name, kind, value, help lines (12 characters at most: the right panel)
    ('MUSIC', 'level', 2, ['THE TITLE,', 'CALM, NIGHT', 'AND CHASE', 'TUNES.']),
    ('SOUND EFFECTS', 'level', 3, ['HITS, STEPS,', 'ZOMBIES,', 'DOORS: ALL', 'THE REST.']),
    ('SCREEN SHAKE', 'switch', 1, ['JOLTS ON', 'HEAVY HITS', 'AND BLASTS.']),
    ('CPU METER', 'switch', 0, ['PERFORMANCE', 'READOUT BY', 'THE CLOCK.']),
]


def value_text(kind, value):
    return LEVELS[value] if kind == 'level' else ('ON' if value else 'OFF')


def tab_bar(s, selected):
    s.panel(12, 4, 216, 14)
    s.text(17, 9, 'L1', QUIET)
    s.text_right(222, 9, 'R1', QUIET)
    gap, pad = 5, 3
    total = pad * 2 + gap * (len(TABS) - 1) + sum(s.width(t) for t in TABS)
    x = (240 - total) // 2 + pad
    for index, name in enumerate(TABS):
        if index == selected:
            s.rect(x - pad, 8, s.width(name) + pad * 2, 7, ACCENT)
            s.text(x, 9, name, FILL)
        else:
            s.text(x, 9, name, QUIET)
        x += s.width(name) + gap


def dimmed_world(s):
    frame = Image.open(os.path.join(ROOT, 'frames', 'street-day.png')).convert('RGB')
    px = frame.load()
    for y in range(160):
        for x in range(240):
            r, g, b = px[x, y]
            s.pixels[x, y] = (r * 3 // 10, g * 3 // 10, b * 3 // 10)


def level_bar(s, x, y, value, selected):
    """Three steps that fill up to the level (OFF: all empty)."""
    for step in range(3):
        on = step < value
        colour = (FILL if on else ACCENT) if selected else (ACCENT if on else EMPTY)
        s.rect(x + step * 5, y, 4, 5, colour)


def settings_rows(s, left, top, width, selected):
    for index, (name, kind, value, _) in enumerate(SETTINGS):
        y = top + index * 12
        chosen = index == selected
        s.rect(left + 2, y - 3, width - 4, 11, ACCENT if chosen else FILL)
        s.text(left + 6, y, name, FILL if chosen else TEXT)
        text = value_text(kind, value)
        right = left + width - 7
        if kind == 'level':
            level_bar(s, right - 14, y, value, chosen)
            right -= 20
        s.text_right(right, y, text, FILL if chosen else QUIET)


def panel_tab():
    s = Screen()
    dimmed_world(s)
    tab_bar(s, 5)
    s.panel(12, 22, 132, 120)
    s.text(18, 27, 'GAME SETTINGS', QUIET)
    s.rect(16, 35, 124, 1, BORDER)
    settings_rows(s, 12, 42, 132, 0)
    s.text(18, 128, 'KEPT IN THE SAVE', DIM)
    s.panel(148, 22, 80, 120)
    name, kind, value, lines = SETTINGS[0]
    s.text(153, 27, name, ACCENT)
    y = 39
    for level in range(4):
        s.text(157, y, LEVELS[level], ACCENT if level == value else QUIET)
        if level == value:
            s.text(153, y, '>', ACCENT)
        y += 8
    y += 6
    for line in lines:
        s.text(153, y, line, QUIET)
        y += 7
    s.hints(147, [('D-PAD', 'CHANGE'), ('B', 'CLOSE')], [('SELECT', 'SAVE+QUIT')])
    return s


def title_backdrop(s):
    # The title's world: pixelated and faded (as in the game, approximated).
    frame = Image.open(os.path.join(ROOT, 'frames', 'street-day.png')).convert('RGB')
    small = frame.resize((30, 20), Image.BILINEAR).resize((240, 160), Image.NEAREST)
    px = small.load()
    for y in range(160):
        for x in range(240):
            r, g, b = px[x, y]
            s.pixels[x, y] = (r * 4 // 10, g * 4 // 10, b * 4 // 10)


def panel_title_menu():
    s = Screen()
    title_backdrop(s)
    s.text_centered(120, 34, 'POCKET OUTBREAK', ACCENT)
    s.panel(52, 52, 136, 58)
    s.text_centered(120, 60, 'CONTINUE', TEXT)
    s.text_centered(120, 69, 'DAY 3 ` 14:20', QUIET)
    s.text_centered(120, 83, 'NEW WORLD', TEXT)
    s.text_centered(120, 97, 'SETTINGS', ACCENT)
    s.hints(147, [('A', 'OK'), ('D-PAD', 'CHOOSE')])
    return s


def panel_title_settings():
    s = Screen()
    title_backdrop(s)
    s.text_centered(120, 10, 'POCKET OUTBREAK', ACCENT)
    s.panel(40, 22, 160, 76)
    s.text_centered(120, 28, 'SETTINGS', ACCENT)
    s.rect(44, 36, 152, 1, BORDER)
    settings_rows(s, 40, 44, 160, 2)
    for index, line in enumerate(SETTINGS[2][3]):
        s.text_centered(120, 104 + index * 8, line, QUIET)
    s.hints(147, [('UP/DOWN', 'PICK'), ('<>', 'CHANGE'), ('B', 'BACK')])
    return s


def panel_new_world():
    s = Screen()
    title_backdrop(s)
    s.text_centered(120, 10, 'POCKET OUTBREAK', ACCENT)
    s.panel(40, 22, 160, 98)
    s.text_centered(120, 28, 'NEW WORLD', ACCENT)
    s.rect(44, 36, 152, 10, ACCENT)
    s.text_centered(120, 39, 'SEED  RANDOM', FILL)
    s.rect(44, 50, 152, 1, BORDER)
    s.text_centered(120, 54, 'SANDBOX', QUIET)
    for index, (name, value) in enumerate([('ZOMBIE SPEED', 'NORMAL'), ('ZOMBIES', 'NORMAL'), ('LOOT', 'NORMAL'),
                                           ('SHUTOFF', 'NORMAL'), ('DAY LENGTH', '60 MIN')]):
        s.text(47, 65 + index * 10, name, QUIET)
        s.text_right(193, 65 + index * 10, value, TEXT)
    s.text_centered(120, 124, 'SELECT  GAME SETTINGS', QUIET)
    s.text(120 - s.width('SELECT  GAME SETTINGS') // 2, 124, 'SELECT', TEXT)
    s.hints(147, [('A', 'START'), ('UP/DOWN', 'PICK'), ('<>', 'CHANGE')])
    return s


def main():
    panels = [
        (panel_tab(), [('1 START MENU: THE SETTINGS TAB', ACCENT), ('AFTER MAP; CHANGES APPLY AT ONCE', QUIET)]),
        (panel_title_menu(), [('2 TITLE WITH A SAVE', ACCENT), ('SETTINGS UNDER NEW WORLD', QUIET)]),
        (panel_title_settings(), [('3 THE SETTINGS FROM THE TITLE', ACCENT), ('THE SAME FOUR, B GOES BACK', QUIET)]),
        (panel_new_world(), [('4 TITLE WITHOUT A SAVE', ACCENT), ('SELECT OPENS THE SETTINGS', QUIET)]),
    ]
    gap = 4
    sheet = Image.new('RGB', (240 * 2 + gap, (160 + 22) * 2 + gap), (0x05, 0x07, 0x0b))
    for index, (screen, lines) in enumerate(panels):
        x = (index % 2) * (240 + gap)
        y = (index // 2) * (160 + 22 + gap)
        sheet.paste(screen.image, (x, y))
        sheet.paste(caption(240, lines), (x, y + 160))
    sheet = sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST)
    sheet.save(os.path.join(ROOT, '18-settings.png'))
    print('wrote mockups/18-settings.png')


if __name__ == '__main__':
    main()
