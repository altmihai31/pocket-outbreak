"""Milestone 11 mockup: the Craft tab (cooking), drawn at 240x160 with the game's 5 px font and palette, saved at 3x.

    python tools/mockup_craft.py        -> mockups/09-craft.png
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mockup_character import (ACCENT, BORDER, DIM, EMPTY, FILL, GOOD, ORANGE, QUIET, RED, TEXT,  # noqa: E402
                              Screen)


def craft_tab(path):
    s = Screen()
    s.tab_bar(2)

    # Left: the heat source nearby, then the recipes: what can be made now first, the rest dimmed.
    s.panel(12, 22, 120, 120)
    s.text(17, 26, 'HEAT', QUIET)
    s.text_right(127, 26, 'CAMPFIRE', GOOD)
    s.rect(16, 33, 112, 1, BORDER)
    recipes = [('BOIL WATER', True), ('HERBAL TEA', True), ('VEGETABLE SOUP', True), ('ROAST MEAT', True),
               ('MUSHROOM FRY', False), ('FRIED FISH', False), ('FISH SOUP', False), ('STEW', False),
               ('BEANS ON TOAST', False), ('CANNED FOOD', False), ('TOAST', False), ('HOT CHOCOLATE', False),
               ('ROAST FISH', False)]
    y = 37
    selected = 'VEGETABLE SOUP'
    for name, ready in recipes:
        if name == selected:
            s.rect(14, y - 1, 116, 7, ACCENT)
            colour = FILL
        else:
            colour = TEXT if ready else DIM
        s.text(17, y, name, colour)
        if ready:
            s.rect(123, y + 1, 3, 3, FILL if name == selected else GOOD)
        y += 7

    # Right: the selected recipe.
    s.panel(136, 22, 92, 120)
    s.text(141, 27, 'VEGETABLE SOUP', ACCENT)
    s.text(141, 37, 'NEEDS', QUIET)
    needs = [('COOKING POT', 'OK', GOOD), ('HERBS', '2/1', GOOD), ('MUSHROOMS', '1/1', GOOD),
             ('WATER BOTTLE', '1/1', GOOD), ('HEAT', 'OK', GOOD)]
    y = 45
    for name, have, colour in needs:
        s.text(141, y, name, TEXT)
        s.text_right(223, y, have, colour)
        y += 7
    s.rect(140, y + 1, 84, 1, BORDER)
    s.text(141, y + 5, 'MAKES', QUIET)
    s.text(141, y + 13, 'SOUP', TEXT)
    s.text(141, y + 20, 'FOOD +40', QUIET)
    s.text(141, y + 27, 'WATER +15 ` MOOD UP', QUIET)
    s.text(141, y + 34, 'TAKES 30 MIN', QUIET)

    s.hints(147, [('A', 'COOK'), ('B', 'CLOSE')], [('SELECT', 'SAVE+QUIT')])
    s.save(path)


if __name__ == '__main__':
    root = os.path.join(os.path.dirname(__file__), '..', 'mockups')
    craft_tab(os.path.join(root, '09-craft.png'))
    print('wrote mockups/09-craft.png')
