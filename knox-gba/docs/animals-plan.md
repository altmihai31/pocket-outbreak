# More animals: boar, farm animals, crows, wolves

The user asked for more animals (2026-11-24). Today the wild has deer and rabbits (`kx_animals`, milestone 13) and
bird flocks. User choices are marked **(chosen)**.

## What they asked for

- **Animals (chosen):** wild boar, farm animals (chickens and goats), crows, wolves.
- **Danger (chosen):** some can hurt you: boar charge when cornered or hurt, wolves attack at night. Everything else
  flees. Animals stay a minor threat next to zombies.

## The animals

| Animal | Where, when | Behaviour | What it gives |
|---|---|---|---|
| Wild boar | forest and woods, day and night, alone | grazes; flees like a deer from afar, but **charges** if you come within 2 tiles or wound it: a short run at you, a gore (a laceration, knockback), then it turns away; 3 long-gun or 4 pistol hits | butchered like a deer: 6 raw meat, a hide |
| Wolf | wild cells at night, packs of 2-3 | roams; notices you from 7 tiles (crouched 3); circles, then one at a time darts in to bite (a laceration, no infection) and backs off; the pack runs when one is killed or from a fire within 4 tiles; zombies and wolves ignore each other | a carcass: 2 raw meat, a hide |
| Chicken | farm lots, by day, 2-4 near the barn | pecks about; flaps away from you slowly (it can be caught: A when next to one picks it up) | 1 raw meat; eggs: on a farm lot with chickens, foraging finds an egg (a new food item) |
| Goat | farm lots, 1-2 | grazes; trots away when you get close | butchered: 3 raw meat, a hide; milk: A next to a goat with an empty bottle fills it (once a day per goat) |
| Crow | where zombie corpses lie, 2-4 together | hop and peck on and beside corpses; take off cawing when you come within 4 tiles (crouched 2): a noise, like a flock, which zombies can hear | nothing; atmosphere and a warning |

## Limits (my calls)

- **Sprites:** the GBA has 128; zombies (40), corpse sprites (16), particles, projectiles, the player and the HUD's
  few leave room for about 8 animals and 5 birds at once. Kept: at most 6 ground animals on screen (a wolf pack
  counts 3), birds or crows 5. Flocks and crows only land when fewer than 30 zombies are about, as now.
- **No new palettes:** the animal palette already holds every colour: boar in the deer's dark browns, wolves in the
  rabbit's greys and the birds' blue-grey, chickens and goats in white with the red, the yellow and tan, crows in the
  blacks.
- **Not saved, as today:** live animals come back from the seed. Saved, small: a farm's chickens and goats killed
  (so a farm empties), the day each goat was last milked. Bump the save format only if these don't fit the world
  block's spare room (checked then).
- **Sounds (6):** boar grunt, wolf howl (heard at night, far) and snarl, chicken cluck, goat bleat, crow caw. Made
  like the zombie sounds, in the user's export format; the user picks by ear.

## Stages

1. **Mockup:** the sprites (boar, wolf, chicken, goat, crow: each facing, moving and dead) and three scenes: a boar
   charging in the woods, a wolf pack at night round a campfire, a farm with chickens and goats, crows on corpses.
2. **Boar:** spawning, grazing, fleeing, the charge and the gore, butchering.
3. **Wolves:** night packs, circling and biting, running from fire and losses; the howl.
4. **Farm animals:** chickens and goats on farm lots, catching, eggs and milk (two new items), the farm's tally in the
   save.
5. **Crows:** landing by corpses, pecking, taking off with a caw.
6. **Sounds:** the six, picked by ear.
7. **Benches:** the census of a wild and a farm (counts, sprite budget), a scripted boar charge and wolf attack;
   frame times.

## Progress

**Stage 1 done 2026-11-24 (art agreed):** `mockups/19-animals.png` (`tools/mockup_animals.py`): the boar, wolf and goat standing, moving and dead, the chicken standing, pecking and flapping, all at 2x and at 1x; the crow perched, pecking, wings up and down (8x8); then crows on two corpses on a road, a boar charging in the woods (a dead one by a tree), a wolf pack circling just outside a campfire's light at night, and chickens and goats on a farm. All in the existing animal palette. Side views only so far: the front and back views (as the deer has) follow once the side views are agreed. **Redrawn on request (wolf, goat, chicken):** the wolf in warm grey-brown (the rabbit's greys) with a dark saddle, a pale muzzle and belly and a bushy tail hanging down (it read as a doberman); the goat with a short neck, a swept-back horn, a beard and a short upturned tail (it read as a llama); the chicken a brown hen with upright tail feathers, a red comb and wattle (white, it read as a duck, and white is the goat's). `mockups/19-animals-redraw.png`: first drafts left, redraws right, large and at 1x. **Recreated from the user's reference (wolf, hen, boar):** the reference is pixel art on a ~11 px grid, so its native pixels were sampled, shrunk with a majority-colour rule into the animal palette, outlined, then fixed by hand (ears, eyes, tail, thin separate legs, the boar's tusk and bristles). The grey wolf (24x16) and the boar (25x16) need 32x16 sprites; the cream hen with a red comb fits 16x16. `mockups/19-animals-ref.png` (`tools/ref_animals.py`): large and at 1x above, the reference below. Standing poses only; moving and dead frames follow in the same style once agreed. **Goat, deer and rabbit redrawn to match (user's pick):** the goat in greige with a dark spine, a white blaze, swept-back horns, a beard and white socks (no longer the hen's white), 22x16; the deer longer and leggier with big ears, a white throat and rump, 20x16 side frames (front and back views and the carcass redone too); the rabbit shrunk to about hen size (it stood as tall as the wolf). Everything stays 16 px tall, the height of a person. `mockups/19-animals-redraw2.png` (`tools/redraw_animals.py`): all frames small on top, the lineup large, at 2x and at 1x.

**Stage 1 finished (user: "happy, carry on"):** all the agreed art now lives in `tools/assets/animal_art.py`, not yet built into the ROM (stages 2-4 move each animal into `animals.bmp`, the deer and rabbit with the first of them). `FRAMES[animal][frame]`, every frame padded to its sprite: the wolf, boar, goat and deer 32x16 (side frames on a fixed offset so the body doesn't jump between frames), the hen and rabbit 16x16. Each animal has side stand and move (the wolf and boar run, the goat and deer step, the hen pecks and flaps), front and back views with a step (one leg lifted), and a dead frame in a blood pool (the deer's carcass). Most frames are colours only, outlined by `make`; the wolf, hen and boar standing frames are the reference recreations, and their run frames keep that body with new legs (`compose`). The hen got a dead frame (lying, feet out) for butchering. Mockup 19 now uses this art throughout and has a sixth panel: the front and back views of all five, and the redrawn deer and rabbit. Crows are unchanged (8x8). When the 32x16 animals are built: one sprite each still, twice the tiles of a 16x16 frame; the deer's sprite shape changes from 16x16, so its spawn and draw code change with it.

**Stage 2 done 2026-11-24 (the boar):**
- **Art in the ROM:** `animals.bmp` (16x16) now holds the new rabbit and the hen; the new `animals_wide.bmp` (32x16) holds the deer, boar, wolf and goat, all from `animal_art.py` (`gen_assets` builds both, `kx_animal_frames.h` names every frame). The deer and rabbit in the game now wear their redrawn art; the deer is a 32x16 sprite. Same animal palette, so no new palette.
- **Spawning:** one boar at most, in wilderness cells (35% of them a day, from the seed), only on forest ground, 10-14 tiles out, day and night. `max_animals` went from 3 to 4 (two deer, a rabbit, a boar).
- **Behaviour** (`kx_animals`, all numbers in `animal_tuning`): grazes and ambles like a deer; within 6 tiles (2 crouched) it walks off at 1.7 px a frame. Within 2 tiles, crouched or not, it winds up (24 frames, standing and shaking) and charges in a straight line at 2.7 px a frame (faster than a sprint, so you step aside) for up to 48 frames. Within 11 px of your feet it gores: a laceration on a leg (trousers meet it with half their scratch defence and wear, as with a grab), you're knocked down for 30 frames and thrown back about 20 px along the charge (a new fading push on the player, `_knock`, stopped by walls), off a bicycle too; then it trots away and won't charge again for 5 s. Missing, running out or hitting a wall ends the charge the same way. Wounded within 7 tiles it charges at once (a 12-frame wind-up); from further off it runs. Loud noises don't break a charge. The survivor in a car is left alone.
- **Hunting:** 16 health: 3 long-gun or 4 pistol hits, 3 spear or axe hits, 6 other melee; hit radius 9 px. Dead, it lies in its blood ("A BOAR DOWN"); hold A with a knife or an axe: 6 raw meat and a hide ("6 RAW MEAT, 1 HIDE"). Boar kills don't mark the cell (only deer do), so nothing new is saved: save format unchanged (v17).
- **Bench 15** (`BENCH=15`, `knox_animals.gba`, test map, `KXBOAR` lines): a boar 5 tiles off walks away (noticed at 80 px); one 24 px off winds up, charges and gores (left leg laceration, thrown 20 px); one shot from 6 tiles charges, the survivor steps 3 tiles aside and it runs out after 129 px; two more shots kill it; butchering gives 6 raw meat and 1 hide. 1389 frames, none missed. Seen in mGBA: the wind-up shake, the charge, the survivor knocked flat with blood and "GORED: L LEG", the boar trotting off.
- **Checks:** bench 13 (the wild) as before: the deer shot twice and butchered for 4 venison, the flock, both rabbits; its only missed frames are its scripted teleport and skipped nights. Bench 1: 552 / 942 per mille, no missed frames.
- **Not yet:** the boar's grunt (stage 6, sounds); untested on the Miyoo.

**Stage 3 done 2026-11-24 (wolves):**
- **Spawning:** at night only, a pack of 2-3 in wilderness cells (40% of them a night, from the seed; not on water or coast), 11-14 tiles out, only when 2+ animal slots are free. `max_animals` went from 4 to 6, the plan's cap on ground animals. They go when off screen by day, or while running away.
- **Behaviour** (`_update_pack` for the pack, `_think_wolf` for each; numbers in `animal_tuning`): they roam (amble and stop). One noticing you (7 tiles, 3 crouched) sets the pack on you. They circle on a ring 4 tiles round you, each at its own spot, the ring turning slowly. 2 s after noticing, then every 2.5-4 s, the nearest one darts in (2.6 px a frame, homing, up to 45 frames). A bite is a laceration on a random arm or leg, never infected: sleeves (jacket, top, gloves) or trousers may stop it with their full scratch defence, and wear. You're shoved a little, with no knock-down. Then it backs off and rejoins the ring. Only one dart at a time, none at someone in a car. A wolf hit but not killed backs off, and the pack takes up the hunt if it hadn't. A wolf killed ("A WOLF DOWN") sends the rest running for 10 s; they leave once off screen. The hunt ends if you get 16 tiles from every wolf, die, or the day comes. Loud noises scare roaming wolves, not hunting ones. Zombies and wolves ignore each other.
- **Fire:** a lit campfire or a burning patch within 4 tiles of you keeps the pack on a 6-tile ring and stops the darts; any wolf within 3 tiles of a fire backs off.
- **Hunting:** 8 health: 2 gun hits, 2 spear or axe hits, 3 other melee; hit radius 8 px. Butchered with a knife or axe: 2 raw meat and a hide ("2 RAW MEAT, 1 HIDE").
- **Shared changes:** the player's animal wounds go through one helper (`_animal_wound`: a laceration, with the covering clothes' chance to stop it), used by the boar's gore and the wolf's bite (`bitten_by_wolf`, a small shove through the same fading `_knock`). Blocked animals now slide along whatever is in the way before turning a quarter, and a blocked wolf keeps to its detour for 16 frames instead of re-aiming into the obstacle (a dart and a circling wolf had stuck on a tree on the test map).
- **Bench 15**, extended (`KXWOLF` lines, from frame 1400, at 22:00):
  - A pack 8 tiles off ignored the survivor.
  - Come upon 5 tiles away, it noticed at 82 px, circled at about 64-68 px and darted twice. The first bite was blocked by the jeans, the second was a laceration on the left arm.
  - Two rifle shots killed the nearest wolf; the other two ran and were gone within 45 frames.
  - By a campfire, a new pack was kept off (64-102 px, the 6-tile ring), with no darts in 450 frames.
  - A wolf killed with the axe was butchered for 2 raw meat and 1 hide.
  - 3099 frames, none missed.
  - Seen in mGBA: the pack circling in the dark, "BLOCKED: JEANS", "WOLF BITE: L ARM", "A WOLF DOWN", wolves pacing outside the firelight.
- **Checks:** bench 13 and bench 1 as at stage 2 (552 / 942, no missed frames). Save unchanged (v17).
- **Not yet:** the howl and snarl (stage 6); no eyeshine at night (the mockup's yellow eyes); untested on the Miyoo.

**Stage 4 done 2026-11-24 (farm animals):**
- **Where:** a farm's yard is the outer door of its barn, stable or farm shed (else its farmhouse), or just in front of it when it opens on a gap, as barns and sheds do (`world::farm_yard`, from the seed). Three quarters of farms keep 2-4 hens and 1-2 goats (from the seed). Within 18 tiles of a yard, its animals come out round the door, hens 3 tiles about and goats 5, on open ground, as many as there are free animal slots. Hens come out only by day and go when off screen at night; goats stay. They amble back towards the door beyond 5 tiles (hens) or 7 (goats).
- **Hens:** peck about. Within 2 tiles (1 crouched) they flap off slowly (1 px a frame, slower than a walk), so you can run one down. A beside a hen: "[A] CATCH" gives raw meat ("CAUGHT A HEN: RAW MEAT"). A hen killed drops its raw meat on the ground and lies there 10 s.
- **Goats:** graze. They trot off only from someone jogging or sprinting at them (faster than 1.25 px a frame), so you can walk up. (First tried: they fled at 2 tiles unless you crouched, which hid the milking; changed.) With an empty bottle, "[A] MILK" turns it into Milk, once a day for each goat. A goat killed (8 health, like a wolf) is butchered for 3 raw meat and a hide.
- **Eggs:** the Eggs item already existed (milestone 14, with a Fried eggs recipe). Foraging within 10 tiles of a yard whose farm has hens left adds eggs to the finds: 1, or 2 with 3+ hens. So only **Milk** is new: a drink (food 10, water 30, spoils after 2 days, the bottle left after), white bottle icon, appended after the kettle (saved item ids unchanged).
- **Saved:** each farm's hens and goats lost (killed or caught) and the day each goat was last milked, for the last 8 farms (the oldest makes way and is restocked), appended to the animals part of the world block with a marker. The save reader now says how many bytes are left (`remaining()`), and a version-17 save without the part loads with no losses. **No format bump** (still 17). To make room, the world block's allowance for its small blocks went from 2048 to 1600 bytes; they measure 1404 (safehouses 804, farm plots 320, traps 128, survivors 68, generators 64, events 20).
- **Bench 15**, extended (`KXFARM` lines, from frame 3110, the next morning, a farm set up on the test map: 3 hens, 2 goats):
  - All 5 came out.
  - A hen flapped off from a survivor 20 px away.
  - Beside a hen the context was catch: caught for raw meat, 1 hen lost.
  - Beside a goat it was milk: milked on day 2, not again that day, milked again on day 3.
  - A goat killed with an axe and butchered: 3 raw meat and a hide (2 goats lost would show; 1 here).
  - A hen killed: its meat on the ground (2 hens lost).
  - Foraging by the yard found an egg, 20 tiles off none.
  - 3797 frames, none missed.
  - Seen in mGBA: hens and goats round the yard, "[A] MILK", "A BOTTLE OF MILK", "CAUGHT A HEN: RAW MEAT", the goat's butchering, the egg among the foraging finds.
- **Bench 14** (towns tour on the fixed seed) now brings farms' animals out at each farm stop, `KXFARM` lines. Its first farm (20,4) has a barn that opens on a gap; its yard is in front of it, and 2 hens and 2 goats came out. A farm at 32,13 brought out 2 hens and a goat, and one at 20,17 brought out 3 hens, with no room left for its goats. The stable's farm was one without animals.
- **Checks:** bench 8's save round trip matches (a world block with every container full still fits). Bench 13 is as before. Bench 1 is 552 / 942, after making the new catch and milk prompts look for animals before asking whether zombies are near (asking first had cost 8 per mille).
- **Not yet:** the cluck and bleat (stage 6); untested on the Miyoo.

**Stage 5 done 2026-11-24 (crows):**
- **Art:** the crow's four 8x8 frames (perched, pecking, wings up, wings down) moved from the mockup into `animal_art.py` (`CROW`) and are built into `birds.bmp` after the songbirds' frames (`crow_perched` .. `crow_wings_down`). Mockup 19 reads them from there.
- **Where, when:** by day, when no flock is about (crows and songbirds share the flock's 5 slots and its rule: fewer than 30 zombies about), a spawn try may (40%) send 2-4 crows to the nearest zombie corpse 5-14 tiles from you. Corpses are the CORPSE containers, the same ones the corpse sprites draw. They glide down from above the screen over a second to spots round the corpse (12 px about).
- **Behaviour:** each crow now and then pecks or stops, turns, or hops a few px to another spot by the corpse. Zombies don't bother them. When you come within 4 tiles (2 crouched), or at a loud noise, they take off up and away like a flock. The take-off is a noise of 9 tiles (a songbird flock's is 6), so zombies come to look. Afterwards none land for 30 s. They give nothing.
- **Bench 15**, extended (`KXCROW` lines, from frame 3810):
  - Two corpses were laid on the field, with a zombie 6 tiles past them and the survivor 6 tiles off.
  - The corpse search found the nearer corpse from 6 tiles and nothing from 18 tiles or 3 (outside 5-14).
  - 3 crows came down in 60 frames and pecked about.
  - The survivor stepped to 3 tiles; they took off with a 9-tile noise.
  - The zombie walked from 49,26 to 43,26, to the crows' corpse, and the crows were gone 150 frames later.
  - 4197 frames, none missed.
  - Seen in mGBA: crows gliding in, standing on and beside the corpses, taking off as the survivor came close.
- **Checks:** bench 13's songbird flocks take off as before (they share the new per-bird drawing: frame and facing); bench 1 553 / 942, no missed frames. Save unchanged.
- **Not yet:** the caw (stage 6; for now the take-off is the flutter, louder); untested on the Miyoo.

**Stage 6 started 2026-11-24 (sounds):** two candidates of each of the six sounds, made by `tools/animal_sound_candidates.py` into `sound-candidates/animals/` (`<sound>_A.wav`, `_B.wav`; `all.wav` plays them all in order, A then B, at their playback volumes). The zombie sounds' toolkit plus a voice-and-mouth model: a band-limited buzz with pitch jitter through formant band-passes, noise, envelopes. Exported in the user's format (22050 Hz, unsigned 8-bit, 100 Hz / 7000 Hz, peak -3 dB). Playback volumes: grunt -8 dB, howl -10, snarl -6, cluck -12, bleat -10, caw -8.
- Boar grunt: A one gruff grunt (0.5 s); B two quick grunts and a lower third (0.9 s).
- Wolf howl: A a long howl rising, held and falling, big reverb (3.2 s); B a yip into a lower howl with a break (3.4 s).
- Wolf snarl: A a growl ending in a snap (1.0 s); B a rising rattling snarl (1.1 s).
- Hen cluck: A buk buk buk (0.6 s); B buk buk buk ba-GAWK (0.9 s).
- Goat bleat: A a grown goat's (0.9 s); B a kid's, higher and shakier (0.8 s).
- Crow caw: A caw... caw (0.9 s); B three rougher caws (1.0 s).
Once picked: built into `audio/` like the zombie sounds and played: the grunt as a boar notices you or winds up; the howl when a pack comes out at night (heard far); the snarl as a wolf darts in; clucks and bleats now and then near the farm; the caw when crows take off.

**Stage 6 done 2026-11-24 (sounds; the user picked all the A's):**
- **Built:** the synthesis moved to `tools/assets/animal_sounds.py` (its `SOUNDS` are the picks). `audio.build_audio` calls `build_animal_sounds`, which writes `audio/boar_grunt.wav`, `wolf_howl`, `wolf_snarl`, `hen_cluck`, `goat_bleat` and `crow_caw` (byte for byte the A candidates). `tools/animal_sound_candidates.py` still makes all twelve to listen to. New sound ids `BOAR_GRUNT` .. `CROW_CAW`; bench builds log each as `KXCALL`.
- **Played** (`_call`: heard within 200 px of the screen's middle, a howl within 640, scaled by the survivor's hearing; half volume from off screen; volumes from the playback dB above, in `animal_tuning`):
  - Grunt: when a boar notices you and walks off, when it winds up (from close or wounded), and from the nearest boar every 10-20 s.
  - Howl: when a pack comes out, and from a pack roaming at night every 25-45 s (not while hunting you).
  - Snarl: as a wolf darts in.
  - Cluck and bleat: the nearest hen every 5-11 s and goat every 8-16 s, and each as it runs off from you.
  - Caw: crows taking off (instead of the flutter), and softly from crows on the ground every 6-12 s.
- **Bench 15** (`KXCALL` lines): a grunt as the first boar noticed the survivor (off screen, half volume), grunts at both wind-ups; a howl as each pack came out (the first off screen) and one roaming howl; a snarl with each of the two darts; clucks and a bleat at the farm, some as hens ran off; a caw as the crows took off. 4197 frames, none missed.
- **Checks:** bench 13 as before (no animal calls: it has no boars, wolves, farms or crows); bench 1 553 / 943, no missed frames. ROM 1.8 MB. Save unchanged (v17).
- **Not yet:** heard in the game by the user; untested on the Miyoo.

**Stage 7 done 2026-11-24 (benches); the animals plan is built (all 7 stages):**
- **Bench 16** (`BENCH=16`, `knox_census.gba`, the fixed seed, `KXCENSUS` lines): the nearest wild cell of woods or forest (18,35) and the nearest farm with a yard (18,36). Each was played 1200 frames by day (10:00) and by night (22:00) with the game's own spawning and the cells' own zombies. Then two worst cases of 600 frames: the farm by day with its animals, crows on eight corpses and a horde of 40; the wild cell at night with a wolf pack, a boar, two deer and a horde of 40. Per phase it logs: the most of each animal at once, birds, zombies, the sprites, sprite tiles and sprite palette colours in use, and the frame's average and worst (the first 60 frames after each move left out).

  | phase | most at once | sprites | frame avg / max (per mille) |
  |---|---|---|---|
  | wild by day | 2 deer, a rabbit, a boar; a flock of 4 | 14 / 128 | 236 / 440 |
  | wild by night | 2 deer, a boar (no pack came that night) | 5 | 219 / 445 |
  | farm by day | 4 hens, a goat, a rabbit (the 6 slots full); a flock of 3 | 11 | 227 / 421 |
  | farm by night | 2 goats, 2 wolves (from the wild next door) | 6 | 221 / 429 |
  | worst: farm by day | 4 hens, 2 goats, 3 crows, 40 zombies | 57 | 525 / 846 |
  | worst: wild at night | 3 wolves, a boar, 2 deer, 40 zombies | 48 | 550 / 827 |

  Sprite tiles peaked at 74 and palette colours at 144 (the zombie outfits'; the animals add none). Every missed frame (65 in 6145) falls on the script's own moves between phases (the world regenerating), none on the animals.
- **A new bench section, `animals`** (their update; their drawing stays in `visuals`). It showed spikes on the spawn ticks, up to 1229 ticks (28% of a frame), first at the farm. Fixed:
  1. A spawn tick looks up at most one new farm cell's yard (about 400 ticks each, the survivor's own cell first) and remembers farm cells without one.
  2. The tick that finds a yard does nothing else.
  3. A farm's hens and goats come out one a frame, topped up to what the farm has left (a new sprite costs up to about 250 ticks; six at once had cost about 670).
  4. The farm placement no longer asks `under_roof` (indoor floor isn't open ground anyway).

  Worst now 657 ticks (15%), once per farm per session; average 73. On the test map (bench 1) the animals cost 10 ticks a frame. (A songbird flock landing still costs about 476 ticks on its frame, unchanged since milestone 13.)
- **Re-checked after the fixes:** bench 15 (all its phases as before), bench 14 (the tour's farms bring out their hens and goats: 2/2, 2/1, 3 hens with no room for goats; one farm without animals), bench 13 as before, bench 1 554 / 944 with no missed frames. Save unchanged (v17).
- **Not yet, for the whole plan:** played on the Miyoo (gpSP and mGBA cores); the sounds heard in the game.
