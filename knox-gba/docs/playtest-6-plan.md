# Playtest 6: the UI pass and saves for big worlds

The two items left open by `playtest-5-plan.md`: the P3 UI polish (the 25-item UI audit, the trait screen, wreck
details, the date after midnight, the New World footer, the daylight blend, the layout notes) and keeping big worlds
safe from a power cut while they save. The UI changes were shown as before and after screenshots first and approved
with two changes **(chosen)**: no dark strip under the New World panel, and no dark space round the left panels'
slots (Worn, the container being searched).

## The UI pass

| Screen | Change |
|--------|--------|
| All | Footers in one order: A, then START, B, the D-pad last (D-PAD CHOOSE where a list moves). A disabled action shows greyed (`hud::hint::enabled`). Counts read `x / y` |
| HUD | Drawn in the vertical blank (`hud::flush` after `bn::core::update`): the one-frame blank boxes came from writing VRAM mid-screen. The hotbar's white frame only on the item in hand; digits on a dark backing; the prompt moves above the survivor's head when they would overlap. Weather changes get a toast (FOG ROLLS IN, THE RAIN STOPS...): the pale daytime screens were fog |
| New Survivor | Ticks for chosen traits, scroll marks on the heading, REPLACES <trait> when the opposite is taken, Up and Down wrap within a column (Down from Eagle-Eyed), Start asks once with points left or over a saved world |
| Title, New World | SEED RANDOM with < > when picked; hints on one row; with a save, STARTING REPLACES THE SAVED WORLD in red above them. Straight on the darkened map like the title's footer **(chosen: no strip)**. The saved date uses the clock text, so after midnight it shows the right day |
| Search | On the Gear tab's frame (padding 4, gap 4). The container's panel fits its 4 x 3 grid and the carried panel ends with the frame **(chosen: no dark space round the slots)** |
| Gear | PACK heading with the weight right-aligned, actions on the highlight bar, the cursor opens on the nearest item. The Worn panel fits its three rows **(chosen)** |
| Car | A wreck says what taking each part needs (the wrench, the skill it trains, the problem); NOTHING TO TAKE or where Siphon is |
| Craft | RECIPES heading with scroll marks, the heat need named, fuel as `x / y L` |
| Health, Skills, Map, Menu | Footers as above; the NO MAP panel on the standard frame; ticked settings |

Not changed: the Skills list has no heading (all 16 fit without scrolling); the real map keeps its inset (the 8 px
tile grid); the title's 1 px move didn't reproduce. The fog blend on gpSP needs a device check.

## Saves for big worlds

Bench 8 measured the world block at about 8 KB new and up to 23 KB (containers up to 14.6 KB, hordes 4.2 KB, map
2.2 KB), against 14,320 bytes per copy, so two copies only covered the first hour or so.

The block is now packed with LZ77 in the GBA BIOS format (`kx_save_lz.h`, packer in IWRAM, one hash candidate per
position): 22,949 bytes pack to 10,095. Packing runs 256 bytes a frame (about 13% of a frame, worst slice measured)
straight to SRAM through a small stage; the world header's size has its top bit set for a packed block. If the packed
block won't fit its copy it is stored as is when that fits, else on copy 0 across the area (only noise does that). Old
saves load unchanged; the load checks the CRC from SRAM and unpacks with bounds checks.

## Checked

- **Power cuts** (`tools/power_cut_test.py`, BENCH=17, now with blocks of noise that don't pack): no cut loses or
  breaks the survivor; every stored block that fits half the area (all patterned sizes up to 28,656 bytes, noise up
  to 14,000) came back whole from every cut. Noise of 20,000 bytes and more is stored across the area and still
  loses the world on cuts during its write.
- Bench 8's save round trip passes (7 saved state lines, settings, sandbox, the next survivor).
- The UI: screenshots from a scripted libretro mGBA run (`tools/retro_drive.py`) of every changed screen.
