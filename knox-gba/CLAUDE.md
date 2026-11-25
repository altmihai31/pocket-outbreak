# Knox-style survival for GBA

A single-player, top-down zombie survival sandbox inspired by Project Zomboid, built as a Game Boy Advance ROM with the Butano engine. The main target is the Miyoo Mini / Mini Plus (OnionOS, gpSP and mGBA cores); the ROM must also run on any GBA emulator and on real hardware.

**Mechanics come first.** Movement and combat feel decide whether the game works, so they are built and tuned before any content.

## Read first

- `docs/design-spec.md` is the full design: every system, its numbers, the hardware budget, the save layout, the cut list and the build order. It is the source of truth. If code and spec disagree, ask before changing either.
- `mockups/*.png` show the six agreed screens at 3× (720×480 = 240×160 GBA pixels). `mockups/*.html` are the same screens as static HTML. Match their layout, palette and HUD placement.
- `docs/palette.md` lists the mockup colours with their GBA 15-bit values.

## Stack

- C++ with [Butano](https://github.com/GValiente/butano) on devkitARM (devkitPro). Start from Butano's `template` project and point `LIBBUTANO` in the Makefile at the Butano checkout.
- Build with `make -j`; output is a `.gba` ROM.
- Test in mGBA on the desktop; every milestone is also tested on the Miyoo under both gpSP and mGBA.

## Hard constraints (GBA hardware)

- 16.78 MHz ARM7TDMI, no FPU: all maths uses `bn::fixed`. No `float` or `double` anywhere.
- 256 KB EWRAM, 32 KB IWRAM, 96 KB VRAM (64 KB background, 32 KB sprites), 128 sprites, 32 affine matrices, 4 background layers in mode 0.
- Save: 32 KB SRAM only (most reliable on gpSP and mGBA). The layout is in the spec's Save system section.
- Locked 60 fps. Hot loops (zombie AI, collision, chunk generation) go in IWRAM as ARM code with `BN_CODE_IWRAM`.
- Respect the per-system CPU and memory budgets in the spec's Hardware budget section. Measure with Butano's profiler (`BN_PROFILER`) and report numbers at the end of each milestone.

## Conventions

- Every tuning value (speeds, frame data, noise radii, timers, loot weights) lives in one `constexpr` data table per system, never inline in logic.
- Layers: BG0 HUD and text, BG1 roofs and canopies, BG2 walls and furniture, BG3 ground and decals. Gameplay HUD text is BG0 tiles; Butano sprite text only in paused menus.
- Avoid heap allocation in the game loop; use Butano's fixed-capacity containers (`bn::vector`, `bn::deque` with explicit capacity).
- Timed world state is lazy: store the start day or time and compute on access (food freshness, crops, generators). Never tick off-screen things per frame.
- Controls use only the GBA's 10 buttons (spec: Controls and UI).
- Nothing from the spec's cut list without asking.

## Current state

Milestones 1 to 14 are built and awaiting device testing; `README.md` lists what each one added. Work now comes from playtest feedback: each round has a plan in `docs/playtest-N-plan.md`, and the latest round is the current work. Large new features get their own plan in `docs/` before any code.
