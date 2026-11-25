"""Survival UI art: the moodle icons (appended to the item icon set)."""
from items import I_BLUE, I_BROWN, I_DARK, I_GREEN, I_GREY, I_ORANGE, I_RED, I_SKY, I_WHITE, I_YELLOW, Icon


def moodle_icons():
    """(name, icon) pairs, in kx::moodle order."""
    icons = []

    i = Icon()                                       # hungry: drumstick
    i.ellipse(6.5, 6.5, 4, 3.5, I_ORANGE)
    i.line(9, 9, 12, 12, I_WHITE, 1.6)
    i.px(12, 13, I_WHITE)
    i.px(13, 12, I_WHITE)
    icons.append(('MOODLE_HUNGRY', i))

    i = Icon()                                       # thirsty: drop
    i.ellipse(7.5, 9, 4, 4, I_BLUE)
    i.line(7.5, 3, 7.5, 6, I_BLUE, 2.4)
    i.px(6, 8, I_SKY)
    icons.append(('MOODLE_THIRSTY', i))

    i = Icon()                                       # tired: moon
    i.ellipse(7.5, 7.5, 5, 5, I_YELLOW, inner=None)
    i.ellipse(10, 6, 4, 4, 2)
    for y in range(16):
        for x in range(16):
            if ((x - 10) / 4) ** 2 + ((y - 6) / 4) ** 2 <= 1:
                i.drawn[y][x] = False
    icons.append(('MOODLE_TIRED', i))

    i = Icon()                                       # exhausted: sweat drops on a dark head
    i.ellipse(7, 8, 4.5, 4.5, I_DARK)
    i.ellipse(12, 5, 1.5, 2, I_SKY)
    i.ellipse(11.5, 10, 1.2, 1.6, I_SKY)
    icons.append(('MOODLE_EXHAUSTED', i))

    i = Icon()                                       # wet: rain
    for x, y in ((4, 3), (8, 5), (12, 3), (5, 9), (10, 10), (7, 12)):
        i.line(x, y, x - 1, y + 2, I_SKY, 1)
    icons.append(('MOODLE_WET', i))

    i = Icon()                                       # cold: snowflake
    i.line(7.5, 2.5, 7.5, 12.5, I_SKY, 1)
    i.line(3, 5, 12, 10, I_SKY, 1)
    i.line(3, 10, 12, 5, I_SKY, 1)
    icons.append(('MOODLE_COLD', i))

    i = Icon()                                       # hot: sun
    i.ellipse(7.5, 7.5, 3, 3, I_ORANGE)
    for x0, y0, x1, y1 in ((7.5, 1.5, 7.5, 3), (7.5, 12, 7.5, 13.5), (1.5, 7.5, 3, 7.5), (12, 7.5, 13.5, 7.5),
                           (3, 3, 4, 4), (11, 11, 12, 12), (3, 12, 4, 11), (11, 4, 12, 3)):
        i.line(x0, y0, x1, y1, I_YELLOW, 1)
    icons.append(('MOODLE_HOT', i))

    i = Icon()                                       # panic: exclamation
    i.rect(6, 2, 9, 9, I_RED)
    i.rect(6, 11, 9, 13, I_RED)
    icons.append(('MOODLE_PANIC', i))

    i = Icon()                                       # unhappy: sad face
    i.ellipse(7.5, 7.5, 5.5, 5.5, I_YELLOW)
    i.px(5, 6, I_BROWN)
    i.px(10, 6, I_BROWN)
    i.line(5, 11, 10, 11, I_BROWN, 1)
    i.px(4, 12, I_BROWN)
    i.px(11, 12, I_BROWN)
    icons.append(('MOODLE_UNHAPPY', i))

    i = Icon()                                       # heavy load: weight
    i.rect(3, 6, 12, 13, I_DARK)
    i.ellipse(7.5, 4.5, 2.5, 2, I_GREY, inner=(1.2, 0.8))
    icons.append(('MOODLE_HEAVY', i))

    i = Icon()                                       # pain: lightning
    i.line(9, 2, 5, 8, I_YELLOW, 1.6)
    i.line(5, 8, 10, 8, I_YELLOW, 1.6)
    i.line(10, 8, 6, 13, I_YELLOW, 1.6)
    icons.append(('MOODLE_PAIN', i))

    i = Icon()                                       # bleeding: drops
    i.ellipse(6, 7, 3, 3.5, I_RED)
    i.line(6, 2.5, 6, 4.5, I_RED, 2)
    i.ellipse(11, 11, 1.8, 2, I_RED)
    icons.append(('MOODLE_BLEEDING', i))

    i = Icon()                                       # sick: green face
    i.ellipse(7.5, 7.5, 5.5, 5.5, I_GREEN)
    i.px(5, 6, I_DARK)
    i.px(10, 6, I_DARK)
    i.line(5, 10, 10, 10, I_DARK, 1)
    icons.append(('MOODLE_SICK', i))

    i = Icon()                                       # fever: thermometer
    i.rect(7, 2, 8, 10, I_WHITE)
    i.rect(7, 6, 8, 10, I_RED)
    i.ellipse(7.5, 11.5, 2.2, 2.2, I_RED)
    icons.append(('MOODLE_FEVER', i))
    return [(name, icon.finish()) for name, icon in icons]
