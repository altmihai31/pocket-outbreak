# Palette

Colours used in the mockups, with the GBA 15-bit value (BGR555) each one snaps to. In Butano, use `bn::color(r, g, b)` with the 0–31 channel values shown.

| Use | RGB hex | bn::color(r, g, b) | BGR555 |
| --- | --- | --- | --- |
| UI background | `#0b0e14` | `bn::color(1, 1, 2)` | `0x0821` |
| Menu background | `#10141b` | `bn::color(2, 2, 3)` | `0x0C42` |
| Panel border | `#2a2f3a` | `bn::color(5, 5, 7)` | `0x1CA5` |
| Empty segment / slot | `#262b35` | `bn::color(4, 5, 6)` | `0x18A4` |
| Slot fill | `#151a22` | `bn::color(2, 3, 4)` | `0x1062` |
| Text | `#e8e4d8` | `bn::color(29, 28, 27)` | `0x6F9D` |
| Quiet text | `#a8b0bc` | `bn::color(21, 22, 23)` | `0x5ED5` |
| Accent / selection | `#e8c547` | `bn::color(29, 24, 8)` | `0x231D` |
| Moodle level 1 | `#e8c547` | `bn::color(29, 24, 8)` | `0x231D` |
| Moodle level 2 | `#e0903a` | `bn::color(28, 18, 7)` | `0x1E5C` |
| Moodle level 3 | `#e05a3a` | `bn::color(28, 11, 7)` | `0x1D7C` |
| Moodle level 4 | `#b02a20` | `bn::color(22, 5, 4)` | `0x10B6` |
| Blood / bleeding | `#d0453a` | `bn::color(26, 8, 7)` | `0x1D1A` |
| Water | `#4a90d9` | `bn::color(9, 18, 27)` | `0x6E49` |
| Cold | `#7fb0e0` | `bn::color(15, 22, 28)` | `0x72CF` |
| Food | `#d98a3a` | `bn::color(27, 17, 7)` | `0x1E3B` |
| Good / clean | `#7fd08a` | `bn::color(15, 26, 17)` | `0x474F` |
| Grass | `#3f6b34` | `bn::color(7, 13, 6)` | `0x19A7` |
| Grass light | `#557f42` | `bn::color(10, 15, 8)` | `0x21EA` |
| Tree canopy | `#24451e` | `bn::color(4, 8, 3)` | `0x0D04` |
| Road | `#3a3d42` | `bn::color(7, 7, 8)` | `0x20E7` |
| Road line | `#c9a33a` | `bn::color(25, 20, 7)` | `0x1E99` |
| Pavement | `#8f8d82` | `bn::color(17, 17, 16)` | `0x4231` |
| Brick | `#7d4b3a` | `bn::color(15, 9, 7)` | `0x1D2F` |
| Brick mortar | `#4a2e24` | `bn::color(9, 5, 4)` | `0x10A9` |
| Wood floor | `#8a6a42` | `bn::color(17, 13, 8)` | `0x21B1` |
| Zombie skin | `#8aa07a` | `bn::color(17, 20, 15)` | `0x3E91` |
| Zombie clothes | `#5a4a5e` | `bn::color(11, 9, 11)` | `0x2D2B` |
| Player jacket | `#4d5a3a` | `bn::color(9, 11, 7)` | `0x1D69` |
| Skin | `#d9a77c` | `bn::color(27, 20, 15)` | `0x3E9B` |
| Night overlay | `#04060c` | `bn::color(0, 0, 1)` | `0x0400` |
| Dusk tint | `#2e1a54` | `bn::color(5, 3, 10)` | `0x2865` |

Font: [Silkscreen](https://fonts.google.com/specimen/Silkscreen) was used for the mockups; in game, draw an 8×8 pixel font of the same style on BG0.
