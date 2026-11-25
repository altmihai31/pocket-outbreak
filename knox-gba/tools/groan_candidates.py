"""Playtest 1, stage 4: zombie groan candidates to pick from by ear. The user picked A (moan), now groan() in
tools/assets/audio.py; this keeps the others to hear again.

The groan before was a low saw wave (88-105 Hz, falling) through a low-pass: a buzz with no vowel
in it, which is why it reads as something other than a voice. These candidates are built like a voice instead: a
throat source (pulses with jitter and shimmer, or creaky fry, or breath) shaped by three formant resonators, the
peaks that make a vowel. Each style has two takes (a and b), as the game picks one of two groans at random.

Written in the game's own format (8-bit mono, 16 kHz, same normalising and loudness cap), so what plays here is what
the GBA plays. Output: sound-candidates/ (not built into the ROM), plus all.wav with every take in a row.

    python tools/groan_candidates.py
"""
import io
import math
import os
import random
import sys
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, 'assets'))

import audio  # noqa: E402

from audio import (RATE, RMS_CAP, amplitude, envelope, formants, groan, lowpass, mix, noise, normalize,  # noqa: E402,F401
                   ramp, rms, throat, tone)

OUT = os.path.join(os.path.dirname(HERE), 'sound-candidates')


def as_in_game(samples):
    """What save_wav does: the same peak, then loud ones brought down to the RMS cap."""
    samples = normalize(samples)
    level = rms(samples)
    if level > RMS_CAP:
        samples = [s * RMS_CAP / level for s in samples]
    return samples


def wav_bytes(samples):
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(RATE)
        w.writeframes(bytes(max(0, min(255, int(128 + s * 127))) for s in samples))
    return buf.getvalue()


def buzz(seed, seconds, base):
    """The groan before playtest 1, for comparison: a low saw wave through a low-pass."""
    rng = random.Random(seed)
    voice = tone(seconds, base, base * rng.uniform(0.7, 0.85), 'saw', vibrato=0.06, vibrato_rate=rng.uniform(4, 7))
    breath = lowpass(noise(seconds, seed), 900)
    voiced = lowpass(mix((voice, 1.0), (breath, 0.35)), 700, 420)
    return envelope(voiced, 0.12, 0, 1.2)


def tremble(n, rate, depth, seed):
    rng = random.Random(seed)
    phase = rng.random()
    return [1 - depth * (0.5 + 0.5 * math.sin(2 * math.pi * (phase + rate * i / RATE))) for i in range(n)]


def rasp(seed, seconds, pitch):
    """B: a creaking, rattling growl: vocal fry through an open 'aah'."""
    n = int(RATE * seconds)
    pitches = ramp(pitch, pitch * 0.8, n)
    source = mix((throat(n, pitches, seed, jitter=0.12, shimmer=0.5, fry=0.35), 1.0), (noise(seconds, seed + 1), 0.12))
    body = formants(source, ramp(780, 650, n), ramp(1250, 1100, n), 2600, gains=(1.0, 0.7, 0.35))
    shape = ramp([(0, 0.0), (0.08, 1.0), (0.6, 0.8), (1, 0.0)], None, n)
    return amplitude(body, [a * b for a, b in zip(shape, tremble(n, 9, 0.35, seed))])


def gurgle(seed, seconds):
    """C: a wet, breathy 'hhhaaah' with a gurgle in the throat and a faint low voice under it."""
    n = int(RATE * seconds)
    breath = formants(noise(seconds, seed), ramp(500, 720, n), ramp(1000, 1250, n), 2400, widths=(140, 160, 220))
    voice = formants(throat(n, ramp(95, 80, n), seed + 3, jitter=0.08, shimmer=0.4, fry=0.2),
                     ramp(500, 720, n), ramp(1000, 1250, n), 2400)
    body = mix((breath, 1.0), (voice, 0.45))
    wet = tremble(n, 22, 0.6, seed + 4)
    shape = ramp([(0, 0.0), (0.25, 1.0), (0.75, 0.7), (1, 0.0)], None, n)
    return amplitude(body, [a * b for a, b in zip(shape, wet)])


def wail(seed, seconds, pitch_from, pitch_to):
    """D: a higher, hollow 'ooo-aah' that opens up and falls: more of a cry than a moan."""
    n = int(RATE * seconds)
    pitch = ramp([(0, pitch_from), (0.35, pitch_from * 1.06), (1, pitch_to)], None, n)
    source = mix((throat(n, pitch, seed, jitter=0.03, shimmer=0.2), 1.0), (noise(seconds, seed + 1), 0.05))
    body = formants(source, ramp([(0, 360), (0.45, 700), (1, 640)], None, n),
                    ramp([(0, 800), (0.45, 1150), (1, 1050)], None, n), 2450)
    shape = ramp([(0, 0.0), (0.2, 1.0), (0.65, 0.9), (1, 0.0)], None, n)
    return amplitude(body, shape)


# Round 2 (playtest 2: the moan "sounds like wind"). Measured, the moan's pitch is barely there (a periodicity of
# 0.30 against 0.70 for the old buzz): jitter, shimmer, breath and narrow resonances that ring on their own drown the
# voice. These keep a vowel but put a steady voice back under it: little jitter, no breath, wider resonances that let
# the harmonics through, a chest path (the voice itself, low-passed) and a soft clip for grit.

def vibrato(pitch, rate, depth):
    return [f * (1 + depth * math.sin(2 * math.pi * rate * i / RATE)) for i, f in enumerate(pitch)]


SPEAKER_CUT = True      # round 3 leaves the low end to the export's 100 Hz high-pass


def speaker(samples, cutoff=200, passes=3):
    """A steep cut under cutoff, last of all: a handheld's speaker only buzzes down there, and the ear hears the
    pitch from the overtones anyway (the missing fundamental)."""
    if not SPEAKER_CUT:
        return samples
    for _ in range(passes):
        low = lowpass(samples, cutoff)
        samples = [a - b for a, b in zip(samples, low)]
    return samples


def drive(samples, amount):
    peak = max(1e-6, max(abs(v) for v in samples))
    return [math.tanh(amount * v / peak) for v in samples]


def voiced(n, pitch, seed, vowel, jitter=0.012, shimmer=0.08, fry=0.0, chest=0.5):
    source = throat(n, pitch, seed, jitter=jitter, shimmer=shimmer, fry=fry)
    f1, f2, f3 = vowel
    body = formants(source, f1, f2, f3, gains=(1.0, 0.7, 0.35), widths=(160, 190, 260))
    # The chest: the voice's own low harmonics, but from 180 Hz up, which a small speaker can still play.
    low = lowpass(source, 700)
    chest_band = [a - b for a, b in zip(low, lowpass(low, 180))]
    voice = mix((body, 1.0), (chest_band, chest))
    # Nothing much under 160 Hz: on a handheld's speaker it is only a buzz (the first groan's trouble).
    return [a - b for a, b in zip(voice, lowpass(voice, 160))]


def moan_voiced(seed, seconds, pitch_from, pitch_to):
    """E: the moan with its voice back: 'uhhh-ohh', steady and round, no breath."""
    n = int(RATE * seconds)
    pitch = vibrato(ramp(pitch_from, pitch_to, n), 5.0, 0.02)
    body = voiced(n, pitch, seed, (ramp(620, 470, n), ramp(1150, 850, n), 2500))
    return speaker(amplitude(drive(body, 1.4), ramp([(0, 0.0), (0.15, 1.0), (0.7, 0.85), (1, 0.0)], None, n)))


def moan_growl(seed, seconds, pitch_from, pitch_to):
    """F: E with a growl: a rough flutter in the throat, a little creak, more grit."""
    n = int(RATE * seconds)
    pitch = vibrato(ramp(pitch_from, pitch_to, n), 5.0, 0.02)
    body = voiced(n, pitch, seed, (ramp(640, 500, n), ramp(1150, 880, n), 2500), jitter=0.02, fry=0.08)
    rough = [1 - 0.35 * (0.5 + 0.5 * math.sin(2 * math.pi * 32 * i / RATE)) for i in range(n)]
    shape = ramp([(0, 0.0), (0.12, 1.0), (0.7, 0.85), (1, 0.0)], None, n)
    return speaker(amplitude(drive(body, 2.4), [a * b for a, b in zip(shape, rough)]))


def groan_deep(seed, seconds, pitch_from, pitch_to):
    """G: a deep, chesty groan, 'ohhh', low but full of harmonics so small speakers still carry it."""
    n = int(RATE * seconds)
    pitch = vibrato(ramp(pitch_from, pitch_to, n), 4.0, 0.015)
    body = voiced(n, pitch, seed, (ramp(520, 450, n), ramp(900, 800, n), 2400), jitter=0.015, chest=0.7)
    return speaker(amplitude(drive(body, 1.9), ramp([(0, 0.0), (0.2, 1.0), (0.75, 0.8), (1, 0.0)], None, n)))


def moan_hungry(seed, seconds, pitch_from, pitch_to):
    """H: two beats, 'uh... UHHH', the second rising then falling and opening up."""
    n = int(RATE * seconds)
    pitch = ramp([(0, pitch_from), (0.3, pitch_from * 0.95), (0.45, pitch_from * 1.15), (1, pitch_to)], None, n)
    vowel = (ramp([(0, 600), (0.4, 600), (0.6, 720), (1, 620)], None, n),
             ramp([(0, 1100), (0.4, 1100), (0.6, 1250), (1, 1000)], None, n), 2500)
    body = voiced(n, vibrato(pitch, 5.5, 0.02), seed, vowel)
    shape = ramp([(0, 0.0), (0.08, 0.7), (0.25, 0.25), (0.42, 1.0), (0.75, 0.8), (1, 0.0)], None, n)
    return speaker(amplitude(drive(body, 1.6), shape))


ROUND2 = [
    ('A_moan', 'now in the game (sounds like wind)', [groan(11, 0.85, 128, 100), groan(12, 0.75, 112, 92)]),
    ('E_voiced', 'the moan with its voice back, round', [moan_voiced(51, 0.85, 118, 92),
                                                       moan_voiced(52, 0.75, 106, 86)]),
    ('F_growl', 'voiced moan with a growl', [moan_growl(61, 0.8, 112, 88), moan_growl(62, 0.7, 100, 82)]),
    ('G_deep', 'deep chesty groan', [groan_deep(71, 0.9, 84, 66), groan_deep(72, 0.8, 78, 62)]),
    ('H_hungry', 'two beats, uh... UHHH', [moan_hungry(81, 1.0, 104, 86), moan_hungry(82, 0.9, 96, 80)]),
]


CANDIDATES = [
    ('0_before', 'the groan before playtest 1', [buzz(5, 0.55, 105), buzz(6, 0.7, 88)]),
    ('A_moan', 'long low moan, rough voice and breath (picked: in the game)',
     [groan(11, 0.85, 128, 100), groan(12, 0.75, 112, 92)]),
    ('B_rasp', 'creaking, rattling growl', [rasp(21, 0.7, 60), rasp(22, 0.6, 52)]),
    ('C_gurgle', 'wet breathy gurgle', [gurgle(31, 0.8), gurgle(32, 0.7)]),
    ('D_wail', 'higher hollow cry', [wail(41, 0.9, 175, 130), wail(42, 0.8, 160, 120)]),
]


def write_round(folder, candidates, finish=None):
    finish = finish or as_in_game
    os.makedirs(folder, exist_ok=True)
    reel = []
    gap = [0.0] * int(RATE * 0.6)
    for name, _, takes in candidates:
        for take, samples in zip('ab', takes):
            with open(os.path.join(folder, 'groan_%s_%s.wav' % (name, take)), 'wb') as f:
                f.write(wav_bytes(finish(samples)))
            reel += finish(samples) + gap
        reel += gap
    with open(os.path.join(folder, 'all.wav'), 'wb') as f:
        f.write(wav_bytes(reel))
    for name, text, _ in candidates:
        print('%-10s %s' % (name, text))


# Round 3 (playtest 2): the same voices exported to the user's settings: 22050 Hz, unsigned 8-bit mono, peak at
# -3 dB, a 100 Hz high-pass and a 7000 Hz low-pass (12 dB an octave each), and no loudness cap.
ROUND3_RATE = 22050
ROUND3_PEAK = 10 ** (-3 / 20)


def biquad(samples, kind, cutoff, q=0.7071):
    """A 12 dB/octave Butterworth filter (RBJ cookbook)."""
    w = 2 * math.pi * cutoff / RATE
    alpha = math.sin(w) / (2 * q)
    cos = math.cos(w)
    if kind == 'high':
        b = [(1 + cos) / 2, -(1 + cos), (1 + cos) / 2]
    else:
        b = [(1 - cos) / 2, 1 - cos, (1 - cos) / 2]
    a = [1 + alpha, -2 * cos, 1 - alpha]
    out = []
    x1 = x2 = y1 = y2 = 0.0
    for x in samples:
        y = (b[0] * x + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2) / a[0]
        x2, x1, y2, y1 = x1, x, y1, y
        out.append(y)
    return out


def exported(samples):
    samples = biquad(biquad(samples, 'high', 100), 'low', 7000)
    return normalize(samples, ROUND3_PEAK)


def round3():
    """The round 2 voices and the moan in the game, rendered at 22050 Hz without the steep 200 Hz cut."""
    global RATE, SPEAKER_CUT
    audio.RATE = RATE = ROUND3_RATE
    SPEAKER_CUT = False
    return [
        ('A_moan', 'the moan in the game now', [groan(11, 0.85, 128, 100), groan(12, 0.75, 112, 92)]),
        ('E_voiced', 'the moan with its voice back', [moan_voiced(51, 0.85, 118, 92), moan_voiced(52, 0.75, 106, 86)]),
        ('F_growl', 'voiced moan with a growl', [moan_growl(61, 0.8, 112, 88), moan_growl(62, 0.7, 100, 82)]),
        ('G_deep', 'deep chesty groan', [groan_deep(71, 0.9, 84, 66), groan_deep(72, 0.8, 78, 62)]),
        ('H_hungry', 'two beats, uh... UHHH', [moan_hungry(81, 1.0, 104, 86), moan_hungry(82, 0.9, 96, 80)]),
    ]


def main():
    write_round(OUT, CANDIDATES)
    write_round(os.path.join(OUT, 'round2'), ROUND2)
    write_round(os.path.join(OUT, 'round3'), round3(), exported)


if __name__ == '__main__':
    main()
