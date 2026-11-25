"""Item icons: 16x16 slot pictures (frame + icon). hud.py maps these palette indices to the canvas' item bank."""
import math

from common import blank, write_header

# Item icon palette indices (colours: the "item" bank in hud.py).
I_TRANS, I_OUTLINE, I_FILL, I_BORDER, I_WHITE, I_GREY, I_DARK, I_RED, I_YELLOW, I_BLUE, I_SKY, I_WOOD, I_BROWN, \
    I_GREEN, I_OLIVE, I_ORANGE = range(16)


class Icon:
    def __init__(self):
        self.g = blank(16, 16, I_FILL)
        self.drawn = [[False] * 16 for _ in range(16)]

    def px(self, x, y, c):
        x, y = int(x), int(y)
        if 1 <= x <= 14 and 1 <= y <= 14:
            self.g[y][x] = c
            self.drawn[y][x] = True

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.px(x, y, c)

    def ellipse(self, cx, cy, rx, ry, c, inner=None):
        for y in range(16):
            for x in range(16):
                d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
                if d <= 1.0:
                    if inner and ((x - cx) / inner[0]) ** 2 + ((y - cy) / inner[1]) ** 2 <= 1.0:
                        continue
                    self.px(x, y, c)

    def line(self, x0, y0, x1, y1, c, width=1.0):
        steps = int(max(abs(x1 - x0), abs(y1 - y0)) * 3) + 1
        r = width / 2.0
        for i in range(steps + 1):
            t = i / steps
            x = x0 + (x1 - x0) * t
            y = y0 + (y1 - y0) * t
            for yy in range(int(math.floor(y - r)), int(math.ceil(y + r)) + 1):
                for xx in range(int(math.floor(x - r)), int(math.ceil(x + r)) + 1):
                    if (xx - x) ** 2 + (yy - y) ** 2 <= r * r + 0.25:
                        self.px(xx, yy, c)

    def finish(self):
        # Outline every drawn shape, then frame the slot.
        g = self.g
        out = [row[:] for row in g]
        for y in range(1, 15):
            for x in range(1, 15):
                if not self.drawn[y][x]:
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < 16 and 0 <= ny < 16 and self.drawn[ny][nx]:
                            out[y][x] = I_OUTLINE
                            break
        for i in range(16):
            out[0][i] = out[15][i] = out[i][0] = out[i][15] = I_BORDER
        return out


def can(label, height=10, top=3):
    i = Icon()
    i.rect(5, top, 10, top + height - 1, I_GREY)
    i.rect(5, top + 2, 10, top + height - 3, label)
    i.rect(6, top, 9, top, I_WHITE)
    return i


def bottle(body, cap, label=None):
    i = Icon()
    i.rect(6, 5, 9, 13, body)
    i.rect(7, 3, 8, 4, body)
    i.rect(7, 2, 8, 2, cap)
    if label is not None:
        i.rect(6, 8, 9, 10, label)
    return i


def pill_bottle(body, cap):
    i = Icon()
    i.rect(5, 5, 10, 12, body)
    i.rect(5, 3, 10, 4, cap)
    i.rect(6, 7, 9, 9, I_WHITE)
    return i


def shirt(colour, trim=None, hood=False):
    i = Icon()
    i.rect(4, 4, 11, 13, colour)
    i.rect(2, 4, 3, 10, colour)
    i.rect(12, 4, 13, 10, colour)
    if hood:
        i.ellipse(7.5, 3.5, 3, 2, colour)
    i.rect(7, 4, 8, 5, I_FILL)
    i.drawn[4][7] = i.drawn[4][8] = i.drawn[5][7] = i.drawn[5][8] = False
    if trim is not None:
        i.rect(7, 6, 8, 13, trim)
    return i


def pants(colour):
    i = Icon()
    i.rect(4, 2, 11, 5, colour)
    i.rect(4, 6, 6, 13, colour)
    i.rect(9, 6, 11, 13, colour)
    i.rect(4, 2, 11, 2, I_BROWN)
    return i


def bag(colour, pocket):
    i = Icon()
    i.ellipse(7.5, 5, 4, 3, colour)
    i.rect(4, 5, 11, 13, colour)
    i.rect(5, 9, 10, 12, pocket)
    i.rect(6, 2, 9, 2, I_DARK)
    return i


def tool_line(handle, head, x0, y0, x1, y1, split, w_handle=1.6, w_head=1.6):
    i = Icon()
    sx = x0 + (x1 - x0) * split
    sy = y0 + (y1 - y0) * split
    i.line(x0, y0, sx, sy, handle, w_handle)
    i.line(sx, sy, x1, y1, head, w_head)
    return i


def build_icons():
    icons = []

    def add(name, icon):
        icons.append((name, icon.finish()))

    add('EMPTY', Icon())
    i = Icon()
    i.ellipse(7.5, 8.5, 5, 4.5, I_ORANGE)
    i.rect(4, 5, 11, 6, I_ORANGE)
    for x in (5, 7, 9, 11):
        i.px(x, 6, I_BROWN)
    add('FISTS', i)

    # Food and drink.
    add('CANNED_BEANS', can(I_RED))
    add('CANNED_TUNA', can(I_BLUE, height=6, top=6))
    i = Icon()
    i.rect(4, 4, 11, 12, I_YELLOW)
    i.rect(4, 3, 11, 3, I_RED)
    i.rect(4, 7, 11, 8, I_RED)
    add('CHIPS', i)
    i = Icon()
    i.rect(3, 6, 12, 10, I_BROWN)
    i.rect(8, 6, 12, 10, I_RED)
    add('CHOCOLATE', i)
    i = Icon()
    i.rect(4, 4, 11, 11, I_ORANGE)
    i.rect(5, 6, 10, 9, I_WHITE)
    add('CRACKERS', i)
    i = Icon()
    i.ellipse(7.5, 8.5, 4, 4, I_RED)
    i.rect(8, 3, 8, 4, I_BROWN)
    i.px(9, 4, I_GREEN)
    i.px(6, 6, I_WHITE)
    add('APPLE', i)
    i = Icon()
    i.ellipse(7.5, 8.5, 5.5, 3.5, I_ORANGE)
    i.rect(4, 7, 11, 7, I_YELLOW)
    add('BREAD', i)
    i = Icon()
    i.rect(5, 5, 10, 12, I_ORANGE)
    i.rect(5, 3, 10, 4, I_RED)
    i.rect(6, 8, 9, 10, I_WHITE)
    add('PEANUT_BUTTER', i)
    i = can(I_RED)
    i.rect(5, 7, 10, 7, I_WHITE)
    add('SODA', i)
    add('WATER_BOTTLE', bottle(I_SKY, I_BLUE, I_BLUE))
    i = Icon()
    i.ellipse(7, 8, 5, 3.5, I_RED)
    i.rect(10, 7, 13, 8, I_WHITE)
    add('RAW_MEAT', i)

    # Medical.
    i = Icon()
    i.rect(3, 5, 12, 10, I_WHITE)
    i.rect(7, 6, 8, 9, I_RED)
    i.rect(6, 7, 9, 8, I_RED)
    add('BANDAGE', i)
    i = Icon()
    i.rect(3, 6, 12, 10, I_GREY)
    i.line(4, 7, 11, 9, I_DARK, 1)
    add('RAG', i)
    add('DISINFECTANT', bottle(I_BROWN, I_WHITE, I_WHITE))
    add('PAINKILLERS', pill_bottle(I_ORANGE, I_WHITE))
    add('ANTIBIOTICS', pill_bottle(I_WHITE, I_RED))
    i = Icon()
    i.rect(5, 2, 6, 13, I_WOOD)
    i.rect(9, 2, 10, 13, I_WOOD)
    i.rect(4, 5, 11, 5, I_WHITE)
    i.rect(4, 10, 11, 10, I_WHITE)
    add('SPLINT', i)

    # Melee weapons and tools.
    add('KITCHEN_KNIFE', tool_line(I_BROWN, I_GREY, 3, 12, 12, 3, 0.35, 1.8, 1.4))
    add('HUNTING_KNIFE', tool_line(I_DARK, I_WHITE, 3, 12, 12, 3, 0.35, 2.0, 2.2))
    add('BASEBALL_BAT', tool_line(I_BROWN, I_WOOD, 3, 12, 12, 3, 0.3, 1.6, 2.8))
    i = tool_line(I_RED, I_RED, 4, 13, 11, 3, 0.5, 1.5, 1.5)
    i.line(11, 3, 13, 5, I_RED, 1.5)
    add('CROWBAR', i)
    add('PIPE', tool_line(I_DARK, I_GREY, 3, 12, 12, 3, 0.0, 2.4, 2.4))
    i = tool_line(I_WOOD, I_WOOD, 4, 13, 9, 6, 1.0, 1.6, 1.6)
    i.line(7, 3, 12, 8, I_DARK, 3)
    add('HAMMER', i)
    i = tool_line(I_WOOD, I_WOOD, 3, 13, 10, 4, 1.0, 1.6, 1.6)
    i.ellipse(11, 5, 2.5, 3.5, I_GREY)
    i.rect(12, 3, 13, 7, I_WHITE)
    add('AXE', i)
    add('SCREWDRIVER', tool_line(I_YELLOW, I_GREY, 3, 12, 12, 3, 0.4, 2.6, 1.0))
    i = Icon()
    for y in range(4, 11):
        i.rect(5, y, 13 - (y - 4) // 3, y, I_GREY)
    for x in range(5, 13, 2):
        i.px(x, 11, I_GREY)
    i.rect(2, 4, 4, 9, I_WOOD)
    add('SAW', i)
    i = Icon()
    for x, top in ((4, 4), (7, 3), (10, 5)):
        i.rect(x, top + 1, x, top + 9, I_GREY)
        i.rect(x - 1, top, x + 1, top, I_DARK)
    add('NAILS', i)
    add('PLANK', tool_line(I_WOOD, I_WOOD, 2, 12, 13, 3, 0.0, 3.2, 3.2))
    i = Icon()
    i.ellipse(7.5, 7.5, 5, 5, I_GREY, inner=(2.2, 2.2))
    add('DUCT_TAPE', i)
    i = Icon()
    i.rect(4, 6, 10, 10, I_WHITE)
    i.rect(11, 7, 12, 9, I_ORANGE)
    i.rect(5, 7, 8, 9, I_BLUE)
    add('GLUE', i)
    i = Icon()
    i.ellipse(7.5, 7.5, 5.5, 4.5, I_WOOD, inner=(3.5, 2.5))
    i.ellipse(7.5, 7.5, 2.2, 1.5, I_WOOD)
    add('ROPE', i)

    # Clothing and bags.
    add('HOODIE', shirt(I_DARK, hood=True))
    add('LEATHER_JACKET', shirt(I_BROWN, I_DARK))
    add('RAINCOAT', shirt(I_YELLOW, I_ORANGE, hood=True))
    add('JEANS', pants(I_BLUE))
    i = Icon()
    i.rect(3, 4, 6, 12, I_BROWN)
    i.rect(3, 11, 7, 13, I_BROWN)
    i.rect(9, 4, 12, 12, I_BROWN)
    i.rect(9, 11, 13, 13, I_BROWN)
    add('BOOTS', i)
    i = Icon()
    i.rect(4, 6, 10, 12, I_DARK)
    for x in (4, 6, 8, 10):
        i.rect(x, 3, x, 6, I_DARK)
    i.rect(11, 7, 12, 9, I_DARK)
    add('GLOVES', i)
    i = Icon()
    i.ellipse(7.5, 9, 5, 5, I_RED)
    i.rect(2, 9, 13, 13, I_FILL)
    for y in range(9, 14):
        for x in range(2, 14):
            i.drawn[y][x] = False
    i.rect(3, 9, 12, 10, I_WHITE)
    add('BEANIE', i)
    add('SCHOOL_BAG', bag(I_BLUE, I_SKY))
    add('HIKING_BAG', bag(I_ORANGE, I_RED))
    add('MILITARY_BAG', bag(I_OLIVE, I_DARK))

    # Misc.
    i = Icon()
    i.rect(3, 4, 12, 11, I_WHITE)
    for x in (6, 9):
        i.rect(x, 4, x, 11, I_GREY)
    i.px(10, 6, I_RED)
    i.line(4, 9, 8, 6, I_BLUE, 1)
    add('MAP', i)
    i = Icon()
    i.ellipse(7.5, 7.5, 5, 5, I_DARK)
    i.ellipse(7.5, 7.5, 3.6, 3.6, I_WHITE)
    i.line(7.5, 7.5, 7.5, 4.5, I_RED, 1)
    i.line(7.5, 7.5, 7.5, 10.5, I_BLUE, 1)
    add('COMPASS', i)
    i = tool_line(I_DARK, I_DARK, 3, 12, 10, 5, 1.0, 3, 3)
    i.line(10, 5, 12, 3, I_YELLOW, 3.4)
    add('FLASHLIGHT', i)
    i = Icon()
    i.rect(5, 4, 10, 12, I_YELLOW)
    i.rect(5, 4, 10, 6, I_DARK)
    i.rect(7, 3, 8, 3, I_GREY)
    add('BATTERY', i)
    i = Icon()
    i.rect(3, 7, 12, 11, I_RED)
    for x in (5, 8, 11):
        i.rect(x, 3, x, 6, I_WOOD)
        i.px(x, 3, I_RED)
    add('MATCHES', i)
    i = Icon()
    i.ellipse(5, 5, 3, 3, I_YELLOW, inner=(1.2, 1.2))
    i.line(7, 7, 12, 12, I_YELLOW, 1.6)
    i.rect(10, 11, 11, 12, I_YELLOW)
    add('HOUSE_KEY', i)

    # Skill books: a cover in the skill group's colour (combat red, survival green, crafts blue), pages, a label.
    for name, cover in (('BOOK_RED', I_RED), ('BOOK_GREEN', I_GREEN), ('BOOK_BLUE', I_BLUE)):
        i = Icon()
        i.rect(4, 3, 11, 12, cover)
        i.rect(4, 3, 5, 12, I_DARK)
        i.rect(11, 4, 11, 12, I_WHITE)
        i.rect(7, 5, 9, 6, I_YELLOW)
        add(name, i)

    # Base building materials.
    i = Icon()
    i.ellipse(7.5, 9, 5, 4.5, I_DARK)
    i.rect(6, 3, 9, 5, I_DARK)
    i.px(7, 2, I_GREY)
    i.px(8, 2, I_GREY)
    i.line(5, 8, 9, 7, I_GREY, 1)
    add('GARBAGE_BAG', i)
    i = Icon()
    for x0, y0, x1, y1 in ((3, 11, 12, 4), (3, 7, 13, 9), (5, 13, 11, 3)):
        i.line(x0, y0, x1, y1, I_WOOD, 1.2)
    add('STICKS', i)
    i = Icon()
    i.ellipse(5, 10, 2.8, 2.2, I_GREY)
    i.ellipse(10, 10, 3.0, 2.4, I_GREY)
    i.ellipse(7.5, 6, 2.6, 2.0, I_GREY)
    i.px(4, 9, I_WHITE)
    i.px(9, 9, I_WHITE)
    add('STONES', i)

    # Build mode pieces (mockup 06's bar).
    i = Icon()
    i.rect(3, 3, 12, 12, I_DARK)
    i.rect(2, 4, 13, 6, I_WOOD)
    i.rect(2, 9, 13, 11, I_WOOD)
    i.px(4, 5, I_DARK)
    i.px(11, 5, I_DARK)
    i.px(4, 10, I_DARK)
    i.px(11, 10, I_DARK)
    add('PIECE_BARRICADE', i)
    i = Icon()
    i.rect(3, 3, 12, 12, I_WOOD)
    i.rect(3, 7, 12, 7, I_BROWN)
    i.line(3, 3, 12, 12, I_BROWN, 1)
    i.rect(3, 3, 3, 12, I_BROWN)
    i.rect(12, 3, 12, 12, I_BROWN)
    add('PIECE_CRATE', i)
    i = Icon()
    i.rect(3, 4, 12, 12, I_BLUE)
    i.rect(3, 4, 6, 12, I_WHITE)
    i.rect(3, 12, 12, 12, I_DARK)
    add('PIECE_BED', i)
    i = Icon()
    i.rect(4, 3, 11, 12, I_BLUE)
    i.rect(4, 3, 11, 3, I_DARK)
    i.rect(4, 6, 11, 6, I_SKY)
    i.rect(4, 9, 11, 9, I_SKY)
    i.rect(5, 4, 10, 4, I_SKY)
    add('PIECE_RAIN', i)
    i = Icon()
    i.line(3, 12, 12, 9, I_WOOD, 1.6)
    i.line(3, 9, 12, 12, I_BROWN, 1.6)
    i.ellipse(7.5, 7, 3, 4, I_ORANGE)
    i.ellipse(7.5, 8, 1.6, 2.2, I_YELLOW)
    i.px(7, 2, I_RED)
    add('PIECE_FIRE', i)

    # Tap water (shown in kitchen and bathroom cabinets while the water is on): a tap with a drop.
    i = Icon()
    i.rect(3, 4, 10, 6, I_GREY)
    i.rect(9, 6, 10, 8, I_GREY)
    i.rect(5, 2, 7, 3, I_DARK)
    i.ellipse(9.5, 11.5, 1.6, 2.2, I_SKY)
    i.px(9, 12, I_WHITE)
    add('TAP_WATER', i)

    i = Icon()
    i.rect(2, 6, 13, 12, I_DARK)
    i.rect(3, 7, 7, 11, I_GREY)
    for y in (8, 10):
        i.rect(3, y, 7, y, I_DARK)
    i.ellipse(10.5, 9, 1.6, 1.6, I_ORANGE)
    i.line(4, 6, 11, 2, I_GREY, 1)
    add('RADIO', i)

    # Foraging finds.
    i = Icon()
    for cx, cy in ((5, 6), (9, 5), (7, 9), (10, 10), (4, 11)):
        i.ellipse(cx, cy, 1.8, 1.8, I_RED)
        i.px(cx - 1, cy - 1, I_WHITE)
    i.line(7, 2, 9, 4, I_GREEN, 1)
    add('BERRIES', i)
    for name, cap, spots in (('MUSHROOMS', I_BROWN, I_WOOD), ('PALE_MUSHROOMS', I_WHITE, I_GREY)):
        i = Icon()
        i.rect(6, 8, 9, 13, I_WHITE)
        i.ellipse(7.5, 7, 5.5, 3.5, cap)
        i.rect(2, 8, 13, 8, I_DARK)
        i.px(5, 5, spots)
        i.px(10, 6, spots)
        add(name, i)
    i = Icon()
    i.line(7.5, 13, 7.5, 4, I_GREEN, 1)
    for cx, cy in ((5, 5), (10, 6), (5, 9), (10, 10)):
        i.ellipse(cx, cy, 2.4, 1.4, I_OLIVE)
    add('HERBS', i)
    i = Icon()
    i.ellipse(7, 8, 5, 3, I_SKY)
    i.ellipse(7, 9, 4, 1.6, I_WHITE)
    i.line(11, 8, 14, 5, I_SKY, 1.4)
    i.line(11, 8, 14, 11, I_SKY, 1.4)
    i.px(4, 7, I_DARK)
    add('FISH', i)

    # Cooking tools.
    i = Icon()
    i.rect(3, 6, 12, 12, I_GREY)
    i.rect(3, 6, 12, 7, I_WHITE)
    i.rect(1, 7, 2, 8, I_DARK)
    i.rect(13, 7, 14, 8, I_DARK)
    add('COOKING_POT', i)
    i = Icon()
    i.ellipse(6, 9, 4.5, 3.5, I_DARK)
    i.ellipse(6, 9, 3, 2, I_GREY)
    i.line(10, 7, 14, 3, I_BROWN, 1.6)
    add('FRYING_PAN', i)
    i = Icon()
    i.rect(4, 5, 11, 13, I_SKY)
    i.rect(4, 3, 11, 4, I_GREY)
    i.rect(5, 7, 6, 11, I_WHITE)
    add('JAR', i)
    i = Icon()
    i.rect(2, 8, 13, 12, I_GREY)
    i.ellipse(5, 7, 2, 1, I_DARK)
    i.ellipse(10, 7, 2, 1, I_DARK)
    i.px(5, 6, I_ORANGE)
    i.px(10, 6, I_ORANGE)
    add('GAS_STOVE', i)
    i = Icon()
    i.ellipse(7.5, 8.5, 4, 5, I_RED)
    i.rect(6, 2, 9, 3, I_GREY)
    i.rect(5, 7, 10, 8, I_WHITE)
    add('PROPANE', i)

    # Cooked food: bowls, plates and mugs in the dish's colours.
    for name, colour, garnish in (('SOUP', I_ORANGE, I_GREEN), ('STEW', I_BROWN, I_ORANGE),
                                  ('FISH_SOUP', I_SKY, I_WHITE)):
        i = Icon()
        i.ellipse(7.5, 8, 5.5, 2.2, colour)
        i.rect(3, 9, 12, 11, I_WHITE)
        i.rect(5, 12, 10, 12, I_WHITE)
        i.px(6, 7, garnish)
        i.px(9, 8, garnish)
        add(name, i)
    for name, food, top in (('ROAST_MEAT', I_BROWN, I_RED), ('MUSHROOM_FRY', I_WOOD, I_WHITE),
                            ('FRIED_FISH', I_YELLOW, I_ORANGE), ('BEANS_ON_TOAST', I_WOOD, I_ORANGE),
                            ('TOAST', I_WOOD, I_YELLOW), ('ROAST_FISH', I_SKY, I_BROWN)):
        i = Icon()
        i.ellipse(7.5, 10, 6, 2.5, I_WHITE)
        i.ellipse(7.5, 8.5, 3.5, 2, food)
        i.px(6, 8, top)
        i.px(9, 9, top)
        add(name, i)
    for name, colour in (('HERBAL_TEA', I_GREEN), ('HOT_CHOCOLATE', I_BROWN)):
        i = Icon()
        i.rect(4, 5, 10, 12, I_WHITE)
        i.rect(5, 5, 9, 6, colour)
        i.rect(11, 7, 12, 10, I_WHITE)
        i.px(6, 3, I_GREY)
        i.px(8, 2, I_GREY)
        add(name, i)
    i = Icon()
    i.rect(4, 5, 11, 13, I_SKY)
    i.rect(5, 7, 10, 12, I_ORANGE)
    i.rect(4, 3, 11, 4, I_GREY)
    add('CANNED_FOOD', i)

    # Magazines (spec: they teach single recipes: canning, trapping, generator use).
    for name, colour in (('MAGAZINE_CANNING', I_ORANGE), ('MAGAZINE_TRAPPING', I_GREEN),
                         ('MAGAZINE_GENERATOR', I_YELLOW)):
        i = Icon()
        i.rect(4, 2, 11, 13, I_WHITE)
        i.rect(4, 2, 11, 5, colour)
        i.rect(5, 7, 10, 7, I_GREY)
        i.rect(5, 9, 10, 9, I_GREY)
        i.rect(5, 11, 8, 11, I_GREY)
        add(name, i)

    # Farming.
    i = tool_line(I_WOOD, I_GREY, 4, 12, 10, 4, 0.55, 1.6, 3.0)
    add('SHOVEL', i)
    i = Icon()
    i.rect(3, 6, 10, 12, I_GREEN)
    i.line(10, 8, 14, 4, I_GREEN, 1.4)
    i.line(4, 6, 8, 3, I_DARK, 1)
    i.px(14, 3, I_SKY)
    add('WATERING_CAN', i)
    for name, colour in (('SEEDS_CARROT', I_ORANGE), ('SEEDS_POTATO', I_BROWN), ('SEEDS_CABBAGE', I_GREEN)):
        i = Icon()
        i.rect(4, 3, 11, 13, I_WHITE)
        i.ellipse(7.5, 8, 2.5, 2.5, colour)
        i.rect(4, 3, 11, 4, I_GREY)
        add(name, i)
    i = Icon()
    i.line(5, 5, 11, 12, I_ORANGE, 2.6)
    i.line(4, 2, 5, 5, I_GREEN, 1)
    i.line(6, 2, 5, 5, I_GREEN, 1)
    add('CARROT', i)
    i = Icon()
    i.ellipse(7.5, 8.5, 5, 4, I_WOOD)
    i.px(6, 7, I_BROWN)
    i.px(9, 10, I_BROWN)
    add('POTATO', i)
    i = Icon()
    i.ellipse(7.5, 8.5, 5, 4.5, I_GREEN)
    i.ellipse(7.5, 8.5, 2.5, 2.5, I_OLIVE)
    add('CABBAGE', i)

    # Power.
    i = Icon()
    i.rect(2, 5, 13, 12, I_GREY)
    i.rect(4, 7, 7, 10, I_YELLOW)
    i.rect(10, 8, 11, 9, I_DARK)
    add('GENERATOR', i)
    i = Icon()
    i.rect(4, 4, 11, 13, I_RED)
    i.rect(6, 2, 9, 3, I_DARK)
    i.line(5, 6, 10, 11, I_DARK, 1)
    add('FUEL_CAN', i)

    # Vehicles (spec: battery, spark plug, tyres; a hose to siphon; the key).
    i = Icon()
    i.rect(3, 6, 12, 12, I_DARK)
    i.rect(3, 6, 12, 7, I_GREY)
    i.rect(4, 4, 5, 5, I_RED)
    i.rect(10, 4, 11, 5, I_BLUE)
    i.rect(5, 9, 6, 10, I_YELLOW)
    add('CAR_BATTERY', i)
    i = Icon()
    i.rect(7, 2, 8, 4, I_GREY)
    i.rect(6, 5, 9, 9, I_WHITE)
    i.rect(5, 10, 10, 11, I_GREY)
    i.rect(7, 12, 8, 13, I_DARK)
    add('SPARK_PLUG', i)
    i = Icon()
    i.ellipse(7.5, 7.5, 6, 6, I_DARK, inner=(3, 3))
    i.ellipse(7.5, 7.5, 2.4, 2.4, I_GREY, inner=(1, 1))
    add('TYRE', i)
    i = Icon()
    i.ellipse(7.5, 8, 5.5, 4.5, I_GREEN, inner=(3.6, 2.6))
    i.line(11, 5, 13, 2, I_GREEN, 1.6)
    i.px(13, 2, I_YELLOW)
    add('HOSE', i)
    i = tool_line(I_GREY, I_GREY, 4, 12, 10, 5, 1.0, 2.0, 2.0)
    i.ellipse(11, 4, 2.6, 2.6, I_GREY, inner=(1.1, 1.1))
    i.px(12, 3, I_FILL)
    i.px(13, 2, I_FILL)
    add('WRENCH', i)
    i = Icon()
    i.rect(3, 3, 7, 7, I_DARK)
    i.px(5, 5, I_GREY)
    i.line(8, 8, 12, 12, I_GREY, 1.6)
    i.rect(10, 12, 11, 13, I_GREY)
    add('CAR_KEY', i)

    # Milestone 12.5: stitching and weapon repair.
    i = Icon()
    i.line(3, 12, 10, 5, I_GREY, 1)
    i.ellipse(11.5, 5, 2, 2.6, I_GREY, inner=(0.9, 1.4))
    i.line(4, 13, 8, 13, I_RED, 1)
    i.line(8, 13, 12, 10, I_RED, 1)
    add('SUTURE_NEEDLE', i)
    i = Icon()
    i.rect(2, 6, 13, 10, I_DARK)
    i.rect(3, 7, 12, 8, I_GREY)
    i.line(3, 9, 12, 9, I_DARK, 1)
    add('WHETSTONE', i)

    # Milestone 12.6: guns and their ammunition.
    i = Icon()
    i.rect(3, 5, 12, 7, I_DARK)
    i.rect(3, 5, 12, 5, I_GREY)
    i.rect(4, 8, 6, 12, I_BROWN)
    i.px(8, 8, I_DARK)
    add('PISTOL', i)
    i = Icon()
    i.line(2, 11, 13, 5, I_DARK, 1.6)
    i.line(2, 12, 6, 10, I_WOOD, 2.2)
    i.line(7, 9, 11, 7, I_BROWN, 1.2)
    add('SHOTGUN', i)
    i = Icon()
    i.line(2, 12, 14, 4, I_GREY, 1.0)
    i.line(2, 12, 7, 9, I_WOOD, 2.2)
    i.line(7, 6, 10, 4, I_DARK, 1.2)
    add('HUNTING_RIFLE', i)
    i = Icon()
    i.line(2, 10, 14, 5, I_DARK, 1.8)
    i.rect(7, 9, 8, 13, I_DARK)
    i.line(2, 11, 4, 12, I_OLIVE, 2.0)
    add('ASSAULT_RIFLE', i)

    def rounds(case, tip, count, height):
        icon = Icon()
        for n in range(count):
            x = 4 + n * 3
            icon.rect(x, 12 - height, x + 1, 12, case)
            icon.rect(x, 11 - height, x + 1, 11 - height, tip)
        return icon

    add('AMMO_9MM', rounds(I_YELLOW, I_ORANGE, 3, 3))
    add('AMMO_SHELLS', rounds(I_RED, I_YELLOW, 3, 5))
    add('AMMO_RIFLE', rounds(I_YELLOW, I_GREY, 2, 7))
    add('AMMO_556', rounds(I_OLIVE, I_YELLOW, 3, 6))

    # Milestone 12.6 stage 5: throwables and the spear.
    i = bottle(I_GREEN, I_GREEN)
    i.rect(7, 6, 7, 11, I_SKY)
    add('EMPTY_BOTTLE', i)
    i = Icon()
    i.ellipse(7.5, 8.5, 4.6, 4.6, I_RED)
    i.ellipse(7.5, 8.5, 3.2, 3.2, I_WHITE)
    i.ellipse(4, 3.5, 1.6, 1.4, I_GREY)
    i.ellipse(11, 3.5, 1.6, 1.4, I_GREY)
    i.line(7.5, 8.5, 7.5, 6.3, I_DARK, 1)
    i.line(7.5, 8.5, 9.3, 8.5, I_DARK, 1)
    i.rect(4, 13, 4, 13, I_GREY)
    i.rect(11, 13, 11, 13, I_GREY)
    add('ALARM_CLOCK', i)
    i = bottle(I_GREEN, I_WHITE)
    i.rect(7, 6, 7, 11, I_SKY)
    i.line(8, 2, 11, 1, I_WHITE, 1)
    i.px(11, 1, I_ORANGE)
    i.px(12, 2, I_YELLOW)
    add('MOLOTOV', i)
    add('SPEAR', tool_line(I_WOOD, I_GREY, 2, 13, 13, 2, 0.72, 1.4, 1.8))

    # Milestone 12.7 stage 4: a handwritten note.
    i = Icon()
    i.rect(3, 2, 12, 13, I_WHITE)
    for y in (4, 6, 8, 10):
        i.rect(5, y, 10 if y != 10 else 8, y, I_GREY)
    i.rect(10, 11, 12, 13, I_FILL)
    i.drawn[11][10] = i.drawn[11][11] = i.drawn[11][12] = False
    i.drawn[12][10] = i.drawn[12][11] = i.drawn[12][12] = False
    i.drawn[13][10] = i.drawn[13][11] = i.drawn[13][12] = False
    add('NOTE', i)

    # Stage 5: logs, a hinge, a doorknob; the wooden door and log wall pieces.
    i = Icon()
    for y0 in (4, 8):
        i.rect(2, y0, 13, y0 + 3, I_WOOD)
        i.ellipse(13, y0 + 1.5, 1.6, 1.9, I_BROWN)
    i.rect(2, 12, 13, 13, I_BROWN)
    add('LOGS', i)
    i = Icon()
    i.rect(3, 3, 7, 12, I_GREY)
    i.rect(8, 3, 12, 12, I_GREY)
    i.rect(7, 2, 8, 13, I_DARK)
    for y in (5, 10):
        i.px(5, y, I_DARK)
        i.px(10, y, I_DARK)
    add('HINGE', i)
    i = Icon()
    i.ellipse(7.5, 6, 3.6, 3.4, I_YELLOW)
    i.px(6, 4, I_WHITE)
    i.rect(6, 9, 9, 13, I_GREY)
    add('DOORKNOB', i)
    # Milestone 13: a deer hide, a pale-edged brown pelt.
    i = Icon()
    i.ellipse(7.5, 8, 5.6, 4.8, I_BROWN)
    i.ellipse(7.5, 8, 4.2, 3.4, I_WOOD)
    for x, y in ((2, 4), (13, 4), (2, 12), (13, 12)):
        i.px(x, y, I_BROWN)
    add('HIDE', i)
    # A pack of cigarettes, its lid open; a plastic lighter.
    i = Icon()
    i.rect(4, 4, 11, 13, I_WHITE)
    i.rect(4, 4, 11, 6, I_RED)
    for x in (5, 7, 9):
        i.rect(x, 2, x, 4, I_ORANGE)
    i.rect(6, 9, 9, 10, I_RED)
    add('CIGARETTES', i)
    i = Icon()
    i.rect(5, 5, 10, 13, I_BLUE)
    i.rect(5, 3, 10, 4, I_GREY)
    i.rect(8, 1, 9, 2, I_ORANGE)
    i.rect(6, 7, 6, 12, I_SKY)
    add('LIGHTER', i)
    i = Icon()
    i.rect(4, 2, 11, 13, I_WOOD)
    i.rect(6, 4, 9, 6, I_BROWN)
    i.rect(6, 8, 9, 11, I_BROWN)
    i.px(10, 8, I_YELLOW)
    add('PIECE_DOOR', i)
    i = Icon()
    for y0 in (3, 7, 11):
        i.rect(1, y0, 14, y0 + 2, I_WOOD)
        i.px(1, y0 + 1, I_YELLOW)
        i.px(14, y0 + 1, I_YELLOW)
    add('PIECE_LOGS', i)

    # Build mode pieces added with farming, generators and traps.
    i = Icon()
    for y in (4, 8, 12):
        i.rect(2, y, 13, y, I_BROWN)
        i.px(4, y - 1, I_GREEN)
        i.px(8, y - 1, I_GREEN)
        i.px(12, y - 1, I_GREEN)
    add('PIECE_PLOT', i)
    i = Icon()
    i.rect(2, 5, 13, 12, I_GREY)
    i.rect(4, 7, 7, 10, I_YELLOW)
    i.px(11, 3, I_DARK)
    i.px(11, 4, I_DARK)
    add('PIECE_GENERATOR', i)
    i = Icon()
    i.rect(3, 6, 12, 12, I_WOOD)
    i.rect(6, 8, 9, 11, I_DARK)
    i.line(13, 2, 13, 6, I_BROWN, 1)
    add('PIECE_TRAP', i)
    i = Icon()
    i.rect(1, 5, 14, 5, I_GREY)
    for x in (2, 7, 12):
        i.rect(x, 6, x + 2, 10, I_WHITE)
    add('PIECE_CANS', i)
    # Milestone 13: a snare, a peg and a wire noose.
    i = Icon()
    i.rect(4, 3, 5, 12, I_WOOD)
    i.rect(3, 12, 6, 13, I_BROWN)
    i.ellipse(10, 8, 3.2, 3.2, I_GREY)
    i.ellipse(10, 8, 2.0, 2.0, I_FILL)
    i.line(5, 8, 7, 8, I_GREY, 1)
    add('PIECE_SNARE', i)

    # Milestone 14 stage 5: about 70 more items (docs/milestone-14-plan.md). Food and drink.
    add('CANNED_SOUP', can(I_ORANGE))
    i = can(I_YELLOW)
    i.ellipse(7.5, 8, 1.6, 1.6, I_ORANGE)
    add('CANNED_PEACHES', i)
    i = Icon()
    i.rect(4, 3, 11, 13, I_BLUE)
    i.rect(5, 6, 10, 10, I_YELLOW)
    for x in (6, 8):
        i.rect(x, 6, x, 10, I_WOOD)
    add('PASTA', i)
    i = Icon()
    i.ellipse(7.5, 9, 4.5, 4.5, I_WHITE)
    i.rect(6, 2, 9, 5, I_WHITE)
    i.rect(5, 5, 10, 5, I_BROWN)
    add('RICE', i)
    i = Icon()
    i.rect(4, 2, 11, 13, I_RED)
    i.ellipse(7.5, 8.5, 2.6, 2.2, I_YELLOW)
    i.rect(5, 3, 10, 4, I_WHITE)
    add('CEREAL', i)
    i = Icon()
    for cx, cy in ((5.5, 6), (10, 8.5), (6, 11)):
        i.ellipse(cx, cy, 2.6, 2.4, I_WOOD)
        i.px(cx, cy, I_BROWN)
    add('COOKIES', i)
    i = Icon()
    i.rect(5, 4, 10, 13, I_ORANGE)
    i.rect(6, 2, 9, 3, I_ORANGE)
    i.rect(5, 7, 10, 9, I_YELLOW)
    i.px(9, 1, I_WHITE)
    add('JUICE', i)
    add('BEER', bottle(I_BROWN, I_YELLOW, I_YELLOW))
    i = Icon()
    i.ellipse(7.5, 9, 4.2, 3.8, I_WOOD)
    i.line(7.5, 5, 7.5, 2, I_GREEN, 1)
    i.line(7.5, 9, 7.5, 12, I_BROWN, 1)
    add('ONION', i)
    i = Icon()
    i.ellipse(7.5, 8.5, 4.6, 4.2, I_RED)
    i.rect(6, 4, 9, 4, I_GREEN)
    i.px(7, 3, I_GREEN)
    i.px(6, 7, I_ORANGE)
    add('TOMATO', i)
    i = Icon()
    i.ellipse(5.5, 8.5, 2.6, 3.4, I_WHITE)
    i.ellipse(10, 8.5, 2.6, 3.4, I_WOOD)
    add('EGGS', i)
    i = Icon()
    i.rect(4, 5, 11, 13, I_YELLOW)
    i.rect(4, 3, 11, 4, I_BROWN)
    i.rect(6, 8, 9, 10, I_ORANGE)
    add('HONEY', i)
    i = Icon()
    i.ellipse(7.5, 9, 6, 3.5, I_WHITE)
    i.ellipse(7.5, 8.5, 3.8, 2.2, I_YELLOW)
    i.px(6, 8, I_RED)
    i.px(9, 9, I_RED)
    i.px(8, 7, I_RED)
    add('PASTA_DINNER', i)
    i = Icon()
    i.ellipse(7.5, 7, 5, 2.2, I_WHITE)
    i.rect(3, 7, 12, 9, I_BROWN)
    i.rect(5, 10, 10, 11, I_BROWN)
    i.px(6, 6, I_GREEN)
    add('RICE_BOWL', i)
    i = Icon()
    i.ellipse(7.5, 8.5, 6, 4.5, I_WHITE)
    i.ellipse(5.5, 8, 1.6, 1.6, I_YELLOW)
    i.ellipse(10, 9, 1.6, 1.6, I_YELLOW)
    add('FRIED_EGGS', i)

    # Medical.
    i = Icon()
    i.rect(3, 4, 12, 11, I_WHITE)
    i.rect(3, 4, 12, 5, I_SKY)
    i.rect(7, 6, 8, 10, I_RED)
    i.rect(5, 7, 10, 8, I_RED)
    add('STERILE_BANDAGE', i)
    i = Icon()
    i.rect(3, 5, 12, 11, I_SKY)
    i.rect(4, 7, 11, 9, I_WHITE)
    i.rect(10, 4, 12, 5, I_BLUE)
    add('ALCOHOL_WIPES', i)
    add('VITAMINS', pill_bottle(I_ORANGE, I_YELLOW))
    add('SLEEPING_PILLS', pill_bottle(I_BLUE, I_SKY))
    add('COLD_PILLS', pill_bottle(I_GREEN, I_WHITE))
    i = Icon()
    i.rect(2, 5, 13, 13, I_RED)
    i.rect(6, 3, 9, 4, I_DARK)
    i.rect(7, 7, 8, 11, I_WHITE)
    i.rect(5, 8, 10, 10, I_WHITE)
    add('FIRST_AID_KIT', i)

    # Melee weapons.
    add('MACHETE', tool_line(I_DARK, I_GREY, 3, 12, 13, 2, 0.28, 1.8, 2.6))
    i = tool_line(I_WOOD, I_WOOD, 4, 13, 10, 5, 1.0, 1.6, 1.6)
    i.rect(9, 3, 13, 6, I_GREY)
    i.rect(12, 3, 13, 6, I_WHITE)
    add('HATCHET', i)
    i = tool_line(I_GREY, I_GREY, 11, 2, 5, 11, 1.0, 1.0, 1.0)
    i.rect(3, 11, 7, 13, I_DARK)
    add('GOLF_CLUB', i)
    i = tool_line(I_WOOD, I_WOOD, 3, 13, 10, 6, 1.0, 1.8, 1.8)
    i.line(7, 2, 13, 8, I_DARK, 4)
    add('SLEDGEHAMMER', i)
    i = tool_line(I_WOOD, I_WOOD, 2, 13, 9, 6, 1.0, 1.4, 1.4)
    for dx, dy in ((0, -3), (2, -1), (3, 1)):
        i.line(9, 6, 11 + dx, 4 + dy, I_GREY, 1)
    add('PITCHFORK', i)
    i = tool_line(I_DARK, I_DARK, 3, 12, 6, 9, 1.0, 1.8, 1.8)
    i.rect(6, 3, 12, 9, I_GREY)
    i.rect(6, 3, 12, 3, I_WHITE)
    i.px(11, 5, I_DARK)
    add('CLEAVER', i)
    i = tool_line(I_BROWN, I_BROWN, 11, 2, 6, 11, 1.0, 1.4, 1.4)
    i.rect(3, 11, 7, 13, I_WOOD)
    add('HOCKEY_STICK', i)
    i = tool_line(I_DARK, I_DARK, 3, 12, 12, 3, 1.0, 1.8, 1.8)
    i.line(5, 8, 7, 12, I_DARK, 1.4)
    add('POLICE_BATON', i)

    # The revolver and its rounds.
    i = Icon()
    i.rect(3, 5, 12, 6, I_GREY)
    i.rect(6, 4, 9, 8, I_DARK)
    i.rect(4, 8, 6, 12, I_WOOD)
    i.px(8, 9, I_DARK)
    add('REVOLVER', i)
    add('AMMO_38', rounds(I_YELLOW, I_GREY, 3, 4))

    # Tools and materials.
    i = Icon()
    i.line(3, 13, 8, 6, I_RED, 1.6)
    i.line(9, 13, 6, 6, I_RED, 1.6)
    i.line(7, 7, 6, 2, I_GREY, 1.4)
    i.line(7, 7, 9, 2, I_GREY, 1.4)
    add('PLIERS', i)
    i = Icon()
    for cy in (5.5, 8, 10.5):
        i.ellipse(7.5, cy, 4.5, 1.8, I_GREY, inner=(3.4, 0.8))
    i.line(12, 10, 14, 13, I_GREY, 1)
    add('WIRE', i)
    i = Icon()
    i.rect(3, 4, 8, 8, I_GREY)
    i.rect(7, 8, 12, 12, I_DARK)
    i.line(9, 3, 12, 6, I_GREY, 1.4)
    i.px(4, 5, I_WHITE)
    add('SCRAP_METAL', i)
    i = Icon()
    i.rect(2, 4, 13, 11, I_GREY)
    i.line(3, 10, 12, 5, I_WHITE, 1)
    i.rect(2, 11, 13, 11, I_DARK)
    add('SHEET_METAL', i)
    i = Icon()
    i.rect(2, 5, 13, 11, I_BLUE)
    i.rect(2, 8, 13, 8, I_SKY)
    for x in (3, 12):
        i.px(x, 6, I_WHITE)
        i.px(x, 10, I_WHITE)
    add('TARP', i)
    i = Icon()
    i.rect(3, 5, 12, 11, I_RED)
    for x in range(3, 13, 3):
        i.rect(x, 5, x, 11, I_WHITE)
    for y in (7, 10):
        i.rect(3, y, 12, y, I_WHITE)
    add('CLOTH', i)
    i = Icon()
    i.ellipse(5, 9, 3, 3.2, I_DARK)
    i.ellipse(10, 9, 3, 3.2, I_DARK)
    i.ellipse(5, 9, 1.5, 1.6, I_SKY)
    i.ellipse(10, 9, 1.5, 1.6, I_SKY)
    i.rect(6, 5, 9, 6, I_DARK)
    add('BINOCULARS', i)
    i = Icon()
    i.rect(3, 6, 12, 12, I_WOOD)
    i.rect(5, 3, 7, 6, I_RED)
    i.rect(5, 4, 7, 4, I_WHITE)
    i.line(9, 2, 12, 7, I_GREY, 1)
    add('SEWING_KIT', i)
    i = Icon()
    i.line(3, 12, 11, 4, I_GREY, 1)
    i.line(11, 4, 12, 6, I_GREY, 1)
    i.line(3, 12, 5, 10, I_DARK, 2)
    add('LOCKPICK', i)
    i = Icon()
    for y in range(6, 14):
        w = 4 - (y - 6) // 3
        i.rect(7 - w, y, 8 + w, y, I_GREY)
    i.ellipse(7.5, 6, 4.6, 3.5, I_DARK, inner=(3.8, 2.8))
    i.rect(3, 6, 12, 6, I_WHITE)
    add('BUCKET', i)
    i = Icon()
    for cx in (5, 10):
        i.ellipse(cx, 8, 2.8, 3.8, I_WHITE, inner=(1.8, 2.8))
    i.rect(4, 12, 11, 12, I_DARK)
    add('ZIP_TIES', i)
    i = Icon()
    i.rect(2, 6, 13, 13, I_RED)
    i.rect(2, 8, 13, 8, I_DARK)
    i.rect(5, 3, 10, 4, I_DARK)
    i.rect(5, 3, 5, 6, I_DARK)
    i.rect(10, 3, 10, 6, I_DARK)
    add('TOOLBOX', i)
    i = Icon()
    i.line(2, 13, 12, 2, I_BROWN, 1.2)
    i.line(12, 2, 13, 10, I_WHITE, 1)
    i.ellipse(4, 11, 1.5, 1.5, I_GREY)
    add('FISHING_ROD', i)

    # Clothing and bags.
    i = shirt(I_WHITE)
    i.rect(2, 8, 3, 10, I_FILL)
    i.rect(12, 8, 13, 10, I_FILL)
    for y in range(8, 11):
        for x in (2, 3, 12, 13):
            i.drawn[y][x] = False
    add('T_SHIRT', i)
    add('SHIRT', shirt(I_SKY, I_WHITE))
    i = shirt(I_RED)
    i.rect(4, 12, 11, 13, I_BROWN)
    add('SWEATER', i)
    i = shirt(I_BLUE, I_DARK)
    i.rect(9, 6, 10, 7, I_YELLOW)
    add('POLICE_JACKET', i)
    add('ARMY_JACKET', shirt(I_OLIVE, I_BROWN))
    i = shirt(I_YELLOW, I_DARK)
    i.rect(4, 10, 11, 10, I_WHITE)
    add('FIREFIGHTER_JACKET', i)
    i = shirt(I_ORANGE)
    for y in (6, 9, 12):
        i.rect(4, y, 11, y, I_RED)
    add('PADDED_JACKET', i)
    i = pants(I_OLIVE)
    i.rect(4, 8, 5, 10, I_BROWN)
    i.rect(10, 8, 11, 10, I_BROWN)
    add('CARGO_PANTS', i)
    add('SUIT_TROUSERS', pants(I_DARK))
    i = pants(I_BLUE)
    i.rect(5, 1, 10, 3, I_BLUE)
    i.rect(5, 1, 5, 1, I_YELLOW)
    i.rect(10, 1, 10, 1, I_YELLOW)
    add('OVERALLS', i)

    def shoes(colour, trim, top):
        icon = Icon()
        for x0 in (2, 9):
            icon.rect(x0, top, x0 + 3, 12, colour)
            icon.rect(x0, 11, x0 + 4, 13, colour)
            icon.rect(x0, 13, x0 + 4, 13, trim)
        return icon

    add('SNEAKERS', shoes(I_WHITE, I_RED, 8))
    add('WELLIES', shoes(I_GREEN, I_DARK, 3))
    add('ARMY_BOOTS', shoes(I_OLIVE, I_DARK, 5))
    i = Icon()
    i.rect(4, 6, 10, 12, I_WOOD)
    for x in (4, 6, 8, 10):
        i.rect(x, 3, x, 6, I_WOOD)
    i.rect(11, 7, 12, 9, I_WOOD)
    i.rect(4, 11, 10, 12, I_BROWN)
    add('WORK_GLOVES', i)
    i = Icon()
    i.ellipse(7, 8, 4.5, 3.8, I_RED)
    i.rect(2, 8, 14, 13, I_FILL)
    for y in range(8, 14):
        for x in range(2, 15):
            i.drawn[y][x] = False
    i.rect(7, 8, 13, 9, I_RED)
    add('CAP', i)
    i = Icon()
    i.ellipse(7.5, 9, 5.5, 5, I_OLIVE)
    i.rect(1, 9, 14, 13, I_FILL)
    for y in range(9, 14):
        for x in range(1, 15):
            i.drawn[y][x] = False
    i.rect(2, 9, 13, 10, I_OLIVE)
    i.px(6, 6, I_GREEN)
    add('HELMET', i)
    i = Icon()
    for y in range(4, 12):
        i.rect(3 + (y - 4) // 2, y, 12 - (y - 4) // 2, y, I_RED)
    i.px(6, 6, I_WHITE)
    i.px(9, 7, I_WHITE)
    add('BANDANA', i)
    i = Icon()
    i.ellipse(7.5, 9, 6, 3.5, I_DARK)
    i.rect(5, 4, 10, 5, I_DARK)
    i.rect(2, 9, 13, 9, I_BLUE)
    add('DUFFEL_BAG', i)
    i = Icon()
    i.rect(3, 6, 12, 12, I_BROWN)
    i.rect(3, 6, 12, 8, I_WOOD)
    i.line(4, 6, 8, 1, I_BROWN, 1)
    i.line(11, 6, 8, 1, I_BROWN, 1)
    add('SATCHEL', i)
    i = bag(I_RED, I_DARK)
    i.rect(3, 4, 12, 13, I_RED)
    i.rect(5, 9, 10, 12, I_DARK)
    i.rect(5, 2, 10, 3, I_RED)
    add('LARGE_BACKPACK', i)

    # Car parts.
    i = Icon()
    i.rect(2, 11, 13, 13, I_RED)
    i.line(4, 11, 8, 6, I_GREY, 1.4)
    i.line(11, 11, 8, 6, I_GREY, 1.4)
    i.rect(6, 4, 10, 5, I_DARK)
    add('CAR_JACK', i)
    i = Icon()
    i.rect(2, 3, 13, 12, I_DARK)
    for x in range(3, 13, 2):
        i.rect(x, 4, x, 11, I_GREY)
    i.rect(5, 1, 6, 2, I_GREY)
    add('RADIATOR', i)
    i = Icon()
    i.ellipse(7.5, 8, 6, 6, I_DARK, inner=(3.6, 3.6))
    i.rect(1, 7, 14, 8, I_FILL)
    for x in range(1, 15):
        i.drawn[7][x] = i.drawn[8][x] = False
    i.ellipse(7.5, 8, 6.4, 6.4, I_GREY, inner=(5.4, 5.4))
    add('BRAKE_PADS', i)

    # Farming: seed packets and a kettle.
    for name, colour in (('SEEDS_TOMATO', I_RED), ('SEEDS_ONION', I_WOOD)):
        i = Icon()
        i.rect(4, 3, 11, 13, I_WHITE)
        i.ellipse(7.5, 8, 2.5, 2.5, colour)
        i.rect(4, 3, 11, 4, I_GREY)
        add(name, i)
    i = Icon()
    i.ellipse(7, 9, 4.6, 4.2, I_GREY)
    i.line(11, 8, 14, 5, I_GREY, 1.4)
    i.rect(5, 3, 9, 4, I_DARK)
    i.px(7, 2, I_DARK)
    add('KETTLE', i)

    # Animals plan stage 4: a goat's milk, in a bottle.
    add('MILK', bottle(I_WHITE, I_BLUE, I_SKY))
    return icons


def write_icon_header(names):
    lines = ['#include <cstdint>', '', 'namespace kx', '{', '',
             '// Item and moodle icons (pixels in kx_icon_pixels.h).',
             'enum class item_icon : uint8_t', '{']
    lines += ['    ' + n + ',' for n in names]
    lines += ['    COUNT', '};', '', '}']
    write_header('kx_item_icons.h', lines)
