# Playtest 7: how doors look

The tester played v0.3 and said doors look wrong in some states. A new bench, `make BENCH=18` (`knox_gallery.gba`),
finds one door of each kind on the fixed seed (side doors with the outside to the right and to the left, front, back,
inner side and inner front). It shows each one closed, open, barricaded (over the open door) and broken, from both
sides. `tools/door_gallery.py` runs it in mGBA's libretro core and saves the 48 shots and a contact sheet.

| # | What looked wrong | Cause | Fix |
|---|-------------------|-------|-----|
| 1 | From inside, a side door showed twice: in its wall, and as its edge outside (a closed door as two slabs, an open one as one long bar through the wall) | Playtest 5's edge is on the canopy of the tile outside, beyond the roof's cut-away, so it stayed | The edges of the building the player is in are not drawn (`world::hide_door_edges`, called when the building changes; the canopy round it is redrawn) |
| 2 | A barricaded side door kept its edge outside | `update_door_edge` returned early once the wall was no longer a door, and `refresh_tile` only redrew beside doors | The edge follows any wall change; `refresh_tile` always redraws the tiles beside and above |
| 3 | A back door (in the top wall) never showed from outside | The roof covers the top wall, as with side doors before playtest 5 | The same edge on the step above it: `DOOR_EDGE_T_CLOSED` (a slab past the roof line) and `DOOR_EDGE_T_OPEN` (swung out beside the step) |
| - | A barricaded side or back door showed nothing from outside, while a front door shows its boards | | `DOOR_EDGE_{R,L,T}_BOARDED`: the ends of two planks past the wall line, for any barricade in those walls (windows too) |

Front doors (under the eave's dip) and inner doors looked right and are unchanged. A locked door looks like a closed
one. The new graphics go after the existing ones, so saved ids stay put.
