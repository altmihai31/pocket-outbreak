# Device sound check (2026-11-25)

**Report:** the music sounds muffled on the Miyoo Mini; check all sounds.

## Cause

A handheld's small speaker plays little under 400-500 Hz. Measured from mGBA recordings, 58-82% of each tune's energy
sat under 400 Hz: GBT Player's note names run an octave low (its C3 is 65 Hz), the wave channel sounds an octave lower
again (the bass at 50-110 Hz), the chords played at 2-5 of 15, and the DMG channels mix at half level. On the speaker
only a quiet melody was left. Playtests 5 and 6 changed no audio.

21 of the 74 sound effects had 79-100% of their energy under 400 Hz (zombie moan, growl, death and choke, blunt and
heavy hits, door close, drop, heartbeat, engine, thunder, stomp and others): faint or missing on the device. The other
53 were fine. The maxmod mixing rate (16 kHz) matches the 16 kHz effects and is not a cause.

## A/B/C sound test

A small ROM (built outside the repo) played every tune and each of the 21 effects in three versions, switchable while
playing:

- Music: A as it was; **B the bass and the chords an octave up, the chords twice as loud**; C everything an octave up
  and the DMG master at full.
- Effects: A as they were; **B harmonics of the low part added (400-3500 Hz), a gentle cut at 90 Hz, same loudness**;
  C more harmonics and a cut at 160 Hz.

**Picked: B for both.**

## Built

- `tools/assets/music.py`: the bass and the chords (or drone) an octave higher in every song, the chords at twice
  their volume. The `.mod` files are byte for byte version B. Measured: under 400 Hz now 50-69% (was 58-82%), and the
  part a small speaker carries is 1-5 dB louder.
- `tools/assets/speaker.py`: the effects pass, applied to the 21 sounds as the generators (`audio`, `zombie_sounds`,
  `animal_sounds`) write them; byte for byte version B. Under 400 Hz now 40-87% (was 79-100%).
