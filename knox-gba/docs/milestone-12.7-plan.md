# Milestone 12.7: doors and hooks

19 Nov 2026 · status: built 20 Nov 2026 (all 6 stages); mockup 12 approved on 19 Nov 2026. Checked in mGBA, not on the Miyoo. Save format version 13.

## What this milestone covers

The audit before 12.5 (`docs/milestone-12.5-plan.md`) found these spec systems unbuilt (spec: Zombies > Behaviour, Exploration > Hooks, Looting > house keys, Base building > Log wall and Wooden door, Character > Burglar):

- doors that open, close, lock and break, with zombies banging on them;
- house keys, the crowbar and lockpicking;
- notes naming places, and gun safes opened with codes;
- the wooden door and log wall pieces, and chopping trees for logs.

Today doorways are open gaps in the walls, house keys are loot with no use, and milestone 9 left the two pieces out until logs, hinges and doorknobs existed.

## Choices you made (19 Nov 2026)

- **Locks:** about a third of houses have their outer doors locked. Three ways in: the matching key (quiet), a crowbar (2 s, loud, breaks the door), or picking the lock with a screwdriver (quick and quiet for the Burglar, slower for anyone else, and it can fail with a noise).
- **Keys:** each key belongs to one locked house in its town. Picking it up marks the house on the Map tab (with a map carried), and its item line names the town.
- **Logs:** chop trees with the axe. The tree becomes a stump for good and gives 2 logs.
- **Safes:** gun safes need a 3-digit code entered on a dial screen; a note in the same town has it. Other notes name a stash.

## Design

### Doors

- **Every doorway in a building template gets a door.** Front and back doors are outer doors; inner doorways get inner doors. New worlds generate them closed; the test map's doors too.
- **A opens or closes the door you face** (prompt "[A] OPEN", "[A] CLOSE"). Opening takes 10 frames with a creak, closing 10 with a thud (noise 2). An open door is drawn swung against the wall and lets anything through.
- **A closed door blocks** movement, sight (zombies can't see through it), shots and throws, like a wall.
- **Zombies bang on closed doors** in their way, as they do on windows and barricades. The flow field routes through doors at a cost, so a closed door slows a chase. A door takes 12 zombie hits (a window takes 3), each one loud (noise 6). After the 12th it is broken: open for good, drawn with splinters. Hits are kept in the existing dent list (8 doors or windows at once), so a door you leave and come back to is whole again unless it broke.
- **Barricades still go on doorways.** A barricade on a door's doorway replaces the door until it breaks, then the door (broken or not) is back.
- **What is saved:** only doors you changed, in the world's wall-change list (the one smashed windows and base pieces use: 512 entries, shared with decals, decals go first). An open, unlocked or broken door is a change; a door closed again and unlocked keeps its "unlocked" mark.

### Locks and keys

- **Which houses are locked:** from the seed, about a third of houses (small and large houses, garages), both outer doors. Shops, petrol stations, barns, the police station and the clinic are open. Inner doors never lock.
- **A locked door** shows "[A] LOCKED" and rattles. What you can do:
  - **Key:** with its key carried, A opens it at once (the door stays unlocked).
  - **Crowbar:** with a crowbar in hand, "[A] FORCE": hold A for 2 s, noise 12, and the door breaks (open for good). The crowbar wears 3.
  - **Lockpick:** otherwise, with a screwdriver carried, "[A] PICK": hold A. The Burglar takes 2 s with no noise and always succeeds. Anyone else takes 5 s, and each try has a 40% chance to fail with a clatter (noise 4), 8% less per Sneaking level.
  - What you hold decides: the key always wins, then a crowbar in hand, then a screwdriver in the bag. Letting go of A stops forcing or picking.
  - **Or a window:** smashing one still works.
- **Keys:** a house key's extra field holds its house (its building lot). Keys are found where they already are in the loot tables (bedrooms, desks, counters, zombies), and each one is rolled for a locked house in the same town as where it was found. Picking it up names the house in its item line ("KEY: HOUSE IN ASHFORD") and, with a map carried, adds an automatic mark on the Map tab.
- **Automatic marks:** up to 16 places found from keys and notes (a key icon, a note icon), kept apart from your own 8 marks so they never push those out. They are saved with the map (save version 13). A mark goes when its house is opened or its stash is emptied.

### Notes, gun safes and stashes

- **Notes** are a new item: its extra field says which note it is. A note in the bag can be READ in the Gear tab: a box with its text (mockup 12), e.g. "SAFE AT 12 MILL ROAD. CODE 4-1-7." Reading one adds its place to the automatic marks. Notes are found in desks, bedrooms, counters and zombie corpses.
- **What notes say:**
  - **Safe codes:** the code and the building of a gun safe in the same town.
  - **Stashes:** "LEFT SUPPLIES IN THE WOODS EAST OF ASHFORD": a crate in the wild near a settlement (one per settlement, placed from the seed), with weapons, ammo, food and medicine. It only appears once its note is read, so there is no reason to comb the woods.
  - **Flavour:** a few handwritten notes that name a place and say what happened there, from a short table.
- **Gun safes:** a new furniture graphic, in every police station and in about 1 house in 5. A ("[A] OPEN SAFE") shows the dial screen: Left/Right pick a digit, Up/Down turn it, A tries the code (a click, or a clunk when it opens), B leaves. The code is worked out from the seed and the building, so it is never stored; opening it is a container's opened bit, as for any container. Inside: police safes hold pistols, shotguns and ammo; house safes a hunting rifle or pistol, ammo, and sometimes a key or a note.
- **No forcing safes:** only the code opens them.

### Wooden door, log wall and chopping

- **Chopping trees:** facing a tree with an axe in hand, hold A ("[A] CHOP"): 4 s, noise 8, the axe wears 2, and the tree falls (its canopy goes too). The trunk becomes a stump for good (a wall change) and 2 logs land in the bag or at your feet. It trains Axe a little.
- **Logs:** a new item, 5 kg each, stack of 4.
- **Wooden door piece:** 4 planks, a hinge and a doorknob, Carpentry 3, a hammer. It goes in an empty doorway (one without a door, or with a broken door) of a safehouse. It works like a house door but is never locked against you; zombies bang on it, with 20 hits to break it. It counts as one of the safehouse's 64 pieces.
- **Log wall piece:** 4 logs and a rope or 6 nails, Carpentry 2, a hammer. One tile, outdoors within 6 tiles of a safehouse. Zombies treat it like a barricade (health 150). One of the 64 pieces.
- **Hinges and doorknobs:** new materials found in garages, tool shelves and shop shelves.
- **Build mode** gains the two pieces in its list (mockup 06's bar).

### Tiles

The world tileset is at its 128-graphic limit (one 16 KB charblock). This milestone needs 10 new graphics: the door closed, open and broken in a front wall and in a side wall (6), the log wall's face and top (2), the gun safe and the stump. To make room, decals (blood, corpses, dropped items) on sand, soil and bridges go (9 graphics: decals there are not drawn), and concrete's decals are drawn with the pavement's (3). That leaves 2 free.

### HUD and sound

- **Prompts:** OPEN, CLOSE, LOCKED, FORCE, PICK, CHOP, OPEN SAFE (all on A), READ (Gear tab).
- **Progress:** forcing, picking and chopping show the existing action bar over the head (the reload bar's box).
- **New sounds:** a door creak, a door thud, a lock rattle, a lock pick's clicks, the crowbar's crack, wood chopping, a tree falling, the safe dial's click and clunk.

## Stages

A ROM to try after each.

1. **Mockup (drawn).** `tools/mockup_doors.py` draws `mockups/12-doors.png`: a house front with a closed, an open and a broken door, the LOCKED and FORCE prompts with the progress bar, the safe dial screen and a note box. While you look at it, stage 2 goes ahead.
2. **Doors (built).** Freeing the tiles, door graphics, doors in the templates and the test map, opening and closing, blocking, zombies banging and breaking, the flow field cost, saving door changes. Save version 13.
3. **Locks and keys (built).** Locked houses from the seed, keys tied to houses, the crowbar, lockpicking, automatic marks on the Map tab.
4. **Notes, safes and stashes (built).** The note item and its box, the safe graphic and dial screen, codes, stashes, their loot.
5. **Chopping, wooden door and log wall (built).** Logs, stumps, hinges and doorknobs, the two pieces in build mode.
6. **Benches and docs (built).** Bench 12 (`knox_doors.gba`, `BENCH=12`) on a generated world: doors opened and closed, a zombie breaking a door, a locked house opened with its key, a crowbar and a lockpick, a note read and its safe opened on the dial, a stash, a tree chopped, a wooden door and a log wall built, logging `KXDOOR` lines. Bench 8 adds door changes, a stump, automatic marks and an opened safe to its round trip. README, spec sections, timings.

## Stage 1 notes

- **`mockups/12-doors.png`** has six panels: a closed front door with OPEN; forcing it with a crowbar (the action bar, a zombie coming); inside with the roof cut away (the front door open, the bathroom door closed with a zombie banging on it, a broken door, a side door open, the gun safe with its prompt); a note read; the safe dial; the Map tab with a key's mark and a note's.
- **It is drawn from the game's own graphics:** the small house template, the world tiles, the player and zombie sprites, the crowbar from the held-item sheet and the 5 px font. The new door, log wall, stump and safe graphics are real: they are functions in `tools/assets/world.py` that stage 2 adds to the tileset.
- **Door frames take the wall colour** (the white siding variant in the mockup), so they will join the painted wall graphics; the door leaves are in the unpainted wood.
- **The dial shows the code from a read note** under the digits ("FROM A NOTE: 4-1-7"), so you don't have to remember it.
- **Automatic marks are cream** (a key, a sheet of paper) to tell them from your yellow marks.

## Stage 2 notes

- **Tiles:** the world tileset went from 128 graphics to 122 with the 6 doors in: sand, soil and bridge decals are gone (9) and concrete shares the pavement's (3). Crops used soil's decals and the vertical bridge the horizontal one's, so those lost theirs too. 6 are left for the log wall, stump and safe.
- **Templates:** a door goes in every doorway that is one tile wide (walls left and right, or above and below). Garages, shops, barns and the petrol station have wide openings and none. Front doors stay out from under the roof, as the doorways were. (Stage 3 tightened this; see its notes.)
- **The test map's 5 doors start open** (the back door, the inner doorway, the double front door, the shed), so the other benches' routes through them are unchanged.
- **Opening and closing is instant** (the plan said 10 frames), with a creak or a thud; a door won't close while the survivor's hitbox or a zombie's body overlaps its tile.
- **A door works even with zombies near** (it has its own context, ahead of searching), but not mid-swing.
- **No flow-field cost:** the flow field is a plain breadth-first fill, so a closed door counts like a floor; zombies route through and bang on it (as with windows). A chase still slows down by the 12 hits it takes.
- **Every door touched is remembered**, even when it goes back to how it was generated: stage 3's "unlocked" needs that, and the wall-change list has room (512).
- **Barricades on doors:** two more "covered" kinds (a doorway with a door, front or side wall). When zombies break the boards, the door behind them is broken.
- **Save version** stays 12 here: nothing in the save changed shape (door states are wall changes). Stage 3's automatic marks bump it to 13.
- **Bench 12** (`knox_doors.gba`, test map): the back door closed, opened and closed again with A (solid and opaque when closed), then a zombie in the kitchen called by a noise bangs on it 12 times and comes out through the broken door. 17% of a frame on average, no missed frames.
- **Timings:** bench 1 576 / 943 (574 before), bench 11 213, bench 2 203, bench 3 158, bench 7 501 (its usual two frames over), bench 10 493, bench 8 unchanged. Bench 9 misses 4 frames on its scripted moments (the car set-up at frame 70 and the bicycle's crash); I have no earlier run of it from this session to compare.

## Stage 3 notes

- **Door placement fixed:** stage 2's rule also put doors on corridor tiles between two walls running across (three in a row along the large house's back corridor). Now a doorway needs at least one jamb that is a plain stretch of its wall. Doors per building: small house 4, large house 4, police station 5, clinic 3.
- **The large house's front opening was two tiles wide,** so it had no front door and could not be locked; it is now one tile, like the small house's. So every house has exactly one outer door (the plan said "both outer doors": there is only the front).
- **Which houses are locked:** small and large houses, a third of them, rolled from the seed and the lot (never stored). A door is locked while it is closed, is a house's outer door, and nobody has changed it (a door once opened is remembered and stays unlocked; broken is broken).
- **House ids:** the lot's macro cell (12 bits) and the lot within it (4 bits), in the key's extra field. A key found in a town gets a locked house of that town other than the one it was found in (0 if the town has none: "KEY: NO DOOR KNOWN"). Looking houses up plans the lot from the seed, so it works for chunks that aren't generated.
- **From inside, A unlocks** (the latch turns): the plan didn't say, but being locked in by your own front door would be silly.
- **The prompts are what A will do:** UNLOCK (key or inside), FORCE (crowbar in hand), PICK (screwdriver in the bag), LOCKED. The key wins, then the crowbar, then the screwdriver.
- **Forcing** is loud all along (noise 6 every half second), 12 when the frame splits; the crowbar wears 3 and can break.
- **The bar** shares the reload bar's box over the head (a plain fill); the prompt hides while it runs.
- **Automatic marks:** a list of 16 in the map's save block (84 bytes; save version 13). Taking a key from a search adds its house's mark (it toasts when you carry a map); unlocking, picking or forcing the house open removes it. A key to a house already opened adds nothing. Marks are drawn on the Map tab in cream (a key; the note icon is ready for stage 4).
- **New sounds:** a rattle, a pick's ticks, a crowbar's crack.
- **Bench 12 moved to the generated world** (the test map has no locked houses). It collects houses from every settlement and runs 8 phases of 400 frames: a door opened; a locked door rattling; its key (the mark added, then removed); a crowbar (broken, the crowbar worn); a screwdriver (this run: the pick slipped); the Burglar (picked, 0% chance to slip); unlocking from inside; a door shut on a zombie. At the end a key adds a mark and the Map tab opens. Teleports cost frames (71 missed), as in bench 10.
- **Timings:** bench 1 577 / 945 (576 before), bench 11 214, bench 2 204, bench 3 160, bench 7 502, bench 10 497, bench 8 and 9 unchanged. IWRAM 26.4 KB (+0.1 KB: the marks list).

## Stage 4 notes

- **Safes:** every police station has one (a locker in its template became the safe); in houses, the furniture swap that already turned some wardrobes into desks or crates now turns 20% into a safe instead of a crate. The same roll is repeated outside world generation, so a note can find a town's safes without the chunks being generated. The seed's world has 27 safes (3 in police stations).
- **One loot table for all safes** (pistols, shotguns, a hunting rifle, ammunition, now and then a key or a letter): the plan had police and house safes apart; a safe doesn't know which building it is in when it is rolled, and one table was enough.
- **Codes** are 3 digits from the seed and the building, never stored; a safe opened is its container's opened bit. The dial knows a code you have read while that note's mark is on the map (the mark goes once the safe is opened).
- **Notes are three items all called "Note"** (a safe's code, a stash, a letter) with the same paper icon: the rolled kind fills its extra field (the safe's building, the stash's town, or the letter and the town it names). When a note is rolled, 45% try to be a safe's code in that town, 30% its stash, the rest letters (and a note with no safe or stash to tell of becomes a letter). Six letters for now.
- **Reading** is instant: the text in the radio's 3-line message box for 6 s, and the Gear tab closes so it can be read.
- **Stashes** have a graphic of their own (a green bag on a tarp; the tileset is at 124 of 128) and a loot table; reading the note puts the bag on a dry tile past the town's clearing, off the corner the seed picks (the corner the note names), as a wall change, so it is saved and comes back like any other. It is searched like furniture.
- **Map marks** are now removed by kind as well as id, so opening a house doesn't take away its safe's note mark.
- **Save version stays 13:** the new items and loot tables are appended, nothing changed shape.
- **Bench 12** adds two phases: a safe's note read (the mark added), the dial opened, a wrong code (a click), the right one, the safe searched (2 items, a gun among them); then a stash's note read, the bag put out north-east of the town and searched (5 items); and a letter.
- **Timings:** bench 1 578 / 946, bench 11 215, bench 2 205, bench 3 160, bench 7 503, bench 10 500, bench 8 and 9 unchanged. IWRAM unchanged at 26.4 KB.

## Stage 5 notes

- **Chopping** uses the same held-A action as forcing and picking locks (now "work": unlock, force, pick, chop), with its bar over the head. It is offered only with no zombie close, so A still swings when one is (a tree in front shouldn't stop you fighting).
- **The canopy:** a tile's canopy is the lower half of its own tree and the upper half of the tree below it, so felling a tree rewrites two canopy tiles from the generator's own table. The canopy layer is now remembered like the walls (the same change list), so the gap stays after the chunk is generated again. The stump doesn't block movement.
- **Logs** are a material (5 kg, stacks of 4, burn as fuel); **hinges and doorknobs** are found on tool shelves.
- **The log wall takes nails only:** the spec says "a rope or nails", but a piece's materials are a fixed list; 6 nails it is.
- **The log wall's direction:** logs run north when a log wall is above or below; one there already that runs across on its own is turned to match. A lone one runs across.
- **Breaking:** wooden doors and log walls keep their health in the piece record like barricades (200 and 150, 10 a hit), so zombies' hits and the horde passes while you are away go for them too. A broken wooden door is left as a broken door; a broken log wall is gone.
- **Tiles:** 127 of 128 (the two log walls and the stump went in).
- **Bench 12** adds two phases: a tree chopped (an oak: its canopy cleared from both tiles, 2 logs, the axe 70 to 68), then, as a Carpenter, a wooden door hung in the doorway the crowbar broke (health 200) and a log wall built three tiles outside (150, solid and breakable); 200 frames later 20 hits break the door and 15 the wall. Breaking them in one scripted frame costs that frame dearly (a scripted moment).
- **Timings:** bench 1 579 / 947, bench 11 216, bench 2 206, bench 3 161, bench 7 505, bench 10 501, bench 8 and 9 unchanged. IWRAM unchanged at 26.4 KB.

## Stage 6 notes

- **Bench 12's zombie phase works:** two bench bugs kept it from running before. The script read a door's orientation and which side was outside from chunks not generated yet (right after a teleport), and the zombie was spawned in such a chunk, where it went straight back into its horde. The door's side now comes from the template (`house_door` returns it), outside from the seed (`under_roof`), and the zombie comes 30 frames later: A shuts the door in its face and it breaks it in 12 hits.
- **Bench 8's round trip** now also saves a door opened and one broken (the start settlement's, found from the seed), a chopped tree's stump and canopy, a key's and a note's automatic marks, and an opened gun safe (the first one in any settlement); the loaded game logs the same, read from the remembered changes (`remembered_gfx`) so it doesn't depend on which chunks are generated. Its setup frame plans every settlement's buildings and costs a long frame beside the save's own.
- **The spec** has the doors, locks, keys, notes, safes, stashes and automatic marks under Exploration > Hooks, the built numbers for the wooden door and log wall, and 12.7 marked built.
- **Final timings (per-mille of a frame, average / worst):** bench 1 579 / 947 (574 at the end of 12.6: +0.9%); bench 12 355, its missed frames all at teleports, the Map tab opening after the whole map is revealed (as in bench 10) and its scripted bash loops; bench 11 216, bench 2 206, bench 3 161, bench 7 505, bench 10 501, bench 9 197.
- **Memory:** IWRAM .iwram 22.5 KB + .bss 3.2 KB + .data 0.1 KB = 26.4 KB. ROM 1028 KB. World tileset 127 of 128 graphics.

## Done when (checked)

1. Doors open, close, block and break as described, and zombies bang on them. **Yes** (bench 12 phases 0 and 7; screenshots in stage 2).
2. Locked houses open with their key, a crowbar or a lockpick, each with its own cost; the Burglar picks quietly. **Yes** (bench 12 phases 1 to 6).
3. A key marks its house; a note marks its safe or stash; the dial opens a safe with the right code only. **Yes** (bench 12; the dial and the note box seen in HUD dumps).
4. A tree chops into a stump and 2 logs; the wooden door and log wall build and hold zombies off. **Yes** (bench 12; 20 and 15 hits; chopping and the pieces seen in mGBA).
5. All of it survives a save and load. **Yes** (bench 8: doors, the stump and canopy, marks, the opened safe; base pieces as before).
6. Bench 1's day timings stay within 1% of now; bench 12 stays inside the frame budget apart from scripted moments. **Yes** (579 against 574; bench 12's misses are teleports, the Map tab and its bash loops).

## Risks

- **Tiles:** losing decals on sand, soil and bridges means blood and corpses there aren't drawn (the corpse can still be searched). If that looks wrong in play, the gun safe could go back to being a plain cabinet graphic, freeing one.
- **The wall-change list** (512 entries) now also holds door changes and stumps. A player who opens every door in a town and chops a forest could fill it; ground decals are dropped first, then the oldest door changes (a door forgotten this way comes back closed and, if its house was locked, unlocked).
- **Flow field:** doors are now on almost every path into a building. Their cost must keep chases moving; a zombie that can't reach you should still bang on the door between you.
- **Zombies inside houses** that are spawned behind locked doors stay there until something breaks a door: a closed house is a surprise box. That is intended.
- **IWRAM** is at 26.3 KB. All new code stays in ROM; door lookups in the zombie and flow loops use the existing tile flags.
- **Save format** becomes version 13, so older saves stop loading.

## My calls, not asked

- The numbers: a third of houses locked, door hits 12 (wooden door 20), crowbar 2 s / noise 12, lockpicking 2 s and 5 s with 40% failure, chopping 4 s / noise 8 / 2 logs, log wall health 150.
- Which tiles are freed (decals on sand, soil, bridges; concrete shares pavement's).
- Inner doors never lock; shops and public buildings are never locked.
- Stashes only exist once their note is read.
- Safes can't be forced.
- A separate list of 16 automatic marks next to your 8.
- Mockup first (mockup 12), as for 12.6.
