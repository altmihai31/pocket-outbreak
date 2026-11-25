# Playtest 4: world info, the empty slot, notifications, the car's containers, the new world screen

The fourth batch of playtest notes (`../ideas`, "PART4", 2026-11-25), what causes each one, and the plan. User choices
are marked **(chosen)**. Follows `playtest-3-plan.md`.

## The notes

| # | Note | Cause | Plan |
|---|------|-------|------|
| 1 | Move the world info from the Map view to the Menu | The Map tab draws the world's sandbox settings (speed, horde, loot, power, day length) in a column at its left, narrowing the map. | MENU gets a third entry, **WORLD (chosen)**: the seed and the five sandbox settings, each with what it means (the new world screen's own lines). The Map tab's view takes the whole width. |
| 2 | The blank area under the inventory when an empty slot is selected | The Gear tab's info box names a worn slot ("HEAD: EMPTY") but draws nothing at all for an empty bag or hotbar slot. | An empty bag slot shows "EMPTY SLOT" and a **bag summary (chosen)**: free slots and the weight carried of the limit ("9 OF 24 SLOTS FREE ` 3.8 / 16.0 KG"); an empty hotbar slot says which slot and how to fill it (an item's HOTBAR action). |
| 3 | Notifications in the inventory aren't readable; put them all in one place, maybe the lower half | In the world a notification shows near the top (y 20); over a menu it is drawn on the tab bar (y 5) in the same dark box as the bar, so it barely shows. One at a time: a new one replaces the last. | One place everywhere, **just above the hotbar (chosen)**, centred: a filled box with a coloured border (its kind: normal, accent, danger) over the world and over every menu. **Up to 3 stack (chosen)**, the newest at the bottom, each on its own timer. The radio's message box moves to sit above them. Mockup first. |
| 4 | From the car screen, TRUNK or GLOVE BOX then B should go back to the car | The car screen closes itself and opens an ordinary search; closing the search goes back to the world. | The search remembers it came from the car screen (the car, the action chosen); B, or Start, reopens the car screen on the same car with TRUNK or GLOVE BOX still chosen. |
| 5 | The new world screen's last two lines: align them, and centre the bottom line | The line under the panel is centred without a save but, with one, "SELECT MENU" sits at the left and the warning at the right; the footer hints always start at the left margin. | Both lines centred: "SELECT MENU" and, with a save, the red warning after it on the same line (180 px of 240); the footer hints centred (183 px with B BACK). The title's other screens' footers are centred too, so the bottom line is centred on every title screen. |

## Decisions (mine, not asked)

- **WORLD page:** the MENU layout: the five settings as rows on the left (read-only, no arrows), the chosen one's values and what it means on the right, the seed at the top. On the title, MENU shows WORLD only with a save (a new world's settings are on the new world screen).
- **The bag summary** counts the bag's slots only (worn and hotbar are separate), the weight as the Gear tab's header shows it, red when over the limit.
- **Notifications:** at most 3 at once; a fourth pushes out the oldest. The same text arriving while it still shows restarts its timer instead of stacking twice. Over a menu they redraw what they covered when they go (as now, `screen_toast`). The in-world prompt ("[A] SEARCH") keeps its place over its target.
- **No save format change.**

## Stages

1. **Notifications:** mockup (world, Gear tab, search screen, 3 stacked); the stack, the place, the box; the radio box above it; checked with bench 3's dumps.
2. **MENU's WORLD and the Map:** the WORLD entry (tab and title), the Map tab without its margin.
3. **The empty slot:** the bag summary and the hotbar line.
4. **The car's containers:** back to the car screen from TRUNK and GLOVE BOX.
5. **The new world screen:** the two lines centred, title footers centred; checked with the title dump (with and without a save).

## Progress

**Stage 1 built 2026-11-25** (untested on device):

- **Mockup** `mockups/22-notifications.png` (`tools/mockup_notifications.py`, drawn over a street screenshot and bench 3's Gear tab and guide dumps).
- **The stack** (`kx_hud`): up to 3 notifications (`toast_line`, oldest first), each with its own timer; a fourth pushes out the oldest; the same text and kind again while it shows only restarts its timer. Drawn centred, the newest box's bottom at y 116 and older ones above, 2 px apart, each as wide as its text; a normal one now has a light grey border (it was the panels' dark grey), good news yellow, danger red. One HUD element (`TOAST`) covers the stack in the world; over a menu `screen_toast` draws the same stack in the same place, and when it changes it clears where the old one was to the backdrop (the Gear tab doesn't repaint the gaps between its panels) and the screen redraws. The radio's message box keeps its place at y 34, above the stack.
- **Checked** (`docs/playtest-4-notifications.png`): a world dump ("DRANK WATER BOTTLE" above the hotbar); bench 3 now raises three at once in the Gear tab (9101: stacked), the shortest gone (9103: the other two moved down, nothing left behind), all gone (9102). Bench 1: 556 per mille, no missed frames.

**Stage 2 built 2026-11-25** (untested on device):

- **WORLD** (`menu_screen::draw_world`, `handle_world`): MENU's third entry ("THIS WORLD'S SEED AND ITS SANDBOX SETTINGS."). Left, the seed (hex, as the new world screen shows it) and the five settings as rows, read-only, Up/Down choosing one, "SET WHEN THE WORLD BEGAN" under them; right, the chosen one's values with this world's marked, then what that value means (the new world screen's line, wrapped to the panel).
- **The title:** MENU from the main menu (with a save) has WORLD too, showing the saved world (`title_world`, from the save's summary); MENU from the new world screen keeps two entries, since there it would show the world being replaced.
- **The Map tab** (`kx_map_screen`) lost its WORLD column: the map runs from x 16 to 224 (it began at 48), 17 cells of 12 px and a 4 px sliver of an 18th, both edges on 8 px cell edges; the view keeps the player's cell in the middle (`view_centre_x`, cell 8: x 112-124).
- **Checked** (`docs/playtest-4-world.png`): bench 3's tab tour dumps the entries with WORLD chosen (9209) and the WORLD page on SHUTOFF (9208); bench 10's Map tab with a map and a compass (frame 3140). Not dumped: the title's WORLD (mGBA from the command line gives the game no save to show).

**Stage 3 built 2026-11-25** (untested on device):

- **The empty slot** (`gear_screen`): an empty bag slot's info box says "EMPTY SLOT" and "16 OF 24 SLOTS FREE ` 3.8 / 16.0 KG" (the weight red when over the limit). The hotbar panel can't be chosen, so it needs no line of its own.
- **Save and quit moved into MENU** (asked for with this stage): a fourth MENU entry, SAVE AND QUIT, held rather than opened: A held on it for a second (`save::quit_hold_frames`, as the Select hold was) fills a bar under the row and the game saves and quits to the title; letting go empties it. It has no arrow (it opens no page) and its footer reads "HOLD A SAVE AND QUIT". The "SELECT SAVE+QUIT" hint is gone from every tab's footer (`hud::quit_hint` removed), and so is the Select hold. The save lists' warning ("WORLD 92% FULL", from 80%, red from 90%) moved from beside that hint to the entry's panel. The title's MENU doesn't have it. The guide's footer has room for B BACK again; the guide's CONTROLS and SAVING pages say where save and quit is now.
- **Checked:** bench 3's tour dumps SAVE AND QUIT chosen (9210) and the Gear tab on an empty bag slot (9211), `docs/playtest-4-slot-quit.png`; bench 8 still saves, quits and loads the same world (it quits through its own script). Not checked: holding A on SAVE AND QUIT in a real run (the input can't be driven safely while mGBA may not be the window in front).

**Stage 4 built 2026-11-25** (untested on device):

- **Back to the car** (`game::search_car_container`, `_search_car`, `_update_search`): the car screen's TRUNK and GLOVE BOX search through the game, which remembers the car; when that search is closed with B or Start, the car screen opens again on the same car, on the same row (the car screen keeps its row). A grab closing the search, or any other search, forgets the car.
- **Checked** on bench 9: after stripping the wreck it opens the wreck's trunk the car screen's way, presses B, and logs "searched=true then B: search=0 car screen=1". Bench 9 runs as before (3 missed frames on its scripted moments, as since milestone 13).

**Stage 5 built 2026-11-25** (untested on device):

- **One helper** (`kx_title.cpp`: `footer_hint`, `footer`): hints drawn along a line, centred on the screen (the button in the text colour, the action quiet, 4 px between them, 10 between hints). Every title footer uses it: the main menu, MENU, SETTINGS, GUIDE, WORLD and the new world screen.
- **The new world screen:** the line under the panel is "SELECT MENU", and with a save the red "REPLACES THE SAVED WORLD" after it, the whole line centred (it sat at the line's two ends); the footer "A START UP/DOWN PICK <> CHANGE", with "B BACK" when there is a save, centred.
- **Checked** (`docs/playtest-4-title.png`): the title dump without a save (the new world screen, MENU, the guide), and a build that drew the new world screen as if replacing a save (taken out again; mGBA from the command line gives the game no save).

Playtest 4 is built; nothing in it is tested on the device yet.
