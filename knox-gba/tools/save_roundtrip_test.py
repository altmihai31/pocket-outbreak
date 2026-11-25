"""Runs the save bench (knox_save.gba, BENCH=8) headless in mGBA and fails if the save round trip loses anything.

    python tools/save_roundtrip_test.py knox_save.gba [--mgba PATH] [--timeout SECONDS] [--log FILE]
    python tools/save_roundtrip_test.py --check-log kx.log     (only check a log made earlier)

The bench saves a generated world and quits, loads it, dies and starts the next survivor. Its first pass is what
this checks, from the KXSAVE / KXSETTINGS lines it logs:

- every "KXSAVE saved ..." line must come back unchanged as "KXSAVE loaded ..." (position, clock, kills, carried
  weight, food, the world's zombie total, containers, the checksum of every stored item stack, the map, the
  flashlight, wounds, traits, doors, the safe, the gun in hand, ...);
- the game settings in the save's header come back ("KXSETTINGS saved" equals "KXSETTINGS loaded");
- the sandbox settings come back (the bench plays with 0,2,2,3,0 and resets them before the load);
- after the death, the next survivor is told about it (deaths=1) and is the strong fire officer the bench made.

Exit code 0 when everything matches, 1 with a list of what didn't.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

DONE = 'KXBENCH done'
SANDBOX = '0,2,2,3,0'
LINE = re.compile(r'(KX[A-Z0-9]+ .*)$')


def run_bench(rom, mgba, timeout, log_path):
    """Plays the ROM in mGBA with no window and no sound until the bench logs that it's done (or time runs out)."""
    work = tempfile.mkdtemp(prefix='kx_save_')
    rom_copy = os.path.join(work, os.path.basename(rom))
    shutil.copyfile(rom, rom_copy)  # a fresh folder, so no old .sav is loaded
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    # -l 7: fatal, error and warn only; Butano's BN_LOG writes at warn. Debug logging slows mGBA a hundredfold.
    command = [mgba, '-l', '7', '-C', 'logToFile=1', '-C', 'logFile=' + log_path,
               '-C', 'videoSync=0', '-C', 'audioSync=0', rom_copy]
    started = time.monotonic()
    process = subprocess.Popen(command, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    try:
        while time.monotonic() - started < timeout:
            if process.poll() is not None:
                break

            if os.path.exists(log_path):
                with open(log_path, errors='replace') as log:
                    if DONE in log.read():
                        break

            time.sleep(0.5)
    finally:
        process.kill()
        process.wait()
        shutil.rmtree(work, ignore_errors=True)

    print('mGBA ran for %.1f s' % (time.monotonic() - started))


def read_lines(log_path):
    """The bench's own lines (KX...), up to the end of its first full run."""
    lines = []

    with open(log_path, errors='replace') as log:
        for raw in log:
            match = LINE.search(raw.rstrip())

            if match:
                lines.append(match.group(1))

                if lines[-1] == DONE:
                    break

    return lines


def check(lines):
    failures = []

    if DONE not in lines:
        failures.append('the bench never logged "%s" (it crashed, hung or ran out of time)' % DONE)

    saved = [line[len('KXSAVE saved '):] for line in lines if line.startswith('KXSAVE saved ')]
    loaded = [line[len('KXSAVE loaded '):] for line in lines if line.startswith('KXSAVE loaded ')]

    if not saved:
        failures.append('no "KXSAVE saved" lines: the bench never saved')
    elif len(saved) != len(loaded):
        failures.append('%d "saved" lines but %d "loaded" lines' % (len(saved), len(loaded)))
    else:
        for before, after in zip(saved, loaded):
            if before != after:
                failures.append('changed by the save:\n    saved:  %s\n    loaded: %s' % (before, after))

    settings_saved = [line.split(' ', 2)[2] for line in lines if line.startswith('KXSETTINGS saved ')]
    settings_loaded = [line.split(' ', 2)[2] for line in lines if line.startswith('KXSETTINGS loaded ')]

    if not settings_saved or not settings_loaded:
        failures.append('missing KXSETTINGS saved/loaded lines')
    elif settings_saved[0] != settings_loaded[0]:
        failures.append('settings changed by the save: saved %s, loaded %s' % (settings_saved[0], settings_loaded[0]))

    sandbox = [line for line in lines if line.startswith('KXSAVE sandbox ')]

    if not sandbox:
        failures.append('no "KXSAVE sandbox" line after the load')
    elif not all(line.split()[2] == SANDBOX for line in sandbox):
        failures.append('sandbox settings not restored (want %s): %s' % (SANDBOX, '; '.join(sandbox)))

    if not any(line.startswith('KXSAVE survivors deaths=1 ') for line in lines):
        failures.append('the next survivor was not told about the death (no "KXSAVE survivors deaths=1")')

    if not any(line.startswith('KXSAVE character job=FIRE OFFICER strong=1 ') for line in lines):
        failures.append('the next survivor is not the strong fire officer the bench made')

    return saved, failures


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('rom', nargs='?', help='knox_save.gba (make BENCH=8)')
    parser.add_argument('--mgba', default=shutil.which('mgba') or '/usr/games/mgba', help='mGBA SDL binary')
    parser.add_argument('--timeout', type=float, default=180, help='seconds before giving up')
    parser.add_argument('--log', default='knox_save.log', help='where mGBA writes its log')
    parser.add_argument('--check-log', help='check this log instead of running the ROM')
    args = parser.parse_args()

    if args.check_log:
        log_path = args.check_log
    elif args.rom:
        log_path = args.log

        if os.path.exists(log_path):
            os.remove(log_path)

        run_bench(args.rom, args.mgba, args.timeout, log_path)

        if not os.path.exists(log_path):
            print('FAIL: mGBA wrote no log (%s)' % log_path)
            return 1
    else:
        parser.error('give the ROM to run, or --check-log')

    saved, failures = check(read_lines(log_path))

    if failures:
        print('FAIL: the save round trip lost something')

        for failure in failures:
            print('  - ' + failure)

        return 1

    print('PASS: %d saved state lines came back unchanged; settings, sandbox and the next survivor are right' %
          len(saved))
    return 0


if __name__ == '__main__':
    sys.exit(main())
