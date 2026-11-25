# Milestone 14: content

23 Nov 2026 · status: built (all 8 stages, 23 Nov 2026), awaiting device testing. Save format version 17.

## Progress

- **Stage 1 (23 Nov):** `tools/mockup_towns.py` draws `mockups/14-towns.png`, nine panels:
  - the supermarket, school and motel
  - the village church with its graveyard
  - the crashed coach, the police barricade and the military checkpoint
  - a hunter stand and a campsite
  - the new furniture, and survivors in police, army, firefighter and farmer clothes

  The 29 new graphics (15 pieces of furniture, the 8-part coach, road block, sandbags, barrier, watchtower, grave, hunter stand) are in `tools/assets/world14.py`. The four buildings shown are in `buildings.TEMPLATES_14`, with new furniture letters and outdoor furniture (graves stand on the natural ground). They join the world in stages 3 and 4.
- **Stage 2 (23 Nov):**
  - **Renderer:** the three world layers are 32×32-cell (256×256 px) wraparound maps, one 2 KB screenblock each; the streamer's index is simply `(y & 31) * 32 + (x & 31)`.
  - **Tileset:** up to 255 graphics (1,020 hardware tiles over two charblocks), not 256: `world_gfx` is a `uint8_t` enum whose COUNT must fit. Now 172:
    - the 128 there were
    - the second plain grass and the two weathered roofs back
    - blood, corpse and item decals on sand, soil (crops share them) and both bridges back
    - the 29 milestone 14 graphics (not placed in the world yet, and not solid until stage 3)
  - **Checked:**
    - A new `CHECK=1` bench build compares every visible cell of the three layers with the world every 30 frames (`KXRENDER` lines). Bench 7 (a sprint loop through a generated world) and bench 10 (night, the roof cut-away, reading, teleports) showed 0 stale cells in 86 and 120 checks.
    - One screenshot of bench 7 in mGBA showed the world in place: a house with its roof cut away, painted walls, doors, furniture, a TV.
  - **Item storage:** 512 pages (168 before) in EWRAM, each record's page in a RAM-only `_page_of` (the saved 8-byte record keeps a has-items flag). The save's room is now a budget of 1,840 stacks over all the containers. Its worst case is the same size as the 168 full pages before, because the save writes only filled slots.
    - When the budget is spent, loot rolls are cut short, and storing or dropping an item is refused ("WORLD STORAGE FULL").
    - The pause menu's WORLD % counts the budget.
    - Bench 8: the same play now keeps 234 stacks at 34% full. Before, it was at 100% and dropping the oldest containers' contents when pages ran out. Its round trip matches.
  - Save format version 17 (graphic numbers moved).
  - **Timings** unchanged: bench 1 570 / 935, bench 7 521, bench 13 201, bench 2 214. The light benches' 3–5% from milestone 13 is not explained yet: it isn't the renderer (its cost didn't move), so it needs the profiler on hardware.
  - Not yet: filling the store past 450 containers in bench 8 (stage 8).
- **Stage 3, buildings (23 Nov):**
  - **25 templates** in `buildings.TEMPLATES` (33 in all; the church waits in `TEMPLATES_14` for stage 4):
    - homes: cottage, bungalow, terraced pair, flat block, trailer, farmhouse, large farmhouse, holiday cabin
    - village landmarks: pub, post office, small school
    - town landmarks: school, supermarket, motel, warehouse, fire station, pharmacy, hardware store, diner
    - military base: barracks, armory, mess hall
    - farms and the wild: farm shed, stable, hunter's cabin

    All the new graphics are solid; bunk beds sleep like beds.
  - **Zone weights:** each row in `kx_world_gen_tuning.h` sums to 100 and is checked against the template count at compile time.
    - **More landmark lots:** towns and the military base have landmarks on all four lots by the crossroads, plus 35% of a town's other lots; villages on two. With one per cell, as before, the 12 kinds of town landmark couldn't go round: a first census had no police station, clinic, school or supermarket.
    - **Farms** may have a second, smaller building (barn, shed or stable) on an inner lot.
    - **3% of forest and woods cells** hold a cabin (hunter's 55%, holiday 35%, trailer 10%) in a small clearing.
  - **Homes** (locks, keys, house safes) are a list now, `world::detail::home()`: the two houses, the new homes and the cabins.
  - **8 new loot tables:** school, office, warehouse, armory, vending, fire station, pub, hunter.
    - Lockers, shop shelves and counters loot by their building: a fire station's lockers, the base's lockers (military), a pharmacy's shelves (medicine), a hardware store's (garage), a pub's or diner's bar, a post office's counter (office), a school's lockers.
    - A gun rack is an armory's, or a hunter's in a hunter's cabin. The stove stays decoration, so the kitchen sink and stove still come from the kitchen cabinets.
  - **The armory's gun safes:** notes name them ("ARMORY SAFES") and the dial says ARMORY.
  - **Census of the fixed seed** (bench 7 at its start, `KXTOWN` lines): every template appears except the post office (0 this seed, 12% of village landmarks). Some counts: small houses 11, large 11, flats 3, trailers 6, police stations 5, schools 4 (and a small one), supermarkets 3, motels 4, fire stations 6, pharmacies 4, barracks 5, armories 4, mess halls on the base, and 47 cabins in the wild (25 hunter's, 18 holiday).
  - **Bench 12** needed three script fixes, none in the game:
    - It now takes the last home safe it finds. The first was in the house where its door phase leaves a zombie.
    - It makes the safe's chunks at once.
    - It keeps zombies away and ends a grab left over from the door phase while it works the dial.

    It passes again: the wrong code, the right one, the safe searched, the stash.
  - **Timings:** bench 1 570 / 937, bench 7 520, bench 12 356 (185 missed frames on its scripted teleports and breaks); bench 8's round trip matches.
  - **Not checked on screen:** the new buildings in the running game (only the template sheet and the census).
- **Stage 4 (23 Nov):** set pieces and the wild.
  - **Tiers:** the generator has no 1-cell town: all three towns are 2×2 cells, the spec's big-town tier (they already loot at ×1.2). So every town gets both the crashed coach and the police barricade. Villages get the church, the military base the checkpoint.
  - **The church** (`building::CHURCH`, now in `TEMPLATES`) takes each village's first landmark lot; its second keeps a shop, petrol station, pub, post office or small school. Pews, the altar, a desk in the corner (desk loot), double doors, and a row of graves on the natural ground behind it. Template tiles with no floor (graves) now keep the ground of the land around them.
  - **Road set pieces** (`road_piece` in `kx_world.h`; placed in `place_pieces` and drawn by `piece_at` in `kx_world_gen.bn_iwram.cpp`). The seed picks the cell of the 2×2 settlement, trying each quarter in turn until one has the road:
    - **Crashed coach:** on an east-west road towards the town's centre, 6 tiles from a crossroads, over two of the three lanes, its crumpled front towards the crossroads. Its door and a middle window are searchable (new loot table `BUS`, "COACH LUGGAGE": clothes, bags, snacks, painkillers, a map, letters). A dead survivor lies beside it, with blood on the road ahead and a corpse behind.
    - **Police barricade:** on a road out of town, 18 tiles from the crossroads: two staggered rows of road blocks (a chicane you can walk through), sandbags along one side ending in a police locker, a dead survivor, blood and a corpse on the road.
    - **Checkpoint:** on a road out of the base, 16 tiles out: a barrier over two lanes (one lane and the pavements stay open), sandbags out to both sides, a watchtower, and a crate that rolls on the military crate table.
    - Set pieces are allowed on bridges. Parked cars and wrecks keep 7 tiles clear of them. Zombies that appear within 12 tiles of the barricade or checkpoint wear police or army uniform 70% of the time, so their corpses carry that loot.
  - **The wild:** the scatter is now tents 22%, dead survivors 15%, campsites 15%, hunter stands 18% and crates 30% (of the 35% of 12×12 blocks with a find).
    - A **campsite** is a tent with a cold campfire and a crate two tiles below it, kept inside its block. The campfire is decoration: no flame, no heat.
    - A **hunter stand** grows only in forest and woods (elsewhere the spot stays empty) and is a container (new table `HUNTER_STAND`: the hunter's cabin loot, 0–2 items).
  - **Census** (bench 7, fixed seed; `KXTOWN` lines now list the set pieces): 9 churches (every village), 3 coaches, 3 barricades, 1 checkpoint. Bench 7 at its start drew each piece and a church as text from the generated tiles to check them; that dump was taken out again, as its frames counted in the bench.
  - **Speed:** the chunk generator now keeps the last row's cell info (a chunk's rows share their macro cell) and keeps the set-piece code out of line. World generation on bench 7 is 156 ticks a frame (150 after stage 3, 139 before it). Most of the change is the world's content (more buildings, the new finds); the frame is unchanged.
  - **Timings:** bench 1 570 / 937, bench 7 524 (7 missed), bench 13 202, bench 9 203. Bench 12 is 362, with 227–245 missed frames against 185, all on its scripted teleports, where frames already ran at 1.03–1.08. Benches 8 and 12 pass: the round trip matches; the wrong code, the right one, the safe, the stash.
  - **Not checked on screen:** the set pieces were checked as text from the running game's tiles, not seen drawn.
- **Stage 5 (23 Nov):** items. 70 new, 200 in all (`item_id` stays a byte; the new ones come after the old, so their ids don't move). Each has its icon, and every one that can be found is in at least one loot table (checked by a script over `kx_loot.h`; the three new dishes are cooked, not found).
  - **The list moved a little from the plan:** the padlock and chain went (nothing to lock against without other survivors), the fishing rod came in; overalls took the scarf's place (no neck slot); the toolbox opens into tools rather than being a second bag; the radiator and brake pads repair a car's condition (nothing could before).
  - **Food and drink (+15):** canned soup and peaches, pasta, rice, cereal, cookies, juice, beer, onions, tomatoes, eggs (raw: cook them), honey, and three dishes: pasta dinner (pasta, a tomato, water), rice bowl (rice, produce, water), fried eggs. Peaches, cookies, honey, juice and beer lift morale like chocolate. Onions and tomatoes count as produce.
  - **Medical (+6):**
    - A sterile bandage bandages and disinfects at once; the treatment picks it when a wound needs both, and keeps it otherwise. Alcohol wipes disinfect like the bottle, one wipe each. Boiling two rags in a pot by water makes two sterile bandages.
    - Vitamins take 10 sickness off, cold pills 30. Sleeping pills make you tired enough to sleep.
    - A first aid kit opens into 2 sterile bandages, 2 wipes, painkillers and a suture needle.
  - **Melee (+8), in the existing classes:** machete (axe class), hatchet (axe class, one hand, chops trees), golf club and hockey stick (bat), sledgehammer (axe class, heavy; forces locked doors like the crowbar), pitchfork (spear), cleaver (knife), police baton (bat, one hand). Each has its own held picture.
  - **Guns (+2):** a revolver (6 rounds, 9 tiles, damage 4, slower than the pistol, noise 35) and .38 rounds; police lockers, the armory, police zombies.
  - **Tools and materials (+13):**
    - Pliers make a lockpick from wire. A lockpick picks in 3 s with a 20% slip (a screwdriver: 5 s, 40%).
    - A hammer beats 3 scrap metal into a sheet. A sheet reinforces a barricade without Carpentry 3 (one sheet each time).
    - A tarp does for the rain collector's garbage bag.
    - Cloth tears into 3 rags. A sewing kit (10 uses) mends the worn piece most worn by 40, with cloth or a rag.
    - Zip ties mend a weapon by 40% (duct tape 50%, glue 25%).
    - A bucket waters plots like the watering can (20 waterings).
    - Binoculars reveal the map 3 chunks round you.
    - A fishing rod gives 2 more finds when foraging by water.
    - A toolbox opens into 3 of: hammer, screwdriver, wrench, pliers, nails, duct tape, zip ties, wire, saw.
  - **Clothing and bags (+20):** T-shirt, shirt, sweater; police, army, firefighter and padded jackets; cargo pants, suit trousers, overalls; sneakers, wellies, army boots; work gloves; cap, helmet, bandana; duffel bag (+12 kg), satchel (+5), large backpack (+22). Each shows on the survivor as a palette swap, in mockup 14's colours for the uniforms. Hats now count against bites to the body, so the helmet helps (the cap and bandana add warmth only).
  - **Car parts (+3):** a car jack halves the time to fit or take off a tyre. A radiator (+40%) or brake pads (+25%) with a wrench repair a car's condition, from a new REPAIR row in the car screen.
  - **Farming (+3):** tomato and onion seeds (14 and 18 days); a plot keeps the crop's third bit in a spare bit, so older saves read the same. A kettle boils water and makes tea and hot chocolate instead of the pot.
  - **The Craft tab** has 23 recipes (16 before), so its list scrolls 14 at a time with arrows above and below.
  - The build screen's materials line names hinges, doorknobs, logs and rope instead of "STICKS" (an old slip).
  - ROM 1.35 MB (1.16 before): the held pictures are 16 KB each.
  - **Timings** unchanged: bench 1 571 / 935, bench 7 525 (7 missed), bench 3 167, bench 9 203, bench 11 226, bench 12 362 (227 missed on its teleports), bench 13 202. Bench 3 turned up the new cookies; bench 8's round trip matches; bench 12 passes (wrong code, right code, safe, stash).
  - **Not checked on screen:** the icons and held pictures were checked as an image from the asset scripts; the new uses (Craft tab scrolling, the car screen's REPAIR row, sewing, binoculars, kits) haven't been tried in the running game.

- **Stage 6 (23 Nov):** music and sound.
  - **Five loops** written by `tools/assets/music.py` (run with the other asset scripts) as 4-channel MOD files in `dmg_audio/`, which Butano converts with mod2gbt for GBT Player on the old (DMG) channels; a `.json` beside each turns off mod2gbt's 50 to 60 Hz speed change, so each song's speed is in frames per row. Each is written note by note in the script: chords, a bass line on the wave channel (triangle), plucked chord tones or a drone on the second pulse, a melody on the first, and a little noise.

    | Loop | Key, speed | Length | Plays |
    |---|---|---|---|
    | Title | A minor, 10 frames a row | 43 s | looped on the title and character screens; 8 bars of chords, then the melody over soft hats |
    | Calm day | A minor, 14 | 45 s | once, now and then by day |
    | Night | D minor, 18 | 58 s | once, now and then by night: low, over a drone |
    | Horde chase | A minor, 6 | 32 s | looped while 3 or more zombies chase: driving bass, a thud and hats |
    | Death | A minor, 16 | 34 s | once under the death screen: a falling line ending on the tonic |

    A piano-roll drawing of the five showed the accompaniment above the melody in four of them and the night drone in unison with it; both were moved down an octave. I can't listen here, so how they sound is for you to judge.
  - **The music director** (`kx_music.h`): silence for 40 s after a world starts, then a quiet tune (calm or night by the clock) every 75–165 s of silence. The chase loop starts at once when 3 zombies are chasing or attacking, plays on 5 s after the last gives up, then fades over 3 s. Sleep fades whatever plays. The death tune starts when the death screen shows. The music sits at half the DMG mixing level, the quiet tunes at 70–75% of that. Tuning in `music_tuning`.
  - **Sound pass:**
    - **Levels:** every effect was normalised to the same peak, which left square-wave blips (menu sounds, the alarm) two to three times louder than gunshots. `audio.py` now brings anything over 0.26 RMS down to it, and drives the guns, crashes, heavy hits, weapon breaks, glass and thunder through a soft clip so they land as hard as the loudest. The spread is now 0.11–0.26 RMS (was 0.11–0.52).
    - **New sounds:** a zombie's feet dragging (the nearest walking zombie within 56 px, every 26 frames, louder the nearer, panned); glass crunching underfoot on a smashed window (quieter crouched); rain drumming on the roof while indoors in rain (louder in storms); gusts of wind outdoors in the wild and anywhere in a storm, every 8–22 s, panned at random. The wind's timing comes from a hash of the frame count, so the game's random numbers and the benches don't move.
    - Stereo: the new sounds pan like the old (by screen position).
  - **Benches log `KXMUSIC`** lines when the director changes track, with whether GBT Player is sounding: bench 1 and 7 chase at 3 chasers, bench 4 the death tune, bench 7 the calm tune after its first 40 s.
  - **Cost:** GBT Player and the director take about 3,600 cycles a frame (1.3%), outside the measured sections, which are unchanged. Bench 1 571 → 584, bench 7 525 → 534 (7 missed, as before), bench 9 209, bench 10 527, bench 12 370, bench 13 206, bench 2 220; bench 8's round trip matches and bench 12 passes. The director chose: the chase at 3 chasers (benches 1, 7, 8, 9, 10), the night tune at night (bench 10), the calm tune after 40 s by day (benches 7, 12, 13), the death tune (benches 4, 8).
  - ROM 1.42 MB.
  - **Not checked:** the music and new sounds haven't been heard (mGBA runs headless here), and gpSP hasn't been tried.

- **Stage 7 (23 Nov):** balancing.
  - **The loot census: bench 14** (`knox_towns.gba`, `BENCH=14`), on the fixed seed. At its start it registers every container of every building (furniture swaps and all, as generated), opens it with the game's own roll on day 1 (zone tier, sandbox and debug loot amounts), tallies it and removes it again. `KXCENSUS` lines per building kind and per zone give the roll and the expected values from the tables (a single roll per container is a small sample). `tools/census_report.py <log>` averages them per building and checks the targets:

    | Target | Range | Result |
    |---|---|---|
    | A house's kitchen feeds you about a day and a half (a day is about 33 food points) | 40–66 points | 58 |
    | A gun about every 25 homes (loose; safes not counted) | 1/35–1/18 | 1/29 |
    | Ammunition more often than guns | 1 find in 12 homes or more | 1 in 2.5 |
    | The military base worth the risk, not an arsenal | 0.5–1.5 guns a building | 1.4 (about 19 on the base, most in the armory) |
    | and its ammunition | 1.5 finds a building or more | 4.4 |

  - **What the census turned up:**
    - **A stage 5 slip:** the first line of new entries in each loot table had landed on the `// Milestone 14:` comment line, so 29 lines of new items never dropped (canned soup, eggs, T-shirts, pliers and more). My stage 5 check read the file with its comments. Fixed, and checked again with comments stripped: everything is in a table except what is cooked, made, grown, chopped or butchered.
    - **Kitchens** gave 88 food points a house (2.7 days) once the new foods were in. Kitchen cabinets now hold 0–4 items (1–5 before), fridges 0–3 (1–4), and the new foods weigh a little less.
    - **The base** gave 2.3 guns a building (33 in all): its lockers rolled on the crash sites' crate table (3–7 items, weapons ×3). They have their own footlocker table now (1–3 items: fatigues, boots, rations, bandages, ammunition, now and then a pistol).
    - **Motels had no containers**, and their rooms were shut by a bath inside each door.
  - **Templates you couldn't walk round:** a check over every template found rooms shut off in several of the stage 3 ones: the flat block's front door opened onto a wall, the large farmhouse's hall door was behind a bed, the pub's and diner's kitchens were behind their bar counters, the school's classrooms and the barracks' back rows were behind desks and bunks; and a few odd crates or cabinets (barn, trailer, hardware store, stable) stood out of reach. All fixed. The check (`check_reachable` in `buildings.py`) now runs with the asset build and fails it if any searchable furniture or bed can't be walked up to; only decoration (blackboards, pews, the altar, tables, stoves, TVs, toilets, baths, hay) may stand out of reach.
  - **Survival numbers:** a day costs about 30 food points (awake 24, asleep 6) and 54 water, more on the move; the survival bench shows the same drain (80 to 63 food and 80 to 21 water over a day with one meal). With the targets above, a house's kitchen is a day and a half, and its taps last until the water goes off (day 10–20). Unchanged.
  - **Zombie density** against loot: a village is about 22 zombies to 4 buildings, a town about 340 to 21, the base about 680 to 14. Homes give a gun for every 150 or so zombies around them, the base one for every 36: about four times better for the risk, which is what "worth the risk" means here. The sandbox's population setting scales all of it (50 / 100 / 150%). Unchanged.
  - **Your playtest notes:** none yet; the list is open for them.
  - **A crash on the way:** the census's first expected-value sums used 64-bit division, whose library routine links into IWRAM: 3.2 KB more there left the stack no room and the bench crashed at boot. The sums are 32-bit now (the bench's IWRAM is back to 24.9 KB, the game's 24.9 KB).
  - **Timings** unchanged: bench 1 584, bench 7 534 (7 missed), bench 3 169, bench 12 370 (passes), bench 13 206; bench 8's round trip matches with the new loot.

- **Stage 8 (23 Nov):** benches and docs.
  - **Bench 14** (`knox_towns.gba`): after the census, a tour: every 40 frames it teleports to the next stop (the first lot of each of the 33 templates, then the first coach, barricade and checkpoint), where the chunks are generated and drawn, and logs a `KXTOUR` line: doors, containers, beds, the set piece's tiles. All 36 stops loaded; every home has its doors, every building its containers (the church its desk), the barracks 12 bunks, the coach 8 tiles, the barricade 12, the checkpoint 9. Shops, the warehouse, the supermarket and the like have open double entrances without doors, as the shop has since milestone 5. The music director picked the calm tune at 40 s (`KXMUSIC`). Its timing counts the teleports (696 missed frames, all on them).
  - **Bench 8** fills the item pool before its save: stashes opened in a far corner until 470 containers hold items; with the route's own, the save carries 512 containers holding items (the pool's limit), 778 records and 1,816 stacks, and the load matches (holding, records, stacks and the checksum of every stack). The one-off fill and the larger save show in its timing (a 220-frame frame at the fill; 885 missed frames, against 475).
  - **Bench 1** had crept to 584 with stage 6; switching the new per-frame work off one piece at a time showed the music director (counting the zombies chasing every frame) and the rest small. The director now looks every 4 frames: 578, 8 thousandths over the start of the milestone (the music playing and the zombies' feet being mixed).
  - README (status, bench 14, a Content section, the layout, the tileset limit) and the spec (the 256×256 maps, the item count and the census targets, the build order) updated.
  - **Timings** (thousandths of a frame, missed frames): bench 1 578 / 950 (0), bench 2 218, bench 3 167, bench 4 241, bench 5 197, bench 7 529 (7), bench 9 207, bench 10 522, bench 11 228, bench 12 367 (245, its teleports), bench 13 205, bench 14 653 (its teleports). Every bench passes its script.

## Done when: where it stands

1. **Maps:** done. The world draws on the 32×32 maps with 255 graphics; bench 7 529 (521 before the change, the rest from the new content), the night bench 522; the render check found no stale cells.
2. **450 containers in the save:** done. Bench 8 saves and loads 512 containers holding items.
3. **33 templates and 4 set pieces:** done. All appear on the fixed seed; bench 14's tour finds their doors and containers, and its census their loot.
4. **About 200 items, all findable:** done. 200; every one is in a loot table except what is cooked, made, grown, chopped or butchered (checked with comments stripped).
5. **The five loops play where they should, on gpSP and mGBA, without frame drops:** in mGBA the director's choices are right on every bench and nothing drops; not heard, and not tried on gpSP.
6. **The loot census meets the targets:** done, all five.
7. **Survives a save and load:** done for what bench 8 carries (the full pool, the world, the survivor); the new items, set pieces and music need nothing saved beyond that.
8. **Bench 1 within 1% of now, bench 14 inside the budget apart from scripted moments:** bench 1 578 against 570 (0.8% of a frame); bench 14's only misses are its teleports.

Not done: device testing on the Miyoo (gpSP and mGBA), listening to the music, and your playtest notes.

## What this milestone covers

The spec's build order item 14: the world filled out. Room for more graphics first, then 25 new buildings, the set pieces and wilderness finds, about 70 new items, music and a sound pass, and a balancing pass. Milestone 13 left two things this milestone also takes on: the save's item storage (it fills after about 168 searched containers) and the light benches running 3–5% slower than at the end of 12.7.

## Choices you made (23 Nov 2026)

- **Room for graphics:** the world maps shrink from 64×64 to 32×32 cells, freeing 18 KB of background VRAM, and the tileset doubles to 256 graphics, all loaded at once (no swapping between town and wild).
- **Music:** original loops composed by a Python script at build time, like every other asset; replaceable by hand-made files later.
- **Translation string table:** later, in a milestone of its own, once the text has settled.
- **Stretch goals:** none in 14 (upstairs and basements, raider camps, fishing, snow footprints wait for their own milestone).

## Where things stand (23 Nov 2026)

| | Now | After 14 |
|---|---|---|
| Items | 130 | about 200 |
| Building templates | 8 | 33 (plus 4 set pieces) |
| World graphics | 128 of 128 (one 16 KB charblock) | about 200 of 256 (32 KB) |
| Background VRAM | about 61 of 64 KB: tileset 16, three 64×64 maps 24, HUD canvas about 21 | about 59: tileset 32, three 32×32 maps 6, canvas 21 |
| Searched containers the save holds | about 168 (one 12-slot page each) | about 500 (only the stacks they hold) |
| Music | none | 5 loops |

## Design

### Room to grow (stage 2)

- **Renderer:** each of the three world layers (ground, walls, canopy) becomes a 32×32-cell (256×256 px) wraparound map. The screen is 240×160, so a 256-px wide map leaves one spare 8-px column. That is enough because the renderer already writes only the rows and columns scrolling in. The night roof cut-away, the roof fade, fog, seasons and the title backdrop keep working the same way.
- **Tileset:** 256 graphics (1,024 hardware tiles, two charblocks; Butano allows 1,024 tiles per regular background). The generator's limit goes from 128 to 256.
- **Graphics put back:** the second plain grass and the two weathered roofs (dropped in 13), and blood, corpse and item decals on sand, soil and bridges (dropped in 12.7).
- **Item storage:** a searched container keeps only the stacks it holds, in one pool, instead of a fixed 12-slot page. The pool has the same 8.4 KB, which fits about three times as many searched containers (most hold 1–3 stacks once looted). The pause menu's WORLD % and the save bench follow the pool.
- **Speed:** the light benches' 3–5% (unexplained in 13) is looked at with the profiler on the new renderer, since rendering is where the untimed time goes.

### Buildings (stage 3)

25 new templates, each fitting a 14×14 lot like the 8 there are, with tagged containers, doors and windows:

| Where | New templates |
|---|---|
| Homes (villages and towns) | cottage, bungalow, terraced pair, flat block (ground floor), trailer, farmhouse, large farmhouse, holiday cabin |
| Village landmarks | pub, post office, small school |
| Town landmarks | school, motel, supermarket, warehouse, fire station, pharmacy, hardware store, diner |
| Military base | barracks, armory, mess hall |
| Farms and the wild | farm shed, stable, hunter's cabin (in forest cells, rare) |

- New furniture graphics for them, about 30: pews, an altar, school desks, a blackboard, bunk beds, racks, a gun rack, tables, a bar counter, filing cabinets, hay bales, a stove, a bath, and so on. Wall, roof and floor colours come from the palette variants as now.
- New loot tables for the new rooms: school, pub, pharmacy, hardware, warehouse, fire station, barracks, armory, mess, stable.
- The zone weights in `kx_world_gen_tuning.h` take the new templates; homes keep their locks and keys, and TVs and freezers swap in as in 13.

### Set pieces and the wild (stage 4)

- **One hand-made set piece per settlement tier** (spec), placed at a fixed spot of each settlement from the seed:
  - **village:** a church (a building template with a graveyard yard)
  - **town:** a crashed bus across the main road (a 2×4-tile wreck, searchable, with bodies)
  - **big town:** a police barricade at an entrance road (cars, road blocks, a police locker, zombies in uniform)
  - **military base:** a checkpoint on its road (sandbags, a barrier, a watchtower, a military crate)
- **Wilderness scatter** gains hunter stands (a raised seat over a container, in forest) and campsites (a tent, a burnt-out campfire and a crate together), alongside the tents, bodies and crates there are.

### Items (stage 5)

About 70 new items, towards the spec's table (in brackets: now, then the target):

| Category | Added | Examples |
|---|---|---|
| Food and drink (32, 45) | +12 | canned soup, canned peaches, pasta, rice, cereal, cookies, juice, beer, onion, tomato, eggs, honey |
| Medical (7, 15) | +6 | sterile bandage, alcohol wipes, vitamins, sleeping pills, cold pills, first aid kit |
| Melee weapons (10, 20) | +8 | machete, hatchet, golf club, sledgehammer, pitchfork, cleaver, hockey stick, police baton |
| Firearms and ammo (8, 10) | +2 | revolver, .38 rounds |
| Tools and materials (about 20, 40) | +14 | pliers, wire, scrap metal, tarp, cloth, sheet metal, binoculars, sewing kit, lockpick, bucket, padlock, chain, zip ties, toolbox (a container you carry) |
| Clothing and bags (10, 35) | +20 | t-shirt, shirt, sweater, police jacket, army jacket, firefighter jacket, padded jacket, cargo pants, suit trousers, sneakers, wellies, army boots, work gloves, cap, helmet, bandana, scarf, duffel bag, satchel, large backpack |
| Car parts (5, 10) | +3 | radiator, brake pads, car jack |
| Farming and cooking (12, 15) | +3 | tomato and onion seeds, a kettle |

Clothing shows through palette swaps of the survivor's sprite (the spec's cut list), as it does now. Weapons use the existing weapon classes (a machete is a heavy blade, a sledgehammer a slow blunt weapon), so no new combat code beyond their numbers. The exact list may move by a few items as the loot tables are written.

### Music and sound (stage 6)

- **Music** on the GBA's old (DMG) sound channels through Butano's `bn::dmg_music`. A script, `tools/assets/music.py`, writes five short `.mod` tracker files at build time: original, moody and sparse (two pulse voices, the wave channel, a little noise), 30–60 s each.
  - **Title**
  - **Calm day:** plays now and then, with long silences between (a minute or more)
  - **Night:** lower and slower
  - **Horde chase:** when several zombies are chasing; fades back to silence afterwards
  - **Death:** under the death screen
- A small music director picks the loop from what is happening; sound effects stay on the direct-sound channels, so the two never compete.
- **Sound pass:** levels balanced across all the effects, a few missing ones added (zombie footsteps up close, glass underfoot, rain on a roof indoors, wind in the wild), and stereo checked.

### Balancing (stage 7)

- **A loot census bench** opens a few hundred containers per zone and table on a fixed seed and logs what came out (items, food days, ammunition, weight). The tables are tuned to written targets:
  - **food:** a house's kitchen feeds you about a day and a half
  - **guns:** a gun about every 25 houses, ammunition more often
  - **military bases:** worth the risk
- **The survival numbers** (hunger, thirst, fatigue, endurance) and zombie density against the sandbox settings are checked with the survival bench and a long generated-world run.
- **Your playtest notes from milestones 12.5–14** feed this stage: anything that feels off goes on its list.
- **The targets as written (stage 7):** a house's kitchen 40–66 food points (a day is about 33); loose guns 1 in 18 to 1 in 35 homes; ammunition at least 1 find in 12 homes; the military base 0.5–1.5 guns and at least 1.5 ammunition finds a building. Checked by `tools/census_report.py` on bench 14's log.

## Stages

A ROM to try after each.

1. **Mockup 14.** `tools/mockup_towns.py` draws `mockups/14-towns.png`:
   - a town street with new buildings (school, supermarket, motel, church)
   - the crashed bus and the police barricade
   - a hunter stand and a campsite
   - a sheet of the new furniture graphics
   - the survivor in a few of the new clothes

   While you look at it, stage 2 goes ahead.
2. **Room to grow.** The 32×32 maps, the 256-graphic tileset, the graphics put back, the item storage pool, the speed check. Save version 17.
3. **Buildings.** 25 templates, their furniture and loot tables, the zone weights.
4. **Set pieces and the wild.** Church, crashed bus, police barricade, checkpoint; hunter stands and campsites.
5. **Items.** About 70, into the loot tables.
6. **Music and sound.** The five loops, the music director, the sound pass.
7. **Balancing.** The census bench, the targets, the survival and density checks, your notes.
8. **Benches and docs.** Bench 14 (`knox_towns.gba`, `BENCH=14`) on the fixed seed:
   - a tour of every template and set piece, logging what each holds (`KXTOWN` lines)
   - the music director's choices

   Bench 8's round trip fills the new item pool past 168 containers. README, spec sections, timings.

## Done when

1. The world draws as before on the new maps (bench 7 and the night bench unchanged or faster), with 256 graphics available.
2. The save holds at least 450 searched containers that still hold items.
3. 33 templates and the 4 set pieces appear in the settlements of a generated world, each with working doors, containers and loot.
4. About 200 items exist and every one can be found somewhere.
5. The five loops play where they should, with silences, on gpSP and mGBA, without frame drops.
6. The loot census meets the written targets.
7. All of it survives a save and load.
8. Bench 1 stays within 1% of now; bench 14 inside the frame budget apart from scripted moments.

## Risks

- **The renderer rework** touches the most-used code in the game: roof fade, night cut-away, fog, seasons and the title backdrop all write into the maps. It goes first, on its own, and benches 1, 7 and 10 check it before anything is built on it.
- **VRAM allocation:** the 32 KB tileset must be one contiguous block on a charblock boundary. Butano's allocator is asked for it first, before the maps and the canvas.
- **Music on gpSP:** the DMG channels are emulated differently by gpSP and mGBA; the loops are kept simple and checked on both.
- **Save format** becomes version 17 (the item pool), so older saves stop loading.
- **Balancing is taste:** the census gives numbers, but whether the game feels right needs your playtests.
- **ROM size** stays far under the 32 MB budget (templates and music are small).

## My calls, not asked

- 33 templates (8 + 25) and the list above; the hunter's cabin in forest cells.
- Which set piece goes with which tier (church: village, bus: town, barricade: big town, checkpoint: military).
- The 70 items and their split by category; clothing gets the most (+20), as the spec's table is furthest behind there.
- The item storage pool (same bytes, about 3× the containers) rather than a bigger save block (the world block has 7 bytes to spare).
- Music as `.mod` files through Butano's GBT Player support; five loops; the director's rules.
- A loot census bench for balancing, and the targets above.
- Mockup first (mockup 14), as for 12.6, 12.7 and 13.
