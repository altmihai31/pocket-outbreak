"""The guide (playtest 3): how to play, in topics, for the MENU's GUIDE page. The text is wrapped here to the page's
width in the small font's own widths and split into pages, so the game only draws lines.

Each topic is a list of paragraphs; a blank line goes between paragraphs (not at the top of a page)."""
from common import write_header
from small_font import GLYPHS

TEXT_WIDTH = 136        # the text panel's inside, in px (kx_guide_screen.cpp: page_width)
PAGE_LINES = 13         # 7 px a line

TOPICS = [
    ('CONTROLS', [
        'D-PAD MOVES. DOUBLE-TAP AND HOLD IT TO JOG.',
        'A: THE ACTION SHOWN, LIKE [A] SEARCH OR [A] DRINK; OTHERWISE SWING WHAT YOU HOLD. HOLD A TO CHARGE A '
        'SWING.',
        'B: TAP TO SHOVE, HOLD WHILE MOVING TO SPRINT.',
        'L1: CROUCH. R1 HELD: AIM A GUN, OR A THROWABLE IN THE OFF HAND.',
        'SELECT: USE THE CHOSEN HOTBAR ITEM. L1 + SELECT: THE NEXT SLOT.',
        'START: THE MENU TABS (L1 AND R1 SWITCH). SAVE AND QUIT IS IN THE MENU TAB: HOLD A ON IT.',
    ]),
    ('SURVIVAL', [
        'THE CLOCK NEVER STOPS: A GAME MINUTE IS 2.5 SECONDS.',
        'THE ICONS BOTTOM-LEFT SHOW WHAT IS WRONG, WORST FIRST: HUNGER, THIRST, TIREDNESS, COLD, PANIC... '
        'THE HEALTH TAB EXPLAINS THEM.',
        'SLEEP IN A BED, NOT WITH ZOMBIES NEAR. STAY UP TOO LONG AND YOU PASS OUT WHERE YOU STAND.',
        'FOOD GOES STALE, THEN ROTTEN. ROTTEN FOOD, RAW MEAT AND RIVER WATER CAN MAKE YOU SICK: BOIL WATER IN '
        'THE CRAFT TAB.',
        'RAIN SOAKS YOU AND COLD WEARS YOU DOWN: CLOTHES AND A CAMPFIRE HELP.',
        'THE TAPS RUN DRY ON A DAY BETWEEN 10 AND 20, THE POWER GOES BETWEEN 14 AND 30. NOBODY WARNS YOU.',
    ]),
    ('ZOMBIES', [
        'BY DAY THEY SEE YOU FROM 8 TILES, AT NIGHT FROM 3 (6 IF YOU CARRY A LIGHT).',
        'CROUCH TO SNEAK: SLOWER, QUIETER AND HARDER TO SEE. IN FOREST, MUD OR CROPS THEY CAN\'T SEE YOU '
        'BEYOND 2 TILES.',
        'NOISE DRAWS THEM: THE METER TOP-RIGHT SHOWS HOW FAR YOURS CARRIES. GUNSHOTS, ALARMS AND HAMMERING '
        'MOVE WHOLE HORDES.',
        'ONE THAT LOSES YOU SEARCHES WHERE IT LAST SAW YOU.',
        'RED ARROWS AT THE SCREEN\'S EDGE: MORE ARE COMING FROM THERE.',
        'TOWNS AND THE MILITARY BASE ARE CROWDED; FARMS AND THE WILD ARE QUIET.',
    ]),
    ('FIGHTING', [
        'A SWINGS YOUR WEAPON; HOLD A FOR A HARDER, SLOWER HIT. STAND OVER A ZOMBIE ON THE GROUND AND A STOMPS '
        'IT.',
        'B SHOVES BACK EVERY ZOMBIE IN FRONT OF YOU: MAKE ROOM WHEN THEY CROWD YOU.',
        'SWINGS TIRE YOU, HEAVY WEAPONS MORE. EXHAUSTED, YOU HIT SLOWER.',
        'WEAPONS WEAR OUT: REPAIR THEM IN THE GEAR TAB WITH DUCT TAPE, GLUE OR A WHETSTONE.',
        'GRABBED? MASH A TO BREAK FREE. EVERY GRAB CAN SCRATCH, CUT OR BITE, AND ONE THAT HOLDS ON TOO LONG '
        'ALWAYS BITES.',
        'HELD BY TWO AT ONCE, TOO SLOW IS DEATH. TWO AT ONCE IS A FIGHT. FIVE IS A REASON TO RUN.',
    ]),
    ('GUNS', [
        'HOLD R1 TO AIM: YOU WALK AT HALF SPEED, THE NEAREST ZOMBIE IN FRONT IS BRACKETED AND THE DOTTED LINES '
        'SHOW THE SPREAD. STAND STILL TO NARROW IT.',
        'A FIRES. B RELOADS FROM YOUR BAG: A IN THE GREEN PART OF THE BAR LOADS AT ONCE, A ANYWHERE ELSE JAMS '
        'THE GUN, AND B CLEARS IT.',
        'SHOTS CARRY 30 TO 45 TILES: EVERYTHING AROUND WILL COME.',
        'PISTOL 15 ROUNDS. SHOTGUN 6, A SPREAD THAT KNOCKS DOWN. HUNTING RIFLE 5, ONE SHOT KILLS. ASSAULT '
        'RIFLE 30: HOLD A TO KEEP FIRING.',
        'WITHOUT AIMING, A GUN IS A CLUB.',
    ]),
    ('WOUNDS', [
        'GRABS SCRATCH, CUT OR BITE AN ARM OR THE TORSO. A BITE ALWAYS INFECTS, A CUT SOMETIMES, A SCRATCH '
        'RARELY. THE INFECTION HIDES, THEN COMES THE FEVER, THEN DEATH IN 2 OR 3 DAYS.',
        'OPEN WOUNDS BLEED: IN THE HEALTH TAB PICK THE PART AND PRESS A TO BANDAGE IT (A RAG WILL DO). CHANGE '
        'A BANDAGE AFTER 12 HOURS, WHEN IT IS DIRTY.',
        'DISINFECTANT OR ANTIBIOTICS STOP A WOUND GOING BAD.',
        'A FALL CAN BREAK A LEG: A SPLINT HELPS IT MEND.',
    ]),
    ('LOOTING', [
        'FACE A CABINET, FRIDGE OR SHELF AND [A] SEARCH. THE ITEMS SHOW ONE BY ONE WHILE THE WORLD KEEPS GOING.',
        'A TAKES ONE, R1 TAKES ALL, SELECT TAKES HALF A STACK. ON YOUR SIDE, A PUTS AN ITEM IN.',
        'WATCH THE WEIGHT: A HEAVY LOAD SLOWS AND TIRES YOU. A BAG ON YOUR BACK CARRIES MORE.',
        'CLOTHES GUARD AGAINST SCRATCHES AND BITES AND KEEP YOU WARM: WEAR THEM FROM THE GEAR TAB.',
        'KITCHENS HOLD FOOD, BATHROOMS MEDICINE, GARAGES TOOLS, POLICE LOCKERS GUNS.',
    ]),
    ('BUILDING', [
        'PUT A HAMMER ON THE HOTBAR AND PRESS SELECT FOR BUILD MODE. L1 AND R1 PICK A PIECE, THE D-PAD MOVES '
        'THE CURSOR, HOLD A TO BUILD AND B LEAVES.',
        'BARRICADE WINDOWS AND DOORWAYS (3 PLANKS, 6 NAILS). ZOMBIES BREAK THEM IN TIME: BUILD ON A DAMAGED '
        'ONE TO MEND IT.',
        'HAMMERING IS LOUD: CLEAR THE AREA FIRST.',
        'A CRATE STORES 40 KG. A CAMPFIRE (STONES, STICKS, MATCHES) WARMS, DRIES, COOKS AND LIGHTS THE NIGHT, '
        'BUT HORDES DRIFT TOWARDS IT.',
        'A RAIN COLLECTOR GIVES WATER WHEN THE TAPS RUN DRY.',
    ]),
    ('FOOD', [
        'COOK IN THE CRAFT TAB BY A CAMPFIRE, OR A STOVE WHILE THERE IS POWER. COOKED FOOD FILLS MORE AND LIFTS '
        'YOUR MOOD.',
        'CROUCH ON GRASS OR IN THE WOODS AND [A] FORAGE: BERRIES, HERBS, MUSHROOMS (PALE ONES ARE POISON) AND, '
        'BY WATER, FISH.',
        'FARM: SELECT A SHOVEL TO DIG A PLOT, A PLANTS SEEDS, AND A WATERING CAN KEEPS THEM GROWING.',
        'BOX TRAPS, AFTER THE TRAPPING MAGAZINE, CATCH SMALL GAME OVERNIGHT.',
        'FRIDGES AND FREEZERS STOP WHEN THE POWER GOES.',
    ]),
    ('ANIMALS', [
        'DEER, RABBITS AND GOATS RUN FROM YOU: CROUCH TO GET CLOSE.',
        'A DEAD ANIMAL: HOLD [A] BUTCHER WITH A KNIFE OR AN AXE FOR MEAT AND A HIDE (ROPE, IN THE CRAFT TAB).',
        'A WILD BOAR CHARGES IF YOU GET TOO CLOSE OR HURT IT.',
        'WOLVES HUNT IN PACKS AT NIGHT IN THE WILD: THEY CIRCLE YOU, THEN DART IN TO BITE. KILL ONE, OR STAND '
        'BY A FIRE, AND THE PACK RUNS.',
        'ON FARMS, FORAGE NEAR CHICKENS FOR EGGS; A GOAT FILLS AN EMPTY BOTTLE WITH MILK.',
        'CROWS ON CORPSES CAW WHEN STARTLED, AND ZOMBIES HEAR IT.',
    ]),
    ('NIGHT', [
        'IT IS DARK FROM 19:00 TO 07:00. ZOMBIES SEE LESS, AND SO DO YOU.',
        'PUT A FLASHLIGHT ON THE HOTBAR: SELECT TAKES IT IN YOUR OFF HAND AND SWITCHES IT ON. A BATTERY LASTS '
        '6 HOURS.',
        'LIGHT GIVES YOU AWAY: LIT, YOU ARE SEEN FROM 6 TILES INSTEAD OF 3. DARK IS STEALTHY.',
        'BUILDINGS HAVE LIGHTS UNTIL THE POWER GOES; AFTER THAT, ONLY NEAR A RUNNING GENERATOR.',
        'READING NEEDS LIGHT.',
    ]),
    ('VEHICLES', [
        '[A] RIDE A BICYCLE: FAST ON ROADS, AND QUIET.',
        '[A] CAR SHOWS ITS CHECKLIST: BATTERY, SPARK PLUG, RADIATOR, TYRES, FUEL. FIT WHAT IS MISSING (MOST '
        'PARTS NEED A WRENCH). A WRECK ONLY GIVES PARTS.',
        'DRIVING: LEFT AND RIGHT STEER, A ACCELERATES, B BRAKES THEN REVERSES, R1 HEADLIGHTS, L1 GETS OUT ONCE '
        'STOPPED.',
        'THE TRUNK HOLDS 60 KG. SIPHON FUEL WITH A HOSE INTO A FUEL CAN.',
    ]),
    ('SAVING', [
        'ONE WORLD IS KEPT. IT SAVES AFTER YOU SLEEP, EVERY GAME HOUR, AND WHEN YOU SAVE AND QUIT (HOLD A ON IT '
        'IN THE MENU TAB).',
        'DEATH IS FOR GOOD. THE WORLD STAYS AND A NEW SURVIVOR STARTS IN A VILLAGE; THE OLD ONE WALKS AGAIN, IN '
        'THEIR CLOTHES, WITH THEIR GEAR.',
        'SKILLS RISE BY DOING. SKILL BOOKS MAKE THEM RISE FASTER.',
        'NEW WORLD, ON THE TITLE, REPLACES THE SAVED ONE.',
    ]),
]


def text_width(text):
    total = 0
    for c in text.upper():
        total += len(GLYPHS.get(c, GLYPHS['?'])[0]) + 1
    return max(total - 1, 0)


def wrap(paragraph, width=TEXT_WIDTH):
    lines = []
    line = ''
    words = paragraph.split()
    # A button prompt stays with its action: "[A] SEARCH" never breaks.
    for index in range(len(words) - 1, 0, -1):
        if words[index - 1].startswith('[') and words[index - 1].endswith(']'):
            words[index - 1:index + 1] = [words[index - 1] + ' ' + words[index]]
    for word in words:
        trial = word if not line else line + ' ' + word
        if text_width(trial) <= width:
            line = trial
        else:
            assert line, word
            lines.append(line)
            line = word
    if line:
        lines.append(line)
    return lines


def pages(paragraphs):
    """Lines per page: paragraphs kept whole on a page when they fit, a blank line between them."""
    result = [[]]
    for paragraph in paragraphs:
        lines = wrap(paragraph)
        page = result[-1]
        needed = len(lines) + (1 if page else 0)
        if len(page) + needed > PAGE_LINES:
            assert len(lines) <= PAGE_LINES, paragraph
            result.append([])
            page = result[-1]
        if page:
            page.append('')
        page.extend(lines)
    return result


def build_guide():
    lines = ['#include <cstdint>', '', 'namespace kx::guide', '{', '',
             'constexpr int topic_count = %d;' % len(TOPICS),
             'constexpr int page_lines = %d;' % PAGE_LINES,
             'constexpr int page_width = %d;' % TEXT_WIDTH, '',
             'constexpr const char* topic_names[] = { %s };' % ', '.join('"%s"' % name for name, _ in TOPICS), '']
    text = []
    page_list = []
    topic_first = []
    for _, paragraphs in TOPICS:
        topic_first.append(len(page_list))
        for page in pages(paragraphs):
            page_list.append((len(text), len(page)))
            text.extend(page)
    topic_first.append(len(page_list))
    lines.append('// Every page\'s lines, one after another ("" is the gap between paragraphs).')
    lines.append('constexpr const char* lines[] = {')
    for line in text:
        lines.append('    "%s",' % line.replace('\\', '\\\\').replace('"', '\\"'))
    lines += ['};', '', 'struct page', '{', '    uint16_t first;', '    uint8_t count;', '};', '',
              'constexpr page pages[] = { %s };' % ', '.join('{ %d, %d }' % p for p in page_list), '',
              '// Topic t has pages topic_first_page[t] .. topic_first_page[t + 1] - 1.',
              'constexpr uint8_t topic_first_page[] = { %s };' % ', '.join(str(p) for p in topic_first), '', '}']
    write_header('kx_guide_text.h', lines)
    return TOPICS, [pages(p) for _, p in TOPICS]


if __name__ == '__main__':
    _, all_pages = build_guide()
    for (name, _), topic in zip(TOPICS, all_pages):
        print(name, [len(p) for p in topic])
