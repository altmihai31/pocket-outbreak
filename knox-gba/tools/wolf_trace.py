"""Reads bench 15's wolf trace (KXWTRACE lines, playtest 3) and counts what makes wolves look erratic, per wolf, per
second of circling or darting: facing flips (left/right), picture changes (side/front/back, stand/step frames
excluded: only a change of view counts), stop-go (moving <-> standing), speed jumps (over 0.5 px a frame in one
frame) and sharp turns (the heading swinging over 60 degrees in one frame) and detours (blocked turns). Movement is
what the wolf really moved each frame when the trace has it (mx, my), else its speed.

    python tools/wolf_trace.py <mgba log> [<another log>]
"""
import math
import re
import sys
from collections import defaultdict

LINE = re.compile(r'KXWTRACE f=(\d+) id=(\d+) st=(\d+) x=(-?\d+) y=(-?\d+) vx=(-?\d+) vy=(-?\d+) fr=(-?\d+) '
                  r'left=(\d) detour=(\d+)(?: mx=(-?\d+) my=(-?\d+))?')
STATES = {7: 'circle', 8: 'dart', 9: 'retreat'}
WOLF_FRAMES = {14: 'side', 15: 'side', 16: 'down', 17: 'down', 18: 'up', 19: 'up'}


def read(path):
    wolves = defaultdict(list)
    for line in open(path, encoding='utf-8', errors='replace'):
        match = LINE.search(line)
        if match:
            f, wid, st, x, y, vx, vy, fr, left, detour = map(int, match.groups()[:10])
            # The movement seen: what it really moved when the trace has it (playtest 3 fix), else its speed.
            mx, my = match.group(11), match.group(12)
            mx, my = (int(mx) / 1000, int(my) / 1000) if mx is not None else (vx / 1000, vy / 1000)
            wolves[wid].append(dict(f=f, st=st, x=x / 16, y=y / 16, vx=mx, vy=my, fr=fr, left=left,
                                    detour=detour))
    return wolves


def count(rows):
    totals = defaultdict(int)
    frames = 0
    for before, now in zip(rows, rows[1:]):
        if now['f'] != before['f'] + 1 or now['st'] not in STATES:
            continue
        frames += 1
        speed_before = math.hypot(before['vx'], before['vy'])
        speed_now = math.hypot(now['vx'], now['vy'])
        totals['facing flips'] += now['left'] != before['left']
        totals['view changes'] += WOLF_FRAMES.get(now['fr']) != WOLF_FRAMES.get(before['fr'])
        totals['stop-go'] += (speed_before == 0) != (speed_now == 0)
        totals['speed jumps'] += abs(speed_now - speed_before) > 0.5 and now['st'] == before['st']
        if speed_before > 0 and speed_now > 0 and now['st'] == before['st']:
            dot = (before['vx'] * now['vx'] + before['vy'] * now['vy']) / (speed_before * speed_now)
            totals['sharp turns'] += dot < 0.5
        totals['detours'] += now['detour'] > before['detour']
    return frames, totals


def report(path):
    print(path)
    keys = ['facing flips', 'view changes', 'stop-go', 'speed jumps', 'sharp turns', 'detours']
    print('  wolf  seconds  ' + '  '.join('%12s' % k for k in keys))
    sums = defaultdict(float)
    seconds = 0
    for wid, rows in sorted(read(path).items()):
        frames, totals = count(rows)
        if not frames:
            continue
        s = frames / 60
        seconds += s
        print('  %4d  %7.1f  ' % (wid, s) + '  '.join('%12.1f' % (totals[k] / s) for k in keys))
        for k in keys:
            sums[k] += totals[k]
    if seconds:
        print('   all  %7.1f  ' % seconds + '  '.join('%12.1f' % (sums[k] / seconds) for k in keys) + '   (per second)')


if __name__ == '__main__':
    for argument in sys.argv[1:]:
        report(argument)
