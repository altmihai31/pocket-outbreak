"""Building templates (spec: Towns and buildings): hand-made tile maps with their containers, doors and windows,
drawn with the front door on the bottom edge. The generator turns each into world graphics for the four directions
its door can face (kx_buildings.h), so chunk generation only copies tiles.

Legend:
    .  not part of the building (the natural ground stays)
    B  wall          w  window (solid, see-through)
    o  wood floor    x  tile floor    c  concrete    _  pavement (forecourts)
    Furniture (solid; containers loot by type) on the template's floor:
    k  kitchen cabinet   R  fridge          D  wardrobe      h  bathroom cabinet   G  tool shelf   Y  bed (sleep)
    S  shop shelf        C  counter         L  locker        M  medicine cabinet   X  crate        K  desk
    P  fuel pump (on pavement)               V  gun safe      T  television (milestone 13)
Doorways are gaps in the walls. Every template fits a 14x14 lot with a tile to spare on each side.
Every tile but '.', '_' and 'P' is under the roof (canopy layer), which fades out while the player is inside, and a
paved step is added outside each doorway of the front wall. Playtest 1: outer doorways are under the roof too (they
used to be gaps in it, so doors stood out of the building); one at the eave gets an eave that dips over it.
"""
from common import write_header

MAX_SIZE = 12     # with the front step: a 14x14 lot keeps a tile of yard

# (name, floor, roof colour, rows)
TEMPLATES = [
    ('SMALL_HOUSE', 'o', 'RED', [
        'BBwBBBwBB',
        'BkkRBYooB',
        'BoooBooDB',
        'BoooooooB',
        'BBoBBBoBB',
        'BxxBoooTB',
        'BhxBooooB',
        'BBBBBoBBB',
    ]),
    ('LARGE_HOUSE', 'o', 'RED', [
        'BBBwBBBBwBBB',
        'BkkkRBYooDoB',
        'BooooBoooooB',
        'BooooooooooB',
        'BBoBBBBBoBBB',
        'BxxBooooooTB',
        'BhxBooooooow',
        'BBBBooooYoDB',
        'woooooooooow',
        'BBBBBoBBBBBB',
    ]),
    ('GARAGE', 'c', 'GREY', [
        'BBBwwBBB',
        'BGGGccGB',
        'BccccccB',
        'BccccccB',
        'BccccccB',
        'BccccccB',
        'BccccccB',
    ]),
    ('SHOP', 'x', 'GREY', [
        'BBBBBBBBBBB',
        'BSSSSxSSSSB',
        'BxxxxxxxxxB',
        'BSSSxxxSSSB',
        'BxxxxxxxxxB',
        'BxxxxxCCKxB',
        'BxxxxxxxxxB',
        'BwwBxxBwwwB',
    ]),
    ('PETROL_STATION', 'x', 'GREY', [
        '.BBBBBBBBB..',
        '.BSSxCKxSB..',
        '.BxxxxxxxB..',
        '.BwwxxBwwB..',
        '____________',
        '____________',
        '__P____P____',
        '____________',
        '__P____P____',
        '____________',
    ]),
    ('BARN', 'o', 'RED', [
        'BBBBBBBBBBBB',
        'BXooooooooXB',
        'BXooooooooGB',
        'BooooooooooB',
        'woooooooooow',
        'BooooooooooB',
        'BXooooooooXB',
        'BBBBooooBBBB',
    ]),
    ('POLICE_STATION', 'x', 'GREY', [
        'BBBwBBBBwBBB',
        'BLLLVBKooKoB',
        'BxxxxBoooooB',
        'BxxxxxoooooB',
        'BBBBxBBBoBBB',
        'BYxBxxxxxxxB',
        'BxxBxxxxxxxw',
        'BBxBxxCCxxxB',
        'wxxxxxxxxxxw',
        'BBBBBxxBBBBB',
    ]),
    ('CLINIC', 'x', 'GREY', [
        'BBwBBBBBBwBB',
        'BMMhBYxxYxMB',
        'BxxxBxxxxxxB',
        'BxxxxxxxxxxB',
        'BBxBBBBBxBBB',
        'BKxxxxxxxxCB',
        'wxxxxxxxxxxw',
        'BxxxxxxxxxxB',
        'BBBBxxxxBBBB',
    ]),
    # Milestone 14 (docs/milestone-14-plan.md stage 3): 25 more. Homes for villages and towns:
    ('COTTAGE', 'o', 'RED', [
        'BBwBBwBB',
        'BkkRBYoB',
        'BoooBoDB',
        'BooooooB',
        'BhoBBoTB',
        'BBBBBoBB',
    ]),
    ('BUNGALOW', 'o', 'GREY', [
        'BBwBBBBBwBB',
        'BkkRBYoBhxB',
        'BoooBoDBxxB',
        'BoZooooooxB',
        'BoTBBBBoooB',
        'BooooooooBB',
        'BBBBBoBBBBB',
    ]),
    ('TERRACED_PAIR', 'o', 'RED', [
        'BwBBBBBBBBwB',
        'BkRoDBBDoRkB',
        'BooooBBooooB',
        'BBoBBBBBBoBB',
        'BYohBBBBhoYB',
        'BooxBBBBxooB',
        'BBoBBBBBBoBB',
    ]),
    ('FLAT_BLOCK', 'o', 'GREY', [
        'BwwBBBBBBwwB',
        'BYokBBBBkoYB',
        'BooRBBBBRooB',
        'BBoBBooBBoBB',
        'BooooooooooB',
        'BBoBBooBBoBB',
        'BhoYBooBYohB',
        'BooTBooBTooB',
        'BBBBBooBBBBB',
    ]),
    ('TRAILER', 'o', 'GREY', [
        'BwBBBBBwB',
        'BkRhooYDB',
        'BoooooooB',
        'BBBBoBBBB',
    ]),
    ('FARMHOUSE', 'o', 'RED', [
        'BBwBBBBBwBB',
        'BkWkRBYooDB',
        'BooooBoooTB',
        'BoZoooooooB',
        'BBBBoBBBoBB',
        'BhxxoooooXB',
        'BxxBoooooGB',
        'BBBBBoBBBBB',
    ]),
    ('LARGE_FARMHOUSE', 'o', 'RED', [
        'BBBwBBBBwBBB',
        'BkkWRBYooDoB',
        'BooooBoooooB',
        'BZoZoooooooB',
        'BBoBBBBBoBBB',
        'BxxBoooYooDB',
        'BhxBooooooow',
        'BBBBoooooXGB',
        'woooooooooow',
        'BBBBBoBBBBBB',
    ]),
    ('HOLIDAY_CABIN', 'o', 'RED', [
        'BBwBBBwBB',
        'BkRWBYYDB',
        'BoooooooB',
        'BoZoBhxoB',
        'BBBoBBBBB',
    ]),
    # Village landmarks:
    ('PUB', 'o', 'RED', [
        'BBwBBBBBBwBB',
        'BRkkWoBtooxB',
        'BoooooBhxxxB',
        'BBBoBBBBoBBB',
        'BCCoCCooooZB',
        'BooooooooooB',
        'wZooZooZooZw',
        'BooooooooooB',
        'BBBBBooBBBBB',
    ]),
    ('POST_OFFICE', 'x', 'GREY', [
        'BBwBBBBwBB',
        'BXXFFBKoFB',
        'BxxxxxxxxB',
        'BCCCxCCCxB',
        'BxxxxxxxxB',
        'BwwBxxBwwB',
    ]),
    ('SMALL_SCHOOL', 'o', 'RED', [
        'BBwwBBwwBB',
        'BNNooKooFB',
        'BJoJoJoJoB',
        'BJoJoJoJoB',
        'BooooooooB',
        'BLLLooootB',
        'BBBBoBBBBB',
    ]),

    ('SCHOOL', 'o', 'GREY', [
        'BBwwwBBwwwBB',
        'BNNooKBNNoKB',
        'BJoJoJBJoJoB',
        'BoooooBooooB',
        'BBoBBBBBoBBB',
        'BoooooooooFB',
        'BLLLooooootB',
        'BBBBBooBBBBB',
    ]),
    ('SUPERMARKET', 'x', 'GREY', [
        'BBBBBBBBBBBB',
        'BffffSSSSSXB',
        'BxxxxxxxxxxB',
        'BSSSxSSSSxXB',
        'BxxxxxxxxxxB',
        'BSSSxSSSSxxB',
        'BxxxxxxxxxxB',
        'BqxqxxxxxxvB',
        'BxxxxxxxxxxB',
        'BwwBxxxBwwwB',
    ]),
    ('MOTEL', 'o', 'RED', [
        'BBBBBBBBBBBB',
        'BYDBBYDBBYDB',
        'BooBBooBBooB',
        'BhoBBhoBBhoB',
        'BBoBBBoBBBoB',
        '____________',
    ]),

    # Town landmarks:
    ('WAREHOUSE', 'c', 'GREY', [
        'BBBBBBBBBBBB',
        'BQQQQcQQQQXB',
        'BccccccccccB',
        'BQQQQcQQQQXB',
        'BccccccccccB',
        'BQQQQcQQQQcB',
        'BccccccccccB',
        'BFKccccccXXB',
        'BBBccccBBBBB',
    ]),
    ('FIRE_STATION', 'c', 'RED', [
        'BBBBwBBBBwBB',
        'BLLLLcBKFUUB',
        'BcccccBooooB',
        'BcccccooooTB',
        'BcccccBBBBBB',
        'BcccccGGGGcB',
        'BccccccccccB',
        'BccccccccccB',
        'BBccccccccBB',
    ]),
    ('PHARMACY', 'x', 'GREY', [
        'BBBBBBBBBB',
        'BMMMMMMMxB',
        'BxxxxxxxxB',
        'BCCCxCCCxB',
        'BxxxxxxxxB',
        'BSSxxxxSSB',
        'BxxxxxxxxB',
        'BwwBxxBwwB',
    ]),
    ('HARDWARE_STORE', 'c', 'GREY', [
        'BBBBBBBBBBB',
        'BGGGGcGGGXB',
        'BcccccccccB',
        'BSSScSSScXB',
        'BcccccccccB',
        'BcSScSSSccB',
        'BccCCccccQB',
        'BwwBcccBwwB',
    ]),
    ('DINER', 'x', 'RED', [
        'BBwwBBBBwwBB',
        'BRkWkfBtxxhB',
        'BxxxxxBxxxxB',
        'BBBxBBBBxBBB',
        'BCCxCCxxxxxB',
        'BxxxxxxxxxxB',
        'wZxxZxxZxxZw',
        'BBBBBxxBBBBB',
    ]),
    # The military base:
    ('BARRACKS', 'c', 'GREY', [
        'BBwBBBBBBwBB',
        'BULUULUULUUB',
        'BccccccccccB',
        'BccccccccccB',
        'BULUcLUcLUUB',
        'BccccccccccB',
        'BtItIccKFccB',
        'BBBBBccBBBBB',
    ]),
    ('ARMORY', 'c', 'GREY', [
        'BBBBBBBBBB',
        'BHHHHcHHHB',
        'BccccccccB',
        'BXXcVVcXXB',
        'BccccccccB',
        'BKFccccLLB',
        'BBBBBcBBBB',
    ]),
    ('MESS_HALL', 'x', 'GREY', [
        'BBwBBBBBBwBB',
        'BkkWWRfkkRkB',
        'BxxxxxxxxxxB',
        'BCCCCxCCCCCB',
        'BxxxxxxxxxxB',
        'BZxZxZxZxZxB',
        'BxxxxxxxxxxB',
        'BZxZxZxZxZxB',
        'BBBBBxxBBBBB',
    ]),
    # Farms and the wild:
    ('FARM_SHED', 'c', 'GREY', [
        'BBBBBBBB',
        'BGGXXOOB',
        'BccccccB',
        'BOccccXB',
        'BBccccBB',
    ]),
    ('STABLE', 'o', 'RED', [
        'BBBBBBBBBBBB',
        'BOoBOoBOoBXB',
        'BooBooBooBoB',
        'BBoBBoBBoBoB',
        'BooooooooooB',
        'BooooooooooB',
        'BOOooooooGXB',
        'BBBBoooooBBB',
    ]),
    ('HUNTERS_CABIN', 'o', 'RED', [
        'BBwBBwBB',
        'BHXoBYDB',
        'BoooBooB',
        'BWZooooB',
        'BBBoBBBB',
    ]),
    # The village set piece (milestone 14 stage 4): one on each village's landmark lot. Graves stand on the
    # natural ground behind it.
    ('CHURCH', 'x', 'GREY', [
        '.r..r..r..r.',
        '............',
        'BBwBBBBBBwBB',
        'BKxxxAAxxxxB',
        'BxxxxxxxxxxB',
        'BEEEExxEEEEB',
        'wxxxxxxxxxxw',
        'BEEEExxEEEEB',
        'BxxxxxxxxxxB',
        'BEEEExxEEEEB',
        'BBBBBxxBBBBB',
    ]),
]

# Milestone 14 set pieces placed whole: the village church (stage 4) is now in TEMPLATES.
TEMPLATES_14 = []

FURNITURE = {'k': 'KITCHEN_CABINET', 'R': 'FRIDGE', 'D': 'WARDROBE', 'h': 'BATH_CABINET', 'G': 'TOOL_SHELF',
             'Y': 'BED', 'S': 'SHOP_SHELF', 'C': 'COUNTER', 'L': 'LOCKER', 'M': 'MED_CABINET', 'X': 'CRATE',
             'K': 'DESK', 'P': 'PUMP', 'V': 'GUN_SAFE', 'T': 'TV'}
FLOORS = {'o': 'WOOD_FLOOR', 'x': 'TILE_FLOOR', 'c': 'CONCRETE', '_': 'PAVEMENT'}

# Milestone 14 furniture (tools/assets/world14.py).
FURNITURE.update({'E': 'PEW', 'A': 'ALTAR', 'J': 'SCHOOL_DESK', 'N': 'BLACKBOARD', 'U': 'BUNK_BED', 'Q': 'RACK',
                  'H': 'GUN_RACK', 'Z': 'TABLE', 'F': 'FILING_CABINET', 'O': 'HAY_BALE', 'W': 'STOVE', 'I': 'BATH',
                  'q': 'CHECKOUT', 'v': 'VENDING', 't': 'TOILET', 'f': 'CHEST_FREEZER'})
# Furniture standing on the natural ground outside the walls (no floor under it, no roof over it): graves.
OUTDOOR_FURNITURE = {'r': 'GRAVE'}
WALLS = 'Bw'

# Door sides, in kx::building_facing order: as drawn, then turned. Each comes as drawn and mirrored (the
# layout flipped left to right before turning), so a template has 8 layouts.
FACINGS = ['SOUTH', 'NORTH', 'EAST', 'WEST']


def mirrored(rows):
    return [row[::-1] for row in rows]


def turned(rows, facing):
    width = len(rows[0])
    if facing == 'SOUTH':
        return rows
    if facing == 'NORTH':
        return [row[::-1] for row in reversed(rows)]
    if facing == 'EAST':       # counter-clockwise: the bottom edge becomes the right one
        return [''.join(row[x] for row in rows) for x in reversed(range(width))]
    return [''.join(row[x] for row in reversed(rows)) for x in range(width)]     # clockwise: bottom to left


UNROOFED = '._Pr'
OUTSIDE = '._Pr'
DOOR_FLOORS = 'oxc'


def with_front_step(rows):
    """The template with a row of paved steps below its front doorways."""
    return rows + [''.join('_' if c in DOOR_FLOORS else '.' for c in rows[-1])]


def outside_door(rows, x, y):
    """A floor tile next to the outside: a doorway in an outer wall."""
    if rows[y][x] not in DOOR_FLOORS:
        return False
    for dx, dy in ((0, -1), (1, 0), (0, 1), (-1, 0)):
        nx, ny = x + dx, y + dy
        if not (0 <= ny < len(rows) and 0 <= nx < len(rows[0])) or rows[ny][nx] in OUTSIDE:
            return True
    return False


# Roof details: a chimney on the upper slope of homes, a vent (red roofs) or skylight (grey) on the lower slope, and
# moss or stains every few tiles.
CHIMNEY_TEMPLATES = ('SMALL_HOUSE', 'LARGE_HOUSE')
WEATHER_EVERY = 9


def roof_part(roofed, ridge, x, y, name):
    """Which roof graphic a roofed tile gets."""
    width = len(roofed[0])
    roofed_below = y + 1 < len(roofed) and roofed[y + 1][x]
    part = 'EAVE' if not roofed_below else 'UPPER' if y < ridge else 'RIDGE' if y == ridge else 'LOWER'
    columns = [i for i in range(width) if roofed[y][i]]
    left, right = columns[0], columns[-1]
    if part != 'RIDGE' and (x == left or x == right):
        return part + ('_L' if x == left else '_R')
    if part == 'UPPER' and name in CHIMNEY_TEMPLATES and y == ridge - 1 and x == left + 2:
        return 'CHIMNEY'
    if part == 'LOWER' and y == ridge + 1 and x == right - 2:
        return 'VENT'
    if part == 'UPPER' and (x * 5 + y * 3 + len(name)) % WEATHER_EVERY == 0:
        return 'WEATHER'
    return part


def door_side(rows, x, y):
    """A doorway gets a door (milestone 12.7): a floor tile with walls left and right ('FACE', in a front or back
    wall) or above and below ('TOP', in a side wall); wider openings (garages, shops, barns) stay open."""
    if rows[y][x] not in DOOR_FLOORS:
        return None

    def wall(wx, wy):
        return 0 <= wy < len(rows) and 0 <= wx < len(rows[0]) and rows[wy][wx] in WALLS

    # In a wall running across: walls left and right, open above and below. In one running down: walls above and
    # below, open left and right. Either way at least one jamb is a plain stretch of that wall (no wall meeting it
    # from the side): a gap beside a corner or between two walls running the other way is a corridor, not a door.
    def plain_across(wx, wy):
        return wall(wx, wy) and not wall(wx, wy - 1) and not wall(wx, wy + 1)

    def plain_down(wx, wy):
        return wall(wx, wy) and not wall(wx - 1, wy) and not wall(wx + 1, wy)

    if wall(x - 1, y) and wall(x + 1, y) and not wall(x, y - 1) and not wall(x, y + 1) and             (plain_across(x - 1, y) or plain_across(x + 1, y)):
        return 'FACE'
    if wall(x, y - 1) and wall(x, y + 1) and not wall(x - 1, y) and not wall(x + 1, y) and             (plain_down(x, y - 1) or plain_down(x, y + 1)):
        return 'TOP'
    return None


def tile_graphics(rows, floor, roof, ids, name):
    """Ground, wall and roof graphic ids per tile, row-major; ground 0 keeps the natural ground."""
    ground = []
    walls = []
    roofs = []
    roofed = [[c not in UNROOFED for c in row] for row in rows]
    roofed_rows = [y for y, row in enumerate(roofed) if any(row)]
    ridge = (roofed_rows[0] + roofed_rows[-1]) // 2
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            below = rows[y + 1][x] if y + 1 < len(rows) else '.'
            part = roof_part(roofed, ridge, x, y, name) if roofed[y][x] else None
            if part == 'EAVE' and outside_door(rows, x, y):
                part = 'EAVE_DOOR'
            roofs.append(ids['ROOF_%s_%s' % (roof, part)] if part else 0)
            if c == '.':
                ground.append(0)
                walls.append(0)
            elif c in OUTDOOR_FURNITURE:
                ground.append(0)
                walls.append(ids[OUTDOOR_FURNITURE[c]])
            elif c in WALLS:
                ground.append(ids[FLOORS[floor]])
                wall_below = below in WALLS
                if c == 'B':
                    walls.append(ids['WALL_TOP' if wall_below else 'WALL_FACE'])
                else:
                    walls.append(ids['WINDOW_TOP' if wall_below else 'WINDOW_FACE'])
            elif c in FLOORS:
                ground.append(ids[FLOORS[c]])
                side = door_side(rows, x, y)
                walls.append(ids['DOOR_%s_CLOSED' % side] if side else 0)
            else:
                ground.append(ids['PAVEMENT' if c == 'P' else FLOORS[floor]])
                walls.append(ids[FURNITURE[c]])
    return ground, walls, roofs


# Furniture that may stand out of reach (decoration: nothing to search or sleep in).
DECORATION = set('NEAZWTtIO')


def check_reachable():
    """Milestone 14 stage 7: every template's searchable furniture and beds can be walked up to from its doors (a
    flood over the floor from the doorways on its edge). Several stage 3 templates had rooms shut off."""
    for name, _, _, rows in TEMPLATES:
        h, w = len(rows), len(rows[0])

        def walk(y, x):
            return 0 <= y < h and 0 <= x < w and rows[y][x] in FLOORS

        seen = set()
        stack = [(y, x) for y in range(h) for x in range(w) if walk(y, x) and (y in (0, h - 1) or x in (0, w - 1))]
        while stack:
            y, x = stack.pop()
            if (y, x) in seen:
                continue
            seen.add((y, x))
            stack += [(y + dy, x + dx) for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)) if walk(y + dy, x + dx)]
        for y in range(h):
            for x in range(w):
                c = rows[y][x]
                if c in FURNITURE and c not in DECORATION:
                    assert any((y + dy, x + dx) in seen for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1))),                         '%s: %s at row %d, column %d cannot be reached' % (name, FURNITURE[c], y, x)


def write_buildings_header(gfx_names):
    check_reachable()
    ids = {name: index for index, name in enumerate(gfx_names)}
    shapes = []
    ground = []
    walls = []
    roofs = []

    for name, floor, roof, rows in TEMPLATES:
        width = len(rows[0])
        assert all(len(row) == width for row in rows), name
        assert width <= MAX_SIZE and len(rows) < MAX_SIZE, name
        row_shapes = []
        for mirror, facing in [(m, f) for m in (False, True) for f in FACINGS]:
            drawn = mirrored(rows) if mirror else rows
            grid = turned(with_front_step(drawn), facing)
            tile_ground, tile_walls, tile_roofs = tile_graphics(grid, floor, roof, ids, name)
            width_now = len(grid[0])
            roofed = [(i % width_now, i // width_now) for i, value in enumerate(tile_roofs) if value]
            roof_left = min(x for x, _ in roofed)
            roof_top = min(y for _, y in roofed)
            roof_right = max(x for x, _ in roofed) + 1
            roof_bottom = max(y for _, y in roofed) + 1
            row_shapes.append((width_now, len(grid), len(ground), roof_left, roof_top, roof_right - roof_left,
                               roof_bottom - roof_top))
            ground += tile_ground
            walls += tile_walls
            roofs += tile_roofs
        shapes.append((name, row_shapes))

    lines = ['#include <cstdint>', '', 'namespace kx', '{', '',
             '// Building templates (tools/assets/buildings.py).',
             'enum class building : uint8_t', '{']
    lines += ['    %s,' % name for name, _, _, _ in TEMPLATES]
    lines += ['    COUNT', '};', '',
              '// Names, for logs.',
              'constexpr const char* building_names[] = { %s };' % ', '.join('"%s"' % name for name, _, _, _ in TEMPLATES),
              '',
              '// Which side the front door faces.',
              'enum class building_facing : uint8_t', '{']
    lines += ['    %s,' % facing for facing in FACINGS]
    lines += ['    COUNT', '};', '',
              'struct building_shape', '{', '    uint8_t width;', '    uint8_t height;',
              '    uint16_t offset;         // into building_ground, building_walls and building_roofs, row-major',
              '    uint8_t roof_left;       // the roofed part (the roof fades over just this rectangle)',
              '    uint8_t roof_top;', '    uint8_t roof_width;', '    uint8_t roof_height;',
              '};', '',
              'constexpr int building_max_size = %d;' % MAX_SIZE, '',
              '// Per template: the 4 facings, then the same mirrored.',
              'constexpr int building_layouts = %d;' % (2 * len(FACINGS)),
              'constexpr building_shape building_shapes[][building_layouts] = {']
    for name, row_shapes in shapes:
        lines.append('    { ' + ', '.join('{ %d, %d, %d, %d, %d, %d, %d }' % shape for shape in row_shapes) +
                     ' },   // ' + name)
    lines += ['};', '',
              '// World graphics per building tile; ground 0 keeps the natural ground (outside the walls).',
              'constexpr uint8_t building_ground[] = {']
    lines += ['    ' + ', '.join(str(v) for v in ground[i:i + 24]) + ',' for i in range(0, len(ground), 24)]
    lines += ['};', '', 'constexpr uint8_t building_walls[] = {']
    lines += ['    ' + ', '.join(str(v) for v in walls[i:i + 24]) + ',' for i in range(0, len(walls), 24)]
    lines += ['};', '', '// Canopy layer: the roof over each tile (0 for forecourts and other open parts).',
              'constexpr uint8_t building_roofs[] = {']
    lines += ['    ' + ', '.join(str(v) for v in roofs[i:i + 24]) + ',' for i in range(0, len(roofs), 24)]
    lines += ['};', '', '}']
    write_header('kx_buildings.h', lines)
