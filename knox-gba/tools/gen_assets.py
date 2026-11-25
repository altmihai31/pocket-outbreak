"""Generates the game's placeholder assets and the index headers that describe them.

Run from anywhere (the Makefile runs it as EXTTOOL before every build). Files are only rewritten when
their content changes, so incremental builds stay incremental.

All art is drawn procedurally from docs/palette.md colours and all sounds are synthesized; replace them
with hand-made assets later by swapping the files in graphics/ and audio/ and deleting the generator.
Modules live in tools/assets/.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'assets'))

from animals import build_animals        # noqa: E402
from audio import build_audio            # noqa: E402
from characters import build_characters  # noqa: E402
from common import BUTANO_COMMON_GRAPHICS, GRAPHICS, write_if_changed  # noqa: E402
from fx import build_fx                  # noqa: E402
from guide import build_guide            # noqa: E402
from hud import build_hud                # noqa: E402
from music import build_music            # noqa: E402
from small_font import build_small_font  # noqa: E402
from vehicles import build_vehicles      # noqa: E402
from weapons import build_weapons        # noqa: E402
from world import build_world            # noqa: E402

# Butano only links graphics from folders, so the menu font is copied into graphics/.
MENU_FONT = 'common_variable_8x8_font'


def copy_menu_font():
    for ext in ('.bmp', '.json'):
        with open(os.path.join(BUTANO_COMMON_GRAPHICS, MENU_FONT + ext), 'rb') as f:
            write_if_changed(os.path.join(GRAPHICS, MENU_FONT + ext), f.read())


def main():
    os.makedirs(GRAPHICS, exist_ok=True)
    copy_menu_font()
    build_world()
    build_hud()
    build_small_font()
    build_characters()
    build_weapons()
    build_fx()
    build_guide()
    build_vehicles()
    build_animals()
    build_audio()
    build_music()
    return 0


if __name__ == '__main__':
    sys.exit(main())
