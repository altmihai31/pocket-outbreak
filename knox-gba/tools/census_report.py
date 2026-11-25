"""Loot census report (milestone 14 stage 7): reads a bench 14 log (knox_towns.gba, the KXCENSUS lines) and prints
what each kind of building holds on average, then checks the written targets (docs/milestone-14-plan.md, Balancing).

Usage: python tools/census_report.py <mgba log>

The census rolls every container of every building on the fixed seed once, so it also logs what the tables give on
average (the x... fields, in thousandths); the targets are checked on those, and the single roll is shown beside them.
"""
import re
import sys

# A day of food is about 33 points (food drain 25 a minute awake, half asleep, a little more on the move).
DAY_FOOD = 33

SETTLEMENT_HOMES = ['SMALL_HOUSE', 'LARGE_HOUSE', 'COTTAGE', 'BUNGALOW', 'TERRACED_PAIR', 'FLAT_BLOCK', 'TRAILER',
                    'FARMHOUSE', 'LARGE_FARMHOUSE']
CABINS = ['HOLIDAY_CABIN', 'HUNTERS_CABIN']

TARGETS = [
    # (name, value function, low, high, text)
    ('kitchen food per house', lambda g: g['house']['xkitchen'] / 1000 / g['house']['n'], DAY_FOOD * 1.2, DAY_FOOD * 2.0,
     "a house's kitchen feeds you about a day and a half (40-66 food points)"),
    ('loose guns per home', lambda g: g['home']['xguns'] / 1000 / g['home']['n'], 1 / 35, 1 / 18,
     'a gun about every 25 homes (safes not counted)'),
    ('ammunition per home', lambda g: g['home']['xammo'] / 1000 / g['home']['n'], 1 / 12, 1.0,
     'ammunition more often than guns: at least one find in 12 homes'),
    ('guns per base building', lambda g: g['base']['xguns'] / 1000 / g['base']['n'], 0.5, 1.5,
     'the military base is worth the risk, not an arsenal: half a gun to one and a half a building'),
    ('ammunition per base building', lambda g: g['base']['xammo'] / 1000 / g['base']['n'], 1.5, 99,
     'and one and a half ammunition finds or more a building'),
]


def parse(path):
    kinds, zones = {}, {}
    with open(path, 'rb') as f:
        for raw in f:
            line = raw.decode('latin-1')
            if 'KXCENSUS' not in line or 'done' in line:
                continue
            # Each building kind or zone comes in two lines (KXCENSUS, then KXCENSUS+).
            match = re.search(r'KXCENSUS\+? (kind|zone) (\S+) (.*)', line)
            if not match:
                continue
            values = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', match.group(3))}
            (kinds if match.group(1) == 'kind' else zones).setdefault(match.group(2), {}).update(values)
    return kinds, zones


def total(kinds, names):
    result = {}
    for name in names:
        for key, value in kinds.get(name, {}).items():
            result[key] = result.get(key, 0) + value
    return result


def main():
    kinds, zones = parse(sys.argv[1])
    if not kinds:
        print('no KXCENSUS lines')
        return 1

    print('%-16s %4s %5s %6s %8s %8s %6s %6s %6s %6s' % ('building', 'n', 'cont', 'items', 'food', 'kitchen', 'guns',
                                                         'ammo', 'med', 'kg'))
    for name, v in kinds.items():
        n = v['n']
        print('%-16s %4d %5.1f %6.1f %4.0f/%-3.0f %4.0f/%-3.0f %6.2f %6.2f %6.1f %6.1f' % (
            name, n, v['cont'] / n, v['items'] / n, v['food'] / n, v['xfood'] / 1000 / n, v['kitchen'] / n,
            v['xkitchen'] / 1000 / n, v['xguns'] / 1000 / n, v['xammo'] / 1000 / n, v['med'] / n, v['w'] / 100 / n))
    print('(food and kitchen: this roll / expected; guns and ammunition: expected per building)')

    groups = {'house': total(kinds, SETTLEMENT_HOMES), 'home': total(kinds, SETTLEMENT_HOMES + CABINS),
              'base': zones.get('MILITARY', {'n': 0})}
    print()
    print('homes %d (houses %d, cabins %d), military base buildings %d' % (
        groups['home']['n'], groups['house']['n'], groups['home']['n'] - groups['house']['n'], groups['base']['n']))
    failed = 0
    for name, value, low, high, text in TARGETS:
        got = value(groups)
        ok = low <= got <= high
        failed += not ok
        print('%-4s %-30s %7.3f  (%s; %.3f-%s)' % ('ok' if ok else 'FAIL', name, got, text, low,
                                                  '%.3f' % high if high < 99 else 'up'))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
