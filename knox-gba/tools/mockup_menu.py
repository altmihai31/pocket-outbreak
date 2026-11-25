"""Playtest 3 mockup: the MENU tab (it was SETTINGS) with its GUIDE. Drawn at 240x160 with the game's 5 px font and
colours, saved at 3x.

    python tools/mockup_menu.py        -> mockups/20-menu.png

1. The Start menu's MENU tab: two entries, SETTINGS and GUIDE; A opens one, B goes back to them.
2. GUIDE: the topics on the left, the chosen topic's page on the right; Up/Down choose a topic, Left/Right turn its
   pages. The text is the game's own (tools/assets/guide.py).
3. The title with a save: CONTINUE, NEW WORLD, MENU; MENU shows the same two entries.
4. The guide from the title: the same page under the title.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'assets'))

from guide import TOPICS, pages  # noqa: E402
from mockup_character import ACCENT, BORDER, FILL, QUIET, TEXT, Screen  # noqa: E402
from mockup_doors import caption  # noqa: E402
from mockup_settings import dimmed_world, title_backdrop  # noqa: E402

ROOT = os.path.join(HERE, '..', 'mockups')
TABS = ['GEAR', 'HEALTH', 'CRAFT', 'SKILLS', 'MAP', 'MENU']
ENTRIES = [
    ('SETTINGS', ['MUSIC AND', 'SOUND LEVELS,', 'SCREEN SHAKE,', 'THE CPU METER.']),
    ('GUIDE', ['HOW TO PLAY,', 'IN %d TOPICS:' % len(TOPICS), 'CONTROLS TO', 'SAVING.']),
]


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


def entry_rows(s, left, top, width, selected):
    for index, (name, _) in enumerate(ENTRIES):
        y = top + index * 12
        chosen = index == selected
        s.rect(left + 2, y - 3, width - 4, 11, ACCENT if chosen else FILL)
        s.text(left + 6, y, name, FILL if chosen else TEXT)
        s.text_right(left + width - 7, y, '>', FILL if chosen else QUIET)


def guide_page(s, top, topic, page_index):
    """The topics (12..76) and the page (80..228), panels from `top`, 120 px tall."""
    s.panel(12, top, 64, 120)
    s.text(18, top + 5, 'GUIDE', QUIET)
    s.rect(16, top + 13, 56, 1, BORDER)
    for index, (name, _) in enumerate(TOPICS):
        y = top + 18 + index * 7
        chosen = index == topic
        if chosen:
            s.rect(14, y - 1, 60, 7, ACCENT)
        s.text(18, y, name, FILL if chosen else TEXT)

    name, paragraphs = TOPICS[topic]
    all_pages = pages(paragraphs)
    s.panel(80, top, 148, 120)
    s.text(86, top + 5, name, ACCENT)
    s.text_right(222, top + 5, '%d/%d' % (page_index + 1, len(all_pages)), QUIET)
    s.rect(84, top + 13, 140, 1, BORDER)
    for index, line in enumerate(all_pages[page_index]):
        s.text(86, top + 18 + index * 7, line, TEXT)


def panel_tab():
    s = Screen()
    dimmed_world(s)
    tab_bar(s, 5)
    s.panel(12, 22, 132, 120)
    s.text(18, 27, 'MENU', QUIET)
    s.rect(16, 35, 124, 1, BORDER)
    entry_rows(s, 12, 42, 132, 1)
    name, lines = ENTRIES[1]
    s.panel(148, 22, 80, 120)
    s.text(153, 27, name, ACCENT)
    for index, line in enumerate(lines):
        s.text(153, 39 + index * 7, line, QUIET)
    s.hints(147, [('A', 'OPEN'), ('B', 'CLOSE')], [('SELECT', 'SAVE+QUIT')])
    return s


def panel_guide():
    s = Screen()
    dimmed_world(s)
    tab_bar(s, 5)
    guide_page(s, 22, 4, 0)
    s.hints(147, [('UP/DOWN', 'TOPIC'), ('<>', 'PAGE')], [('SELECT', 'SAVE+QUIT')])
    return s


def panel_title_menu():
    s = Screen()
    title_backdrop(s)
    s.text_centered(120, 34, 'POCKET OUTBREAK', ACCENT)
    s.panel(52, 52, 136, 58)
    s.text_centered(120, 60, 'CONTINUE', TEXT)
    s.text_centered(120, 69, 'DAY 3 ` 14:20', QUIET)
    s.text_centered(120, 83, 'NEW WORLD', TEXT)
    s.text_centered(120, 97, 'MENU', ACCENT)
    s.hints(147, [('A', 'OK'), ('D-PAD', 'CHOOSE')])
    return s


def panel_title_menu_open():
    s = Screen()
    title_backdrop(s)
    s.text_centered(120, 10, 'POCKET OUTBREAK', ACCENT)
    s.panel(60, 40, 120, 52)
    s.text_centered(120, 46, 'MENU', ACCENT)
    s.rect(64, 54, 112, 1, BORDER)
    entry_rows(s, 60, 62, 120, 0)
    s.hints(147, [('A', 'OPEN'), ('D-PAD', 'CHOOSE'), ('B', 'BACK')])
    return s


def panel_title_guide():
    s = Screen()
    title_backdrop(s)
    s.text_centered(120, 9, 'POCKET OUTBREAK', ACCENT)
    guide_page(s, 22, 0, 0)
    s.hints(147, [('UP/DOWN', 'TOPIC'), ('<>', 'PAGE'), ('B', 'BACK')])
    return s


def main():
    panels = [
        (panel_tab(), [('1 START MENU: THE MENU TAB', ACCENT), ('SETTINGS AND GUIDE; A OPENS ONE', QUIET)]),
        (panel_guide(), [('2 THE GUIDE', ACCENT), ('TOPICS LEFT, PAGES RIGHT', QUIET)]),
        (panel_title_menu(), [('3 TITLE WITH A SAVE', ACCENT), ('MENU UNDER NEW WORLD', QUIET)]),
        (panel_title_menu_open(), [('4 MENU FROM THE TITLE', ACCENT), ('THE SAME TWO ENTRIES', QUIET)]),
        (panel_title_guide(), [('5 THE GUIDE FROM THE TITLE', ACCENT), ('THE SAME PAGES', QUIET)]),
    ]
    gap = 4
    columns = 2
    rows = (len(panels) + columns - 1) // columns
    sheet = Image.new('RGB', (240 * columns + gap, (160 + 22) * rows + gap * (rows - 1)), (0x05, 0x07, 0x0b))
    for index, (screen, lines) in enumerate(panels):
        x = (index % columns) * (240 + gap)
        y = (index // columns) * (160 + 22 + gap)
        sheet.paste(screen.image, (x, y))
        sheet.paste(caption(240, lines), (x, y + 160))
    sheet = sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST)
    sheet.save(os.path.join(ROOT, '20-menu.png'))
    print('wrote mockups/20-menu.png')


if __name__ == '__main__':
    main()
