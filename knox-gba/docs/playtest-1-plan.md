# Playtest 1: fixes and polish (milestone 14.5)

The first playtest notes (`../ideas`, 2026-11-23), what causes each one, and the plan. User choices are marked **(chosen)**.

## The notes, sorted

### Bugs (cause found in the code)

| # | Note | Cause | Fix |
|---|------|-------|-----|
| 1 | Map tab: moving left/right shows the hidden map | `map_screen::_draw` (`src/kx_map_screen.cpp`) draws all the land first, then paints fog over it. The canvas writes VRAM directly and a full redraw takes longer than a frame, so the screen shows land with no fog for a frame on every scroll (and on opening). | Draw each chunk either as land or as fog, never land then fog. Also faster. |
| 2 | Popups after eating/drinking are not seen in the inventory | `hud` skips the toast while a menu screen is up (`_backdrop`) but keeps counting it down, so it expires unseen. | Menu screens show the latest toast in their own message line (the gear screen's footer); toasts raised in a menu do not count down there. |
| 3 | Corpses change the floor colour | Decal graphics only exist for some grounds. Grass B, lush, flowers and dry grass borrow grass A's corpse/blood graphics (`DECAL_ALIASES` in `tools/assets/world.py`), so a corpse brings a square of the wrong grass. Painted floors (walnut, carpet) draw their decals with the decal bank, which has the default pine/white colours, so a corpse brings a square of the wrong floor. | Floors: put the blood/corpse colours into the three floor paint banks and paint floor decals like floors (palette only, no new graphics). Grass: give the four grass looks their own decal graphics if the tileset has room, else draw the corpse on the walls layer over any ground (see stage 2). |
| 4 | Corpses don't line up with the zombie that died | The dying zombie is a sprite at any pixel; after 24 frames it becomes a ground tile on the 16 px grid, always lying the same way. | During the dying slide, ease the body onto its tile's centre so the swap doesn't jump; mirror the corpse tile (free hardware flip) to match which way the zombie fell. |
| 5 | Zombies pushed onto walls when they die | The killing blow's knockback slides the dying body with the standing zombie's 8x5 box, but the lying sprite is about 16 px wide, so it ends up drawn over the wall. (Likely cause; to confirm on a bench.) | Slide dying bodies with a lying-body box, and stop the slide short of walls. |

### Needs a look on screen

| # | Note | Plan |
|---|------|------|
| 6 | Menu things not aligned (map view, etc.) | Render every menu tab with the HUD dump benches and compare with the mockups; fix what's off. Screenshots of the spots you saw would make this quicker. |
| 7 | Doors stick out of the building | Mockup 12 already shows the door frame running past the wall's bottom edge. Mockup first: a few door looks that stay inside the wall line, you pick. |

### Look and feel

| # | Note | Plan |
|---|------|------|
| 8 | Zombie groan sounds like farting | The groan (`tools/assets/audio.py`, `groan()`) is a low saw wave (88-105 Hz, falling) through a low-pass, played as 8-bit: that's a buzz. Redo it as a voice: higher pitch, vowel-like formants, breath and rasp. I can't hear audio, so I make 3-4 candidates and you pick by ear. |
| 9 | Dimmed world behind menus, like mockup 03 **(chosen)** | Menus stop painting the flat screen colour; the world stays visible behind the panels, darkened (Butano brightness/blending on the world layers and sprites). |
| 10 | Zombie variety: more clothes and looks **(chosen)** | More outfits plus skin, hair and shirt-colour palette swaps and mirrored sprites. No new behaviour types; combat balance stays as tuned. Mockup first (sprite sheet of the variants). |
| 11 | More variety elsewhere (flipped textures) | Already flipped: grounds, roofs, trees. Add: corpses (above), furniture and props that read the same mirrored, blood splats rotated by flips, a second wall-face/floor detail where the tileset has room. |
| 12 | Grainier colour look | Mockup first, three looks side by side: a palette grade (warmer/desaturated, free), grain baked into the ground tiles (dither), and both. GBA has no post-process, so the look has to live in palettes and tiles. |
| 13 | New name **(chosen: an original name)** | "Project GBAzoid" stays the folder/codename. Candidates for the title screen and ROM title (12 chars max): QUIET ROADS, HOLLOW COUNTY, POCKET OUTBREAK, STILL STANDING, LAST CARTRIDGE. You pick (or suggest another). |

## Stages

1. **Bugs that need no art**: map fog leak (1), toasts in menus (2), dying slide and wall overlap (5), corpse easing and mirroring (4).
2. **Corpse colours** (3): floor paint banks; grass decals or corpses on the walls layer, depending on tileset room. Bench check on every ground.
3. **Menus**: dump-render every tab, fix alignment (6); dimmed world behind menus (9).
4. **Sounds**: 3-4 groan candidates, you pick (8).
5. **Mockups**: doors (7), zombie looks (10), colour/grain look (12) — one mockup sheet each, you pick.
6. **Build the picks**: doors, zombie variants, colour look, extra flips (11), the new name (13).

Save format: if the zombie looks need more bits than the saved outfit byte has (checked in stage 6), bump `format_version`.

## Progress

**Stage 1 built 2026-11-24** (untested on device):

- **Map fog (1):** `map_screen::_draw` draws each 6 px chunk as land (clipped to the chunk) or as fog, never land then fog; settlement blocks only over explored chunks. Bench 12's Map tab now scouts 5 chunks round the player instead of revealing everything, so its dump shows the fog's edge.
- **Popups in menus (2):** `hud::screen_toast()` draws the popup over the top strip of any menu screen (tabs, search, car, safe) and counts it down only there; when it goes, `game::_screen_toast()` calls `refresh()` on the screens so they redraw. Bench 3 (DUMP=1) raises one in the Gear tab: dumps 9101 (shown) and 9102 (gone, tab redrawn).
- **Bodies over walls (5):** the real cause was the drawing, not the slide: lying frames hold the body in the sprite's top rows, so a lying zombie was drawn 8-16 px above its feet, up on the wall behind it. Lying zombies are now drawn 1 px below their feet (`lying_feet_offset`). Knocked-down and dying bodies also shift up to 3 px off a side wall (a 14 px wide lying box).
- **Corpse alignment (4):** over the last 10 of its 24 dying frames the body eases onto the corpse decal's spot in its tile (8 px across, 10 down). Corpse decals are mirrored per tile (`world::decal_mirrored`, flip kind 4, nothing stored) and the dying sprite takes the same side. Bench runs log `KXCORPSE` (position, tile, mirror, sprite flip); bench 2's two corpses land exactly on their spots with matching flips.
- Bench 1: frame average 580 per mille (578 before), no missed frames. Bench 2, 3 and 12 run clean.
- Not done here: the player's own fallen sprite has the same draw-above-the-feet offset (it only shows at death).

**Stage 2 built 2026-11-24** (untested on device):

- **Corpse colours (3):** every ground that takes decals has its own corpse, blood and sack graphics (`DECAL_GROUNDS` in `tools/assets/world.py`; `DECAL_ALIASES` is empty): grass B, lush, flowers and dry grass, concrete, crops and both road lines gained 24 graphics (tileset 196 of 255). They are appended, so the decal ids already in saves are unchanged and no format bump is needed (bench 8's save and load match).
- Painted floors: the three floor colour banks start from the decal bank, and decals on wood and tile floors take the floor's paint like the floor does. `build_world` asserts that floor graphics use none of the decal bank's colour indices.
- Side effect: on painted floors the dropped-items sack is drawn in the floor's wood colour (dark on walnut, red on carpet).
- Checked with an offline sheet of every ground and floor colour (plain, corpse, mirrored corpse, blood, sack) built from the generator's own data; bench 2 and 8 clean.


**Stage 3 built 2026-11-24** (untested on device):

- **Menu alignment (6):** rendered every tab (bench 3 with DUMP=1 now tours Gear, Health, Craft, Skills and Map: dumps 9200-9204) and compared them with the mockups. Two things were off:
  - The tab bar: only the selected tab had its padding, so every tab moved sideways when you switched. Each tab now keeps its place.
  - The Map tab: the map ended 12 px left of the other tabs' panels and the sandbox column sat 2 px from the screen edge. The map is now 176 px wide from x 48 (14 cells and a sliver; still on 8x8 cell edges) and the column starts at the 12 px margin.
  - Gear, Health, Craft, Skills and the search screen matched their mockups.
- **Dimmed world behind menus (9):** menu screens no longer paint a solid backdrop (`canvas::backdrop` is see-through); while one is open, `game::_update_menu_dim()` fades the world's own palette (`world_renderer::set_dimmed`) and the sprite palettes 70% to black, once a frame before it is shown, so a tab switch never flashes. The HUD palette and the night, fog and roof blending are not touched. A popup raised in a world-running screen still shows over it. `docs/playtest-1-menus.png` is two screenshots from mGBA: the search screen and the Gear tab with a popup.
- Reading from the Gear tab closes it and starts the fast-forward in the same frame; the world palette still undims, and the sleep fade keeps the sprites' fade.
- Bench 1: 581 per mille (580 before), no missed frames.

**Stage 4 built 2026-11-24** (heard by the user on the desktop, not yet on the Miyoo):

- **Groan (8):** `tools/groan_candidates.py` writes four styles, two takes each, in the game's own format (8-bit, 16 kHz, same normalising and loudness cap), to `sound-candidates/` (not built into the ROM): A moan (rough low voice, "uhhh-ohh"), B rasp (creaking vocal-fry growl, "aah"), C gurgle (wet breathy "hhhaah" with a throat flutter), D wail (higher hollow "ooo-aah" cry). `all.wav` plays the current groan and then each style in that order.
- They are built like a voice: glottal pulses with jitter and shimmer (or fry, or breath) through three formant resonators. The current groan has 85-88% of its energy under 250 Hz, below what small handheld speakers reproduce, so only the buzz is heard; the candidates have 78-95% between 250 Hz and 3 kHz, where voices sit. Their RMS is lower than the current one (0.13-0.22 against 0.26) but in the band ears hear best.
- **Picked: A, the moan.** It is now `groan()` in `tools/assets/audio.py`, with the voice helpers (`ramp`, `resonator`, `formants`, `throat`); `audio/groan_a.wav` and `groan_b.wav` are byte for byte the takes heard. `tools/groan_candidates.py` still writes the other styles and the old buzz (`0_before`) to hear again. The new groans are longer (0.85 and 0.75 s against 0.55 and 0.7 s). Bench 2 clean.

**Stage 5 made 2026-11-24, waiting for picks:**

- **Doors (7), `mockups/15-door-looks.png`** (`tools/mockup_door_looks.py`). Cause, seen in mGBA captures of bench 12: every doorway in an outer wall is left out of the roof (`buildings.tile_graphics`), so a front door's whole 16x16 tile stands 11 px above the strip of wall under the eave, and a side door makes a box in the roof's side edge. The roof tile behind a front door also turns into an eave, showing a bar of floor above the door. Options: 0 now; **1 under the eave (recommended)**: a roof graphic over front doors whose edge dips 3 px, side doors under the roof with a paved step outside; 2 in the wall strip: the plain eave over front doors, only the door's foot shows; 3 porch: the notch kept, a porch roof in front. In 1-3 the roof's slopes and edges are worked out as if doorways were roofed. Building: 2 roof graphics (red, grey), a template change in `buildings.py`, side steps; from inside nothing changes.
- **Zombie looks (10), `mockups/16-zombie-looks.png`** (`tools/mockup_zombie_looks.py`). The 16 sprite palettes are mostly used, so no new palettes: three more bodies in the zombie sheet use the four spare indices (12-15) of the 5 outfit palettes for a second hair colour and a second shirt. A today's; B long hair (second hair) and the second shirt; C bald, shirt torn (skin and blood), a bare arm; D a beanie in the second colour. 20 looks; walkers facing up or down also mirrored at random. Building: 3 x 17 more frames, the second colours per outfit, a body per zombie (2 bits: no save change if it fits the outfit byte, checked then).
- **Colour look (12), `mockups/17-colour-look.png`** (`tools/mockup_colour_look.py`, from two mGBA frames in `mockups/frames/`). No post-processing and no spare background layer on the GBA, so a look lives in the colours: 0 now; 1 faded grade (palettes graded once at load: a third less saturation, blacks lifted, whites pulled down, cool shadows, warm lights; the HUD keeps its colours); **2 grain in the tiles (recommended: what the note asked, and it keeps the colours readable on a small screen)**: about one pixel in seven of the ground, floor and wall textures a shade darker or lighter, still as you move; 3 both.

**Stage 6 built 2026-11-24** (the user's picks: doors 1, all four zombie bodies, colour 2; untested on device):

- **Doors (7):** outer doorways are under the roof (`buildings.tile_graphics`), so the roof's slopes and edges are worked out with them; a doorway in the eave row gets `ROOF_RED_EAVE_DOOR` / `ROOF_GREY_EAVE_DOOR`, an eave whose edge is 3 px higher and open below. Other outer doors sit under the roof beside the front step the generator already adds (turned with the template, so it is outside the front door whichever way the building faces). The two graphics are appended after the decals (`WORLD_GFX_LATE`), so saved graphic ids stay put; `is_roof()` takes them too. `outer_door()` in `kx_world.cpp` found a house's door as the door tile with no roof: it now looks for a door beside open ground or the lot's edge (bench 12 found no houses until then). `docs/playtest-1-doors.png` is from mGBA: a front door under the dip, side doors under the roof with their steps.
- **Zombie bodies (10):** the zombie sheet holds 4 bodies of 17 frames (`characters.ZOMBIE_BODIES`, `zombie_body()`); the five outfit palettes carry a second hair colour and a second shirt in indices 12-15. Each zombie's body and whether it is mirrored walking up or down come from its existing spawn roll (`anim`), so no new random draws: the benches' sequences are unchanged. Survivors who rose keep body A (their clothes are on its areas). Not saved (zombies go back to the hordes). `docs/playtest-1-zombie.png`: a doctor with body B in mGBA.
- **Grain (12):** about one pixel in nine of the ground, floor, wall and roof textures is swapped for a neighbouring shade of the same material (`GRAIN_SHADES`); colours whose nearest shade is far off (wood, road, brick) are rarer; painted roofs, walls and floors swap only among the indices their colour variants repaint. Applied to the grids, so decals and the eave-door roofs carry it.
- **Benches:** bench 1 585 per mille (581), no missed frames; bench 2 clean; bench 8's round trip matches; bench 12 passes every phase, 371 per mille, 248 missed frames on its teleports (227-245 at milestone 14); bench 14 census passes all 5 targets, the tour loads all 36 stops with every home's doors, 705 missed on its teleports (696).
- Not done: extra flips (11) beyond the corpses.

**Name (13) 2026-11-24:** the user picked **Pocket Outbreak**. The title screens show it (`game_name` in `kx_title.cpp`) and the ROM header's title is `POCKETOUTBRK` (12 characters, the most it holds). Kept as they were, on purpose: the ROM and save file names (`knox.gba`, `knox.sav`: emulators match saves by file name), the game code `KNXE`, the save slots' "KNOX" marker (changing it would make every save unreadable) and the test map's seed. `docs/playtest-1-title.png` is the new world screen in mGBA.

## Open questions

- Screenshots of the misaligned menu spots (6), if you have them.
