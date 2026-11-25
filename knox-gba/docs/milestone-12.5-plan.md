# Milestone 12.5: night, map and loose ends

10 Nov 2026 · status: approved. All 6 stages built on 10 Nov 2026 and checked in mGBA (bench logs, screenshots, HUD dumps). Not yet tested on the Miyoo. Save format version 11.

## Why this milestone exists

The spec describes night darkness and a flashlight cone (cut list, `docs/palette.md`, mockup 02), but the build order never named them, so milestones 1–12 passed without building them. An audit of the whole spec against the code found more gaps of the same kind. This milestone builds night and the smaller gaps. Two new milestones take the larger ones.

## Audit: what the spec describes and no milestone built

"Missing" means there is no code for it. Each gap now has a home.

| Gap | What exists today | Goes to |
| --- | --- | --- |
| Night darkness, dusk and dawn | The clock says NIGHT; zombie sight, air temperature and reading follow it. The screen never darkens. | 12.5 |
| Flashlight cone and batteries | The Flashlight is loot. Carrying one only counts as light to read by. | 12.5 |
| Campfire glow, car headlights, indoor lights | Headlights switch on and drain the battery, with no beam. Campfires give reading light only. | 12.5 |
| Map tab, fog of war, compass, marks | The MAP label is on the tab bar; Map and Compass are loot that do nothing. Mockup 05 is agreed. | 12.5 |
| Stitching deep wounds | A code comment promised it for milestone 8. There is no suture needle item. | 12.5 |
| Weapon repair | Duct tape and Glue are loot with no use. There is no whetstone. | 12.5 |
| Traits: Keen Hearing, Hard of Hearing, Outdoorsman, Short-sighted | Left out of milestone 8 until their systems existed. Those systems exist now. | 12.5 |
| Zombies attacking a car you sit in | Deferred in milestone 12. | 12.5 |
| Guns, ammo, aiming, active reload | Aiming and Reloading are skills with books, and nothing trains them. | 12.6 |
| Spears | The Spear skill reads "NO SPEARS YET". | 12.6 |
| Throwables, Molotovs, fire tiles, burns | The Burn wound type exists and nothing causes it. | 12.6 |
| Belt and secondary-hand slots | Left out "until the items that need them". | 12.6 |
| Doors that open, lock and break | Doorways are open gaps in walls. House keys are loot with no use. | 12.7 |
| Lockpicking, notes naming places, gun safes | Nothing. | 12.7 |
| Wooden door and log wall pieces | Deferred in milestone 9. | 12.7 |
| Animals and hunting | Nothing; traps give meat without an animal. | 13 |
| Swimming | Deep water is solid. | 13 |
| Sandbox settings on world creation | The title screen says "come later". | 13 |
| Snares, freezers, wells, Smoker, save caps in the UI | Nothing. | 13 |

Checked and found built: palette fades for thirst and blood loss, panic jitter, the 120° sight cone, stereo panning, smoke columns, double-buffered character save, hourly autosave, hitting zombies with a car, crash wounds, book volumes, dust and splash sprites.

## Choices you made (10 Nov 2026)

- **Scope:** split by theme, as in the table above.
- **Flashlight:** Select on the hotbar switches it on and off. A battery gives about 6 in-game hours of light.
- **Being seen:** your own light gives you away. Lit, zombies see you from 6 tiles at night instead of 3.
- **Indoor lights:** the building you are in is lit at night while the power is on. After the shutoff it is dark unless a running generator is within 10 tiles.

## Design

### How darkness is drawn

The GBA can darken chosen layers in 16 steps (a brightness fade) and can switch that effect off inside windows. Night uses both.

- **Darkness:** the three world layers and every world sprite (player, zombies, vehicles, particles, rain, smoke) fade towards black. The HUD stays at full brightness.
- **Level:** full night is 10 of 16 steps, a tuning value. It ramps up from 19:00 to 20:00 and down from 06:00 to 07:00, one step at a time.
- **Light:** anything inside a light window is drawn at full brightness. Light windows are window sprites (the spec's "OBJ-window" cone) and one rectangle for the building you are in.
- **No new world tiles.** The world tileset is at its 128-graphic limit, and this design needs none.

The GBA has one blending mode at a time. Three effects share it, so they need rules:

| Clash | Rule |
| --- | --- |
| Night and the roof fading out over 8 frames | While any darkness shows, the roof is not drawn: its cells are written empty while you are inside (changed in stage 1, see below). By day it fades as before. |
| Night and fog haze | Fog at night has no white haze. It still cuts sight, and it deepens the darkness by 2 steps. |
| Night and lightning, sleep fade, thirst and blood grey-out | No clash: these change palettes and stack with the darkness. Lightning lights the whole night for a moment. |

New module: `kx_light` (`include/kx_light.h`, `src/kx_light.cpp`) owns the darkness level, the list of lights on screen and the window sprites. `kx_world_renderer` gains the night path beside its roof and haze code. Night values go in one tuning table with a debug-menu page.

### Lights

| Light | Shape | When |
| --- | --- | --- |
| Flashlight | A cone about 120 px long ahead of the player, plus a small glow around them | Switched on from the hotbar |
| Headlights | The same cone from the car, along its heading | Lights on (R1, already built) |
| Campfire | A round glow about 4 tiles wide | While it burns; up to 2 on screen |
| Indoor light | The whole building you are in | Grid power on, or a running generator within 10 tiles |

- The cone is one 64×64 window sprite, drawn at double size and turned by the hardware. It uses 1 of the 32 rotation slots; the game uses none today.
- The flashlight cone and the headlight cone never show together: the flashlight goes off in the driver's seat. It works on a bicycle.
- Buildings are lit only while you are inside. Lit windows seen from the street would need new tiles.

### Flashlight rules

- **Toggle:** Select on the hotbar slot switches it on or off, with a click sound.
- **Battery:** the flashlight holds a charge of 360 in-game minutes and loses 1 per minute while on. Found flashlights have a random charge.
- **Empty:** it goes off with the message "FLASHLIGHT: BATTERY DEAD". Switching it on again uses a Battery from your bag. With none, it clicks and stays off.
- **HUD:** the hotbar slot shows the charge as a bar under the icon, as in mockup 02.
- **Daylight:** it switches itself off at 07:00, so a forgotten light doesn't waste the battery.
- **Reading:** needs a flashlight that is on, not only carried. Campfire, powered building and car light count as before.

### Being seen

- Zombies see you from 3 tiles at night (as now). They see you from 6 tiles if you are lit: flashlight on, driving with headlights, within 4 tiles of a burning campfire, or inside a lit building.
- A burning campfire at night draws hordes: each in-game hour it heats its map cell like a noise of 12 tiles. This is the spec's "visible at night".
- Zombies outside the light stay on screen as dim sprites, as in mockup 02. Nothing is hidden outright.

### Map tab (mockup 05)

- **Opening it:** the MAP tab works only while you carry a Map. Without one it shows "NO MAP".
- **View:** 15×10 map cells at 12 px each, showing land, water, roads and settlements. It opens centred on you. The D-pad scrolls.
- **Fog of war:** a 128×128 bitset, one bit per chunk (2 KB, as the spec's save table plans). Chunks around you clear as you walk. Unexplored ground is dark hatching.
- **Compass:** with one you get the red position marker and the north arrow. Without one the map opens where you last left it.
- **Names:** each settlement gets a name from the seed, out of a list of 24 in ROM. The name shows once you have been there.
- **Marks:** A sets or clears a mark at the cursor, up to 8. Your safehouses show as house icons. With a compass, the nearest mark shows as a small arrow at the screen edge in the world.
- **Footer:** the cursor's grid position and the scale bar, as in the mockup.
- **Ownership:** fog and marks belong to the world, so a new survivor keeps them but still needs a Map to see them.

### Loose ends

- **Stitching:** a new Suture needle item (clinics; rare in bathrooms). At First Aid 2, treating a deep wound with a needle stitches it: the bleeding stops and it heals twice as fast.
- **Weapon repair:** a REPAIR action on a worn weapon in the Gear tab. Duct tape restores half the condition and Glue a quarter; both are used up. A new Whetstone item sharpens a blade to full, 5 times.
- **Traits:** 3 new pairs, so 22 traits in all.
  - Keen Hearing / Hard of Hearing: you hear zombies 50% farther away, or you lose stereo cues and distant sounds.
  - Outdoorsman / Homebody: rain and cold affect you 50% less, or 50% more.
  - Eagle-eyed / Short-sighted: your light cone is a quarter longer or shorter, and dusk comes an hour later or earlier for you.
- **Zombies and your car:** zombies that reach a car you sit in hit it. Each hit costs condition and makes noise 6. Below a quarter condition they can grab you out of the seat.

## Stage 1 findings

- **The roof is hidden by its map cells, not a window.** The GBA's rectangle windows win over the window sprites, so a rectangle hiding the roof would also have blocked the flashlight cone inside a dark building. Writing the roof's cells empty frees both rectangles; the lit building uses one of them.
- **Butano's `window::restore()` leaves the blending flag off.** It shows every layer and the sprites again, but a window once set to no blending stayed that way. Every roof cut-away left the outside without blending afterwards, so since fog arrived in milestone 10 the haze could not show once you had entered a building. It would also have kept the night from darkening anything outdoors. `kx_world_renderer` now turns blending back on whenever it restores a window.
- **Cost:** about 0.3% of a frame on bench 1 by day (sprites are shaded when created and again only when night begins or ends). Bench 10 at night with a 40-zombie horde averages 41% of a frame.

## Stage 2 notes

- **Flashlight state lives in the item.** Bit 15 of the item's extra field is on/off, the rest the charge in minutes, so it is saved with the item and needs no save bump. Flashlights in older saves have an empty battery.
- **Driving:** the flashlight goes dark and stops draining in the driver's seat, and lights again when you get out (the plan said it goes off; pausing it means you don't have to switch it on again).
- **Sleeping** doesn't drain it.
- **Light goes through walls.** A beam from inside a dark building also lights the ground outside it. Stopping it would need a shadow per wall, which the window sprites can't do.
- **The debug Test light is gone;** bench 10 now uses the real lights.
- **Cost.** By day the lights update is skipped once no light window is up.
  - Bench 1 (day): 56.8% of a frame on average, against 55.7% before the night work, with no missed frames.
  - Bench 10 (night, with every light and a 40-zombie horde): 46% on average. Its only missed frames are its own teleports.
  - Bench 7 (travel): one frame (2336) runs 6.6% over, where zombies, horde migration, the flow field and chunk generation peak together. Its worst frame was 99.5% before this work. Bench misses now log a per-system breakdown, which is how this was found.

## Stage 3 notes

- **Sight:** lit, zombies see you from 96 px (6 tiles) and keep you in sight to 160 px; unlit it stays 48 and 80. The two values are on the Night debug page, because the zombie tuning table was at its 32-entry limit.
- **Car light for reading** was any car with its lights on among the vehicles around you, even 10 tiles away. It is now the car you drive, or lamps within 4 tiles (as close as a campfire must be).
- **Reading stops when the light goes,** and the book has to be read again (no partial progress).
- **Campfires burnt out are marked out each hour.** Their burn-out hour is kept mod 256, so an old fire used to look lit for up to 12 hours about every 11 days. That would now have drawn hordes. Fires are only marked out when an in-game hour passes, so the debug skips (Skip 10 days, Set hour) can still briefly show an old fire lit.

## Stage 4 notes

- **No palette bank was free** (the world uses 11 of the 16 background banks, the HUD 5). The Map tab puts its 8 colours into the Health tab's bank while it is open and puts them back when it closes. The world is hidden behind the menu, so nothing else shows those colours.
- **The map's area starts on 8x8 cell edges** (x 32, y 24, 184x120), because each HUD cell has one palette bank. That fits 15 cells and a third of a 16th across.
- **GRID and the scale bar sit inside the map's bottom edge,** not in the footer as in mockup 05, because the footer's right side holds the save-and-quit hint (added in milestone 7, after the mockup).
- **The cursor moves by chunks** (6 px), so GRID counts chunks (0–127).
- **The first time without a compass,** the map opens where you are; after that it opens where you left it.
- **Save version 10.** Fog of war and the marks add 2,068 bytes to the world block.
- The HUD dump now logs the live palette, so `tools/render_hud_dump.py` shows the swapped colours.
- **Debug:** a "Reveal map" action on the Debug page explores everything, for checking the map.
- **Cost:** exploring and the mark arrow are skipped cheaply when nothing changes. Bench 1 averages 56.8% and bench 7 49.3%, the same as after stage 2. Bench 7's two peak frames (416 and 2336) now run 5% and 7% over budget. Both are where zombies, horde migration, the flow field and streaming peak together, and they are worth a look once the game is measured on the Miyoo.

## Stage 5 notes

- **Stitching** uses up the needle (needle and thread are one item, stacks of 4). A stitched deep wound heals even without a bandage, at twice the speed. The order is disinfect, then stitch, then bandage.
- **Repair** prefers the whetstone for knives and the axe, then duct tape, then glue. Only weapons are repaired (not clothes).
- **Hard of Hearing** keeps sounds at 60% of their distance and loses the off-screen crowd groans altogether, as well as stereo.
- **Eagle-eyed** gets its longer reach from a second beam sprite, because one 64x64 frame at double size can't reach past 128 px. It uses one more rotation slot.
- **Car attacks** happen when a zombie's lunge reaches the car you sit in (moving or not; at speed you usually hit them first).
- **Save version 11:** wounds gained a stitched flag and the trait mask went from 16 to 32 bits.
- **Bench checks.** The night bench now starts with self-checks of stitching, repair and the six traits (`KXFIVE` lines). It ends with a siege: four zombies around a car at 30% condition. They hit it down to 24%, then drag the driver out and grab them (`KXCAR besiege` lines). The vehicles bench's own drive kills its zombies before any reach the stopped car.
- **Bench numbers.** Bench 1 averages 56.9% of a frame and bench 7 49.4%, both unchanged. The vehicles bench misses 3 frames, all during its scripted car-screen work (fast-forwarded repairs).

## Stage 6 notes

- **Bench 10 runs on a generated world, not the test map** (the plan said the test map): the test map's house has no roof, so roofs and indoor lights could not be tried there. It starts at 22:55 so the campfire calls the hordes at 23:00.
- **Bench 8 now round-trips the new data:** what is explored, the marks, a flashlight's state and charge, a stitched wound, and traits past bit 15. Saved and loaded values match. The new survivor keeps the map and marks.
- **HUD dumps checked:** the creation screen's dump script now also shows the trait list scrolled to its last pair. The scroll arrows sit on the panel's edges.
- **Design spec:** a Night and light section; the Map tab under Exploration; the 22 traits; cars besieged; the fog-of-war save row includes the marks.

## Results

Bench timings in per-mille of a frame (average / worst):

| Bench | Frame | Zombies | Visuals | HUD | Flow field | Hordes | Missed frames |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1, day, 40-zombie horde (test map) | 569 / 937 | 254 / 298 | 40 / 83 | 22 / 316 | 45 / 219 | 6 / 11 | 0 |
| 10, night, every light, horde, siege | 489 (with its teleports) | 208 / 294 | 44 / 119 | 21 / 347 | 5 / 223 | 22 / 188 | 4 teleports, 1 siege frame |
| 7, travel (stress) | 494 / 1068 | 157 / 219 | 31 / 67 | 17 / 284 | 21 / 224 | 33 / 250 | 2 |

- **Before 12.5,** bench 1 averaged 557. Night costs nothing by day apart from its checks, about 1% in all.
- **Missed frames:**
  - Bench 7's two worst frames run 5% and 7% over. They were at 99.5% before 12.5, where zombies, horde migration, the flow field and streaming peak together.
  - Bench 10 misses the frame where zombies drag the driver out of a car, by 11.6%: the pull-out, the grab, a toast and a flow-field refill all fall in one frame.
  - Both are worth a look once the game is measured on the Miyoo.
- **Memory:** IWRAM .iwram 22.8 KB + .bss 4.7 KB + .data 0.1 KB = 27.6 KB, at the 28 KB guide. EWRAM .sbss 138 KB (fog of war 2 KB of it). ROM 772 KB.

## Done when (checked)

1. Outdoors at night the world is dark and the HUD bright, with a lit cone ahead when the flashlight is on (screenshots in mGBA).
2. Flashlight, headlights, campfire glow and indoor lights all work; the building goes dark on the shutoff day (screenshots, bench 10).
3. Zombies see a lit player from 96 px and an unlit one from 48 (bench 10 logs the values the zombies use).
4. The Map tab draws mockup 05's map and keeps what is explored and the marks over save and CONTINUE (HUD dumps, bench 8).
5. Stitching, repair, the 6 traits and the car siege work (bench 10 self-checks and siege).
6. Day timings are within 1% of before 12.5. Night is inside the budget except the two frames above.

## Stages

A ROM to try after each stage, as in earlier milestones.

1. **Darkness (built).** Start with a small proof that fade, window sprites and the roof rectangle work together in mGBA. Then the night level, dusk and dawn, the three clash rules, and a "Set hour" row on the debug page.
2. **Lights (built).** The flashlight with its toggle, battery and HUD bar; headlight beam; campfire glow; indoor lights and the lights going out at the power shutoff.
3. **Night rules (built).** Lit sight for zombies, campfires drawing hordes, the reading rule.
4. **Map tab (built).** Fog of war, the view, names, compass, marks and the edge arrow.
5. **Loose ends (built).** Stitching, weapon repair, the 6 traits, zombies attacking an occupied car.
6. **Benches and docs (built).** Bench 10 (`knox_night.gba`, `BENCH=10`) on the test map at 23:00, logging KXNIGHT lines. Save round trip for the new data in bench 8. README, spec sections for night and the map, profiler numbers.

## Done when

1. Outdoors at 23:00 the screen matches mockup 02: dark world, bright HUD, a lit cone with the flashlight on.
2. Every light in the table works, and the building you stand in goes dark at the power shutoff.
3. Zombies notice a lit player from 6 tiles and an unlit one from 3.
4. The MAP tab matches mockup 05 and remembers explored ground and marks after save and CONTINUE.
5. The four loose ends work as described.
6. Daytime bench timings are unchanged, and bench 10 keeps the frame inside its budget.

## Risks

- **gpSP on the Miyoo.** Window sprites with a fade are the least-tested part, and no milestone has been tried on the device yet. Fallback: a "Light style" tuning switch. In plain style there is no cone, and being lit lowers the darkness by 4 steps instead.
- **Sprite drawing time.** The double-size cone costs about a fifth of a scanline's sprite budget. Bench 10 runs it with 40 zombies on screen to check nothing drops out.
- **Save space.** Fog of war adds 2 KB to the 28 KB world block. The spec planned for it, but the real block size must be measured with the caps full.
- **Save format.** The trait mask grows from 16 to 32 bits, and the flashlight charge, stitched wounds, fog and marks are new. The save becomes version 10, so older saves stop loading, as with every bump.
- **Character creation screen.** It lists 8 trait pairs today and must scroll to fit 11.

## My calls, not asked

These fill gaps the spec leaves open. Any of them can change.

- Full night is 10 of 16 steps; dusk and dawn each take one in-game hour.
- Dusk is a plain dimming. Mockup 01's purple tint is not built: the hardware fade only goes to black.
- Fog at night has no haze; roofs snap at night.
- Buildings are lit only from the inside.
- Lit interiors don't draw hordes; only campfires do.
- Fog of war and marks belong to the world, not the survivor.
- The trait opposites Homebody and Eagle-eyed are my names; the spec gives only one of each pair.
- Smoker waits for milestone 13, because it needs cigarettes.
