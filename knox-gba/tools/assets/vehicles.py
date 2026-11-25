"""Vehicle sprites (spec: Vehicles): a sedan seen from above in 16 directions, 9 drawn and the rest mirrored (32x32,
not hardware-rotated), in four body colours plus three wrecks (mockups/wreck-options-3.png: burnt out with the
bonnet gone; picked clean; its bonnet in the road), each also with moss growing on it (left standing for months);
and a bicycle (16x16, side and end on).

One 16-colour palette holds every body colour, so all vehicles on screen share a single sprite palette; the body
colour is picked by the frame, not by a palette swap.
"""
import math

from common import TRANSPARENT, hexc, save_bmp, write_header

# Palette roles.
V_TRANS, V_OUTLINE, V_GLASS, V_RUST, V_DARK, V_LIGHT, V_LAMP, V_MID = range(8)
V_BODY = 8                      # then base and shade for each body colour

BODY_COLOURS = [
    ('BLUE', '4a78c0', '34558c'),
    ('RED', 'c0483c', '8c3028'),
    ('CREAM', 'd8d0b8', 'a8a08c'),
    ('GREEN', '5c9458', '3f6a40'),
]

VEHICLE_PALETTE = [TRANSPARENT, hexc('14161c'), hexc('7fb0e0'), hexc('8a4a2c'), hexc('2a2f3a'), hexc('a8b0bc'),
                   hexc('e8c547'), hexc('5c6470')]
for _name, _base, _shade in BODY_COLOURS:
    VEHICLE_PALETTE += [hexc(_base), hexc(_shade)]

DIRECTIONS = 16                 # 0 east, clockwise (4 south, 8 west, 12 north)
DRAWN = 9                       # north round to south through east; the west half is mirrored
WRECK = len(BODY_COLOURS)       # the variants after the body colours: the wreck looks
WRECK_LOOKS = 3

HALF_LENGTH = 13.0
HALF_WIDTH = 6.5


# Moss on a car left standing (the green body colours): clumps on the roof, the bonnet and the boot, as (along,
# across, half length, half width). A clump's middle is the lighter green, its rim the darker.
MOSS_CLUMPS = [(-1.5, 1.0, 3.4, 2.6), (8.2, -2.0, 2.3, 1.9), (-10.2, 2.6, 1.5, 1.4), (1.2, -2.6, 1.3, 1.1)]
MOSS = V_BODY + 6
MOSS_DARK = V_BODY + 7


def moss_at(u, v, variant):
    """The moss over a point of the car, or None. On the green car the clumps are drawn darker, or they would not
    show."""
    for cu, cv, lu, lv in MOSS_CLUMPS:
        d = ((u - cu) / lu) ** 2 + ((v - cv) / lv) ** 2
        if d <= 1.0:
            green = variant == 3
            if d <= 0.45:
                return MOSS_DARK if green else MOSS
            return V_DARK if green else MOSS_DARK
    return None


def car_role(u, v, variant, mossy=False):
    """Palette index at a point of the car: u along its length (front positive), v across it."""
    role = car_role_plain(u, v, variant)
    # Wrecks have lost their bonnet: no moss in the engine bay.
    if variant >= WRECK and u > 5.6:
        return role
    if mossy and role not in (V_TRANS, V_DARK) and abs(v) <= HALF_WIDTH:
        moss = moss_at(u, v, variant)
        if moss is not None:
            return moss
    return role


# ---- wrecks (mockups/wreck-options-3.png, tools/mockup_wreck.py) ----

def wreck_shape(u, v):
    """The car's outline: None outside, else 'tyre' or 'body'."""
    au, av = abs(u), abs(v)
    if 6.0 <= au <= 10.5 and 6.0 <= av <= 7.4:
        return 'tyre'
    if au > HALF_LENGTH or av > HALF_WIDTH:
        return None
    if au > 11.0 and av > 4.5 and (au - 11.0) ** 2 + (av - 4.5) ** 2 > 4.0:
        return None
    return 'body'


def wreck_zone(u, v):
    av = abs(v)
    if 2.4 <= u <= 5.6 and av <= 4.8:
        return 'windscreen'
    if -8.0 <= u <= -5.4 and av <= 4.8:
        return 'rear window'
    if -5.4 < u < 2.4:
        return 'roof' if av <= 4.6 else 'side window'
    return 'bonnet' if u > 0 else 'boot'


def engine_bay(u, v):
    """Under a missing bonnet: the radiator across the front, the engine block, a battery, the rest dark."""
    av = abs(v)
    if av > 5.4:
        return None                                 # the wings stay
    if u > 10.8:
        return V_LIGHT if int(v + 10) % 2 else V_MID
    if 6.6 < u < 10.0 and av < 2.6:
        return V_LIGHT if abs(u - 8.3) < 0.5 else V_MID
    if 6.4 < u < 8.6 and 3.2 < v < 5.0:
        return V_RUST
    return V_OUTLINE


def boot_well(u, v):
    """Under a missing boot lid: a dark well with the spare wheel."""
    if abs(v) > 5.4:
        return None
    d = (u + 10.3) ** 2 + v ** 2
    if d < 0.8:
        return V_MID
    if d < 4.0:
        return V_DARK
    return V_OUTLINE


def doors_off(u, v, side):
    """The doors on one side (v of that sign) gone: the sill and the seats where they were; None elsewhere."""
    if v * side < 4.2 or not (-5.2 < u < 2.3):
        return None
    if v * side > 5.6:
        return V_TRANS                              # the door's thickness is gone from the outline
    seat = -1.4 < u < 1.6 or -5.0 < u < -3.2
    return V_RUST if seat else V_DARK


def wreck_burnt(u, v):
    """G: burnt out, the bonnet gone: soot, rusted edges, black window holes, tyres burnt to the rims."""
    part = wreck_shape(u, v)
    if part is None:
        return V_TRANS
    au, av = abs(u), abs(v)
    if part == 'tyre':
        return V_MID if abs(au - 8.25) < 1.0 and av > 6.6 else V_OUTLINE
    if u > 5.6:
        bay = engine_bay(u, v)
        if bay is not None:
            return bay
    where = wreck_zone(u, v)
    if where in ('windscreen', 'rear window', 'side window'):
        return V_OUTLINE
    if av > 5.2 or au > 11.2 or (6.0 <= au <= 10.5 and av > 4.4):
        return V_RUST
    return V_MID if where == 'roof' else V_DARK


def wreck_picked(u, v):
    """I: picked clean, faded red: the bonnet, the boot lid, the doors on one side and a front wheel gone."""
    red, red_shade = V_BODY + 2, V_BODY + 3
    part = wreck_shape(u, v)
    if part is None:
        return V_TRANS
    if part == 'tyre':
        return V_TRANS if (u > 0 and v < 0) else V_DARK
    door = doors_off(u, v, -1)
    if door is not None:
        return door
    if u > 5.6:
        bay = engine_bay(u, v)
        if bay is not None:
            return bay
    if u < -8.0:
        well = boot_well(u, v)
        if well is not None:
            return well
    if wreck_zone(u, v) in ('windscreen', 'rear window', 'side window'):
        return V_OUTLINE
    if 6.0 <= abs(u) <= 10.5 and abs(v) > 4.6 and u > 0 and v < 0:
        return V_OUTLINE                            # the empty arch
    return red_shade if abs(v) > 5.4 else red


def wreck_bonnet(u, v):
    """J: cream, its bonnet lying in the road beside it, the front door off, the windscreen smashed."""
    cream, cream_shade = V_BODY + 4, V_BODY + 5
    du, dv = u - 2.0, v + 12.2
    angle = math.radians(8)
    ru = du * math.cos(angle) + dv * math.sin(angle)
    rv = -du * math.sin(angle) + dv * math.cos(angle)
    if abs(ru) < 3.2 and abs(rv) < 2.2:
        return cream_shade if abs(ru) > 2.3 else cream
    part = wreck_shape(u, v)
    if part is None:
        return V_TRANS
    if part == 'tyre':
        return V_DARK
    door = doors_off(u, v, 1)
    if door is not None and u > -1.6:
        return door
    if u > 5.6:
        bay = engine_bay(u, v)
        if bay is not None:
            return bay
    where = wreck_zone(u, v)
    if where == 'windscreen':
        return V_OUTLINE if (int(u * 2) + int(v * 2)) % 3 else V_GLASS
    if where in ('rear window', 'side window'):
        return V_GLASS if where == 'rear window' else V_OUTLINE
    return cream_shade if abs(v) > 5.4 else cream


WRECK_ROLES = [wreck_burnt, wreck_picked, wreck_bonnet]
assert len(WRECK_ROLES) == WRECK_LOOKS


def car_role_plain(u, v, variant):
    if variant >= WRECK:
        return WRECK_ROLES[variant - WRECK](u, v)

    au, av = abs(u), abs(v)

    # Tyres stick out a little at the four corners.
    if 6.0 <= au <= 10.5 and 6.0 <= av <= 7.4:
        return V_DARK
    if au > HALF_LENGTH or av > HALF_WIDTH:
        return V_TRANS
    if au > 11.0 and av > 4.5 and (au - 11.0) ** 2 + (av - 4.5) ** 2 > 4.0:
        return V_TRANS

    base = V_BODY + (variant * 2)
    shade = base + 1

    if u > 11.4 and 2.6 <= av <= 5.6:
        return V_LAMP
    if u < -11.6 and 2.6 <= av <= 5.6:
        return V_BODY + 2                           # tail lights in the red body colour
    if av > 5.4:
        return shade
    if 2.4 <= u <= 5.6 and av <= 4.8:
        return V_GLASS                              # windscreen
    if -8.0 <= u <= -5.4 and av <= 4.8:
        return V_GLASS                              # rear window
    if -5.4 < u < 2.4:
        if av <= 4.6:
            return base                             # roof
        return V_GLASS                              # side windows
    return shade if (u > 5.6 and av > 4.2) or u < -8.0 else base


def car_frame(direction, variant, mossy=False):
    angle = direction * 2 * math.pi / DIRECTIONS
    cos_a, sin_a = math.cos(angle), math.sin(angle)
    grid = [[V_TRANS] * 32 for _ in range(32)]
    for y in range(32):
        for x in range(32):
            dx, dy = x - 15.5, y - 15.5
            grid[y][x] = car_role((dx * cos_a) + (dy * sin_a), (-dx * sin_a) + (dy * cos_a), variant, mossy)

    # A dark outline around the whole car.
    out = [row[:] for row in grid]
    for y in range(32):
        for x in range(32):
            if grid[y][x] != V_TRANS:
                continue
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < 32 and 0 <= ny < 32 and grid[ny][nx] != V_TRANS:
                    out[y][x] = V_OUTLINE
                    break
    return out


BIKE_FRAMES = [
    # side, facing right
    ['................',
     '................',
     '................',
     '.........55.....',
     '..........5.....',
     '...44.....5.....',
     '....5555555.....',
     '....5....5.5....',
     '..444...5..444..',
     '.4...4.5..4...4.',
     '.4.5.455..4.5.4.',
     '.4...4....4...4.',
     '..444......444..',
     '................',
     '................',
     '................'],
    # end on (riding up or down)
    ['................',
     '................',
     '.......44.......',
     '.......44.......',
     '....55555555....',
     '.......55.......',
     '.......55.......',
     '.......55.......',
     '......4444......',
     '.......55.......',
     '.......44.......',
     '.......44.......',
     '.......44.......',
     '.......44.......',
     '................',
     '................'],
]


def build_vehicles():
    rows = []
    for mossy in (False, True):
        for variant in range(WRECK + WRECK_LOOKS):
            for drawn in range(DRAWN):
                rows += car_frame((12 + drawn) % DIRECTIONS, variant, mossy)
    save_bmp('car', rows, VEHICLE_PALETTE, {'type': 'sprite', 'height': 32})

    bike = []
    for frame in BIKE_FRAMES:
        for row in frame:
            assert len(row) == 16
            bike.append([int(c) if c != '.' else 0 for c in row])
    save_bmp('bike', bike, VEHICLE_PALETTE, {'type': 'sprite', 'height': 16})

    lines = ['namespace kx::vehicle_frames', '{', '',
             '// graphics/car.bmp: per variant (body colours, then the wreck looks), %d headings from north round to south' % DRAWN,
             '// through east; the west half is the same frames mirrored. Then all of them again with moss on.',
             'constexpr int car_directions = %d;' % DIRECTIONS,
             'constexpr int car_drawn = %d;' % DRAWN,
             'constexpr int car_colours = %d;' % len(BODY_COLOURS),
             'constexpr int car_wreck = %d;' % WRECK,
             'constexpr int car_wreck_looks = %d;      // burnt (bonnet gone), picked clean, bonnet in the road' % WRECK_LOOKS,
             'constexpr int car_mossy = %d;            // variants from here on: the same, with moss' % (WRECK + WRECK_LOOKS),
             'constexpr const char* car_colour_names[] = { %s };' % ', '.join('"%s"' % c[0] for c in BODY_COLOURS),
             '',
             '// graphics/bike.bmp.',
             'constexpr int bike_side = 0;',
             'constexpr int bike_end_on = 1;', '', '}']
    write_header('kx_vehicle_frames.h', lines)
