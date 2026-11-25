"""Music (milestone 14, docs/milestone-14-plan.md stage 6): five short, sparse, original loops for the GBA's old (DMG)
sound channels, written as 4-channel MOD files that Butano converts for GBT Player (mod2gbt) into dmg_audio/.

Each song is chords, a bass line and a written melody, composed here note by note rather than generated at random,
with the accompaniment kept below the melody:
  channel 1  pulse, 25% duty: the melody
  channel 2  pulse, 50% or 12.5% duty: plucked chord tones or a quiet drone
  channel 3  wave (triangle; it sounds an octave below the pulses): the bass
  channel 4  noise: soft hats and a thud, only in the title and the chase

Device check (2026-11-25): GBT Player's note names run an octave low (its C3 is 65 Hz) and the wave channel sounds an
octave lower again, so the bass sat at 50-110 Hz and the chords at 100-200 Hz, quiet. A handheld's small speaker plays
little under 400-500 Hz, and the music sounded muffled on the Miyoo Mini. The bass and the chords now sit an octave
higher, with the chords at twice their volume (version B of the A/B/C sound test); the melodies are unchanged.

GBT Player takes only its own instruments: samples 1-4 are the pulse duties, 8-15 the waves, 16-31 the noise kits, so
the MOD's sample headers stay empty. Each song's speed is in frames per row (its .json turns off mod2gbt's 50 Hz to
60 Hz speed conversion). Notes run from C3 to B8 in GBT Player's naming. The files are replaceable by hand-made ones
(same names) later.
"""
import json
import os
import struct

from common import ROOT, write_if_changed

DMG_AUDIO = os.path.join(ROOT, 'dmg_audio')

# mod2gbt's period table: index 0 is GBT Player's C3.
PERIODS = [
    1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960, 907,
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
    428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
    214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
    107, 101, 95, 90, 85, 80, 75, 71, 67, 63, 60, 56,
    53, 50, 47, 45, 42, 40, 37, 35, 33, 31, 30, 28,
]
SEMITONES = {'C': 0, 'C#': 1, 'D': 2, 'D#': 3, 'E': 4, 'F': 5, 'F#': 6, 'G': 7, 'G#': 8, 'A': 9, 'A#': 10, 'B': 11}

# GBT Player's instruments, as MOD sample numbers.
PULSE_12 = 4        # 12.5% duty (sample & 3 == 0)
PULSE_25 = 1
PULSE_50 = 2
WAVE_TRIANGLE = 13  # wave 5
NOISE_HAT = 28      # kit 12: 15-bit noise, brightest
NOISE_THUD = 26     # kit 10: 15-bit noise, low

ROWS_PER_PATTERN = 64

CHORDS = {
    'Am': ['A', 'C', 'E'], 'F': ['F', 'A', 'C'], 'C': ['C', 'E', 'G'], 'G': ['G', 'B', 'D'],
    'Dm': ['D', 'F', 'A'], 'E': ['E', 'G#', 'B'], 'Em': ['E', 'G', 'B'], 'Bb': ['A#', 'D', 'F'],
    'Gm': ['G', 'A#', 'D'], 'A': ['A', 'C#', 'E'], 'D': ['D', 'F#', 'A'],
}


def note_index(name):
    pitch, octave = name[:-1], int(name[-1])
    index = (octave - 3) * 12 + SEMITONES[pitch]
    assert 0 <= index < len(PERIODS), name
    return index


def mod_volume(level, wave=False):
    """MOD volume (0-64) for a GB volume: 0-15 on the pulses and noise; 0, 25, 50, 75 or 100% on the wave."""
    return min(64, (level * 64) // 100) if wave else min(64, (level * 64 + 7) // 15)


class Song:
    def __init__(self, name, speed, bars, rows_per_bar=16):
        self.name = name
        self.speed = speed
        self.rows_per_bar = rows_per_bar
        self.rows = bars * rows_per_bar
        assert self.rows % ROWS_PER_PATTERN == 0, (name, self.rows)
        self.cells = [[None] * 4 for _ in range(self.rows)]
        # The speed, on the noise channel's first row (nothing else goes there).
        self.cells[0][3] = (0, 0, 0xF, speed)

    def note(self, channel, row, name, sample, volume):
        if row >= self.rows:
            return
        wave = channel == 2
        effect, param = 0xC, mod_volume(volume, wave)
        assert self.cells[row][channel] is None or self.cells[row][channel][1] == 0, (self.name, channel, row)
        self.cells[row][channel] = (sample, PERIODS[note_index(name)], effect, param)

    def hit(self, row, sample, volume):
        """A noise hit (the note does not matter on channel 4)."""
        if row < self.rows and self.cells[row][3] is None:
            self.cells[row][3] = (sample, PERIODS[note_index('C5')], 0xC, mod_volume(volume))

    def cut(self, channel, row):
        if row < self.rows and self.cells[row][channel] is None:
            self.cells[row][channel] = (0, 0, 0xE, 0xC1)

    def line(self, channel, start, tokens, step, sample, volume, gate=None):
        """Tokens a step of rows each: a note, '-' to hold it, '.' to stop. A gate cuts each note after that many
        rows."""
        for index, token in enumerate(tokens.split()):
            row = start + index * step
            if token == '-':
                continue
            if token == '.':
                self.cut(channel, row)
                continue
            self.note(channel, row, token, sample, volume)
            if gate is not None and gate < step:
                self.cut(channel, row + gate)

    def bar_row(self, bar):
        return bar * self.rows_per_bar

    def write(self):
        patterns = self.rows // ROWS_PER_PATTERN
        out = bytearray()
        out += self.name.encode('ascii')[:20].ljust(20, b'\0')
        for _ in range(31):
            # Empty sample headers: name, length 0, finetune 0, volume 64, loop 0 / 1.
            out += b'\0' * 22 + struct.pack('>HBBHH', 0, 0, 64, 0, 1)
        out += bytes([patterns, 127])
        out += bytes(range(patterns)) + bytes(128 - patterns)
        out += b'M.K.'
        for row in range(self.rows):
            for channel in range(4):
                cell = self.cells[row][channel]
                sample, period, effect, param = cell if cell else (0, 0, 0, 0)
                out += bytes([(sample & 0xF0) | ((period >> 8) & 0x0F), period & 0xFF,
                              ((sample & 0x0F) << 4) | (effect & 0x0F), param])
        os.makedirs(DMG_AUDIO, exist_ok=True)
        write_if_changed(os.path.join(DMG_AUDIO, self.name + '.mod'), bytes(out))
        settings = json.dumps({'mod_speed_conversion': False}, indent=4) + '\n'
        write_if_changed(os.path.join(DMG_AUDIO, self.name + '.json'), settings.encode('ascii'))
        return self.rows * self.speed / 60.0


def bass(song, progression, octave, volume, hold, fifth_at=None, step=None, gate=None):
    """The chord roots on the wave channel: one held note a bar (or the fifth late in the bar), or repeated notes."""
    for bar, chord in enumerate(progression):
        root = CHORDS[chord][0] + str(octave)
        start = song.bar_row(bar)
        if step:
            song.line(2, start, ' '.join([root] * (song.rows_per_bar // step)), step, WAVE_TRIANGLE, volume, gate)
            continue
        song.note(2, start, root, WAVE_TRIANGLE, volume)
        if fifth_at is not None:
            fifth = CHORDS[chord][2] + str(octave + (1 if SEMITONES[CHORDS[chord][2]] < SEMITONES[CHORDS[chord][0]] else 0))
            song.cut(2, start + hold)
            song.note(2, start + fifth_at, fifth, WAVE_TRIANGLE, volume)
            song.cut(2, start + song.rows_per_bar - 1)
        else:
            song.cut(2, start + hold)


def arpeggio(song, progression, octave, step, gate, volume, sample=PULSE_50, order=(0, 1, 2, 1)):
    """Plucked chord tones on the second pulse."""
    for bar, chord in enumerate(progression):
        tones = CHORDS[chord]
        notes = []
        for index in range(song.rows_per_bar // step):
            tone = tones[order[index % len(order)]]
            up = SEMITONES[tone] < SEMITONES[tones[0]]
            notes.append(tone + str(octave + (1 if up else 0)))
        song.line(1, song.bar_row(bar), ' '.join(notes), step, sample, volume, gate)


def drone(song, progression, octave, volume, hold, tone=2, sample=PULSE_12):
    """One quiet held chord tone a bar on the second pulse."""
    for bar, chord in enumerate(progression):
        name = CHORDS[chord][tone]
        up = SEMITONES[name] < SEMITONES[CHORDS[chord][0]]
        song.note(1, song.bar_row(bar), name + str(octave + (1 if up else 0)), sample, volume)
        song.cut(1, song.bar_row(bar) + hold)


def melody(song, bars, step, volume, first_bar=0, sample=PULSE_25):
    for offset, tokens in enumerate(bars):
        song.line(0, song.bar_row(first_bar + offset), tokens, step, sample, volume)


def title():
    """A minor, about 43 s, looping: chords and bass alone for 8 bars, then the melody over soft hats."""
    progression = ['Am', 'F', 'C', 'G', 'Am', 'F', 'Dm', 'E'] * 2
    song = Song('title', 10, len(progression))
    bass(song, progression, 5, 75, 11, fifth_at=12)
    arpeggio(song, progression, 5, 2, 1, 8)
    melody(song, [
        'E5 - - - D5 - C5 -',
        'A4 - - - - - . .',
        'G4 - C5 - E5 - G5 -',
        'D5 - - - - - . .',
        'E5 - - - G5 - E5 -',
        'F5 - - - E5 - C5 -',
        'D5 - - - F5 - E5 -',
        'G#4 - - - - - - .',
    ], 2, 9, first_bar=8)
    for bar in range(8, len(progression)):
        for beat in (4, 12):
            song.hit(song.bar_row(bar) + beat, NOISE_HAT, 2)
    song.cut(0, song.rows - 1)
    return song


def calm_day():
    """A minor, about 45 s, played once now and then: long notes, no drums."""
    progression = ['Am', 'Em', 'F', 'C', 'Dm', 'Am', 'Em', 'Am', 'F', 'C', 'Dm', 'E']
    song = Song('calm', 14, len(progression))
    bass(song, progression, 5, 50, 14)
    arpeggio(song, progression, 5, 4, 3, 6, order=(0, 2, 1, 2))
    melody(song, [
        'C5 - - -',
        'E5 - D5 C5',
        'D5 - - F5',
        'E5 - - -',
        'B4 - - -',
        'A4 - - .',
        'A5 - G5 F5',
        'E5 - - -',
        'F5 - E5 D5',
        'E5 - - .',
    ], 4, 8, first_bar=2)
    for channel in range(3):
        song.cut(channel, song.rows - 2)
    return song


def night():
    """D minor, about 58 s, played once now and then: low, slow and sparse over a drone."""
    progression = ['Dm', 'Dm', 'Bb', 'A', 'Gm', 'Dm', 'Bb', 'A', 'Dm', 'Gm', 'A', 'Dm']
    song = Song('night', 18, len(progression))
    bass(song, progression, 5, 50, 15)
    drone(song, progression, 4, 4, 15)
    melody(song, [
        '. . . .',
        'A4 - - -',
        'F4 - - D4',
        'E4 - - -',
        '. . G4 -',
        'A4 - F4 -',
        'D5 - - -',
        'C#5 - - .',
        '. . . .',
        'A#4 - - -',
        'A4 - E4 -',
        'D4 - - .',
    ], 4, 7)
    for channel in range(3):
        song.cut(channel, song.rows - 1)
    return song


def chase():
    """A minor, about 32 s, looping while a horde chases: driving bass, a plucked ostinato, a thud and hats."""
    progression = ['Am', 'Am', 'F', 'E', 'Am', 'Am', 'Dm', 'E', 'F', 'F', 'E', 'E', 'Am', 'C', 'D', 'E',
                   'Am', 'Am', 'F', 'E']
    song = Song('chase', 6, len(progression))
    bass(song, progression, 5, 100, 0, step=2, gate=1)
    arpeggio(song, progression, 5, 2, 1, 10, sample=PULSE_12, order=(0, 2, 1, 2, 0, 2, 1, 0))
    melody(song, [
        'E5 - - -', 'C5 - D5 E5', 'F5 - - -', 'E5 - D5 B4',
        'A4 - - -', 'C5 - E5 A5', 'F5 - - D5', 'G#5 - - -',
        'A5 - G5 F5', 'E5 - - -', 'G#5 - F5 E5', 'D5 - B4 -',
        'A4 - - -', 'E5 - G5 -', 'F#5 - - A5', 'G#5 - - -',
        'E5 - - -', 'A5 - - -', 'C6 - B5 A5', 'G#5 - - .',
    ], 4, 10)
    for bar in range(len(progression)):
        start = song.bar_row(bar)
        for beat in (0, 8):
            song.hit(start + beat, NOISE_THUD, 9)
        for beat in (4, 12):
            song.hit(start + beat, NOISE_HAT, 4)
    return song


def death():
    """A minor, about 34 s, played once under the death screen: a slow falling line that ends on the tonic."""
    progression = ['Am', 'G', 'F', 'E', 'Dm', 'Am', 'E', 'Am']
    song = Song('death', 16, len(progression))
    bass(song, progression, 5, 75, 15)
    drone(song, progression, 5, 6, 15, tone=1, sample=PULSE_50)
    melody(song, [
        'E5 - - -',
        'D5 - - -',
        'C5 - - -',
        'B4 - G#4 -',
        'A4 - F4 -',
        'E4 - - -',
        'G#4 - B4 -',
        'A4 - - .',
    ], 4, 8)
    for channel in range(3):
        song.cut(channel, song.rows - 2)
    return song


SONGS = [title, calm_day, night, chase, death]


def build_music():
    lengths = {}
    for make in SONGS:
        song = make()
        lengths[song.name] = song.write()
    return lengths


if __name__ == '__main__':
    for name, seconds in build_music().items():
        print('%-6s %.1f s' % (name, seconds))
