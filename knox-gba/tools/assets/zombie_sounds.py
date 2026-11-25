"""Playtest 2: five zombie sounds made to the user's specification (moan, breath, growl, choke, death), exported with
the user's settings: 22050 Hz, unsigned 8-bit mono, a 100 Hz high-pass and a 7000 Hz low-pass, peak at -3 dB.

Each spec's "Final Volume" is not baked into the file: 8 bits leave little room, and -12 dB in the samples would throw
away two of them. The files keep the -3 dB peak and the game plays them at their final volume (FINAL_DB below).

Built into the game by tools/gen_assets.py (audio.build_audio calls build_zombie_sounds: audio/zombie_*.wav); run on
its own it also writes the listening copies:

    python tools/assets/zombie_sounds.py      -> sound-candidates/zombie/*.wav, all.wav

Interpretations where the spec leaves room: levels in dB are relative to the main tone; distortion N% blends N% of a
tanh-saturated copy into the dry signal; soft clipping at D dB drives tanh by D dB; reverb N% is the wet share of a
small Schroeder reverb (4 combs, 2 all-passes; room size and damping from the death groan's spec, defaults elsewhere);
pitch in semitones glides from the base pitch; a harmonic follows the main tone's pitch; the reverb runs inside the
duration and the fades come last, so each sound is exactly its specified length.
"""
import io
import math
import os
import random
import wave

from common import AUDIO, write_if_changed

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(os.path.dirname(HERE)), 'sound-candidates', 'zombie')
RATE = 22050
PEAK = 10 ** (-3 / 20)

# The spec's final volume of each sound, applied at playback.
FINAL_DB = {'zombie_moan': -12, 'zombie_breath': -16, 'zombie_growl': -6, 'zombie_choke': -8, 'zombie_death': -9}


def db(value):
    return 10 ** (value / 20)


def count(seconds):
    return int(RATE * seconds)


def glide(n, points):
    """A value per sample through breakpoints [(t 0-1, value)], exponential between them (pitch glides)."""
    out = []
    for i in range(n):
        t = i / max(1, n - 1)
        for (t0, v0), (t1, v1) in zip(points, points[1:]):
            if t0 <= t <= t1:
                u = (t - t0) / max(1e-9, t1 - t0)
                out.append(v0 * (v1 / v0) ** u)
                break
    return out


def sine(freqs, phase=0.0):
    out = []
    for f in freqs:
        phase += f / RATE
        out.append(math.sin(2 * math.pi * phase))
    return out


def white(n, seed):
    rng = random.Random(seed)
    return [rng.uniform(-1, 1) for _ in range(n)]


def pink(n, seed):
    """Paul Kellet's filter on white noise."""
    b = [0.0] * 7
    out = []
    for w in white(n, seed):
        b[0] = 0.99886 * b[0] + w * 0.0555179
        b[1] = 0.99332 * b[1] + w * 0.0750759
        b[2] = 0.96900 * b[2] + w * 0.1538520
        b[3] = 0.86650 * b[3] + w * 0.3104856
        b[4] = 0.55000 * b[4] + w * 0.5329522
        b[5] = -0.7616 * b[5] - w * 0.0168980
        out.append(b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362)
        b[6] = w * 0.115926
    return unit(out)


def brown(n, seed):
    out, y = [], 0.0
    for w in white(n, seed):
        y = (y + 0.02 * w) / 1.002
        out.append(y)
    return unit(out)


def unit(samples):
    """Scaled to an RMS of 0.5, so noise levels in dB compare with a sine's (RMS 0.707)."""
    level = math.sqrt(sum(s * s for s in samples) / max(1, len(samples)))
    return [s * 0.5 / max(level, 1e-9) for s in samples]


def mix(*parts):
    n = max(len(p) for p, _ in parts)
    out = [0.0] * n
    for samples, gain in parts:
        for i, s in enumerate(samples):
            out[i] += s * gain
    return out


def biquad(samples, kind, cutoff, q=0.7071):
    """12 dB/octave Butterworth (RBJ cookbook)."""
    w = 2 * math.pi * cutoff / RATE
    alpha = math.sin(w) / (2 * q)
    cos = math.cos(w)
    b = [(1 + cos) / 2, -(1 + cos), (1 + cos) / 2] if kind == 'high' else [(1 - cos) / 2, 1 - cos, (1 - cos) / 2]
    a = [1 + alpha, -2 * cos, 1 - alpha]
    out = []
    x1 = x2 = y1 = y2 = 0.0
    for x in samples:
        y = (b[0] * x + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2) / a[0]
        x2, x1, y2, y1 = x1, x, y1, y
        out.append(y)
    return out


def peak(samples):
    return max(1e-9, max(abs(s) for s in samples))


def distortion(samples, percent):
    p = peak(samples)
    return [s + (percent / 100) * (p * math.tanh(3 * s / p) - s) for s in samples]


def soft_clip(samples, drive_db):
    p = peak(samples)
    g = db(drive_db)
    return [p * math.tanh(g * s / p) / math.tanh(g) for s in samples]


def reverb(samples, wet_percent=None, room=0.3, damping=0.5, wet_db=None, dry_db=0.0, tail=0.5):
    """A small Schroeder reverb: 4 damped combs in parallel, 2 all-passes in series."""
    samples = samples + [0.0] * count(tail)
    feedback = 0.70 + 0.28 * room
    wet = [0.0] * len(samples)
    for delay_ms in (29.7, 37.1, 41.1, 43.7):
        d = count(delay_ms / 1000)
        buf = [0.0] * d
        index, low = 0, 0.0
        for i, s in enumerate(samples):
            out = buf[index]
            low = out * (1 - damping) + low * damping
            buf[index] = s + low * feedback
            index = (index + 1) % d
            wet[i] += out * 0.25
    for delay_ms, gain in ((5.0, 0.7), (1.7, 0.7)):
        d = count(delay_ms / 1000)
        buf = [0.0] * d
        index = 0
        for i, s in enumerate(wet):
            b = buf[index]
            out = -s + b
            buf[index] = s + b * gain
            index = (index + 1) % d
            wet[i] = out
    if wet_db is not None:
        wet_gain, dry_gain = db(wet_db), db(dry_db)
    else:
        wet_gain, dry_gain = wet_percent / 100, 1 - wet_percent / 100
    return [s * dry_gain + w * wet_gain for s, w in zip(samples, wet)]


def fades(samples, fade_in, fade_out):
    n = len(samples)
    a, b = count(fade_in), count(fade_out)
    out = []
    for i, s in enumerate(samples):
        g = 1.0
        if a and i < a:
            g = i / a
        if b and i > n - b:
            g = min(g, (n - i) / b)
        out.append(s * g)
    return out


def export(samples):
    samples = biquad(biquad(samples, 'high', 100), 'low', 7000)
    p = peak(samples)
    return [s * PEAK / p for s in samples]


def wav_bytes(samples, bits=8):
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(bits // 8)
        w.setframerate(RATE)
        if bits == 8:
            w.writeframes(bytes(max(0, min(255, int(round(128 + s * 127)))) for s in samples))
        else:
            w.writeframes(b''.join(max(-32768, min(32767, int(round(s * 32767)))).to_bytes(2, 'little', signed=True)
                                   for s in samples))
    return buf.getvalue()


def semitones(base, steps):
    return base * 2 ** (steps / 12)


# The five sounds, as specified.

def zombie_moan(seed=1):
    """Distant undead moan: 2.8 s, sine 105 -> 82 Hz, its octave at -18 dB, pink/brown noise at -30 dB, 5%
    distortion, 12% reverb, 350 ms in, 800 ms out."""
    n = count(2.8)
    pitch = glide(n, [(0, 105), (1, 82)])
    tone = mix((sine(pitch), 1.0), (sine([2 * f for f in pitch]), db(-18)))
    noise = mix((pink(n, seed), 0.5), (brown(n, seed + 1), 0.5))
    body = distortion(mix((tone, 1.0), (noise, db(-30))), 5)
    return fades(reverb(body, 12, tail=0), 0.35, 0.8)


def zombie_breath(seed=2):
    """Idle breathing: 2.4 s, pink noise 250-2500 Hz, sine 140 Hz at -25 dB, two breaths, 3% distortion, 5% reverb."""
    n = count(2.4)
    air = biquad(biquad(pink(n, seed), 'high', 250), 'low', 2500)
    tone = sine([140.0] * n)
    body = mix((air, 1.0), (tone, db(-25)))
    # Two cycles: each a breath in (rising, softer) then a longer breath out, with a pause after.
    cycle = n // 2
    shape = []
    for i in range(n):
        t = (i % cycle) / cycle
        if t < 0.35:
            g = 0.55 * math.sin(math.pi * t / 0.35)
        elif t < 0.42:
            g = 0.0
        elif t < 0.85:
            g = math.sin(math.pi * (t - 0.42) / 0.43)
        else:
            g = 0.0
        shape.append(g)
    body = distortion([s * g for s, g in zip(body, shape)], 3)
    return reverb(body, 5, tail=0)


def zombie_growl(seed=3):
    """Detection growl: 1.5 s, sine 95 Hz falling 4 semitones, its octave at -12 dB, brown noise at -24 dB, 12%
    distortion, soft clip at 4 dB, 8% reverb, 80 ms in, 350 ms out."""
    n = count(1.5)
    pitch = glide(n, [(0, 95), (1, semitones(95, -4))])
    tone = mix((sine(pitch), 1.0), (sine([2 * f for f in pitch]), db(-12)))
    body = mix((tone, 1.0), (brown(n, seed), db(-24)))
    body = soft_clip(distortion(body, 12), 4)
    return fades(reverb(body, 8, tail=0), 0.08, 0.35)


def zombie_choke(seed=4):
    """Damage / choking: 0.9 s, brown noise, sine 180 Hz at -15 dB and 360 Hz, pitch up 2 semitones then down 5,
    120-1800 Hz, 15% distortion, soft clip at 5 dB, 5% reverb."""
    n = count(0.9)
    pitch = glide(n, [(0, 180), (0.3, semitones(180, 2)), (1, semitones(180, -5))])
    tone = mix((sine(pitch), db(-15)), (sine([2 * f for f in pitch]), db(-15)))
    body = mix((brown(n, seed), 1.0), (tone, 1.0))
    body = biquad(biquad(body, 'high', 120), 'low', 1800)
    body = soft_clip(distortion(body, 15), 5)
    return fades(reverb(body, 5, tail=0), 0.02, 0.15)


def zombie_death(seed=5):
    """Death groan: 2.2 s, sine 110 -> 55 Hz, its octave at -12 dB, pink noise at -27 dB, 8% distortion, reverb (room
    35%, damping 65%, wet -12 dB, dry 0 dB), 150 ms in, 900 ms out."""
    n = count(2.2)
    pitch = glide(n, [(0, 110), (1, 55)])
    tone = mix((sine(pitch), 1.0), (sine([2 * f for f in pitch]), db(-12)))
    body = distortion(mix((tone, 1.0), (pink(n, seed), db(-27))), 8)
    return fades(reverb(body, room=0.35, damping=0.65, wet_db=-12, dry_db=0, tail=0), 0.15, 0.9)


SOUNDS = [('zombie_moan', zombie_moan), ('zombie_breath', zombie_breath), ('zombie_growl', zombie_growl),
          ('zombie_choke', zombie_choke), ('zombie_death', zombie_death)]


def build_zombie_sounds():
    """The five sounds into the game's audio folder (only rewritten when they change)."""
    for name, make in SOUNDS:
        write_if_changed(os.path.join(AUDIO, name + '.wav'), wav_bytes(export(make())))


def main():
    os.makedirs(OUT, exist_ok=True)
    reel = []
    gap = [0.0] * count(0.8)
    for name, make in SOUNDS:
        samples = export(make())
        with open(os.path.join(OUT, name + '.wav'), 'wb') as f:
            f.write(wav_bytes(samples))
        # The reel plays each at its final volume, as the game will.
        reel += [s * db(FINAL_DB[name] + 3) for s in samples] + gap
        print('%-14s %.2f s, final volume %d dB' % (name, len(samples) / RATE, FINAL_DB[name]))
    # 16-bit, so the quiet ones are not heard through 8-bit hiss the game (mixing at its own precision) won't have.
    with open(os.path.join(OUT, 'all.wav'), 'wb') as f:
        f.write(wav_bytes(reel, 16))


if __name__ == '__main__':
    main()
