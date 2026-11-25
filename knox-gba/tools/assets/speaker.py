"""Speaker pass (2026-11-25): sounds that live under 400 Hz get harmonics a handheld's speaker can play.

On the Miyoo Mini these sounds were faint or missing: 79-100% of their energy sat under 400 Hz, and a small speaker
plays little below 400-500 Hz. Each one keeps its sound and gains a copy of its low part squared off (its harmonics),
band-passed to 400-3500 Hz and following its loudness, so the ear hears the thump through the speaker. A gentle cut at
90 Hz takes away rumble the speaker can't play; the loudness (RMS) stays as it was. This is version B of the A/B/C
sound test the user picked on the device.

Works on the finished 8-bit WAV bytes, so every generator (audio, zombie_sounds, animal_sounds) can pass its output
through finish(). Pure Python: the CI image has no numpy.
"""
import io
import math
import wave

SOUNDS = {
    'bang', 'boar_grunt', 'chop', 'denied', 'door_close', 'drop', 'engine', 'grab', 'growl', 'gulp', 'heartbeat',
    'hit_blunt', 'hit_heavy', 'starter', 'stomp', 'thunder', 'timber', 'zombie_choke', 'zombie_death', 'zombie_growl',
    'zombie_moan',
}

HARMONICS = 0.8         # the harmonics' level against the low part's
DRIVE = 10              # how hard the low part is squared off
PEAK = 0.9

# Butterworth filters as second-order sections (b0, b1, b2, a0, a1, a2), from scipy.signal.butter(..., output='sos'):
# low: 2nd order low-pass at 400 Hz; envelope: 1st order low-pass at 30 Hz; band: 2nd order band-pass 400-3500 Hz;
# high: 2nd order high-pass at 90 Hz.
FILTERS = {
    16000: {
        'low': [(0.005542717210280682, 0.011085434420561363, 0.005542717210280682, 1.0, -1.7786317778245846,
                 0.8008026466657073)],
        'envelope': [(0.00585605892206321, 0.00585605892206321, 0.0, 1.0, -0.9882878821558736, 0.0)],
        'band': [(0.19654994910855889, 0.39309989821711777, 0.19654994910855889, 1.0, -0.34033923617209766,
                  0.25018285374830124),
                 (1.0, -2.0, 1.0, 1.0, -1.782639204420836, 0.8088306761684041)],
        'high': [(0.9753184320492833, -1.9506368640985665, 0.9753184320492833, 1.0, -1.9500275914987848,
                  0.9512461366983469)],
    },
    22050: {
        'low': [(0.0030028859801922276, 0.006005771960384455, 0.0030028859801922276, 1.0, -1.8391167973252027,
                 0.8511283412459717)],
        'envelope': [(0.004256109851111111, 0.004256109851111111, 0.0, 1.0, -0.9914877802977778, 0.0)],
        'band': [(0.11814885074930817, 0.23629770149861634, 0.11814885074930817, 1.0, -0.8065675310783161,
                  0.3415982228729065),
                 (1.0, -2.0, 1.0, 1.0, -1.8440913820929186, 0.8584410072626112)],
        'high': [(0.982029206321149, -1.964058412642298, 0.982029206321149, 1.0, -1.9637354371385563,
                  0.9643813881460399)],
    },
}


def sosfilt(sections, samples):
    """scipy.signal.sosfilt: each section in transposed direct form II, from rest."""
    out = list(samples)
    for b0, b1, b2, _, a1, a2 in sections:
        z1 = z2 = 0.0
        for i, x in enumerate(out):
            y = b0 * x + z1
            z1 = b1 * x - a1 * y + z2
            z2 = b2 * x - a2 * y
            out[i] = y
    return out


def mean_square(samples):
    return sum(s * s for s in samples) / max(1, len(samples))


def brighten(samples, rate):
    f = FILTERS[rate]
    low = sosfilt(f['low'], samples)
    envelope = [math.sqrt(max(0.0, v)) for v in sosfilt(f['envelope'], [s * s for s in low])]
    harmonics = [math.tanh(DRIVE * s / (e + 1e-3)) for s, e in zip(low, envelope)]
    harmonics = [h * e for h, e in zip(sosfilt(f['band'], harmonics), envelope)]
    scale = math.sqrt(mean_square(low) / (mean_square(harmonics) + 1e-12))
    out = [s + HARMONICS * h * scale for s, h in zip(sosfilt(f['high'], samples), harmonics)]
    gain = math.sqrt(mean_square(samples) / mean_square(out))
    out = [s * gain for s in out]
    peak = max(abs(s) for s in out)
    return [s * PEAK / peak for s in out] if peak > PEAK else out


def finish(name, wav_bytes):
    """The WAV as written, or brightened when the sound is one of SOUNDS."""
    if name not in SOUNDS:
        return wav_bytes
    with wave.open(io.BytesIO(wav_bytes)) as w:
        rate, data = w.getframerate(), w.readframes(w.getnframes())
        assert w.getsampwidth() == 1 and w.getnchannels() == 1, name
    samples = brighten([(b - 128) / 127 for b in data], rate)
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(rate)
        w.writeframes(bytes(max(0, min(255, round(128 + s * 127))) for s in samples))
    return buf.getvalue()
