"""Animals plan stage 6: the six animal sounds (boar grunt, wolf howl and snarl, hen cluck, goat bleat, crow caw),
exported like the zombie sounds in the user's format: 22050 Hz, unsigned 8-bit mono, a 100 Hz high-pass and a 7000 Hz
low-pass, peak at -3 dB; the final volume (FINAL_DB) is applied at playback.

Two candidates of each were made (tools/animal_sound_candidates.py writes them to sound-candidates/animals); the user
picked all the A's by ear on 2026-11-24. Built into the game by tools/gen_assets.py (audio.build_audio calls
build_animal_sounds: audio/<sound>.wav).

Made by source and filter: a voice (a band-limited buzz with pitch jitter, or noise) through formants (the mouth's
resonances, as band-pass filters), shaped by envelopes, on zombie_sounds.py's toolkit.
"""
import math
import os
import random

from common import AUDIO, write_if_changed  # noqa: E402
from zombie_sounds import (RATE, biquad, brown, count, db, distortion, export, fades, glide, mix, pink, reverb,  # noqa
                           soft_clip, unit, wav_bytes, white)


# Playback volume of each sound (dB), as FINAL_DB for the zombies.
FINAL_DB = {'boar_grunt': -8, 'wolf_howl': -10, 'wolf_snarl': -6, 'hen_cluck': -12, 'goat_bleat': -10,
            'crow_caw': -8}


# ---- Sources and filters.

def jitter(n, amount, rate_hz, seed):
    """A slow random wobble around 1 (pitch or loudness): smoothed noise, +-amount."""
    rng = random.Random(seed)
    step = max(1, int(RATE / rate_hz))
    points = [rng.uniform(-1, 1) for _ in range(n // step + 2)]
    out = []
    for i in range(n):
        k, u = divmod(i, step)
        u /= step
        u = u * u * (3 - 2 * u)
        out.append(1 + amount * (points[k] * (1 - u) + points[k + 1] * u))
    return out


def buzz(freqs, tilt=1.0, seed=0):
    """A band-limited pulse-like voice: harmonics up to 6 kHz falling off as 1/k^tilt, the phase from the pitch."""
    out = []
    phase = random.Random(seed).random()
    top = 6000.0
    for f in freqs:
        phase = (phase + f / RATE) % 1.0
        harmonics = max(1, min(48, int(top / max(f, 1))))
        s = 0.0
        a = 2 * math.pi * phase
        for k in range(1, harmonics + 1):
            s += math.sin(k * a) / (k ** tilt)
        out.append(s)
    return unit(out)


def bandpass(samples, centre, width):
    """A resonance: RBJ band-pass with 0 dB at the centre, width in Hz."""
    q = max(0.3, centre / max(width, 1))
    w = 2 * math.pi * centre / RATE
    alpha = math.sin(w) / (2 * q)
    cos = math.cos(w)
    b0, b2 = alpha, -alpha
    a0, a1, a2 = 1 + alpha, -2 * cos, 1 - alpha
    out = []
    x1 = x2 = y1 = y2 = 0.0
    for x in samples:
        y = (b0 * x + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1, y2, y1 = x1, x, y1, y
        out.append(y)
    return out


def formants(samples, bands):
    """Parallel resonances [(centre Hz, width Hz, gain dB)], summed."""
    return mix(*[(bandpass(samples, c, w), db(g)) for c, w, g in bands])


def shape(n, points):
    """A loudness curve through [(t 0-1, gain)], straight between points."""
    out = []
    j = 0
    for i in range(n):
        t = i / max(1, n - 1)
        while j < len(points) - 2 and t > points[j + 1][0]:
            j += 1
        (t0, g0), (t1, g1) = points[j], points[j + 1]
        u = 0 if t1 == t0 else min(1, max(0, (t - t0) / (t1 - t0)))
        out.append(g0 + (g1 - g0) * u)
    return out


def times(a, b):
    return [x * y for x, y in zip(a, b)]


def scaled(freqs, wobble):
    return [f * w for f, w in zip(freqs, wobble)]


def silence(seconds):
    return [0.0] * count(seconds)


def place(length, pieces):
    """Pieces [(start seconds, samples)] laid into a silence of length seconds."""
    out = silence(length)
    for start, samples in pieces:
        k = count(start)
        for i, s in enumerate(samples):
            if k + i < len(out):
                out[k + i] += s
    return out


# ---- Boar: a low grunt, a short snort of air into a nasal buzz.

def grunt(seconds, f0, f1, seed, rough=0.08):
    n = count(seconds)
    pitch = scaled(glide(n, [(0, f0), (1, f1)]), jitter(n, rough, 40, seed))
    voice = buzz(pitch, 1.1, seed)
    body = formants(voice, [(280, 120, 0), (750, 220, -4), (1900, 400, -14)])
    snort = biquad(biquad(white(n, seed + 7), 'high', 600), 'low', 2500)
    loud = times(jitter(n, 0.35, 28, seed + 3), shape(n, [(0, 0), (0.06, 1), (0.5, 0.8), (1, 0)]))
    burst = shape(n, [(0, 0), (0.03, 1), (0.25, 0.1), (1, 0)])
    return mix((times(body, loud), 1.0), (times(snort, burst), db(-10)))


def boar_grunt_a():
    """One gruff grunt, 0.5 s, 90 -> 70 Hz."""
    return distortion(grunt(0.5, 90, 70, 11), 18)


def boar_grunt_b():
    """Two quick grunts and a third, rougher and lower, 0.9 s."""
    return distortion(place(0.9, [(0.0, grunt(0.22, 95, 80, 12, 0.12)), (0.27, grunt(0.2, 92, 78, 13, 0.12)),
                                  (0.52, grunt(0.36, 85, 62, 14, 0.15))]), 25)


# ---- Wolf howl: a pure, rising, held then falling voice, heard far off.

def howl(seconds, points, seed, vibrato=0.012):
    n = count(seconds)
    pitch = glide(n, points)
    wobble = [1 + vibrato * math.sin(2 * math.pi * 5.0 * i / RATE) for i in range(n)]
    pitch = scaled(scaled(pitch, wobble), jitter(n, 0.004, 8, seed))
    voice = mix((buzz(pitch, 2.4, seed), 1.0))
    body = formants(voice, [(700, 300, 0), (1150, 300, -8), (2600, 500, -22)])
    air = biquad(biquad(pink(n, seed), 'high', 500), 'low', 3000)
    loud = shape(n, [(0, 0), (0.15, 0.9), (0.75, 1), (1, 0)])
    return mix((times(body, loud), 1.0), (times(air, loud), db(-26)))


def wolf_howl_a():
    """A long howl, 3.2 s: rising 330 -> 560 Hz, held, falling away to 380 Hz, in a big reverb."""
    body = howl(3.2, [(0, 330), (0.25, 560), (0.7, 540), (1, 380)], 21)
    return fades(reverb(body, room=0.8, damping=0.4, wet_db=-6, dry_db=0, tail=0), 0.05, 0.6)


def wolf_howl_b():
    """A short yip into a lower howl with a break in it, 3.4 s."""
    yip = howl(0.35, [(0, 420), (0.5, 640), (1, 500)], 22, 0.0)
    long = howl(2.8, [(0, 290), (0.3, 480), (0.55, 470), (0.62, 430), (0.7, 470), (1, 330)], 23)
    body = place(3.4, [(0.0, yip), (0.55, long)])
    return fades(reverb(body, room=0.8, damping=0.4, wet_db=-6, dry_db=0, tail=0), 0.03, 0.6)


# ---- Wolf snarl: a rattling growl through bared teeth.

def snarl(seconds, f0, f1, seed, rattle_hz=28):
    n = count(seconds)
    pitch = scaled(glide(n, [(0, f0), (1, f1)]), jitter(n, 0.15, 60, seed))
    voice = buzz(pitch, 0.9, seed)
    body = formants(voice, [(450, 200, 0), (1200, 300, -3), (2600, 600, -10)])
    hiss = biquad(biquad(white(n, seed + 1), 'high', 1500), 'low', 5000)
    rattle = [0.55 + 0.45 * math.sin(2 * math.pi * rattle_hz * i / RATE) for i in range(n)]
    loud = shape(n, [(0, 0), (0.1, 1), (0.85, 0.9), (1, 0)])
    return mix((times(times(body, rattle), loud), 1.0), (times(hiss, loud), db(-14)))


def bark(seed):
    n = count(0.16)
    pitch = glide(n, [(0, 520), (1, 300)])
    body = formants(buzz(pitch, 1.0, seed), [(800, 300, 0), (1700, 400, -4)])
    return times(body, shape(n, [(0, 0), (0.08, 1), (1, 0)]))


def wolf_snarl_a():
    """A growl that ends in a snap, 1.0 s."""
    body = place(1.0, [(0.0, snarl(0.78, 120, 105, 31)), (0.8, [s * 1.4 for s in bark(32)])])
    return soft_clip(distortion(body, 20), 4)


def wolf_snarl_b():
    """A rising, rattling snarl, 1.1 s, no snap."""
    return soft_clip(distortion(snarl(1.1, 100, 140, 33, 34), 25), 5)


# ---- Hen: short clucks, buk buk, and a ba-GAWK.

def cluck(seconds, f0, f1, seed, bright=0):
    n = count(seconds)
    pitch = scaled(glide(n, [(0, f0), (1, f1)]), jitter(n, 0.05, 50, seed))
    body = formants(buzz(pitch, 1.0, seed), [(850, 250, 0), (2100 + bright, 400, -4), (3200, 500, -12)])
    click = biquad(white(n, seed + 1), 'high', 2000)
    return mix((times(body, shape(n, [(0, 0), (0.12, 1), (0.4, 0.6), (1, 0)])), 1.0),
               (times(click, shape(n, [(0, 1), (0.1, 0), (1, 0)])), db(-12)))


def hen_cluck_a():
    """Three soft clucks, buk buk buk, 0.6 s."""
    return place(0.6, [(0.0, cluck(0.07, 380, 330, 41)), (0.18, cluck(0.07, 400, 340, 42)),
                       (0.38, cluck(0.09, 370, 320, 43))])


def hen_cluck_b():
    """Buk, buk, ba-GAWK: two clucks and a long call that rises, 0.9 s."""
    gawk = cluck(0.3, 520, 700, 46, 400)
    return place(0.9, [(0.0, cluck(0.07, 400, 340, 44)), (0.16, cluck(0.07, 410, 350, 45)),
                       (0.34, cluck(0.08, 430, 380, 47)), (0.5, gawk)])


# ---- Goat: a bleat, "meh-eh-eh", a buzz that trembles.

def bleat(seconds, f0, f1, tremolo_hz, depth, seed):
    n = count(seconds)
    pitch = glide(n, [(0, f0), (0.3, f0 * 1.04), (1, f1)])
    shake = [1 + 0.03 * math.sin(2 * math.pi * tremolo_hz * i / RATE) for i in range(n)]
    voice = buzz(scaled(pitch, shake), 0.8, seed)
    body = formants(voice, [(620, 160, 0), (1750, 250, -3), (2600, 350, -9)])
    tremble = [1 - depth * (0.5 + 0.5 * math.sin(2 * math.pi * tremolo_hz * i / RATE)) for i in range(n)]
    loud = shape(n, [(0, 0), (0.08, 1), (0.8, 0.85), (1, 0)])
    nose = biquad(biquad(pink(n, seed), 'high', 800), 'low', 3000)
    return mix((times(times(body, tremble), loud), 1.0), (times(nose, loud), db(-24)))


def goat_bleat_a():
    """A grown goat's bleat, 0.9 s, 290 -> 250 Hz, trembling 7 times a second."""
    return distortion(bleat(0.9, 290, 250, 7.0, 0.55, 51), 10)


def goat_bleat_b():
    """A higher, shakier bleat, a kid's, 0.8 s, 430 -> 380 Hz."""
    return distortion(bleat(0.8, 430, 380, 9.0, 0.7, 52), 8)


# ---- Crow: a harsh caw, a buzz full of noise through a nasal resonance.

def caw(seconds, f0, f1, seed, rough=0.6):
    n = count(seconds)
    pitch = scaled(glide(n, [(0, f0), (0.25, f0 * 1.05), (1, f1)]), jitter(n, 0.06, 90, seed))
    voice = buzz(pitch, 0.7, seed)
    noisy = mix((voice, 1.0), (white(n, seed + 1), rough))
    body = formants(noisy, [(1050, 300, 0), (1650, 350, -2), (2700, 500, -8)])
    return times(body, shape(n, [(0, 0), (0.05, 1), (0.6, 0.85), (1, 0)]))


def crow_caw_a():
    """Two caws, caw... caw, 0.9 s."""
    body = place(0.9, [(0.0, caw(0.32, 620, 520, 61)), (0.48, caw(0.34, 600, 500, 62))])
    return soft_clip(distortion(body, 30), 5)


def crow_caw_b():
    """Three quicker, rougher caws, 1.0 s."""
    body = place(1.0, [(0.0, caw(0.22, 680, 560, 63, 0.9)), (0.3, caw(0.22, 660, 560, 64, 0.9)),
                       (0.62, caw(0.3, 640, 520, 65, 0.9))])
    return soft_clip(distortion(body, 35), 6)


CANDIDATES = [
    ('boar_grunt', 'A', boar_grunt_a), ('boar_grunt', 'B', boar_grunt_b),
    ('wolf_howl', 'A', wolf_howl_a), ('wolf_howl', 'B', wolf_howl_b),
    ('wolf_snarl', 'A', wolf_snarl_a), ('wolf_snarl', 'B', wolf_snarl_b),
    ('hen_cluck', 'A', hen_cluck_a), ('hen_cluck', 'B', hen_cluck_b),
    ('goat_bleat', 'A', goat_bleat_a), ('goat_bleat', 'B', goat_bleat_b),
    ('crow_caw', 'A', crow_caw_a), ('crow_caw', 'B', crow_caw_b),
]


# The picks (all the A's).
SOUNDS = [('boar_grunt', boar_grunt_a), ('wolf_howl', wolf_howl_a), ('wolf_snarl', wolf_snarl_a),
          ('hen_cluck', hen_cluck_a), ('goat_bleat', goat_bleat_a), ('crow_caw', crow_caw_a)]


def build_animal_sounds():
    """The six sounds into the game's audio folder (only rewritten when they change)."""
    for name, make in SOUNDS:
        write_if_changed(os.path.join(AUDIO, name + '.wav'), wav_bytes(export(make())))
