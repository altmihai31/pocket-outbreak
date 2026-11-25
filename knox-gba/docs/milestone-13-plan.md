# Milestone 13: the wild and loose ends

20 Nov 2026 · status: built (all 7 stages, 22 Nov 2026), awaiting device testing. Save format version 16.

## Progress

- **Stage 1 (22 Nov):** `tools/mockup_wild.py` draws `mockups/13-wild.png`. The four new world graphics (snare, well, chest freezer, TV) are in the tileset, which is full again at 128. The new sprite sheets are `animals.bmp` (deer, carcass, rabbit), `birds.bmp`, `ripple.bmp`, plus swim frames in `player.bmp`. Two changes from the plan:
  - **How the tiles were freed:** the second plain grass (`GRASS_B`; flips still vary `GRASS_A`) and the two weathered roofs (moss and rust stains), instead of a snare drawn as a box-trap palette swap (a palette can't change the box's shape) and a shared floor (no two floors look alike).
  - **Save version 14 now, not in stage 6:** saves remember wall changes by graphic number, so dropping graphics shifted them. Older saves stop loading from this build.
- **Stage 2 (22 Nov):**
  - Tired 2 and 3 make searching, building, reading and cooking or crafting take 25% and 50% longer (`tired_tuning`, `survival::action_time`). Tired 3 shows "YOU NEED SLEEP" once. Tired 4 dims the screen over 3 s into the forced 4-hour sleep.
  - Unhappy XP: `character_tuning::unhappy_xp_percent`.
  - Aim camera: camera tuning `Aim shift px` and `Aim ease frames`.
  - Full bag: the in-game HUD has no weight bar, so the search screen's carried weight blinks red 4 times when a take is blocked.
  - Save caps: the pause menu shows `WORLD n%` beside SELECT SAVE+QUIT above 80% (red from 90%). It shows the fullest of container records, item pages, wall and canopy changes and a safehouse's pieces, and the toast warns once at 90%. Item pages are one per searched container holding anything, so about 168 searched containers fill them long before 2,000 stacks; bench 8 reaches 100%. That cap needs a look in milestone 14.
  - Zone loot: the `zone_loot` table in `kx_loot.h`. The test world (benches) keeps 100%.
  - TV: in the small and large houses' living rooms (40% swapped out, 10% a desk). A on it shows the radio's broadcasts plus four of its own while powered, and "NO POWER" after the shutoff unless a generator runs nearby.
- **Stage 3, swimming (22 Nov):**
  - Deep water stays solid for zombies, cars and bicycles; only the survivor's collision lets it through (`box_blocked(..., swimmer)`), and not with Heavy load 2 or worse, unless already swimming.
  - In the water: half walking speed (`Swim mult`), a little louder (`Swim noise +`), no sprinting, crouching, attacking, aiming, items, context actions, building or cooking ("NOT WHILE SWIMMING"). Soaked through at once.
  - Endurance drains like sprinting (`Swim exertion/frame` 3, moving or not): about 50 s to Exhausted 4, which costs 1 health a second with bubbles ("YOU ARE DROWNING"); death cause DROWNED.
  - The swim frames, the ripple and a splash on each stroke; the held items are hidden while swimming.
  - My call on the world's edge: you can't swim into a tile with no land within 8 tiles along the 8 directions ("TOO FAR FROM LAND", the move is undone), which also closes the outer sea.
  - Zombies: a chaser that sees you straight across something solid now follows the flow field around it instead of pressing into it. Round a pond it waits at the shore while you swim, then walks round once you're out. This also helps with fences.
  - Bench 13 (`knox_wild.gba`, `BENCH=13`, test map) started with the swimming phases, logging `KXSWIM` lines. A zombie never entered deep water; it waited at the pond's edge and went round after the survivor got out. Exhausted 4 came after ~530 frames at 20 exertion a frame, then 1 health a second until the survivor reached the shore. With 8 logs (Heavy load 4) the survivor stopped in the shallows; without them they swam out to 8 tiles from land. 18.5% of a frame on average, no missed frames.
  - Bench 1: 567 / 933 per mille (579 / 947 at the end of 12.7), no missed frames.
- **Stage 4, animals and hunting (22 Nov):** `kx_animals.h/.cpp`, every number in `animal_tuning`.
  - **Deer:** up to 2. They spawn 10–14 tiles away on grass, forest, fields or mud in wild and farm cells, in half of those cells each day (from the seed and the day). They graze (head down) and look up, and they notice you at 8 tiles, or 3 crouched (×0.75 in forest, ×0.5 while grazing). Then they flee at sprinting speed, as they do from any noise of 6 or more. 12 health: 2 rifle or shotgun hits, 3 pistol, 2 spear or axe hits, 4 other melee hits.
  - **Carcass:** a dead deer stays on the ground. With a knife or an axe in hand, "[A] BUTCHER" (hold 4 s, the work bar) gives 4 raw meat (venison) and 1 deer hide, plus 15 Foraging XP.
  - **Rabbits:** one at a time by day, outside towns and military bases. They notice you at 4 tiles, or 1 crouched, and a kill drops its raw meat on the ground.
  - **Birds:** flocks of 3–5 on grass, roads and fields by day, only with fewer than 30 zombies about. They fly off with a flutter (a noise of 6) when you come within 5 tiles (3 crouched), a zombie comes within 3, or there's a loud noise.
  - **Guns** lock on to an animal when no zombie is in front; a pellet that misses every zombie can hit an animal (one hit a shot), and hits give Aiming XP. **Melee** swings that hit no zombie hit the nearest animal in reach.
  - **Hide:** "ROPE FROM HIDE" in the Craft tab (by hand, 30 min) makes 2 rope (recipes now have a result count).
  - **Saving:** live animals aren't saved. The latest hunted cells are kept, and a cell where a deer was killed has none for 7 days. Save version 15.
  - Changes from the plan:
    - The spear is a melee weapon in this game (nothing throws it), so spear hunting means stalking crouched: a rabbit lets you within 1 tile, and a deer grazing in the open within 1.5.
    - The hunted-cells list keeps 8 cells, not 16: the world save block had 40 bytes to spare.
    - Deer have a 10 px hit radius (zombies 7, rabbits 6). With Aiming 0 a still, crouched shot at 6 tiles hits about three times in four.
    - The animals have their own random generator, so the zombies' rolls (and the benches) stay as they were.
  - Bench 13 adds hunting phases, logging `KXWILD` lines:
    - A deer 6 tiles away is shot twice with the hunting rifle from a crouch (the bench hunter has Aiming 3, so the run repeats) and drops.
    - It's butchered with a kitchen knife: 4 raw meat, 1 hide.
    - A flock of 5 takes off when walked into.
    - A rabbit is killed with the pistol, and another with the spear from a crouch.
    - One missed frame, right after a scripted teleport.
  - Bench 7 logs `KXWILD7` lines: on the fixed seed, deer appear in the wild cells (up to 2), rabbits now and then (one in the village), and two flocks, one of which took off.
  - Timings: bench 7 is at 519 per mille, against 514 with spawning switched off; the other stage 2–3 changes took it from 505 to 514. Bench 1 is at 570 / 935. Bench 8's save round trip matches.
  - Not seen on screen yet: the sprites and the sounds (flutter, bubbles).
- **Stage 5, snares, freezers and wells (22 Nov):**
  - **Snare:** a new build piece (SNARE: 1 rope, 1 plank, no tool or magazine, 2 s, 6 Carpentry XP), set on grass, forest or mud, walkable. It shares the 16-trap list with box traps (a flag in the trap record, so the save didn't grow). A night's catch chance is 30% + 5% per Foraging level (box traps keep +8%), rolled lazily as before. Snares catch rabbits; box traps catch a rabbit or a bird (from the seed), and the toast says which. Both give raw meat as before.
  - **Chest freezer:** swapped in for 12% of fridges and 15% of shop shelves (shops and petrol stations), with its own loot table (meat, fish, bread, vegetables). Food rolled in one hasn't aged for the days it had power. Food kept in one doesn't age while it has power: worked out once a day at midnight (the grid, or a generator running within 10 tiles). After that it ages at the normal rate.
  - **Well:** every farm and half of the villages (from the seed) have one, on a corner of the crossroads: the lot corner no building covers.
  - Change from the plan: the game has no separate "tainted water" item (unboiled water is drunk straight from a source, at a chance of sickness). So A on a well drinks like a river, with rain water's 10% chance of sickness instead of a river's 35%. It also fills watering cans and counts as a water source for BOIL WATER (the clean bottle), any time, power or not.
  - Bench 13 adds stage 5 phases (`KXWILD` lines):
    - A snare built in build mode and a box trap: both caught on night 5 (the box trap a rabbit), the catches were taken (+2 raw meat) and both set again.
    - A freezer: rolled 2 raw meat and a fish, all fresh. Meat stored on day 7 kept its freshness to day 15, then aged from the power shutoff on day 16 (stale at 4 days, rotten at 6).
    - A well: solid, a water source, and A drank from it (water 79 to 99).
  - On bench 7's seed (a one-off scan, since taken out): the start village had a well and 2 TVs, and the farms along the route had wells on grass and on soil. There was no freezer nearby, which is down to chance.
  - Timings: bench 1 570 / 935, bench 7 521 (3 missed frames, its usual 2 plus one), bench 8's save round trip matches. Save still version 15.
- **Stage 6, sandbox settings, Smoker, lighter (22 Nov):**
  - **Sandbox** (`kx_sandbox.h/.cpp`): the new world screen now has the seed and a SANDBOX list, laid out as in mockup 13 (checked with a DUMP render). Up and down pick a line; left and right roll the seed or change a setting; a line below says what the value does.
    - Zombie speed: Shamblers / Normal / Fast. Sprinters 0 / tuning (2%) / 10%, fast shamblers 10 / tuning (18%) / 30%.
    - Zombies: Few / Normal / Many, ×0.5 / ×1 / ×1.5 on the head counts at world creation.
    - Loot: Rare / Normal / Plenty, ×0.5 / ×1 / ×1.5, on top of the zone and the debug menu's amount.
    - Shutoff: Week 1 (water day 4–6, power 6–8) / Normal (the timeline's 10–20 and 14–30) / Month 2 (40–50, 50–62) / Never.
    - Day length: 30 / 60 / 90 real minutes (scales the frames per in-game minute).
  - The settings live in the save header's spare bytes, covered by its CRC, so the world block didn't grow. Save version 16. Loading restores them before anything reads them.
  - Changes from the plan:
    - Shutoff's options are Week 1 / Normal / Month 2 / Never rather than Week 1 / Week 2 / Month 1 / Never, so Normal stays the game as it was.
    - The pause menu has no room for the settings, so they show read-only in the Map tab's left margin (the 30 px beside the map) as WORLD and five short lines.
  - **Smoker:** a bad trait worth 2 points, the only one without a good opposite. The creation screen's last row has it alone (checked with a DUMP render). After 8 in-game hours without a cigarette ("YOU NEED A SMOKE"), unhappiness climbs a point every 10 minutes and panic is held up from 20, rising 5 an hour to 45. The craving clock is in the survival block, so it is saved.
  - **Cigarettes:** packs of 20, found in bedrooms, shops and petrol station tills. Using one (Gear tab or hotbar) takes 3 s, needs a lighter or matches, and takes 20 off unhappiness and 10 off panic.
  - **Lighter:** a tool with 50 uses that works wherever matches do (campfires, relighting, cigarettes). Matches are still never used up, as before; a lighter loses a use each time it lights something when no matches are carried, and an empty one is thrown away. Found in tills, shops and bedrooms.
  - Bench 13 adds stage 6 phases:
    - Fast, many, rare, month 2, 90 minutes: 225 frames a minute, water off day 47, power 58, loot ×0.5, population ×1.5. Of 30 zombies, 1 sprinter and 12 fast.
    - A Smoker 9.5 hours without a cigarette is craving (Panic 1). A cigarette lit with the lighter ends it (19 left, lighter 49).
    - A campfire lit with the lighter alone (48 left).
    - The Map tab with the settings in its margin.
  - Bench 8 now plays with settings that aren't the defaults (shamblers, many, plenty, never, 30 minutes) and resets them before loading: the loads bring them back, and the world round trip matches.
  - Timings: bench 1 571 / 936, bench 7 522, unchanged.
- **Stage 7, benches and docs (22 Nov):**
  - Bench 13 (`knox_wild.gba`, `BENCH=13`) adds a flock scattered by a pistol shot from 8 tiles (out of its notice range), and Tired 4: after the next in-game minute the screen dims and the survivor sleeps 4 hours where they stand (fatigue 95% to 23%). Then the Map tab with the settings. It plays the test map rather than a generated world (the plan's wish), so its phases can place what they test; bench 7's `KXWILD7` lines and a one-off scan cover the generated world.
  - Bench 8's round trip adds a snare, a hunted deer cell and the Smoker's clock (100 minutes), on top of the sandbox settings; all come back after loading.
  - The chase check from stage 3 now reuses the straight facing it works out (one square root fewer), and the swimmer's deep-water lookup only runs when the survivor changes tile.
  - README (status, bench 13, bench 8, controls, a milestone 13 section, the layout) and the spec (traits, Tired, Morale, zombie speed, deep water, the aim camera, zone loot, freezers, traps, hunting, wells, the shutoff windows, TV, world creation, the full bag, the caps) are updated.
- **Timings at the end** (per mille of a frame, average / worst; end of 12.7 in brackets):
  - bench 1 571 / 936 (579 / 947), no missed frames
  - bench 2 214 (206)
  - bench 3 167 (161)
  - bench 11 226 (216)
  - bench 7 522 (505), its usual missed frames plus one
  - bench 10 512 (501), 48 missed frames on its scripted teleports
  - bench 12 367, 130 missed frames on its scripted teleports and breaks
  - bench 9 203, 3 missed on its scripted moments
  - bench 13 202, its missed frames all scripted (teleports, spawning 30 zombies, ticking hours in one frame)
  - bench 8's round trip matches; bench 6 runs to its scripted infection death (it never prints a summary)
  - IWRAM 26.6 KB (26.4): the swimmer test in the IWRAM collision code and the well in chunk generation.
- **Not explained:** the light test-map benches (2, 3, 11) are 3–5% slower than at the end of 12.7. Their timed systems are close to where they were, and the difference is in the untimed part (rendering, Butano, audio). Bench 1, the heavy one, got faster. It needs a profile on the device.
- **Not checked yet:** everything on screen and in sound apart from the two screens rendered from DUMP logs (the new world screen, the creation screen's Smoker row, the Map tab's margin): the swim frames and ripple, the animals, birds, carcass, snare, well, freezer and TV tiles in the game, the flutter and the bubbles. And the Miyoo.

## What this milestone covers

The spec's build order item 13 ("Content and polish") is split in two (your choice, 20 Nov 2026):

- **Milestone 13 (this plan): the spec systems still missing.** Animals and hunting, swimming in deep water, snares, freezers and wells, sandbox settings on world creation, the Smoker trait, and the small loose ends the audit below found.
- **Milestone 14 (planned later): content.** Filling out to about 200 items and 30+ building templates, set pieces per town tier, music, a sound pass and balancing, then stretch goals. Its outline is at the end of this file.

## Audit: the spec against the code (20 Nov 2026)

Every section of `docs/design-spec.md` was read against the source. These are the gaps, with where each one goes.

| Spec says | What the code has | Goes to |
|---|---|---|
| Animals 6 sprites; hunting rifle "farms and barns"; snares catch small game | no animals; box traps only (they roll small game) | 13, stage 4 |
| Deep water is swimming (endurance drain, soaks you) | deep water blocks you like a wall | 13, stage 3 |
| Snare and box traps (Foraging) | box traps only | 13, stage 5 |
| Fridges and freezers slow spoilage while powered | fridges only | 13, stage 5 |
| Water from rain collectors, wells or rivers after the shutoff | rain collectors and rivers; no wells | 13, stage 5 |
| World creation: sandbox settings (zombie speed, loot rarity) | seed only; `kx_title.h`: "Sandbox settings come later"; a "Loot amount %" stand-in in the debug menu | 13, stage 6 |
| Header holds sandbox settings | not in the header | 13, stage 6 |
| Smoker trait waits for cigarettes | no Smoker, no cigarettes, no lighter | 13, stage 6 |
| Tired: slower actions, then forced sleep | Tired 3+ slows attacks only; no forced sleep | 13, stage 2 |
| Morale: slower XP | Unhappy has no effect on XP | 13, stage 2 |
| Aiming shifts the camera towards the aim | no aim shift | 13, stage 2 |
| A full bag flashes when blocked | a "BAG FULL" toast only | 13, stage 2 |
| Save caps (items, chunks, pieces) show in the UI when close to full | only the world-storage-full toast | 13, stage 2 |
| Loot multiplied by zone tier (military ×3 weapons, coast ×0.5) | weights per building only | 13, stage 2 |
| TV and radio broadcasts | radio only | 13, stage 2 |
| Machete (melee), lighter (cooking) | neither exists | machete 14; lighter 13 (stage 6, the Smoker needs it) |
| ~200 items | about 145 | 14 |
| 30–40 building templates | 8 | 14 |
| One set piece per town tier (church, crashed bus, police barricade, military checkpoint) | none (landmark weights only) | 14 |
| Wilderness scatter: hunter stands, wrecked cars, campsites, bodies | tents, bodies, crates | 14 |
| Music and sound pass | sound effects only | 14 |
| All text in one ROM string table, ready for translation | strings live in each screen's code | 14, ask first |
| Stretch: upstairs and basements, raider camps, fishing mini-game, snow footprints | none | 14, if budgets allow |

Checked and already built: the colour drain for dehydration and blood loss, the panic camera shake, the Park ranger's forest perk, the Hot and Fever moodles, fractures from falls, the car stall, the rare-item chime, the world-storage-full toast, the 8×8 variable font.

## Choices you made (20 Nov 2026)

- **Split:** milestone 13 builds the missing systems; milestone 14 is content.
- **Animals:** rabbits and birds caught in snares and box traps; deer in the woods that flee from noise, hunted with a gun or a spear, butchered into meat and hide. Birds also live on the map as flocks that scatter.
- **Music (milestone 14):** 4–5 short, moody original loops on the GBA's old sound channels (cheap on CPU): title, calm day, night, horde chase, death, with long silences between.
- **Content (milestone 14):** the spec's amount, about 200 items and 30+ templates, with a second tile bank for town buildings.

## Design

### Loose ends (stage 2)

- **Tired:** at Tired 2 searching, crafting, building and reading take 25% longer, at Tired 3 50%. At Tired 4 the screen dims for 3 s and you fall asleep where you stand (the sleep screen, 4 hours, no choice), loud enough nearby zombies may find you.
- **Unhappy:** XP is multiplied by 90% / 75% / 60% / 50% at Unhappy 1–4, after traits and books.
- **Aim camera:** while aiming, the camera's lookahead target moves 24 px further along the aim, eased over 12 frames, and back when you let go.
- **Full bag:** when a pick-up or a take fails for weight, the weight bar in the HUD and the Gear tab flashes red for 4 frames (as well as the toast).
- **Save caps:** the pause menu's Save line shows "WORLD 92%" when any of the world lists (items on the ground, generated-chunk changes, base pieces, the wall-change list) is above 80%, and a toast warns once at 90%.
- **Zone loot:** each container roll is scaled by its cell's zone: military ×3 on weapons and ammo, town tiers 1/2/3 ×0.8/×1/×1.2 on everything, coast and wilderness ×0.5. The debug menu's "Loot amount %" stays as a multiplier on top (and becomes the sandbox loot setting in stage 6).
- **TV:** houses get a TV in some living rooms (furniture swap). With the power on, A shows a broadcast as the radio does (same texts plus a few TV-only ones); after the power shutoff it shows static.

### Swimming (stage 3)

- **Deep water can be entered.** In it you swim: 50% of walking speed, no sprinting or crouching, no attacking, aiming or using items; the held weapon stays in hand.
- **Costs:** endurance drains (Exhausted 4 makes you start drowning: 1 health per second and a bubbles sound), Wet goes to 100 at once, and Heavy load 2+ stops you swimming at all (you can't step in).
- **Look:** the player sprite is drawn in a swimming frame with its lower half hidden; a ripple sprite follows.
- **Zombies don't swim:** they stop at the shore and wait. A swim across a river is a real escape.
- **The world's edge** stays closed: swimming more than 8 tiles past the last land of the map pushes you back.

### Animals and hunting (stage 4)

- **Deer:** up to 2 at once, spawned (from the seed and the day) in forest and field chunks away from towns as you walk; up to 4 animal sprites in total. They graze, notice you at 8 tiles (4 crouched, less in forest), and flee at sprinting speed from you, gunshots and other noise above 6. A deer takes 2 rifle or shotgun hits, 3 pistol hits, 2 spear throws or 4 melee hits. A dead deer is a carcass: with a knife or axe in hand, hold A for 4 s to butcher it into 4 venison (raw meat) and 1 hide. A deer you kill doesn't come back to that macro cell for 7 days.
- **Rabbits:** hop about in grass and forest in daytime, 1 at a time near you, flee when you come within 4 tiles. A thrown spear or any gun kills one. Mostly caught in traps.
- **Birds:** a flock of 3–5 sprites sits in fields and on roads; it flies off when you or a zombie come near or a gun is fired, with a flutter sound. Flying birds are a warning and a noise of their own (radius 6). Birds are caught in box traps, not shot.
- **Hunting** gives Aiming XP for shots and Foraging XP for butchering. Hide is crafting material (milestone 14 uses it; for now it goes into rope, 1 hide → 2 rope).
- **Saving:** live animals are not saved (they respawn from the seed); only the per-cell "deer killed on day N" list (16 entries) is.

### Snares, freezers and wells (stage 5)

- **Snare:** a build piece from 1 rope and 1 plank (no magazine needed); set in forest or grass, health 1. It catches a rabbit overnight 30% of the time (+5% per Foraging level), lazily like box traps (rolled from the seed, the trap and the day). Box traps catch rabbits and birds.
- **Freezer:** a chest freezer furniture in shops, the petrol station and some kitchens (furniture swap). Food in a powered freezer doesn't spoil; when the power goes, it starts spoiling from that moment at the normal rate (worked out lazily from the shutoff day or the generator's last run).
- **Well:** one per farm and in about half of villages. A on it fills a water container with tainted water (boil it), any time, power or not.

### Sandbox settings, Smoker, lighter (stage 6)

- **World creation** gets a SANDBOX line after the seed. It opens a list (up/down to pick, left/right to change):
  - Zombie speed: Shamblers / Normal / Fast (sprinter shares 0 / 2 / 10%, fast shambler 10 / 18 / 30%).
  - Zombies: Few / Normal / Many (population ×0.5 / ×1 / ×1.5).
  - Loot: Rare / Normal / Plenty (×0.5 / ×1 / ×1.5).
  - Shutoff: Week 1 / Week 2 / Month 1 / Never (water and power).
  - Day length: 30 / 60 / 90 minutes.
  - The settings live in the save header and are shown, read-only, in the pause menu.
- **Smoker** (negative trait, gives 2 points): without a cigarette for 8 in-game hours, unhappiness and panic climb slowly. Smoking (from the hotbar, needs a lighter or matches) takes 3 s, lifts unhappiness by 20 and panic by 10. Cigarettes (packs of 20) are loot in houses, shops and petrol stations.
- **Lighter:** lights campfires like matches but with 50 uses instead of 10.

### Tiles and sprites

- **World tiles:** the tileset has 1 free graphic (127 of 128); this milestone needs 4 (snare, well, chest freezer, TV). Stage 1 frees 3 by drawing the snare as a box-trap variant on the wall layer's palette swap and by sharing two near-identical grass and floor graphics. The real fix, a second tile bank, comes in milestone 14.
- **Sprites:** deer (2 frames each way, 32×32 tiles of 16×16 frames), rabbit (16×16), bird (8×8, 2 frames), the swimming player frame, the ripple.

### Saving

Version 14: the sandbox settings in the header, the Smoker's last cigarette time in the character, snares in the trap list (16 shared with box traps), freezers' contents as containers, the deer-killed list in the world block.

## Stages

A ROM to try after each.

1. **Mockup 13.** `tools/mockup_wild.py` draws `mockups/13-wild.png`: swimming with zombies stuck at the shore, a deer, rabbit and a flock, a butchered carcass with the progress bar, a snare, a well, a chest freezer and a TV, and the sandbox settings list. While you look at it, stage 2 goes ahead.
2. **Loose ends.** Tired and Unhappy effects, forced sleep, the aim camera, the bag flash, save caps in the UI, zone loot, TV broadcasts.
3. **Swimming.** Deep water, the swim frame, drowning, zombies at the shore.
4. **Animals and hunting.** Deer, rabbits, birds, hunting, butchering.
5. **Snares, freezers and wells.** With the tiles freed in stage 1.
6. **Sandbox settings, Smoker, lighter.** The world creation list, the header, the trait and its items. Save version 14.
7. **Benches and docs.** Bench 13 (`knox_wild.gba`, `BENCH=13`) on a generated world: a swim across a river with a zombie left at the shore, a deer shot and butchered, birds scattering at a shot, a snare and a box trap checked after a night, a freezer before and after the power shutoff, a well, forced sleep, logging `KXWILD` lines. Bench 8 adds the sandbox settings, a snare, the Smoker's timer and the deer list to its round trip. README, spec sections, timings.

## Done when

1. You can swim across deep water at the stated cost, drown when exhausted, and zombies wait at the shore.
2. Deer, rabbits and birds appear where described, flee as described, and can be hunted, trapped and butchered.
3. Snares, freezers and wells work as described, also after the shutoff.
4. A new world can be made with each sandbox setting, and the setting changes the game as described.
5. The Smoker needs cigarettes; the lighter works.
6. The loose ends of stage 2 all work.
7. All of it survives a save and load.
8. Bench 1's day timings stay within 1% of now; bench 13 stays inside the frame budget apart from scripted moments.

## Risks

- **Sprites:** the spec budgets 6 animal sprites; deer, a rabbit and a flock of 5 birds could be 8. Birds share one sprite per pair if needed, and flocks only appear when fewer than 30 zombies are on screen.
- **Tiles:** freeing 3 more graphics costs some variety (one grass and one floor look). If that shows, the TV waits for the second tile bank in 14.
- **Swimming** changes a wall into something you can cross: escapes zombies can't follow could make rivers too safe. Endurance cost is the brake; tune it in play.
- **Forced sleep** can kill a careless player outright. Tired 3 gets a clear toast ("YOU NEED SLEEP") first.
- **IWRAM** is at 26.4 KB. Animal code stays in ROM; there are at most 4 animals.
- **Save format** becomes version 14, so older saves stop loading.

## My calls, not asked

- All the numbers above (deer hits and senses, snare odds, drowning, sandbox values, Smoker timing, the Tired and Unhappy scales).
- Zombies don't swim.
- Animals are not saved, only the deer-killed list.
- Hide turns into rope until milestone 14 gives it more uses.
- The settings list and its options (spec: "zombie speed and loot rarity", the rest added).
- TV goes into this milestone (it is a small system); the machete waits for 14's items.
- Mockup first (mockup 13), as for 12.6 and 12.7.

## Milestone 14 outline: content (planned later)

1. **A second tile bank** for town buildings, swapped as you move between wild and town areas, so new templates have room.
2. **Templates:** from 8 to 30+ (church, school, motel, supermarket, warehouse, fire station, bar, apartments, farmhouse variants, cabins and others).
3. **Set pieces per town tier:** church, crashed bus, police barricade, military checkpoint.
4. **Items:** from about 145 to about 200 (machete, hunting knife, more food, clothing, tools, books), and hunter stands and campsites in the wilderness scatter.
5. **Music:** 4–5 moody loops on the GBA's old sound channels (title, calm day, night, horde chase, death), and a sound pass.
6. **Balancing** across loot, zombies, survival and skills.
7. **Translation string table**, only if you still want it (ask first).
8. **Stretch goals** if budgets allow: upstairs and basements, raider camps, fishing mini-game, snow footprints.
