# Playtest 8: building round 4

Source: the "Building round 4" section of the playtest report (UI audit items 26 to 34). Mihai approved the
barricade strength and the mockups (`build-panel.png`, `barricade-damage.png` in the project files) on 2026-11-25.

| Finding | Cause | Fix |
| --- | --- | --- |
| One zombie breaks a reinforced barricade in ~19 s | 10 damage per hit, a hit every 45 frames | `base_tuning::barricade_damage = 1`: one zombie needs ~75 s for a plain barricade and ~3 min for a reinforced one. Built doors and log walls keep 10. |
| A barricade looks the same at any health, with no warning | One graphic per wall side | `BARRICADE_{FACE,TOP}_{WORN,FAILING}` under 67% and 34% (`world::barricade_gfx`). Mending restores the full look. "BARRICADE FAILING" toast under 25%. |
| Cursor yellow and "HAMMER OK" without materials (28, 29) | Cursor and status only checked the spot and the tool | The cursor is red whenever A would do nothing. The status says "NEED LOGS" (the first short material) in red. |
| Mend shows no materials (30) | The materials loop skipped repairs | A mend lists the planks and nails it uses. |
| Mend says "BARRICADE REINFORCED" (31) | The message checked the skill, not the barricade | "BARRICADE MENDED" unless the mend turns a plain barricade reinforced. |
| Third material cut off (32) | Text that runs off screen isn't drawn | The materials get a full-width row of their own. |
| Panel edge to edge with only a top border (26) | Mockup 06 layout | A framed panel inset 4 px, like the other panels. |
| Only "HOLD A" as a hint (27) | | A hint row: HOLD A BUILD, B EXIT, L1/R1 PIECE, D-PAD AIM. |
| Labels: BARR, LOGS, a 2 px gap (33) | Short labels under 24 px slots | No labels under the icons. The title names the selected piece in full. |
| 1 px red line by the outdoor MASH A box (34) | Not reproduced | Probably fixed by playtest 6's vertical-blank HUD flush. Check on the device. |

## Checking

`make BENCH=18` and `python tools/door_gallery.py knox_gallery.gba out` now end with nine build-mode shots (49 to 57):
the barricade with and without planks, the log wall, the rain collector, the wooden door, a mend, and a barricade at
100%, 50% and 20%.
