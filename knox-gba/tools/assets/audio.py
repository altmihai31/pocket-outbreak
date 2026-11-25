"""Procedural sound effects (8-bit mono WAV for maxmod). Placeholders until a proper sound pass (milestone 13)."""
import io
import math
import os
import random
import struct
import wave

import speaker
from common import AUDIO, write_if_changed
from zombie_sounds import build_zombie_sounds
from animal_sounds import build_animal_sounds

RATE = 16000


def silence(seconds):
    return [0.0] * int(RATE * seconds)


def envelope(samples, attack, release, curve=2.0):
    n = len(samples)
    a = max(1, int(RATE * attack))
    out = []
    for i, s in enumerate(samples):
        if i < a:
            g = i / a
        else:
            g = max(0.0, 1.0 - (i - a) / max(1, n - a)) ** curve
        out.append(s * g)
    return out


def noise(seconds, seed):
    rng = random.Random(seed)
    return [rng.uniform(-1, 1) for _ in range(int(RATE * seconds))]


def lowpass(samples, cutoff_start, cutoff_end=None):
    cutoff_end = cutoff_start if cutoff_end is None else cutoff_end
    out, y, n = [], 0.0, len(samples)
    for i, s in enumerate(samples):
        cutoff = cutoff_start + (cutoff_end - cutoff_start) * i / max(1, n - 1)
        k = 1 - math.exp(-2 * math.pi * cutoff / RATE)
        y += k * (s - y)
        out.append(y)
    return out


def highpass(samples, cutoff):
    low = lowpass(samples, cutoff)
    return [s - l for s, l in zip(samples, low)]


def tone(seconds, freq_start, freq_end, shape='sine', vibrato=0.0, vibrato_rate=6.0):
    out, phase = [], 0.0
    n = int(RATE * seconds)
    for i in range(n):
        f = freq_start + (freq_end - freq_start) * i / max(1, n - 1)
        f *= 1 + vibrato * math.sin(2 * math.pi * vibrato_rate * i / RATE)
        phase += f / RATE
        p = phase % 1.0
        if shape == 'sine':
            out.append(math.sin(2 * math.pi * p))
        elif shape == 'saw':
            out.append(2 * p - 1)
        else:
            out.append(1.0 if p < 0.5 else -1.0)
    return out


def mix(*parts):
    n = max(len(p) for p, _ in parts)
    out = [0.0] * n
    for samples, gain in parts:
        for i, s in enumerate(samples):
            out[i] += s * gain
    return out


def normalize(samples, peak=0.9):
    m = max(1e-6, max(abs(s) for s in samples))
    return [s * peak / m for s in samples]


# Milestone 14 sound pass: every effect is normalised to the same peak, which left square-wave blips two to three
# times louder (RMS) than gunshots. Loud ones are brought down to this RMS, and the impacts below are driven through
# a soft clip so they hit harder than the menu sounds.
RMS_CAP = 0.26
DRIVEN = {'shot_pistol': 3.0, 'shot_shotgun': 3.0, 'shot_rifle': 3.0, 'crash': 2.5, 'hit_heavy': 2.0,
          'weapon_break': 2.0, 'glass': 2.0, 'thunder': 2.0}


def rms(samples):
    return math.sqrt(sum(s * s for s in samples) / max(1, len(samples)))


def save_wav(name, samples):
    samples = normalize(samples)
    if name in DRIVEN:
        drive = DRIVEN[name]
        samples = normalize([math.tanh(drive * s) for s in samples])
    level = rms(samples)
    if level > RMS_CAP:
        samples = [s * RMS_CAP / level for s in samples]
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(RATE)
        w.writeframes(bytes(max(0, min(255, int(128 + s * 127))) for s in samples))
    write_if_changed(os.path.join(AUDIO, name + '.wav'), speaker.finish(name, buf.getvalue()))


# Voices (playtest 1): a throat source shaped by formant resonators, the peaks that make a vowel. The first groan was
# a low saw wave through a low-pass, 85% of it under 250 Hz: on small speakers only its buzz was heard.
# tools/groan_candidates.py builds the other styles the user heard (rasp, gurgle, wail) from these.

def ramp(start, end, n):
    """A value per sample from start to end, or breakpoints [(t 0-1, value), ...] with smoothstep between."""
    if isinstance(start, list):
        points = start
        out = []
        for i in range(n):
            t = i / max(1, n - 1)
            for (t0, v0), (t1, v1) in zip(points, points[1:]):
                if t0 <= t <= t1:
                    u = (t - t0) / max(1e-9, t1 - t0)
                    u = u * u * (3 - 2 * u)
                    out.append(v0 + (v1 - v0) * u)
                    break
        return out
    return [start + (end - start) * (i / max(1, n - 1)) for i in range(n)]


def resonator(samples, freqs, bandwidth):
    """Two-pole resonator (a formant) following a frequency per sample; unity gain at its peak."""
    out = []
    y1 = y2 = 0.0
    r = math.exp(-math.pi * bandwidth / RATE)
    for s, f in zip(samples, freqs):
        c = 2 * r * math.cos(2 * math.pi * f / RATE)
        y = (1 - r) * s + c * y1 - r * r * y2
        y2, y1 = y1, y
        out.append(y)
    return out


def formants(source, f1, f2, f3, gains=(1.0, 0.6, 0.25), widths=(90, 110, 160)):
    n = len(source)
    tracks = [f if isinstance(f, list) else [f] * n for f in (f1, f2, f3)]
    return mix(*[(resonator(source, track, width), gain) for track, width, gain in zip(tracks, widths, gains)])


def throat(n, pitch, seed, jitter=0.04, shimmer=0.25, fry=0.0):
    """Glottal pulses at a pitch per sample, each period a little off (jitter) and louder or softer (shimmer); fry
    (0-1) skips and doubles periods at random, the creak of a ruined voice."""
    rng = random.Random(seed)
    out = [0.0] * n
    i = 0.0
    while i < n:
        f = pitch[int(i)] * (1 + rng.uniform(-jitter, jitter))
        if fry and rng.random() < fry:
            f *= rng.choice((0.5, 0.6, 0.75))
        period = RATE / f
        amp = 1 + rng.uniform(-shimmer, shimmer)
        start = int(i)
        for k in range(int(period)):
            if start + k >= n:
                break
            t = k / period
            if t < 0.4:         # quick rise, slower fall, then closed
                v = 0.5 * (1 - math.cos(math.pi * t / 0.4))
            elif t < 0.6:
                v = math.cos(0.5 * math.pi * (t - 0.4) / 0.2)
            else:
                v = 0.0
            out[start + k] = v * amp
        i += period
    # The flow's derivative is what the mouth gives out.
    return [b - a for a, b in zip([0.0] + out, out)]


def amplitude(samples, levels):
    return [s * g for s, g in zip(samples, levels)]


def groan(seed, seconds, pitch_from, pitch_to):
    """A zombie's moan, 'uhhh-ohh': a low rough voice with breath (style A, picked by ear in playtest 1)."""
    n = int(RATE * seconds)
    pitch = ramp(pitch_from, pitch_to, n)
    source = mix((throat(n, pitch, seed, jitter=0.05, shimmer=0.3), 1.0), (noise(seconds, seed + 1), 0.08))
    voiced = formants(source, ramp(620, 470, n), ramp(1150, 850, n), 2500)
    breath = formants(noise(seconds, seed + 2), ramp(620, 470, n), ramp(1150, 850, n), 2500)
    body = mix((voiced, 1.0), (breath, 0.18))
    return amplitude(body, ramp([(0, 0.0), (0.18, 1.0), (0.7, 0.85), (1, 0.0)], None, n))


def build_audio():
    os.makedirs(AUDIO, exist_ok=True)
    save_wav('swing', envelope(lowpass(noise(0.16, 1), 600, 3500), 0.07, 0, 1.5))
    save_wav('hit_blunt', mix((envelope(tone(0.16, 110, 45), 0.002, 0, 2.5), 1.0),
                              (envelope(lowpass(noise(0.08, 2), 1500), 0.001, 0, 3), 0.7)))
    save_wav('hit_blade', mix((envelope(highpass(noise(0.09, 3), 2500), 0.001, 0, 2), 0.9),
                              (envelope(tone(0.06, 700, 300), 0.001, 0, 3), 0.3)))
    save_wav('hit_heavy', mix((envelope(tone(0.24, 90, 35), 0.002, 0, 2), 1.0),
                              (envelope(lowpass(noise(0.2, 4), 2500, 600), 0.001, 0, 2), 0.9)))
    # Playtest 2: the zombies' voices (moan, breath, growl, choke, death) are made to the user's specification in
    # zombie_sounds.py; groan() stays for tools/groan_candidates.py.
    build_zombie_sounds()
    # Animals plan stage 6: the animals' calls (animal_sounds.py).
    build_animal_sounds()
    save_wav('shove', mix((envelope(lowpass(noise(0.12, 7), 900), 0.01, 0, 2), 0.8),
                          (envelope(tone(0.1, 160, 110, 'saw'), 0.005, 0, 2), 0.4)))
    save_wav('stomp', mix((envelope(tone(0.2, 70, 30), 0.001, 0, 2), 1.0),
                          (envelope(lowpass(noise(0.15, 8), 3000, 800), 0.001, 0, 1.5), 1.0)))
    growl = mix((tone(0.45, 70, 60, 'saw', vibrato=0.15, vibrato_rate=28), 0.8), (lowpass(noise(0.45, 9), 700), 0.6))
    save_wav('grab', envelope(lowpass(growl, 800), 0.03, 0, 1.0))
    crunch = []
    rng = random.Random(10)
    for _ in range(5):
        crunch += envelope(highpass(noise(0.035, rng.randrange(1000)), 1200), 0.001, 0, 3) + silence(0.02)
    save_wav('bite', mix((crunch, 1.0), (envelope(tone(0.3, 80, 50, 'saw'), 0.01, 0, 1), 0.5)))
    save_wav('break_free', mix((envelope(lowpass(noise(0.18, 11), 500, 2500), 0.03, 0, 1.5), 0.8),
                               (envelope(tone(0.12, 140, 70), 0.005, 0, 2), 0.6)))
    save_wav('weapon_break', mix((envelope(highpass(noise(0.12, 12), 1800), 0.001, 0, 3), 1.0),
                                 (envelope(tone(0.08, 1800, 900, 'square'), 0.001, 0, 3), 0.3)))
    save_wav('charge_ready', envelope(tone(0.06, 1200, 1600, 'square'), 0.002, 0, 2))
    save_wav('step_hard', envelope(highpass(noise(0.04, 13), 800), 0.001, 0, 3))
    save_wav('step_soft', envelope(lowpass(noise(0.06, 14), 1200), 0.004, 0, 2))
    save_wav('step_water', envelope(lowpass(highpass(noise(0.12, 15), 500), 3500, 1500), 0.01, 0, 1.5))
    save_wav('swap', envelope(tone(0.05, 500, 700, 'square'), 0.002, 0, 2))
    save_wav('pop', envelope(tone(0.05, 700, 1100, 'square'), 0.002, 0, 2.5))
    save_wav('chime', mix((envelope(tone(0.3, 1320, 1320), 0.002, 0, 1.5), 0.6),
                          (silence(0.07) + envelope(tone(0.3, 1760, 1760), 0.002, 0, 1.5), 0.6)))
    save_wav('rummage', envelope(lowpass(noise(0.18, 16), 1800, 700), 0.02, 0, 1.2))
    save_wav('pickup', envelope(lowpass(noise(0.07, 17), 2500), 0.003, 0, 2))
    save_wav('drop', mix((envelope(tone(0.08, 160, 90), 0.002, 0, 2), 0.7),
                         (envelope(lowpass(noise(0.06, 18), 1200), 0.002, 0, 2), 0.5)))
    save_wav('zip', envelope(highpass(noise(0.16, 19), 1500), 0.01, 0, 0.8))
    save_wav('denied', envelope(tone(0.09, 220, 180, 'square'), 0.002, 0, 1.5))
    save_wav('growl', envelope(lowpass(mix((tone(0.6, 55, 75, 'saw', vibrato=0.2, vibrato_rate=9), 0.8),
                                            (noise(0.6, 20), 0.3)), 300), 0.1, 0, 1.0))
    save_wav('shiver', envelope(mix((highpass(noise(0.4, 21), 2000), 0.6),
                                    (tone(0.4, 30, 30, 'square'), 0.0)), 0.02, 0, 0.8))
    beat = envelope(tone(0.09, 60, 40), 0.003, 0, 2)
    save_wav('heartbeat', beat + silence(0.12) + [s * 0.7 for s in beat])
    save_wav('cough', mix((envelope(lowpass(noise(0.18, 22), 1400), 0.005, 0, 1.5), 1.0),
                          (envelope(tone(0.15, 180, 120, 'saw'), 0.005, 0, 1.5), 0.3)))
    save_wav('eat', mix((envelope(highpass(noise(0.05, 23), 1500), 0.001, 0, 3) + silence(0.07) +
                         envelope(highpass(noise(0.05, 24), 1500), 0.001, 0, 3), 0.8)))
    save_wav('gulp', envelope(tone(0.12, 300, 180), 0.01, 0, 1.5))
    save_wav('bandage', envelope(highpass(noise(0.3, 25), 2500), 0.02, 0, 0.6))
    save_wav('moodle', envelope(tone(0.08, 660, 520), 0.004, 0, 2))
    # Base building: a hammer on a nail, glass breaking, a zombie banging on boards.
    save_wav('hammer', mix((envelope(tone(0.07, 900, 600, 'square'), 0.001, 0, 3), 0.5),
                           (envelope(highpass(noise(0.05, 26), 1500), 0.001, 0, 3), 0.8)))
    shards = []
    rng = random.Random(27)
    for _ in range(4):
        shards += envelope(highpass(noise(0.05, rng.randrange(1000)), 3000), 0.001, 0, 2.5) + silence(0.015)
    save_wav('glass', mix((envelope(highpass(noise(0.3, 28), 2200), 0.001, 0, 2), 0.8), (shards, 0.7)))
    save_wav('gunshot', mix((envelope(lowpass(noise(0.5, 31), 1800, 300), 0.001, 0, 2.2), 1.0),
                            (envelope(tone(0.2, 120, 50), 0.001, 0, 2), 0.6)))
    siren = []
    for i in range(4):
        siren += tone(0.12, 880, 880, 'square') + tone(0.12, 660, 660, 'square')
    save_wav('alarm', envelope(siren, 0.01, 0.05, 1.0))
    chop = []
    for i in range(6):
        chop += envelope(lowpass(noise(0.05, 40 + i), 600), 0.002, 0, 2) + silence(0.035)
    save_wav('rotor', chop)
    save_wav('radio_static', envelope(highpass(noise(0.6, 32), 1200), 0.02, 0.1, 1.0))
    save_wav('thunder', mix((envelope(lowpass(noise(1.4, 30), 900, 120), 0.02, 0, 1.2), 1.0),
                            (envelope(tone(1.0, 55, 35), 0.05, 0, 1.0), 0.5)))
    save_wav('bang', mix((envelope(tone(0.18, 95, 55), 0.001, 0, 2), 1.0),
                         (envelope(lowpass(noise(0.14, 29), 1200, 500), 0.001, 0, 2), 0.9)))

    # Vehicles: the engine (a short putter, repeated while it runs), the starter, a crash and a bicycle chain.
    putt = []
    for i in range(4):
        putt += envelope(mix((tone(0.045, 95, 70, 'saw'), 0.9), (lowpass(noise(0.045, 50 + i), 500), 0.5)),
                         0.002, 0, 1.2)
    save_wav('engine', putt)
    crank = []
    for i in range(5):
        crank += envelope(mix((tone(0.07, 150, 110, 'square'), 0.5), (lowpass(noise(0.07, 60 + i), 900), 0.6)),
                          0.004, 0, 1.0) + silence(0.02)
    save_wav('starter', crank)
    save_wav('crash', mix((envelope(lowpass(noise(0.4, 70), 2600, 500), 0.001, 0, 1.6), 1.0),
                          (envelope(tone(0.25, 80, 35), 0.001, 0, 2), 0.8),
                          (silence(0.05) + envelope(highpass(noise(0.2, 71), 2800), 0.001, 0, 2), 0.5)))
    save_wav('chain', envelope(highpass(noise(0.03, 72), 2200), 0.001, 0, 3) + silence(0.03) +
             envelope(highpass(noise(0.03, 73), 2600), 0.001, 0, 3))
    # Guns (milestone 12.6): a crack with a short low thump; the shotgun's boom; the rifles' sharper, longer
    # crack; a magazine going in.
    save_wav('shot_pistol', mix((envelope(highpass(noise(0.10, 80), 900), 0.0005, 0, 3), 1.0),
                                (envelope(tone(0.08, 220, 70), 0.0005, 0, 2.5), 0.7)))
    save_wav('shot_shotgun', mix((envelope(lowpass(noise(0.32, 81), 3200, 500), 0.0005, 0, 2), 1.0),
                                 (envelope(tone(0.22, 120, 35), 0.0005, 0, 2), 1.0)))
    save_wav('shot_rifle', mix((envelope(highpass(noise(0.22, 82), 1200), 0.0005, 0, 2.2), 1.0),
                               (envelope(tone(0.16, 300, 60), 0.0005, 0, 2.5), 0.6)))
    save_wav('reload', envelope(highpass(noise(0.02, 83), 1500), 0.001, 0, 3) + silence(0.09) +
             mix((envelope(highpass(noise(0.03, 84), 2000), 0.001, 0, 3), 0.9),
                 (envelope(tone(0.03, 900, 500, 'square'), 0.001, 0, 3), 0.3)))
    # Throwables (milestone 12.6 stage 5): a quick whoosh; a fire catching (a soft roar with crackles); an
    # alarm clock's tinny double ring.
    save_wav('whoosh', envelope(lowpass(highpass(noise(0.14, 85), 400), 2600, 900), 0.04, 0, 1.6))
    crackle = []
    rng_fire = random.Random(86)
    for _ in range(6):
        crackle += silence(rng_fire.uniform(0.01, 0.05)) + envelope(highpass(noise(0.012, rng_fire.randrange(1000)),
                                                                            2500), 0.0005, 0, 4)
    save_wav('fire', mix((envelope(lowpass(noise(0.45, 87), 900, 300), 0.06, 0.1, 1.2), 0.9), (crackle, 0.6)))
    ring = []
    for _ in range(4):
        ring += envelope(tone(0.05, 2100, 2100, 'square', vibrato=0.04, vibrato_rate=60), 0.002, 0, 0.8) + \
            silence(0.025)
    save_wav('ring', ring)
    # Doors (milestone 12.7): a creaking hinge as one opens; a wooden thud as one shuts.
    save_wav('door_open', mix((envelope(tone(0.32, 420, 300, 'saw', vibrato=0.08, vibrato_rate=22), 0.03, 0.05, 1.2),
                               0.5), (envelope(lowpass(noise(0.32, 88), 1400), 0.02, 0, 1.4), 0.4)))
    save_wav('door_close', mix((envelope(tone(0.16, 120, 60), 0.001, 0, 2.2), 1.0),
                               (envelope(lowpass(noise(0.12, 89), 1600, 500), 0.001, 0, 2), 0.8)))
    # Locks (milestone 12.7): a locked handle rattling; a pick's ticks; a crowbar splitting a door frame.
    rattle = []
    for step in range(3):
        rattle += envelope(highpass(noise(0.03, 90 + step), 1800), 0.001, 0, 3) + silence(0.035)
    save_wav('rattle', mix((rattle, 0.9), (envelope(tone(0.2, 300, 260, 'square'), 0.002, 0, 2), 0.15)))
    picks = []
    for step in range(4):
        picks += envelope(tone(0.012, 3200 - step * 150, 2600, 'square'), 0.0005, 0, 4) + silence(0.06)
    save_wav('lockpick', picks)
    save_wav('crowbar', mix((envelope(lowpass(noise(0.35, 95), 2200, 500), 0.001, 0, 1.6), 1.0),
                            (envelope(tone(0.25, 180, 60, 'saw'), 0.002, 0, 1.8), 0.7)))
    # Chopping (milestone 12.7 stage 5): an axe biting wood; a tree cracking and crashing down.
    save_wav('chop', mix((envelope(tone(0.09, 260, 140), 0.001, 0, 2.5), 0.9),
                         (envelope(lowpass(noise(0.07, 96), 2400), 0.001, 0, 3), 0.8)))
    save_wav('timber', mix((envelope(highpass(noise(0.25, 97), 900), 0.002, 0, 2.2), 0.7),
                           (silence(0.18) + envelope(lowpass(noise(0.5, 98), 900, 200), 0.005, 0, 1.4), 1.0),
                           (silence(0.18) + envelope(tone(0.4, 90, 40), 0.003, 0, 1.6), 0.8)))
    # Drowning (milestone 13): bubbles rising, three short blips that climb.
    blip = [envelope(tone(0.045, 260 + i * 120, 700 + i * 180), 0.004, 0, 2) for i in range(3)]
    save_wav('bubbles', blip[0] + silence(0.05) + blip[1] + silence(0.035) + blip[2])
    # A flock taking off: quick soft wingbeats over a rush of air.
    beats = []
    for i in range(7):
        beats += envelope(lowpass(noise(0.035, 120 + i), 1400), 0.003, 0, 2) + silence(0.025)
    save_wav('flutter', mix((beats, 0.9), (envelope(highpass(noise(len(beats) / 22050.0, 127), 1800), 0.05, 0.1, 1.2),
                                           0.3)))
    # Milestone 14 sound pass: a zombie's feet dragging close by; glass crunching underfoot; rain drumming on a
    # roof (played over and over indoors while it rains); a gust of wind in the wild.
    save_wav('shuffle', mix((envelope(lowpass(noise(0.24, 140), 900, 300), 0.05, 0, 1.4), 1.0),
                            (silence(0.1) + envelope(lowpass(noise(0.12, 141), 1400, 500), 0.02, 0, 1.8), 0.6)))
    crunch = []
    rng = random.Random(142)
    for _ in range(4):
        crunch += envelope(highpass(noise(0.018, rng.randrange(1000)), 2600), 0.0005, 0, 3) + silence(0.012)
    save_wav('glass_step', mix((crunch, 1.0), (envelope(lowpass(noise(0.12, 143), 1200), 0.002, 0, 2), 0.3)))
    patter = [0.0] * int(RATE * 1.2)
    rng = random.Random(144)
    for _ in range(70):
        start = rng.randrange(len(patter) - 200)
        tap = envelope(highpass(noise(0.006, rng.randrange(1000)), 1800), 0.0005, 0, 3)
        gain = rng.uniform(0.3, 1.0)
        for i, v in enumerate(tap):
            patter[start + i] += v * gain
    save_wav('rain_roof', mix((patter, 1.0), (lowpass(noise(1.2, 145), 2200), 0.25)))
    gust = lowpass(noise(1.1, 146), 250, 900) + lowpass(noise(1.1, 147), 900, 250)
    save_wav('wind', envelope(gust, 0.7, 0, 1.3))
    # A flashlight's switch: a short, high, dry click.
    save_wav('click', mix((envelope(highpass(noise(0.012, 74), 3000), 0.0005, 0, 4), 0.8),
                          (envelope(tone(0.012, 2400, 1800, 'square'), 0.0005, 0, 4), 0.4)))
