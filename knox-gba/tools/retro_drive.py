"""Drives the game in mGBA's libretro core (apt: libretro-mgba) with scripted buttons and saves screenshots: a way
to reach and capture the menu screens, which the benches (they drive their own inputs) can't open.

    from retro_drive import Emu
    emu = Emu('knox.gba', sram='knox.sav')       # sram: optional; loaded if it exists, written by save_sram()
    emu.run(120)                                 # 120 frames, no buttons
    emu.press('A')                               # 2 frames held, then 12 frames released
    emu.hold('DOWN', 40)
    emu.shot('title.png')                        # 240x160 PNG of the last frame (scale=3 for 720x480)

Buttons: A B SELECT START RIGHT LEFT UP DOWN R L.
"""
import ctypes as C
import os

from PIL import Image

CORE = '/usr/lib/x86_64-linux-gnu/libretro/mgba_libretro.so'
IDS = {'B': 0, 'SELECT': 2, 'START': 3, 'UP': 4, 'DOWN': 5, 'LEFT': 6, 'RIGHT': 7, 'A': 8, 'L': 10, 'R': 11}

ENV_CB = C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
VIDEO_CB = C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
AUDIO_CB = C.CFUNCTYPE(None, C.c_int16, C.c_int16)
AUDIO_BATCH_CB = C.CFUNCTYPE(C.c_size_t, C.c_void_p, C.c_size_t)
POLL_CB = C.CFUNCTYPE(None)
STATE_CB = C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)


class GameInfo(C.Structure):
    _fields_ = [('path', C.c_char_p), ('data', C.c_void_p), ('size', C.c_size_t), ('meta', C.c_char_p)]


class Emu:
    def __init__(self, rom, sram=None, core=CORE):
        self.lib = C.CDLL(core)
        self.held = set()
        self.frame = None
        self.format = 0                       # 0: 0RGB1555, 1: XRGB8888, 2: RGB565
        self.frames = 0
        self.sram_path = sram
        self._dir = C.c_char_p(os.path.dirname(os.path.abspath(rom)).encode())

        def env(cmd, data):
            if cmd == 10:                     # SET_PIXEL_FORMAT
                self.format = C.cast(data, C.POINTER(C.c_int))[0]
                return True
            if cmd in (9, 31):                # system / save directory
                C.cast(data, C.POINTER(C.c_char_p))[0] = self._dir.value
                return True
            if cmd == 3:                      # GET_CAN_DUPE
                C.cast(data, C.POINTER(C.c_bool))[0] = True
                return True
            return False

        def video(data, width, height, pitch):
            if data:
                self.frame = (C.string_at(data, pitch * height), width, height, pitch)

        self._cbs = [ENV_CB(env), VIDEO_CB(video), AUDIO_CB(lambda l, r: None),
                     AUDIO_BATCH_CB(lambda d, n: n), POLL_CB(lambda: None),
                     STATE_CB(lambda port, dev, idx, i: int(port == 0 and dev == 1 and i in self.held))]
        self.lib.retro_set_environment(self._cbs[0])
        self.lib.retro_init()
        self.lib.retro_set_video_refresh(self._cbs[1])
        self.lib.retro_set_audio_sample(self._cbs[2])
        self.lib.retro_set_audio_sample_batch(self._cbs[3])
        self.lib.retro_set_input_poll(self._cbs[4])
        self.lib.retro_set_input_state(self._cbs[5])

        self._rom = open(rom, 'rb').read()
        info = GameInfo(os.path.abspath(rom).encode(), C.cast(C.c_char_p(self._rom), C.c_void_p), len(self._rom), None)
        if not self.lib.retro_load_game(C.byref(info)):
            raise RuntimeError('the core refused ' + rom)
        if sram and os.path.exists(sram):
            self._sram()[:len(open(sram, 'rb').read())] = open(sram, 'rb').read()

    def _sram(self):
        self.lib.retro_get_memory_data.restype = C.c_void_p
        self.lib.retro_get_memory_size.restype = C.c_size_t
        size = self.lib.retro_get_memory_size(0)
        return (C.c_ubyte * size).from_address(self.lib.retro_get_memory_data(0))

    def save_sram(self, path=None):
        data = bytes(self._sram())
        with open(path or self.sram_path, 'wb') as f:
            f.write(data)

    def run(self, frames, *buttons):
        self.held = {IDS[b] for b in buttons}
        for _ in range(frames):
            self.lib.retro_run()
            self.frames += 1
        self.held = set()

    def hold(self, button, frames, *more):
        self.run(frames, button, *more)

    def press(self, *buttons, hold=2, after=12):
        self.run(hold, *buttons)
        self.run(after)

    def presses(self, button, count, after=12):
        for _ in range(count):
            self.press(button, after=after)

    def image(self):
        data, width, height, pitch = self.frame
        if self.format == 1:
            img = Image.frombuffer('RGBX', (width, height), data, 'raw', 'BGRX', pitch, 1).convert('RGB')
        else:
            img = Image.new('RGB', (width, height))
            px = img.load()
            for y in range(height):
                row = data[y * pitch:(y + 1) * pitch]
                for x in range(width):
                    v = row[x * 2] | row[x * 2 + 1] << 8
                    if self.format == 2:
                        px[x, y] = ((v >> 11 & 31) * 255 // 31, (v >> 5 & 63) * 255 // 63, (v & 31) * 255 // 31)
                    else:
                        px[x, y] = ((v >> 10 & 31) * 255 // 31, (v >> 5 & 31) * 255 // 31, (v & 31) * 255 // 31)
        return img

    def shot(self, path, scale=1):
        img = self.image()
        if scale != 1:
            img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
        img.save(path)
        return img
