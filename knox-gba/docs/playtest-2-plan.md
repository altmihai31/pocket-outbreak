# Playtest 2: groan, corpses, settings

The second batch of playtest notes (`../ideas`, "PART2", 2026-11-24), what causes each one, and the plan. User choices
are marked **(chosen)**. Follows `playtest-1-plan.md`.

## The notes

| # | Note | Cause | Plan |
|---|------|-------|------|
| 1 | The zombies now sound like wind | The moan picked in playtest 1 (A) is a voice shaped by vowel resonances, with breath mixed in. Played quietly (a third of full volume) through a small speaker, the breath and the resonances carry and the pitched voice under them does not, so it reads as a gust. | A second round of candidates, more voice and less air: stronger, steadier pitch, little or no breath, a growl in the throat. You pick by ear again; the playback volume can rise too. |
| 2 | A corpse doesn't wear the clothes of the zombie that was just killed | Corpses are ground tiles drawn in one fixed set of colours (purple clothes) whatever the zombie wore: the world's palettes have no room for five outfits, and baking each outfit into every ground would need about 100 more tiles. | On screen, a corpse is drawn as a sprite: the zombie's own lying frame, in its own outfit palette (the five already loaded, no new palettes) and its own body (long hair, beanie...), on the same spot and side the dying body eases onto, so the swap can't be seen. The ground tile stays underneath, redrawn in the lying sprite's exact outline so the sprite covers it; it is what shows if more corpses are on screen than the sprite pool holds. |
| 3 | A settings menu (turn off the music...) | There is none; the music and sound levels are fixed in the code. | A SETTINGS tab after MAP in the Start menu, and SETTINGS on the title screen **(chosen)**, with music volume, sound effects volume, screen shake and the CPU meter **(chosen)**. |

## Decisions (mine, not asked)

- **Settings are kept in the save without a format change.** The save rejects any slot written by another format version, so a version bump would make the current save unreadable. The settings (4 bytes) go at the end of the character data instead: the slot header already records that data's length, so a save written before them loads with the defaults. Saved with the character slots on every save; changed on the title screen with no save yet, they are written with the new world.
- **Levels:** music and sound effects each OFF / LOW / MEDIUM / FULL (0, 33, 66, 100% of today's levels), default FULL. Screen shake ON / OFF, default ON. CPU meter ON / OFF, default ON (as now).
- **Corpse sprites:** a pool of 16, given to the corpses nearest the player; the body is kept in the corpse's container record (spare bits of its "opened" byte, so old corpses read as body A: no format change).
- **The tab:** drawn like the other tabs (a list on the left, the selected setting's help on the right); Up/Down choose, Left/Right change, changes apply at once. Mockup first.

## Stages

1. **Groan, round 2:** 3-4 candidates in `sound-candidates/`, you pick.
2. **Corpses in their clothes:** the corpse decal redrawn as the lying sprite's outline; the body in the container record; the corpse sprite pool; a bench check that every corpse's sprite matches its zombie.
3. **Settings:** mockup; the settings store in the save; the Start menu tab and the title entry; music and sound volumes, shake, the CPU meter.

## Progress

**Stage 1 made 2026-11-24, waiting for a pick:** `tools/groan_candidates.py` writes round 2 to `sound-candidates/round2/` (`all.wav`: A, the moan in the game now, then E, F, G, H, two takes each).

- Why A reads as wind, measured: its pitch is barely there, a periodicity of 0.30 (the old buzz 0.70), drowned by jitter (5%), shimmer, breath and narrow vowel resonances that ring on their own.
- Round 2 keeps a vowel and puts a steady voice back: jitter 1-2%, no breath, wider resonances, the voice's own harmonics from 180 Hz up, a soft clip for grit, then a steep cut under 200 Hz (a handheld's speaker only buzzes there; the ear takes the pitch from the overtones). E voiced moan (round "uhhh-ohh"), F growl (E with a throat flutter and a little creak), G deep chesty groan ("ohhh"), H hungry (two beats, "uh... UHHH").
- Measured: periodicity 0.58-0.74 (A 0.31), 74-86% of the energy between 250 Hz and 3 kHz (the old buzz 8-15%), loudness about A's.
- **Round 3** (the user's export settings: 22050 Hz, unsigned 8-bit mono, peak at -3 dB, 100 Hz high-pass, 7000 Hz low-pass), `sound-candidates/round3/`: the same voices without round 2's steep 200 Hz cut and without the game's loudness cap, filtered with 12 dB/octave Butterworth filters. Checked: every file 22050 Hz, 8-bit, mono, peak -3.0 dB, nothing above 7 kHz. With the cut at 100 Hz they keep their low end (54-59% of the energy under 250 Hz): fuller through headphones, less of it carried by a small speaker. Periodicity unchanged (0.58-0.75; A 0.32).
- **Five sounds to the user's spec** (moan, breath, growl, choke, death, with durations, tones, noise, distortion, reverb, fades, final volumes and when each plays): `tools/zombie_sounds.py` writes them to `sound-candidates/zombie/` in the export format (22050 Hz, unsigned 8-bit, 100 Hz-7 kHz, peak -3 dB). Each final volume (-6 to -16 dB) is applied at playback rather than in the samples, to keep the 8 bits; `all.wav` (16-bit) plays them at those volumes. Checked: exact durations, pitch glides as specified (moan 102 to 86 Hz measured across it, growl 92 to 78, death 98 to 62). My readings of the spec are in the script's docstring. The moan, growl and death are mostly a sine at 55-110 Hz (64-89% of their energy under 100 Hz): full through headphones, faint on a handheld's speaker.
- Proposed wiring: moan every 8-15 s from one zombie in earshot (a shared timer, not every zombie, with 6 sound channels), breath every 4-7 s from the nearest unaware zombie within a few tiles, growl when a zombie notices the player, choke when one is hit and survives, death on a kill; panned by screen position like the groan now.
- **Built 2026-11-24, the user's five sounds** (they replace the groan; the round 2 and 3 candidates were not picked): `tools/assets/zombie_sounds.py` (moved from `tools/`) is part of the asset build (`audio.build_audio` calls `build_zombie_sounds`, writing `audio/zombie_*.wav`, byte for byte the copies heard); `groan_a.wav` and `groan_b.wav` are gone. New sound ids `BREATH`, `SNARL` (the growl; `GROWL` is the stomach), `CHOKE`, `DEATH_GROAN`; `GROAN` plays `zombie_moan` everywhere it did (the zombies' moan, the hordes waiting at the screen's edges). Volumes and timings in `zombie_sound_tuning` (`kx_zombies.h`), each at its specified final volume.
  - Moan: a shared timer, every 8-15 s, from a zombie picked at random within 160 px of the screen's middle (scaled by hearing); the per-zombie random groan is gone (the debug menu's `GROAN_CHANCE` no longer does anything).
  - Breath: every 4-7 s from the nearest zombie that hasn't noticed you, within 5 tiles.
  - Growl: when an unaware zombie notices you (at least 40 frames between growls).
  - Choke: when a zombie is hit and still standing; death groan: when one is killed (replacing the groan the player's hit used to play). Heard within 200 px of the screen's middle.
  - Each comes from its zombie's side, with the usual random pitch. Its own random source, so its timing doesn't disturb the game's; removing the per-zombie groan's draw did change the game's random sequence once (bench 2 now kills 1 zombie of 3 instead of 2).
  - Bench runs log `KXVOICE` for each: bench 1 moans every 10-11 s, growls 9 times, chokes and dies; bench 2 all five. Bench 1 551 per mille, no missed frames; bench 2 210, none.
 The groans play at a third of full volume (`kx_zombies.cpp`); that can rise too.

**Stage 2 built 2026-11-24** (untested on device):

- **The decal** (`world.decal_corpse`) is the zombie's own lying frame (body A) placed where the corpse sprite lies in its tile (its top 3 px down), in the decal bank's colours. Checked: none of its pixels fall outside any body's lying sprite, so the sprite always covers it.
- **The body is kept** in the corpse's container record (`container_record::body`, 2 bits beside `opened` in the same byte, which a save from before reads as body A: no format change; the record stays 8 bytes). `game::add_corpse` takes the body and says whether it made a record.
- **Corpse sprites** (`zombie_manager::_corpses`, 16): the zombie's lying frame for its body, in its outfit palette (the five already loaded), on the spot and side the dying body eases to, behind everything sorted by depth, shaded at night like zombies. A killed zombie's corpse gets one in the same frame its zombie goes; every 8 frames one pass over the containers gives sprites to the zombie corpses in view and takes them back from those gone or out of view; when all 16 are used, the one farthest from the player gives its up. Corpses of past survivors and event soldiers keep the decal (their colours aren't an outfit palette).
- **Checked:** bench 2's doctor keeps its white coat as a corpse (`docs/playtest-2-corpse.png`: dying, then lying under the player); bench 1's farmer with body C gets a sprite and its record says body 2 (`KXCORPSE sprite` lines). Bench 1 552 per mille (551), no missed frames; the scan frame puts the zombies' worst at 371 per mille of a frame (304). Bench 8's round trip matches.

**Stage 3 built 2026-11-24** (untested on device):

- **Mockup** `mockups/18-settings.png` (`tools/mockup_settings.py`): the tab, the title menu, the title's settings, the new world screen's Select line.
- **The store** (`kx_settings.h`): music and sound effects OFF / LOW / MEDIUM / FULL (0, 33, 66, 100% of today's levels), screen shake and the CPU meter on/off; defaults full, full, on, and the CPU meter as before (on in debug builds). Kept in the slot header's reserved bytes (a marker then the values, outside the header CRC), written with every save and read whenever the save's summary is (the title): a save from before has no marker and keeps the defaults. Changed with no save yet, they go out with the new world's first save; changed on the title with a save, with the next save.
- **Applied:** the music's every level is scaled (`music::apply_volume` re-applies it to what plays when the setting changes); every sound effect is scaled and none plays at OFF (`sfx::play`); `camera_controller::shake` does nothing when off; the CPU meter is the HUD's readout (set at the start of a game and when the setting changes; the debug menu still toggles the readout alone).
- **The tab** (`settings_screen`, sixth after MAP, the tab bar's gap now 5 px so six fit): the settings on the left (volumes with a three-step bar), the chosen one's values and what it does on the right; Up/Down choose, Left/Right change at once. **The title:** CONTINUE / NEW WORLD / SETTINGS with a save; without one, "SELECT GAME SETTINGS" under the new world panel. Both use the tab's rows (`settings_screen::draw_rows`, `handle`).
- **Checked:** bench 8 changes three settings before its save, scrambles them after the load and reads them back from the save's header: they match. Dumps (`docs/playtest-2-settings.png`): the tab (bench 3's tab tour, now six tabs), the title menu, the title's settings (DUMP=1 steps SOUND EFFECTS to MEDIUM and logs dump 9), the new world screen with its Select line. Bench 1 552 per mille, no missed frames.
- Not checked by ear: the volumes. mgba-sdl run from the command line gives the game an empty SRAM (it detects the save type on first access and doesn't load the file then), so a save copied next to a ROM can't be continued there; the title menu was dumped with a save faked for the run, not committed.

**Follow-up 2026-11-24** (the user: settings from the title didn't open when creating a world; what can be centred on the Map tab). Tested with real key presses in mGBA (`drive_mgba.py` in my scratchpad sends keys to mgba-sdl and grabs the screen):

- **Start on the title menu's SETTINGS opened and closed them in one frame:** Start opens them and also leaves them, and it was still down on their first frame. `title_settings` now waits a frame before reading the buttons.
- **The new world screen over a save had no way to the settings:** Select now opens them there too; its line under the panel holds "SELECT SETTINGS" on the left and the red "REPLACES THE SAVED WORLD" on the right (without a save, "SELECT GAME SETTINGS" centred as before).
- **Found on the way, older:** Start pressed to begin (character screen) or to CONTINUE also opened the Start menu on the game's first frame: a new game's input reader took no button as held before it. It now takes what is held on its first read as the previous state (`input_source::_primed`).
- **Map tab:** the view may go half a view past the world's edges (all unexplored there), so the cursor, and the survivor when the map opens on them, sits in the middle anywhere; it used to stop at the edge and leave a survivor near one in a corner. The WORLD settings column is centred on the map's height. Already centred: NO MAP, the grid and scale labels (in the map's bottom corners), the tab bar.
- `docs/playtest-2-fixes.png`: the new world screen with a save, its settings, the game started with Start (no menu), and the Map tab before and after.

**Tab polish 2026-11-24** (the user: centre the Health tab's figure; check every tab's alignment). Measured each tab's panels and what is drawn in them (1x HUD dumps of bench 3's tab tour):

- **Health:** the figure and its part label (99 px) are centred in their panel: 10 px above, 11 below (they were 5 and 14); it was already centred across (18 px each side). The panels end at 141 like the other tabs' (they ended at 139).
- **Gear:** starts at 22 like every other tab (it started at 21); the gaps between its three panels are 4 px like the other tabs' (6 and 4: the worn panel is 2 px wider, its slots shifted 1 px to stay centred, as the hotbar's are); the item box ends at 141; the weight reads "3.8 / 16.0 KG" like the Health tab's and the search screen's (it was "3.8/16.0 KG").
- **Skills:** the list's rows a pixel lower (3 px under the border, 4 at the bottom; they were 2 and 5).
- **Craft and Skills:** right-aligned text in the right panel keeps 4 px from the border, as the text on the left does (it kept 3).
- **Unchanged, already in line:** the tab bar (5 px inside both ends), Settings, the Map tab (its map is bound to the 8 px tile grid: 24-144).
- **Found on the way:** a popup going away over the search screen redrew it in one frame, 5% over budget (bench 3's one missed frame); `search_screen::refresh` now uses the split two-frame draw of its opening. Bench 3: no missed frames, 169 per mille.
- `docs/playtest-2-tabs.png`: Health before and after, then Gear, Craft, Skills and Settings after.

**Corpses in blood 2026-11-24** (the user: "the zombie corpse don't show when they are dead, we should have some blood around them"). In bench 1 and 2 the corpse sprites were there; what was missing is that they didn't look dead: since stage 2 a corpse is the zombie's lying frame, the same one a knocked-down zombie or a crawler shows, and the sprite hides the decal (whose old art had blood round it).

- **Dead frames** (`characters.dead_frame`, 3 per body after the crawl frames: `zombie_dead`, `zombie_dead_frames`; 20 frames a body, 80 in the sheet): the lying body in an elliptic pool of blood under and round it (the outfit palettes' own blood colours, indices 10 and 11, dark with a few wet highlights and a ragged rim), kept in the sprite's top 13 rows so it fits the corpse's tile. The pool grows: small and medium while the dying body eases onto its spot (its last 10 frames), full for the corpse sprite.
- **The decal** is body A's full dead frame in the decal colours, so a corpse without a sprite looks the same; checked: none of its pixels fall outside any body's corpse sprite.
- Checked in mGBA (`docs/playtest-2-blood.png`: bench 2's doctor in its pool, then with the player on it); bench 1's farmer corpse takes frame 59 (body C's full pool). Bench 1 552 per mille, no missed frames; bench 2 none.

**Car screen polish 2026-11-24** (the user: text running out of the car screen, "A WRECK CAN'T BE FIXED"; alignment and layout). Rendered with bench 9 (DUMP=1) and measured like the tabs:

- 20 of the strings the detail panel can show were wider than its 82 px of text (problems up to 119 px; three hand-split description lines by 2-4 px). The panel's text is now wrapped at spaces to its width (`wrap` in `kx_car_screen.cpp`): problems take at most two lines (one at the panel's foot grows upward), the trunk, glove box and repair descriptions are whole sentences wrapped to three.
- Right-aligned values in the detail panel and the header's state ("WILL NOT START") keep 4 px from the border, as the text on the left does (they kept 3).
- `docs/playtest-2-car.png`: the battery missing, its problem wrapped inside the panel.
- Bench 9 passes (the wreck too). Its 4 missed frames were older (the same with the wrap taken out). **Then fixed:** the car screen draws over two frames (`_draw_left`: the header and the list; `_draw_right`: the selected line's panel and the footer), without the full-screen clear (each panel fills its own area; the screen is cleared when it opens), so nothing blanks in between. Bench 9 now misses 3 frames, all scripted: 763, and 795 (the teleport to the wreck: world generation, camera and streaming in the same frame, 1.06 frames, 1.41 before); the screen opening at frame 70 no longer overruns.
- Asked in passing: the sitting rabbit (`animals.py RABBIT`) reads poorly at 16x16 (stub ears, flat body); a redraw was offered.

**Rabbit 2026-11-24** (the user asked whether the grey thing was a rabbit): redrawn in `tools/assets/animals.py` (`RABBIT`, same frames and palette): sitting, tall ears (the far one in shade, light inside), a round back over a shaded haunch, a light chest, the white tail; hopping, stretched out with the ears laid back. `docs/playtest-2-rabbit.png`: old (left) and new at 8x, then 3x and 1x on grass.
