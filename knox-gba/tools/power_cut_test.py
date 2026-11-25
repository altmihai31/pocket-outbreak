"""Runs the power-cut bench (knox_cut.gba, BENCH=17) headless in mGBA and fails if a cut-off save loses the survivor.

    python tools/power_cut_test.py knox_cut.gba [--mgba PATH] [--timeout SECONDS] [--log FILE]
    python tools/power_cut_test.py --check-log kx.log     (only check a log made earlier)

The bench writes two complete saves, then cuts the power after every 61 bytes of a third and loads what is left, for
world blocks of several sizes, filled with a pattern that packs well and with noise that doesn't (playtest 6: world
blocks are stored packed, so "stored" is what the block took in SRAM). For each case it logs:

    KXCUT world=<bytes> <pattern|noise> stored=<bytes> cuts=<n> same=<n>
    KXCUT other_world=<n> world_lost=<n> lost=<n> wrong=<n>

same: the survivor and the world came back from the same save. other_world: from different saves. world_lost: the
survivor came back, the world didn't (a fresh world from the seed). lost: no survivor. wrong: a survivor that doesn't
check out (broken data that loaded).

Passes when no cut loses or breaks the survivor, and every cut keeps the whole save while the stored world block fits
the two-copy limit (half the world area, 14,320 bytes).

Exit code 0 when everything holds, 1 with a list of what didn't.
"""

import argparse
import os
import re
import shutil
import sys

from save_roundtrip_test import read_lines, run_bench, DONE

TWO_COPY_LIMIT = 14320
HEAD = re.compile(r'^KXCUT world=(\d+) (pattern|noise) stored=(\d+) cuts=(\d+) same=(\d+)$')
TAIL = re.compile(r'^KXCUT other_world=(\d+) world_lost=(\d+) lost=(\d+) wrong=(\d+)$')


def check(lines):
    failures = []
    results = []

    if DONE not in lines:
        failures.append('the bench never logged "%s" (it crashed, hung or ran out of time)' % DONE)

    cut_lines = [line for line in lines if line.startswith('KXCUT ') and ' at ' not in line]

    for head, tail in zip(cut_lines[0::2], cut_lines[1::2]):
        top = HEAD.match(head)
        bottom = TAIL.match(tail)

        if not top or not bottom:
            failures.append('unreadable lines:\n    %s\n    %s' % (head, tail))
            continue

        world, fill, stored, cuts, same = (value if index == 1 else int(value)
                                           for index, value in enumerate(top.groups()))
        other, world_lost, lost, wrong = (int(value) for value in bottom.groups())
        name = 'world %d bytes (%s, stored %d)' % (world, fill, stored)
        results.append((name, cuts, same, other, world_lost, lost, wrong))

        if lost or wrong:
            failures.append('%s: %d cuts lost the survivor, %d loaded a broken one' % (name, lost, wrong))

        if stored <= TWO_COPY_LIMIT and same != cuts:
            failures.append('%s fits two copies: only %d of %d cuts kept the whole save' % (name, same, cuts))

    if not results:
        failures.append('no KXCUT results in the log')

    for line in lines:
        if line.startswith('KXCUT wrong at '):
            failures.append(line)

    return results, failures


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('rom', nargs='?', help='knox_cut.gba (make BENCH=17)')
    parser.add_argument('--mgba', default=shutil.which('mgba') or '/usr/games/mgba', help='mGBA SDL binary')
    parser.add_argument('--timeout', type=float, default=180, help='seconds before giving up')
    parser.add_argument('--log', default='knox_cut.log', help='where mGBA writes its log')
    parser.add_argument('--check-log', help='check this log instead of running the ROM')
    args = parser.parse_args()

    if args.check_log:
        log_path = args.check_log
    elif args.rom:
        log_path = args.log

        if os.path.exists(log_path):
            os.remove(log_path)

        run_bench(args.rom, args.mgba, args.timeout, log_path)
    else:
        parser.error('give the ROM or --check-log')

    results, failures = check(read_lines(log_path))

    for name, cuts, same, other, world_lost, lost, wrong in results:
        print('%-42s %3d cuts, %3d whole save, %3d mixed, %3d world lost, %d survivor lost, %d broken'
              % (name, cuts, same, other, world_lost, lost, wrong))

    if failures:
        print('FAIL:')

        for failure in failures:
            print('  ' + failure)

        return 1

    print('PASS: no cut lost or broke the survivor; worlds that fit two copies always came back whole')
    return 0


if __name__ == '__main__':
    sys.exit(main())
