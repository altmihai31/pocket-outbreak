"""Wrecked car options: the current burnt-out wreck next to three cleaner looks, each drawn with the shared vehicle
palette (all 16 colours are taken, so no new ones), on a road at game scale, saved at 3x.

    python tools/mockup_wreck.py        -> mockups/wreck-options.png, -2.png, -3.png

A. Burnt shell: charred body in two flat tones, rust only round the edges and wheel arches, the windows black
   holes, the tyres burnt down to rims.
B. Rusted and faded: grey faded paint, two big rust patches, a cracked windscreen, flat tyres.
C. Crashed: the front crumpled short with a fold across the bonnet, faded beige paint, a starred windscreen,
   glass on the ground in front.

The second sheet, beside A:
D. Overturned: on its roof, the underside up (chassis rails, axles, exhaust, the engine's sump), red paint showing
   along the sides.
E. Stripped: wheels gone (it sits on its arches), a door swung open, the bonnet up over a bare engine bay, blue
   paint.
F. Long abandoned: faded cream paint under moss on the roof and bonnet, grimy windows, flat tyres.

The third sheet, parts taken off (the bonnet, doors, the boot lid, a wheel):
G. Burnt, bonnet gone: A's shell with the engine bay open (engine block, radiator, battery).
H. Doors gone: faded blue, both doors on one side off (the sill and the seats show in the gap), the bonnet gone.
I. Picked clean: faded red, the bonnet, the boot lid (a spare wheel inside), the doors on one side and a front
   wheel gone.
J. Bonnet in the road: cream, its bonnet lying beside it, a door off, the windscreen smashed.
"""
import math
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'assets'))

from mockup_character import ACCENT, QUIET, Screen  # noqa: E402
import vehicles  # noqa: E402
import world  # noqa: E402
from vehicles import (HALF_LENGTH, HALF_WIDTH, V_DARK, V_GLASS, V_LIGHT, V_MID, V_OUTLINE, V_RUST,  # noqa: E402
                      V_TRANS)

ROOT = os.path.join(HERE, '..', 'mockups')
PALETTE = vehicles.VEHICLE_PALETTE
CREAM_SHADE = vehicles.V_BODY + 5           # the cream body's shade: a faded beige


def shape(u, v, half_length=HALF_LENGTH, front=HALF_LENGTH, jagged=False):
    """The car's outline (as vehicles.car_role): None outside, else 'tyre' or 'body'."""
    au, av = abs(u), abs(v)
    if 6.0 <= au <= 10.5 and 6.0 <= av <= 7.4 and not (jagged and u > 0 and v < 0):
        return 'tyre'
    limit = front if u > 0 else half_length

    # A crumpled front: pushed in further on one side, its edge buckled.
    if jagged and u > 0:
        limit = front - (1.2 if v < 0 else 0.0) - (0.8 if int(v + 10) % 3 == 0 else 0.0)
    if au > limit or av > HALF_WIDTH:
        return None
    end = limit - 2.0
    if au > end and av > 4.5 and (au - end) ** 2 + (av - 4.5) ** 2 > 4.0:
        return None
    return 'body'


def zone(u, v):
    """Which part of the body a point is on."""
    av = abs(v)
    if 2.4 <= u <= 5.6 and av <= 4.8:
        return 'windscreen'
    if -8.0 <= u <= -5.4 and av <= 4.8:
        return 'rear window'
    if -5.4 < u < 2.4:
        return 'roof' if av <= 4.6 else 'side window'
    return 'bonnet' if u > 0 else 'boot'


def option_a(u, v):
    part = shape(u, v)
    if part is None:
        return V_TRANS
    if part == 'tyre':
        # Burnt down to the rims: a thin grey ring.
        return V_MID if abs(abs(u) - 8.25) < 1.0 and abs(v) > 6.6 else V_OUTLINE
    au, av = abs(u), abs(v)
    where = zone(u, v)
    if where in ('windscreen', 'rear window', 'side window'):
        return V_OUTLINE
    # Rust round the edges and the wheel arches; soot elsewhere, the roof a shade lighter.
    if av > 5.2 or au > 11.2 or (6.0 <= au <= 10.5 and av > 4.4):
        return V_RUST
    return V_MID if where == 'roof' else V_DARK


def option_b(u, v):
    part = shape(u, v)
    if part is None:
        return V_TRANS
    if part == 'tyre':
        return V_DARK
    au, av = abs(u), abs(v)
    where = zone(u, v)
    if where == 'windscreen':
        # Cracked: the glass with pale lines from a point on the driver's side.
        dx, dy = u - 4.0, v + 2.0
        crack = abs(dy - dx * 1.6) < 0.7 or abs(dy + dx * 0.7) < 0.7 or (abs(dx) < 0.5 and dy > 0)
        return V_LIGHT if crack else V_GLASS
    if where in ('rear window', 'side window'):
        return V_OUTLINE
    # Two big rust patches (bonnet and a rear quarter), the rest faded grey paint, darker along the sides.
    if (u - 8.0) ** 2 / 6.0 + (v + 1.5) ** 2 / 3.5 <= 1.0 or (u + 9.0) ** 2 / 5.0 + (v - 3.0) ** 2 / 4.0 <= 1.0:
        return V_RUST
    if av > 5.4 or (6.0 <= au <= 10.5 and av > 4.6):
        return V_DARK
    return V_LIGHT if where == 'roof' else V_MID


def option_c(u, v):
    front = 10.5                    # crumpled short, more on the left
    part = shape(u, v, front=front, jagged=True)
    if part is None:
        # Glass in the road ahead of the crumpled front.
        for gu, gv in ((12.5, -3.0), (13.5, 1.0), (12.0, 3.5), (14.5, -1.0)):
            if abs(u - gu) < 0.6 and abs(v - gv) < 0.6:
                return V_GLASS
        return V_TRANS
    if part == 'tyre':
        return V_DARK
    au, av = abs(u), abs(v)
    where = zone(u, v)
    if where == 'windscreen':
        # Starred where a head hit it.
        dx, dy = u - 4.2, v - 1.5
        star = (dx * dx + dy * dy < 1.3) or (abs(dx) < 0.35 and abs(dy) < 2.6) or (abs(dy) < 0.35 and abs(dx) < 2.4)
        return V_LIGHT if star else V_GLASS
    if where in ('rear window', 'side window'):
        return V_OUTLINE
    # The bonnet folded across, rust where the paint cracked; faded beige elsewhere.
    if u > 5.6 and abs(u - 7.6 - 0.35 * v) < 0.5:
        return V_OUTLINE
    if u > 8.0:
        return V_RUST if abs(v) < 3.5 and int(v + 10) % 2 else V_DARK
    if av > 5.4 or (u < -8.0):
        return V_MID
    return CREAM_SHADE


RED, RED_SHADE = vehicles.V_BODY + 2, vehicles.V_BODY + 3
BLUE, BLUE_SHADE = vehicles.V_BODY, vehicles.V_BODY + 1
CREAM = vehicles.V_BODY + 4
MOSS, MOSS_DARK = vehicles.V_BODY + 6, vehicles.V_BODY + 7


def option_d(u, v):
    part = shape(u, v)
    if part is None:
        return V_TRANS
    au, av = abs(u), abs(v)
    if part == 'tyre':
        # Wheels in the air: grey tread round a pale hub.
        return V_LIGHT if abs(au - 8.25) < 0.7 and av < 7.0 else V_MID
    # Paint shows only along the sides; the rest is the underside.
    if av > 5.4:
        return RED_SHADE
    if abs(au - 8.25) < 0.6:
        return V_MID                                # axles
    if abs(av - 3.2) < 0.55:
        return V_OUTLINE                            # chassis rails
    if abs(v + 1.0) < 0.5 and -12.0 < u < 3.0:
        return V_LIGHT                              # exhaust
    if 4.0 < u < 7.0 and av < 2.4:
        return V_MID                                # sump
    if -12.0 < u < -10.0 and abs(v + 1.0) < 1.4:
        return V_RUST                               # silencer
    return V_DARK


def option_e(u, v):
    au, av = abs(u), abs(v)

    # A door swung open on the right-hand side, hinged at its front edge: a panel angled out and back.
    t = v - 6.6
    if 0.0 < t < 3.6 and -1.8 - t * 0.9 < u < 1.2 - t * 0.9:
        return V_GLASS if u > 0.0 - t * 0.9 else BLUE_SHADE
    part = shape(u, v)
    if part is None:
        return V_TRANS
    if part == 'tyre':
        # No wheels: the arches are empty.
        return V_TRANS
    where = zone(u, v)
    if u > 6.0:
        # The bonnet up (a lighter panel at the very front, seen edge on) over a bare engine bay.
        if u > 11.0:
            return BLUE
        return V_MID if (int(u) + int(v + 10)) % 3 else V_DARK
    if where == 'windscreen':
        return V_GLASS
    if where in ('rear window', 'side window'):
        return V_OUTLINE if (where == 'side window' and v > 0) else V_GLASS
    if 6.0 <= au <= 10.5 and av > 4.6:
        return V_OUTLINE                            # empty wheel arches
    if av > 5.4:
        return BLUE_SHADE
    return BLUE


def option_f(u, v):
    part = shape(u, v)
    if part is None:
        return V_TRANS
    au, av = abs(u), abs(v)
    if part == 'tyre':
        return V_DARK
    where = zone(u, v)

    # Moss on the roof and the bonnet, in soft clumps.
    clump = (u - 0.0) ** 2 / 9.0 + (v - 1.0) ** 2 / 6.0 <= 1.0 or (u - 8.5) ** 2 / 3.0 + (v + 2.0) ** 2 / 3.0 <= 1.0         or (u + 3.5) ** 2 / 2.0 + (v + 3.0) ** 2 / 1.5 <= 1.0
    if clump and where in ('roof', 'bonnet'):
        inner = (u - 0.0) ** 2 / 5.0 + (v - 1.0) ** 2 / 3.0 <= 1.0 or (u - 8.5) ** 2 / 1.2 + (v + 2.0) ** 2 / 1.2 <= 1.0
        return MOSS if inner else MOSS_DARK
    if where in ('windscreen', 'rear window', 'side window'):
        return V_MID                                # grime
    if av > 5.4:
        return CREAM_SHADE
    if au > 11.4:
        return V_RUST
    return CREAM


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


def option_g(u, v):
    return vehicles.wreck_burnt(u, v)


def option_h(u, v):
    """H (not used in the game): faded blue, the doors on one side gone, the bonnet gone."""
    part = vehicles.wreck_shape(u, v)
    if part is None:
        return V_TRANS
    if part == 'tyre':
        return V_DARK
    door = vehicles.doors_off(u, v, 1)
    if door is not None:
        return door
    if u > 5.6:
        bay = vehicles.engine_bay(u, v)
        if bay is not None:
            return bay
    where = vehicles.wreck_zone(u, v)
    if where in ('windscreen', 'rear window', 'side window'):
        return V_OUTLINE if where == 'side window' else V_GLASS
    return BLUE_SHADE if abs(v) > 5.4 else BLUE


def option_i(u, v):
    return vehicles.wreck_picked(u, v)


def option_j(u, v):
    return vehicles.wreck_bonnet(u, v)


def current(u, v):
    """The wreck before milestone 12.7's change: dark metal with rust patches picked pixel by pixel."""
    au, av = abs(u), abs(v)
    if 6.0 <= au <= 10.5 and 6.0 <= av <= 7.4:
        return V_DARK
    if au > HALF_LENGTH or av > HALF_WIDTH:
        return V_TRANS
    if au > 11.0 and av > 4.5 and (au - 11.0) ** 2 + (av - 4.5) ** 2 > 4.0:
        return V_TRANS
    patch = (int(u * 0.9 + 40) * 7 + int(v * 0.9 + 40) * 13 + int(u * v * 0.2 + 400)) % 5
    base = V_RUST if patch < 2 else V_DARK
    shade = V_DARK if patch < 3 else V_RUST
    if (u > 11.4 or u < -11.6) and 2.6 <= av <= 5.6:
        return V_MID
    if av > 5.4:
        return shade
    if (2.4 <= u <= 5.6 or -8.0 <= u <= -5.4) and av <= 4.8:
        return V_OUTLINE
    if -5.4 < u < 2.4:
        return base if av <= 4.6 else V_OUTLINE
    return shade if (u > 5.6 and av > 4.2) or u < -8.0 else base


def frame(role, direction):
    angle = direction * 2 * math.pi / vehicles.DIRECTIONS
    cos_a, sin_a = math.cos(angle), math.sin(angle)
    grid = [[V_TRANS] * 32 for _ in range(32)]
    for y in range(32):
        for x in range(32):
            dx, dy = x - 15.5, y - 15.5
            grid[y][x] = role((dx * cos_a) + (dy * sin_a), (-dx * sin_a) + (dy * cos_a))
    out = [row[:] for row in grid]
    for y in range(32):
        for x in range(32):
            if grid[y][x] != V_TRANS:
                continue
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < 32 and 0 <= ny < 32 and grid[ny][nx] not in (V_TRANS, V_GLASS):
                    out[y][x] = V_OUTLINE
                    break
    return out


def paste(s, grid, left, top):
    for y in range(32):
        for x in range(32):
            if grid[y][x]:
                s.pixels[left + x, top + y] = PALETTE[grid[y][x]]


def road_panel(role):
    """A 112x80 stretch of road with pavement, the wreck at three headings."""
    s = Screen()
    gfx, _, _ = world.world_graphics()
    grids = {name: grid for name, grid, _ in gfx}
    for ty in range(5):
        for tx in range(7):
            name = 'PAVEMENT' if ty == 0 else 'ROAD_LINE_H' if ty == 3 and tx % 2 else 'ROAD'
            grid = grids[name]
            for y in range(16):
                for x in range(16):
                    s.pixels[tx * 16 + x, ty * 16 + y] = world.WORLD_PALETTE[grid[y][x]]
    paste(s, frame(role, 0), 4, 18)          # east
    paste(s, frame(role, 3), 40, 26)         # south-east-ish
    paste(s, frame(role, 12), 78, 34)        # north
    return s.image.crop((0, 0, 112, 80))


def render(options, name):
    width = 112
    gap = 4
    close_height = 70
    sheet = Image.new('RGB', (len(options) * (width + gap) - gap, 80 + close_height + 22), (0x05, 0x07, 0x0b))
    for index, (role, title, line) in enumerate(options):
        x = index * (width + gap)
        sheet.paste(road_panel(role), (x, 0))

        # Close up: the car heading east at twice the size, on the road.
        close = Screen()
        close.image = Image.new('RGB', (34, 34), world.WORLD_PALETTE[world.W_ROAD])
        close.pixels = close.image.load()
        paste(close, frame(role, 0), 1, 1)
        sheet.paste(close.image.resize((68, 68), Image.NEAREST), (x + (width - 68) // 2, 81))

        caption = Screen()
        caption.image = Image.new('RGB', (width, 22), (0x05, 0x07, 0x0b))
        caption.pixels = caption.image.load()
        caption.text(3, 3, title, ACCENT)
        caption.text(3, 11, line, QUIET)
        sheet.paste(caption.image, (x, 80 + close_height))
    sheet = sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST)
    sheet.save(os.path.join(ROOT, name))
    print('wrote mockups/' + name)


def main():
    render([
        (current, 'BEFORE', 'NOISY RUST AND SOOT'),
        (option_a, 'A  BURNT SHELL', 'SOOT, RUSTED EDGES, RIMS'),
        (option_b, 'B  RUSTED, FADED', 'GREY PAINT, RUST, CRACKED'),
        (option_c, 'C  CRASHED', 'CRUMPLED FRONT, GLASS'),
    ], 'wreck-options.png')
    render([
        (option_a, 'A  BURNT SHELL', 'FOR COMPARISON'),
        (option_d, 'D  OVERTURNED', 'ON ITS ROOF'),
        (option_e, 'E  STRIPPED', 'NO WHEELS, BONNET UP'),
        (option_f, 'F  LONG ABANDONED', 'MOSS, GRIME, FLAT TYRES'),
    ], 'wreck-options-2.png')
    render([
        (option_g, 'G  BURNT, NO BONNET', 'ENGINE BAY OPEN'),
        (option_h, 'H  DOORS GONE', 'SEATS SHOW, NO BONNET'),
        (option_i, 'I  PICKED CLEAN', 'BONNET, BOOT, DOORS'),
        (option_j, 'J  BONNET IN ROAD', 'DOOR OFF, GLASS GONE'),
    ], 'wreck-options-3.png')


if __name__ == '__main__':
    main()
