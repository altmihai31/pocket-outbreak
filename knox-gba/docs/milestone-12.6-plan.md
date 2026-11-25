# Milestone 12.6: ranged combat

10 Nov 2026 · status: built 19 Nov 2026; mockup 11 approved on 11 Nov 2026. All 6 stages built and checked in mGBA, not on the Miyoo. Save format version 12.

## What this milestone covers

The audit before 12.5 found the spec's ranged combat unbuilt (spec: Combat > Guns, Throwables; Character > Aiming, Reloading, Spear). This milestone builds:

- four guns and their ammo, with aiming and active reload;
- the spear;
- throwables, Molotovs, fire and burns;
- the secondary hand.

## Choices you made (10 Nov 2026)

- **Slots:** a secondary hand, no belt. A flashlight or a throwable can be held in it next to a one-handed weapon; two-handed weapons empty it. The hotbar keeps doing the belt's job, so the spec's belt goes to the cut list.
- **Fire:** a Molotov sets a 3×3 patch burning for about 15 s where it lands. It doesn't spread.
- **Controls:** hold R to aim, A fires, B reloads while aiming. During the reload bar, A at the sweet spot is a fast reload; missing it jams the gun, and B clears the jam.
- **Mockup first:** a PNG mockup of aiming and the reload bar, approved before those two are built.

## Design

### Weapons

| Weapon | Hands | Loaded | Ammo | Range | Damage | Noise (tiles) | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Pistol | one | 15 | 9 mm rounds | 8 tiles | 3 | 30 | common police loot; quick to reload |
| Shotgun | two | 6 | shells | 5 tiles | 2 × 5 pellets in a 30° spread | 40 | knocks back everything hit; biggest hitstop and shake |
| Hunting rifle | two | 5 | rifle rounds | 14 tiles | 9 (kills any tier) | 45 | slow bolt: 40 frames between shots |
| Assault rifle | two | 30 | 5.56 rounds | 10 tiles | 3 | 45 | military only, ammo very rare; holding A fires every 6 frames |
| Spear | two | – | – | 28 px reach | medium | 3 | spec frame data 9/5/18, one target, narrow arc; the Spear skill trains it |

- **Health and hits:** zombies have 2, 5 or 9 health by their hidden tier, so a rifle kills anything, a pistol needs one to three shots.
- **Shots are instant:** the first zombie along the line is hit. A tracer and a muzzle flash show for 2 frames, so no bullet sprites fly about.
- **Without aiming,** A swings the gun as a club. A pistol hits like fists; long guns like a weak bat.
- **Guns don't wear;** jams come from failed reloads.
- **The loaded count and a jam** are kept in the gun item's extra field, which the spec planned for "ammo loaded", so no new save block is needed for them.
- **Skills:**
  - **Aiming:** gains XP on hits, and each level narrows the cone and the sway.
  - **Reloading:** gains XP on reloads, and each level widens the sweet spot and shortens the bar.
  - **Spear:** gains XP on spear hits.
  - The Police officer already starts with Aiming 3 and Reloading 2.

### Aiming

- **Hold R** on foot (not in a car, where R1 is the headlights).
- **Movement:** half speed. Facing locks to the aim, in 8 directions from the D-pad.
- **Soft auto-aim:** the nearest zombie inside a cone around the aim becomes the target, shown with a marker.
- **The cone:**
  - starts at 20° each side;
  - narrows to 8° after half a second standing still;
  - is 2° tighter per Aiming level;
  - is wider while panicked (spec: Panic gives aim sway).
- **Where shots go:** each shot lands at a random angle within the cone, so a narrow cone means hits.
- **B while aiming reloads,** so there's no shove while aiming. Sprinting needs R let go.

### Active reload

- **B starts the reload:** a bar fills over 48 frames, with a sweet spot of 10 frames somewhere along it.
- **A inside the sweet spot** finishes the reload at once. A outside it jams the gun. No press: the reload finishes when the bar does.
- **A jam:** firing clicks and does nothing; B clears it in 30 frames.
- **Reloading skill:** each level shortens the bar by 4 frames and widens the sweet spot by 2.
- **Rounds:** a reload fills the gun from the matching ammo in the bag, up to its capacity. A shotgun's shells go in as one reload too, to keep it simple.

### Secondary hand

- **New equipment slot:** the OFF HAND, shown in the Gear tab's worn panel. It holds one item: a flashlight or a throwable.
- **Two-handed weapons empty it:** taking up a shotgun, either rifle, the bat, the axe or the spear puts its item back in the bag.
- **The flashlight now has to be in the off hand to shine** (a change from 12.5, where it shone from the bag). Select on the flashlight in the hotbar puts it in the off hand and switches it on; Select again switches it off. Its beam and battery work as before.
- **What you hold shows:** the item in the off hand is drawn in the player's other hand, as the main-hand item is.

### Throwables and fire

| Item | Where from | On landing |
| --- | --- | --- |
| Empty bottle | kitchens, bars, camps; left over when you finish a water bottle or soda | smashes: noise 12 (spec: breaking glass) |
| Alarm clock | bedrooms | rings for 20 s: noise 10 every 2 s, so zombies gather there |
| Molotov | crafted in the Craft tab: empty bottle + rag + 1 l of fuel from a fuel can | smashes and sets a 3×3 patch burning for 15 s |

- **Throwing:** a throwable in the off hand is thrown with R (aim) and A when the main hand holds no gun. It flies in an arc up to 6 tiles, stopping at walls.
- **Fire:** flame sprites on the 3×3 patch.
  - At night the patch gives light, like a campfire.
  - A zombie in the fire takes 1 damage every half second and keeps burning for 3 s after it walks out.
  - Stepping in gives you a Burn wound on a leg (the Burn wound type already exists).
- **The spear** is crafted in the Craft tab from 2 sticks and a kitchen knife, with no heat needed. It is sometimes found in camps.
- **The Craft tab** today only holds cooking recipes. It gains recipes that need no heat.

### Loot

- Police lockers: pistols, 9 mm, now and then a shotgun and shells.
- Police zombies: pistols and 9 mm.
- Farms and barns: hunting rifles, rifle rounds, shells.
- Bedrooms and garages: rarely a shotgun or shells.
- Crash sites and soldier zombies: assault rifles and 5.56 rounds, rare.
- Alarm clocks in bedrooms, empty bottles in kitchens.

### HUD and sound

- **Aiming:** the target marker, and the loaded rounds and the rounds left in the bag next to the equipped slot ("12 / 30"). The reload bar shows while reloading. The mockup decides the exact look.
- **New sounds** (synthesised like the others): pistol, shotgun and rifle shots, the empty click, reload, jam, the throw, the smash, fire, the alarm clock.

## Stages

A ROM to try after each.

1. **Mockup (drawn).** `tools/mockup_guns.py` draws `mockups/11-aiming.png`: aiming at a zombie with the cone, the marker, the ammo count and the reload bar. While you look at it, stage 2 goes ahead.
2. **Secondary hand (built).** The slot, the Gear tab, two-handed weapons, the held picture, and the flashlight moved into it. Save version 12.
3. **Guns (built).** Items, ammo, loot, held pictures, aiming, firing, damage and knockback, noise, sounds, tracers and muzzle flash, the ammo count on the HUD, Aiming XP.
4. **Active reload (built).** The bar, the sweet spot, jams, Reloading XP, built to the approved mockup.
5. **Spear, throwables, fire (built).** Crafting without heat, the spear's melee class, throwing, the alarm clock, Molotovs, fire patches, burns, fire light at night.
6. **Benches and docs (built).** Bench 11 (`knox_guns.gba`, `BENCH=11`) on the test map is a shooting range: each gun at zombies, reloads hit and missed, a jam, throws and a fire, logging `KXGUN` lines. Bench 8 adds a gun with rounds loaded and an item in the off hand to its round trip. README, spec sections, timings.

## Stage 2 notes

- **Two-handed** means the bat, crowbar, pipe, shovel and axe. Knives, the hammer and the frying pan are one-handed.
- **It works both ways:** taking up a two-handed weapon puts the off hand's item in the bag, and taking something in the off hand puts a two-handed weapon in the bag. Either needs a free bag slot, or it is refused ("NO ROOM").
- **Flashlight:** Select (hotbar) or HOLD (Gear tab) takes it in the off hand, switched on. Select again switches it off and on. REMOVE in the Gear tab puts it away. In the bag it is always off.
- **Gear tab layout:** the worn panel was a full 2×4 grid, so it is now 3×3 (hand, off hand, bag on the top row) and the hotbar panel is one slot wide, titled BAR.
  - A palette-bank clash drew an orange line beside the new hotbar panel; the panel now starts on an 8 px cell edge.
- **The held picture:** a flashlight look was added to the held-item sprites (32 angles). The off hand's item is a second sprite at the other hand.
- **Checked:** bench 10 logs the hand swaps (`KXHAND` lines), bench 8 round-trips a lit flashlight in the off hand, HUD dumps of the Gear tab, and mGBA screenshots of the held flashlight in four facings.

## Stage 3 notes

- **The aim follows your facing.** The D-pad still moves you (at half speed) and turns you; the gun then swings to the nearest zombie within a 50° arc in front. I dropped "facing locks to the aim" from the design: the spec only says the aim snaps to 8 directions with soft auto-aim.
- **Reloading is plain for now:** B while aiming refills the gun after its reload time (pistol 48 frames, shotgun and hunting rifle 60, assault rifle 54). Stage 4 adds the bar, the sweet spot and jams.
- **Shots:** a random angle inside the cone (plus 15° each side for the shotgun's pellets); the first zombie within 7 px of the line is hit, if no wall is between. Pellets can hit the same zombie more than once.
- **What you see:** two dotted cone edges and yellow corner brackets while aiming; for 3 frames after a shot, a 64 px streak from the muzzle (three for the shotgun) and a muzzle flash. These are sprites turned by the hardware: up to 5 rotation slots.
- **Clubs:** the pistol's melee class is fists and the long guns' is the bat, with the bat's full numbers (the design said "a weak bat").
- **Tuning:** the four guns' numbers are a table in `kx_guns.h`; the cone, auto-aim arc, hit radius and aim speed are on a new Guns debug page.
- **Bench 11 came early** (the plan had it in stage 6): the range was needed to test this stage. One run: pistol 19 shots for 3 kills (half of them fired after the zombies were dead); shotgun 4 shells, 11 pellet hits; hunting rifle 3 kills in 4 aimed shots; assault rifle 5 kills in 12 rounds. It averages 21% of a frame, worst 90%.
- **A bug found on the way:** the game copies input fields one by one when it merges frames skipped by hitstop, so the new "R held" never reached the player until it was added there.

## Stage 4 notes

- **The bar sits over your head** while aiming (mockup 11): a 44 px box with the track, what has filled, the green sweet spot and a white mark. It only shows while you hold R1; letting go drops the reload (and a jam being cleared), as before.
- **Timing:** the bar is the gun's reload time (pistol 48 frames, shotgun and hunting rifle 60, assault rifle 54), 4 frames shorter per Reloading level (never under 20). The sweet spot is 10 frames, 2 more per level (at most half the bar), and starts at a random point between 35% and 70% of the bar.
- **A in the sweet spot** loads the gun at once with a chime. **A outside it** jams the gun: the reload stops, the rounds stay as they were, a red "JAMMED B CLEAR" box shows over your head, the ammo count turns red, and A only clicks. **B** clears it in half a second ("CLEARING"), then B reloads again. No press: the reload finishes when the bar ends.
- **The jam is kept on the gun** (bit 15 of its rounds field), so a jammed gun stays jammed in the bag and in the save; no save version bump was needed.
- **Reloading XP:** 3 for a reload, 8 for a fast one.
- **I dropped the "JAMMED" toast** I first had: the box over the head says the same thing where you are looking.
- **Tuning:** the bar and sweet spot numbers and the clearing time are on the Guns debug page.
- **Bench 11** now ends with a reload test (a pistol with 5 rounds): A 3 frames into the bar jams it, A on the jammed gun clicks (no shot), B clears it in 30 frames, then the script presses A at the sweet spot: "fast at 22 (sweet 21-31) loaded=15". HUD dumps of those frames showed the three boxes.
- **Timings (per-mille of a frame, average / worst):** bench 11 207 / 792, no missed frames; bench 1 573 / 940 (569 before), none missed; bench 2 200 / 416; bench 7 497, its usual two frames over; bench 10 495, only its 4 teleports and the siege pull-out over; bench 8's misses are its save, load and new-world frames, and its round trip still checks out. IWRAM unchanged at 27.9 KB.

## Stage 5 notes

- **Throwing:** with a throwable in the off hand and no gun in hand, R1 aims a throw like a gun but with no cone: one dotted line, and a small yellow box where it will land (the nearest zombie in front within 6 tiles, else 6 tiles ahead), short of the first wall. A throws; the flight takes 3 frames per tile in an arc up to 20 px high, with a shadow on the ground. The next one of the same kind comes up from the bag. A gun in hand always wins: you can't throw while holding one.
- **Landing:** a bottle smashes (the glass sound, noise 12). An alarm clock rings 20 s (noise 10 every 2 s, louder the nearer you are), shaking, then becomes a floor pile you can search. A Molotov smashes and starts the fire. Up to 2 in the air, 2 clocks and 2 fires at once; a third replaces the oldest.
- **Fire:** 9 flame sprites (a new 16x16 flickering sheet, not shaded at night), 15 s. Every half second the zombies with their feet in the patch catch fire for 3 s; burning zombies lose 1 health each half second and flash, but keep walking (no stagger). You get a Burn on a random leg on stepping in and every second you stay. At night the patch's light and glow are the campfire's (it takes one of the 2 fire glows, before campfires), and it counts as a light near you.
- **Time skips put fires out:** sleeping, cooking and reading skip minutes in one go, so fires end, clocks stop (left on the ground) and anything in the air lands. Fires and ringing clocks are not saved: they last seconds.
- **The spear:** its own weapon class with a debug page (9/5/18 frames, 3 damage, 1 target, 28 px reach, 40 degree arc, knockback 1.5, noise 3), two-handed, 40 hits of condition, trains Spear. Its swing is the same sweep as the other weapons, not a thrust.
- **The Craft tab** gained a "no heat" kind of recipe: no heat line, no Cooking XP, the clock still runs for it (spear 20 minutes, Molotov 5). "Fuel (litres)" counts what the fuel cans carried hold, and a litre is poured from them. The spear's kitchen knife is used up, from the hand if it is the one held.
- **Empty bottles** come from kitchens, camps and shop tills, and from finishing a water bottle or a soda. Boiling water still makes a bottle from nothing, so bottles are never short.
- **Gear tab:** a throwable's line says "THROWN".
- **Sprites:** at most 4 in the air (2 items and their shadows), 2 clocks and 18 flames.
- **IWRAM:** the spear's tuning table pushed IWRAM over the 28 KB guide, so all 13 tuning tables moved to EWRAM (1.8 KB). IWRAM is now 26.3 KB.
- **Bench 11** goes on at 23:00 after the reload test: an alarm clock, a bottle and a Molotov thrown east, 4 zombies burned to death by the fire, the survivor stood in it for 1.5 s (2 Burns), the spear against two zombies, then the Craft tab makes a Molotov (fuel 10 to 9 litres) and a spear (the sticks and a knife used).
- **Timings (per-mille of a frame, average / worst):** bench 11 212 / 799, no missed frames; bench 1 574 / 941 (573 before), none missed; bench 2 201 / 417; bench 3 158 / 963; bench 7 499, its usual two frames over; bench 10 496, only its teleports and the siege pull-out over; bench 8's misses are its save, load and new-world frames. Moving the tuning tables to EWRAM cost nothing measurable.

## Stage 6 notes

- **Bench 8's round trip** now also saves a jammed pistol with 7 rounds in hand, 12 rounds of 9mm, a spear and 2 Molotovs next to the flashlight (on, 123 minutes) in the off hand; the loaded game logs the same line ("hand=Pistol loaded=7 jammed=true carried 9mm=12 spears=1 molotovs=2"). Save version stays 12: the new items are new ids, the gun's state is in the item's extra field.
- **Bench 11's reload test** now runs as a Police officer (Reloading 2, Aiming 3): the pistol's bar is 40 frames and the sweet spot 14 ("jammed at 4 of 40 (sweet 14-28)", "fast at 15 (sweet 14-28)"), against 48 and 10 at level 0 before, so Reloading is seen changing them.
- **The spec** (`docs/design-spec.md`) now has the guns' full table, aiming, active reload, throwables and fire as built, the spear's numbers, R's controls, the fire's light at night, the hotbar without the belt, and 12.6 marked built in the build order.
- **Final timings (per-mille of a frame, average / worst):** bench 1 574 / 941 (557 before 12.5, 569 after it, 573 before this milestone's stage 5): within 1%. Bench 11 212 / 799, no missed frames. Bench 2 201 / 417. Bench 7 499, its usual two frames over. Bench 10 496, only its teleports and the siege pull-out over. Bench 8's misses are its save, load and new-world frames.
- **Memory:** IWRAM .iwram 22.5 KB + .bss 3.1 KB + .data 0.1 KB = 26.3 KB (27.6 KB before the milestone). ROM 937 KB.

## Done when (checked)

1. Each gun aims, fires, hits, kills and is heard as in the table, with its own sound and kick. **Yes** (bench 11's four phases; sounds and shake checked in mGBA in stage 3).
2. The reload bar matches the approved mockup, the sweet spot and jams work, and Reloading changes them. **Yes** (HUD dumps in stage 4; bench 11 at Reloading 2).
3. The off hand holds a flashlight or a throwable, and two-handed weapons empty it. **Yes** (bench 10's KXHAND checks, bench 11's throws, bench 8's round trip).
4. Bottles, alarm clocks and Molotovs draw zombies or burn them as described; a Burn wound comes from stepping in fire. **Yes** (bench 11: 4 zombies burned to death, 2 Burns in 1.5 s, 8 rings; the fire and its night light seen in mGBA).
5. The spear is crafted and fights with its frame data. **Yes** (bench 11: made in the Craft tab, 2 hits with it).
6. Bench 1's day timings stay within 1% of now; bench 11 stays inside the frame budget apart from scripted moments. **Yes** (574 against 569; bench 11 worst 80%, nothing missed).

## Risks

- **Sprites:**
  - Up to 21 more: the target marker, 5 shotgun tracers, the muzzle flash, a thrown item, 9 flames, a fire glow, the off-hand item.
  - The worst case is 40 zombies, rain, fire and lights at once, which would be close to the 128 limit.
  - Fallback: rain drops drop from 20 to 12 while a fire burns.
- **Rotation slots:** tracers are turned by the hardware, 5 of the 32 slots. The lights use up to 3.
- **IWRAM** is at 27.6 KB of 28. All new code stays in ROM, and new tables go in EWRAM.
- **Controls:**
  - R already switches menu tabs and turns the headlights on in a car. Aiming is only on foot outside menus.
  - B while aiming reloads, so you can't sprint and aim. Players have to learn this.
- **Save format** becomes version 12, so older saves stop loading.
- **The flashlight change** from 12.5 (it must be held) changes how night plays: with a two-handed weapon you're in the dark.

## My calls, not asked

These fill gaps the spec leaves open. Any of them can change.

- The numbers in the weapons table (capacities, ranges, damage, rates, the cone angles).
- Shots are instant with a tracer, not flying bullets.
- Guns don't wear out.
- Shotgun shells reload all at once.
- Without aiming, a gun is a weak club.
- The spear is crafted from 2 sticks and a kitchen knife; the knife is used up.
- Molotovs take 1 l from a fuel can.
- Finishing a water bottle or soda leaves an empty bottle.
- The alarm clock rings for 20 s.
- The fire patch is 3×3 tiles for 15 s and lights the night.
