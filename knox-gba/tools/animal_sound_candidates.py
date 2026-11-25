"""Animals plan stage 6: the candidates for the six animal sounds, two of each, to pick by ear (the user picked all
the A's; the sounds themselves live in tools/assets/animal_sounds.py, which the build uses).

    python tools/animal_sound_candidates.py     -> sound-candidates/animals/<sound>_<A|B>.wav, all.wav

all.wav plays every candidate in order (A then B of each), at its final volume, 16-bit, with a gap between.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, 'assets'))

from animal_sounds import CANDIDATES, FINAL_DB, RATE, db, export, silence, wav_bytes  # noqa: E402

OUT = os.path.join(os.path.dirname(HERE), 'sound-candidates', 'animals')


def main():
    os.makedirs(OUT, exist_ok=True)
    reel = []
    gap = silence(0.9)
    for name, letter, make in CANDIDATES:
        samples = export(make())
        with open(os.path.join(OUT, '%s_%s.wav' % (name, letter)), 'wb') as f:
            f.write(wav_bytes(samples))
        reel += [s * db(FINAL_DB[name] + 3) for s in samples] + gap
        print('%-11s %s  %.2f s  (plays at %d dB)  %s' % (name, letter, len(samples) / RATE, FINAL_DB[name],
                                                         make.__doc__.split(',')[0]))
    with open(os.path.join(OUT, 'all.wav'), 'wb') as f:
        f.write(wav_bytes(reel, 16))


if __name__ == '__main__':
    main()
