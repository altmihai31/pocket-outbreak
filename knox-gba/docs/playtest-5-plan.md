# Playtest 5: the debug menu crash, grabs, the death screen, empty slots, held directions

Two reports from 2026-11-24/25, merged: a manual mGBA run ("Knox playtest report": 9 findings and a layout pass) and
automated mGBA runs (scripted and random input, a save power-cut test, a 25-item UI audit). The user chose the crash,
save and gameplay items (P1 and P2) for this round; the UI audit and smaller polish wait for a separate pass. Follows
`playtest-4-plan.md`. User choices are marked **(chosen)**.

Already fixed by playtest 4 before these reports reached the code: notifications over the tab bar (now above the
hotbar), B in a wreck's trunk (back to the car screen), the blank info box on an empty Gear slot.

## The notes

| # | Note | Cause | Plan |
|---|------|-------|------|
| 1 | Select + Start opens a tuning menu that crashes on its DEBUG page ("No more sprite tiles items available"); random input found it twice | The menu opened in every build. Butano allows 128 sprite tile allocations by default; a sprite text redraw holds the old and the new tiles for a frame, and the DEBUG page's redraw over the world needs about 150 | The menu only in tester (`DEBUG=1`) and bench builds; `BN_CFG_SPRITE_TILES_MAX_ITEMS=256` (a few KB of EWRAM), which also gives a busy screen in play headroom |
| 2 | A power cut during Save and Quit wipes both save headers | Nothing writes 0xFF, but a power-cut bench (BENCH=17: cuts every 61 bytes into a save, then loads) found two real losses. (a) A cut while the survivor's slot is written: the load chose the other, whole slot's header but kept the broken slot's block, read last (`read_newest` re-read only when both checked out): a broken survivor loaded. (b) A cut while the world is written: one copy, so the world's changes are lost (in 95% of cut points: the world is most of the bytes) | (a) The chosen slot is always read again. (b) **Two world copies when the block fits half the area (chosen: "fix if the test finds a problem")**, written in turn; the load takes the copy saved with the survivor's slot. Same save format, old saves load. `tools/power_cut_test.py` runs the bench in CI |
| 3 | After Continue the survivor and cars stay dark until a menu opens | SAVE AND QUIT is in the MENU tab, so the game quits with the sprites dimmed behind the menu (70% fade); the next game starts with that fade still set | Every game starts with the bg and sprite palettes' fade and grayscale at 0 |
| 4 | Losing a grab kills at full blood, the guide says a bite kills in 2 or 3 days, and "BLOCKED" can show just before | The grab timer running out was always death | **One grabber: a guaranteed bite (any clothing), then free; two or more: death (chosen)**. The guide says so |
| 5 | The death screen shows "0 DAYS 0 H" and "0.0 KM" for a short life | Days and hours, kilometres to one decimal | Under a day: "2 H 26 MIN" / "26 MIN"; under a kilometre: metres; "1 DAY" singular |
| 6 | An empty slot still offers A ACTIONS (Gear), an empty trunk A TAKE and R TAKE ALL | Fixed footers | Footers from the chosen slot and the container: A TAKE (DRINK for a tap) only on an item, R TAKE ALL only while something is left, SELECT SPLIT only on a stack; A STORE only on an item; B CLOSE always |
| 7 | An exterior door is hidden under the roof until its prompt shows | Playtest 1's pick: outer doors sit under the roof (front doors under a 3 px dip) | Not changed: it reverses an earlier choice, so it needs a mockup first (see Open) |
| 8 | Holding Down in the recipe list moves one row | Lists stepped on presses only | `menu_repeat`: in the Start menu's tabs (not the map, which has its own), searches and the car screen, a held direction repeats after 18 frames, every 5; lists wrap on a press but stop at their ends while held (`step_list`) |

## Checked

- **1:** a bench build that opened the menu and walked the DEBUG page logged 66 tile items in the world, about 150 at
  the redraw's peak (over 128: the crash), and with 256 every row redraws (83 items between frames). The release
  build no longer opens it (`KX_DEBUG || KX_BENCH`).
- **2:** the power-cut bench before the fix: 20 cuts per world size loaded a broken survivor, and 77 of 99 cuts lost
  a 4.7 KB world. After: no cut loses or breaks the survivor at any size; worlds up to 14,000 bytes come back whole
  from every cut; 20,000 and 28,656 bytes still lose the world on cuts during its write.
- Bench 8's save round trip passes; bench 1 runs its route (no missed frames); bench 4's restarts run through.
- Not checked in a real run: 3, 4, 5, 6 and 8 need button input mGBA from the command line can't give; they build
  and the benches pass.

## Open

- **The save, big worlds (2):** real worlds pass half the area early (bench 8: 8 KB new, 20 KB after looting 470
  containers, most of it container records), so two copies only cover the first hour or so. Beyond that a cut
  mid-write still loses the world's changes (never the survivor). Keeping both copies needs the world block under
  14 KB: smaller container records and horde counts packed into fewer bytes.
- **Doors (7):** mockup `mockups/23-side-doors.png` (`tools/mockup_side_doors.py`), side doors closed and open: A now
  (the step only), B a notch (the roof steps back 6 px, the door frame shows in the gap: wood shut, dark open), C the
  door's edge past the wall line (a slab shut, swung out along the step open), D both. Waiting on the pick.
- **P3, a later UI pass:** wreck details for parts-only wrecks, trait screen (opposite traits, unspent points, Down
  from Eagle-Eyed), the date after midnight, the new world footer strip, the 25-item UI audit and the cross-screen
  conventions, the washed-out daylight blend on the device.
