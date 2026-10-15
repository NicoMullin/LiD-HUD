/*
 * lid-hud.dll for Let It Die - a plugin for the LiD Plugin Loader that adds to
 * the game's HUD. It changes game code in memory only, never on disk.
 *
 * 0.1.0: durability numbers on the weapon bars.
 * 0.2.0: the Enemies Left counter ("Enemies left: 12 / 20"), moved here from
 *        the manager's Enemies Left mod.
 * 0.3.0: durability in the pause menu's fighter panel too, and a pop-up when a
 *        weapon loses or gets back durability; an option to hide the Serial
 *        Killer decal's screen distortion.
 * 0.4.0: escalators on the pause menu's area map sit on a disc of their
 *        line's colour, as on the floor map.
 * 0.5.0: the bow arc - while a bow is drawn, a line shows where the arrow
 *        will fly and land.
 * 0.6.0: a minimap - the area map, always on, in a corner of the screen.
 * 0.7.0: a layout mode in the game: F8, then move and size each part with the
 *        keyboard; saved to lid-hud.ini.
 * 0.7.1: every part on one scale, the same at any screen size: all text is
 *        drawn the way the game draws its own HUD.
 * 0.8.0: every setting in the game too (O in the layout mode); the minimap can
 *        go right up to the screen's edges.
 * 0.8.1: the options page in groups, with a heading each; the minimap reaches
 *        every edge from any corner.
 * 0.9.0: floor info - beasts, chests and materials left on the floor, and
 *        whether it has a blueprint or a Fun Fungus; rage - how full the rage
 *        gauge is, and how much the last hit gave.
 * 0.9.1: floor info and rage only while the HUD shows (not in films or
 *        loading screens); beasts counted alive; taken items no longer
 *        counted.
 * 0.9.2: mushrooms left counted on the floor too (they go down as they are
 *        picked), and shown by default.
 * 0.9.3: the layout mode's pages easier to read: a dark panel behind them,
 *        bigger and thicker text (both set on the options page), names on
 *        the left and values on the right, the key help on two lines; B
 *        switches the panel.
 * 0.9.4, 0.9.5: Tengoku's medium chests counted as blueprint chests, and a
 *        Tengoku (H4) blueprint on the floor named - both taken out again:
 *        what a chest holds is only decided when it is opened.
 * 0.9.6: the blueprint line taken out too; the text colour of the durability
 *        numbers, floor info (and its Fun Fungus line) and rage can be set.
 * 0.9.7: each durability number (3 slots a hand) and each floor info line can
 *        be moved on its own (Page Up / Page Down in the layout mode); the
 *        durability numbers can be shown for the weapons in hand only.
 * 0.9.8: colours work: the text is drawn with the font's KeepColor call (the
 *        plain one puts its own colour back).
 * 0.9.9: "in hand" goes by the part each slot holds, matched to the part in
 *        the hand (the pawn's equip slot number stays 0).
 * 0.10.0: low durability colours (a warning and a low colour, each below a
 *        % of its own, and flashing); the bag's space; the mushroom effects
 *        running and their time left; run totals - time in the tower and
 *        the Kill Coins, SP, Bloodnium, EXP and Death Metals it brought.
 * 0.10.1: steady text: the HUD's text state is copied without the moves,
 *        turns and passing size changes the game gives its own numbers;
 *        a background can go behind each part's text; the layout mode shows
 *        the mushroom effects whatever part is picked.
 * 0.11.0: deaths and continues in the run totals; the bag counts the
 *        fighter being played (it read fighter 0's); our text is drawn with
 *        the HUD's draw transform off, which another part may leave on.
 * 0.11.1: deaths and continues counted from the game-over screen (the
 *        fighter's health never reads 0).
 * 0.11.2: "only the weapons in hand" taken out - no way was found to tell
 *        which slot a hand has out.
 *
 * The HUD draws each weapon slot's durability as a bar. The plugin writes the
 * numbers on it - "45/60", "45" or "75%" - in the HUD's own font, through the
 * game's own text drawing. Melee weapons and guns are switched separately, and
 * the format, place, size and colour are set in lid-hud.ini next to the plugin
 * (see read_settings()); the file is read again when it changes, so a change
 * shows without restarting the game.
 *
 * How it works:
 *   - Once a frame (the game asks Windows for the foreground window once a
 *     frame, from one place in its main loop; the plugin answers that call
 *     itself, then does its work), it finds the game's UI manager through the
 *     engine's own object data: the world, its game, the game's UI manager.
 *   - It hooks the HUD's "draw a gauge image" function in the HUD class's
 *     table of functions. After the game has drawn a gauge, the hook looks at
 *     who asked: when it is the HUD's weapon slot drawing, it reads that slot's
 *     durability - and draws the number only if the bar it just drew shows
 *     exactly that much (within 0.2%). So it never labels any other bar.
 *   - Durability comes from the game's own data: the weapon's current
 *     durability, and its maximum from the game's own CalcPartParam, the same
 *     sum the game uses.
 *   - Every name is looked up in the engine's own list of classes and fields,
 *     so a game update that moves fields around is followed, not guessed.
 *
 * The way in - which engine functions to use and where the HUD keeps a slot's
 * numbers - was worked out by the author of an earlier HUD plugin, who allowed
 * us to build on it.
 *
 * Safety rules:
 *   - It only does anything inside BrgGame-Steam.exe.
 *   - Every piece of game code is found by its exact bytes and must be found
 *     exactly once. Anything missing switches that feature off and is named in
 *     the log; after a game update that moves the code, the plugin does
 *     nothing at all.
 *   - Every pointer is checked before it is read.
 *   - No network, no registry. It writes only lid-hud.log next to itself.
 *   - Delete lid-hud.dll and the game is exactly stock again.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "version.h"

/* ------------------------------------------------------------------ log */

static wchar_t g_log_path[MAX_PATH];
static int g_log_started;

static void log_line(const char *fmt, ...)
{
    char msg[768];
    SYSTEMTIME t;
    va_list ap;
    int n;
    HANDLE f;
    DWORD written;

    if (!g_log_path[0])
        return;
    GetLocalTime(&t);
    n = snprintf(msg, sizeof msg, "%02u:%02u:%02u  ", t.wHour, t.wMinute, t.wSecond);
    va_start(ap, fmt);
    n += vsnprintf(msg + n, sizeof msg - n - 2, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof msg - 3)
        n = (int)sizeof msg - 3;
    msg[n++] = '\r';
    msg[n++] = '\n';

    /* a fresh log each time the game starts */
    f = CreateFileW(g_log_path, g_log_started ? FILE_APPEND_DATA : GENERIC_WRITE,
                    FILE_SHARE_READ, NULL, g_log_started ? OPEN_ALWAYS : CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    g_log_started = 1;
    WriteFile(f, msg, (DWORD)n, &written, NULL);
    CloseHandle(f);
}

/* Say something once, not once a frame. */
#define LOG_ONCE(flag, ...) do { if (!(flag)) { (flag) = 1; log_line(__VA_ARGS__); } } while (0)

/* ------------------------------------------------------------ settings */

/* lid-hud.ini, next to the plugin. Every line is optional.
 *
 *   [durability]
 *   melee=1       numbers on melee weapons' bars
 *   guns=1        numbers on guns' and bows' bars
 *   format=0      0 current/max, 1 current, 2 percent left
 *   position=0    0 on the bar, 1 above it, 2 below it
 *   size=100      text size, % of the HUD's own (30 to 300)
 *   x=0           move right (or left, negative), in 1920 x 1080 pixels
 *   y=0           move down (or up, negative)
 *   right1_x=0, right1_y=0 ... right3_, left1_ ... left3_
 *                 one slot's number moved on its own, on top of x and y
 *                 (the hand's slots 1 to 3)
 *   warn=0        below this % of its durability a number turns warn_colour
 *   warn_colour=FFE63C     (0: never)
 *   low=25        below this %, low_colour (0: never)
 *   low_colour=FF3C3C
 *   flash=1       a low number flashes
 *   colour=FFFFFF its text colour: red, green, blue in hex, or "game" for the
 *                 slot's own colour (red when empty). Files from before 0.9.6
 *                 may have white=1 (FFFFFF) or white=0 ("game") instead.
 *   shadow=1      a dark outline behind the text
 *   menu=1        also on the equipment bars of the pause menu's fighter panel
 *   popup=1       briefly show how much durability a weapon lost or got back
 *   loss=0        its colour for a loss, gain=4 for a gain: 0 red, 1 orange,
 *   gain=4        2 yellow, 3 white, 4 green, 5 cyan, 6 blue, 7 magenta
 *
 *   [enemies]
 *   show=1        "Enemies left: 12 / 20" while you explore a floor
 *   total=1       also how many the floor started with ("/ 20")
 *   x=1880        where it goes, on the game's 1920 x 1080 screen
 *   y=160
 *   align=2       0 starting at that point, 1 centred on it, 2 ending at it
 *   size=100      text size, % of the HUD's own text (40 to 300)
 *   colour=FFFFFF red, green, blue in hex
 *
 *   [screen]
 *   hide_serial_killer=0   take away the colour-split distortion the Serial
 *                          Killer decal puts on the screen
 *   background_darkness=50 how dark the backgrounds below are, % (10 to 100)
 *
 *   Every part with text - [durability], [enemies], [floor], [rage], [bag],
 *   [effects], [run] - also takes background=1: a dark box behind its text.
 *
 *   [map]
 *   escalators=1       on the area map, each escalator on a disc of its
 *                      line's colour
 *   escalator_size=24  the disc's radius on screen (4 to 120); the same at
 *                      any zoom
 *
 *   [bow]
 *   arc=1              while a bow is drawn, show where the arrow will fly
 *   stop_at_enemies=1  0: the arc only stops at walls and the floor
 *   enemy_colour=0     the arc's colour when it ends on an enemy: 0 the
 *                      game's own, 1 red, 2 orange, 3 yellow, 4 green, 5 cyan
 *   headshot=0         brighter when the arrow would hit the head or neck
 *
 *   [minimap]
 *   show=1             the area map, always on, in a corner of the screen
 *   corner=0           0 top right, 1 top left, 2 bottom right, 3 bottom left,
 *                      4 right middle, 5 left middle
 *   x=40               from that side of the screen, in 1920 x 1080 pixels
 *                      (below 0 to sit right on the edge)
 *   y=200              from the top or bottom (down from the middle, for 4
 *                      and 5; negative goes up)
 *   size=320           its width and height (50 to 1000)
 *   zoom=0             the area map's own zoom: 0 near, 1 far
 *   zoom_percent=100   finer: over 100 zooms in, under 100 out (25 to 400)
 *
 *   [floor]
 *   show=1             what is left on this floor, under the enemies counter
 *   x=1880, y=200      where it goes, on the 1920 x 1080 screen
 *   align=2            0 starting at x, 1 centred on it, 2 ending at it
 *   size=80            text size, % of the HUD's own (40 to 300)
 *   beasts=1           beasts left
 *   chests=1           chests not yet opened
 *   materials=1        materials lying on the floor
 *   fun_fungus=1       "Fun Fungus on this floor", when there is one
 *   mushrooms=1        mushrooms left
 *   colour=FFFFFF      the text colour (red, green, blue in hex)
 *   fungus_colour=50FF50   the Fun Fungus line's own colour
 *   beasts_x=0, beasts_y=0, and mushrooms_, chests_, materials_, fungus_
 *                      one line moved on its own, on top of x and y
 *
 *   [rage]
 *   show=1             "Rage: 63%" - how full the rage gauge is (how far to the
 *                      next one)
 *   gain=1             and "+4%" for a few seconds - what the last hit gave
 *   x=60, y=200        where it goes, on the 1920 x 1080 screen
 *   align=0            0 starting at x, 1 centred on it, 2 ending at it
 *   size=80            text size, % of the HUD's own (40 to 300)
 *   colour=FFFFFF      the text colour (red, green, blue in hex)
 *
 *   [bag]
 *   show=1             "Bag: 12 / 20" - what is in the bag, and how much fits
 *   x=1880, y=380, align=2, size=80, colour=FFFFFF
 *   full_colour=FF3C3C its colour when the bag is full
 *
 *   [effects]
 *   show=1             each mushroom effect running, with its time left
 *   x=60, y=240, align=0, size=75, colour=FFFFFF
 *
 *   [run]
 *   show=1             a trip into the tower: from leaving the Waiting Room
 *                      until coming back
 *   time=1, kill_coins=1, sp=1, bloodnium=1, exp=1, death_metals=1
 *                      its time, and what each went up (or down) by
 *   hub=1              the last trip, while in the Waiting Room
 *   x=60, y=420, align=0, size=75, colour=FFFFFF
 *   time_x=0, time_y=0 ... death_metals_x, _y   one line moved on its own
 *
 *   [layout]
 *   key=F8             the key that opens the layout mode in the game (F1 to
 *                      F12). The layout mode writes the places and sizes
 *                      above back to this file.
 *   text_size=100      the layout mode's own text, % (80 to 200)
 *   thick_text=1       its text drawn twice, a little apart: thicker
 *   panel=1            a dark panel behind it (B in the layout mode)
 */
struct settings {
    int melee, guns, format, position, size, x, y, white, shadow;
    int menu, popup, loss, gain;
    int hide_serial;
    unsigned colour;            /* the numbers' colour, or COLOUR_GAME */
    int off_x[6], off_y[6];     /* each slot moved on its own: right 1-3, left 1-3 */
    int warn, low, flash;       /* low durability: below these %, these colours */
    unsigned warn_colour, low_colour;
    int background;             /* a dark box behind each number */
};

/* A colour setting that means "the game's own colour", not one of ours. */
#define COLOUR_GAME 0x1000000u

struct enemy_settings {
    int show, total, x, y, align, size;
    unsigned colour;
    int background;
};

struct map_settings {
    int escalators, escalator_size;
};

struct bow_settings {
    int arc, pawns, enemy_colour, headshot;
};

struct minimap_settings {
    int show, corner, x, y, size, zoom, zoom_percent;
};

static struct settings g_set = {1, 1, 0, 0, 100, 0, 0, 1, 1, 1, 1, 0, 4, 0, 0xFFFFFF, {0}, {0},
                                0, 25, 1, 0xFFE63C, 0xFF3C3C, 0};
static int g_settings_changed;
static struct enemy_settings g_enemies = {1, 1, 1880, 160, 2, 100, 0xFFFFFF, 0};
static int g_background_darkness = 50;      /* % */
static struct map_settings g_map = {1, 24};
static struct bow_settings g_bow = {1, 1, 0, 0};
static struct minimap_settings g_mini = {1, 0, 40, 200, 320, 0, 100};

struct floor_settings {
    int show, x, y, align, size;
    int beasts, chests, materials, fungus, mushrooms;
    unsigned colour, fungus_colour;
    int off_x[5], off_y[5];     /* each line moved on its own (FLOOR_LINES) */
    int background;
};
static struct floor_settings g_floor = {1, 1880, 200, 2, 80, 1, 1, 1, 1, 1, 0xFFFFFF, 0x50FF50, {0}, {0}, 0};

/* The pieces of a part that can be moved on their own, and their names in
 * lid-hud.ini. */
static const wchar_t *SLOT_KEYS[6] = {L"right1", L"right2", L"right3", L"left1", L"left2", L"left3"};
enum { LINE_BEASTS, LINE_MUSHROOMS, LINE_CHESTS, LINE_MATERIALS, LINE_FUNGUS, FLOOR_LINES };
static const wchar_t *LINE_KEYS[FLOOR_LINES] = {L"beasts", L"mushrooms", L"chests", L"materials", L"fungus"};

struct rage_settings {
    int show, gain, x, y, align, size;
    unsigned colour;
    int background;
};
static struct rage_settings g_rage = {1, 1, 60, 200, 0, 80, 0xFFFFFF, 0};

/* A part that is a few lines of text: shown or not, where, how big, its
 * colour. */
struct block_settings {
    int show, x, y, align, size;
    unsigned colour;
    int background;
};
static const struct block_settings BAG_HOME = {1, 1880, 380, 2, 80, 0xFFFFFF, 0};
static const struct block_settings EFFECTS_HOME = {1, 60, 240, 0, 75, 0xFFFFFF, 0};
static const struct block_settings RUN_HOME = {1, 60, 420, 0, 75, 0xFFFFFF, 0};

struct bag_settings {
    struct block_settings b;
    unsigned full_colour;
};
static struct bag_settings g_bag = {{1, 1880, 380, 2, 80, 0xFFFFFF, 0}, 0xFF3C3C};
static struct block_settings g_effects = {1, 60, 240, 0, 75, 0xFFFFFF, 0};

enum { RUN_TIME, RUN_KC, RUN_SP, RUN_BLOOD, RUN_EXP, RUN_DM, RUN_DEATHS, RUN_CONTINUES, RUN_LINES };
static const wchar_t *RUN_KEYS[RUN_LINES] = {L"time", L"kill_coins", L"sp", L"bloodnium", L"exp", L"death_metals",
                                             L"deaths", L"continues"};
struct run_settings {
    struct block_settings b;
    int lines[RUN_LINES];       /* each line on or off */
    int hub;                    /* the last trip, in the Waiting Room */
    int off_x[RUN_LINES], off_y[RUN_LINES];
};
static struct run_settings g_run = {{1, 60, 420, 0, 75, 0xFFFFFF, 0}, {1, 1, 1, 1, 1, 1, 1, 1}, 1, {0}, {0}};
static UINT g_layout_key = VK_F8;

/* How the layout mode's own pages look. */
struct menu_settings {
    int text_size, thick, panel;
};
static struct menu_settings g_menu = {100, 1, 1};

/* The layout mode: the part being moved, and when it last changed. */
enum { PART_DURABILITY, PART_ENEMIES, PART_FLOOR, PART_MINIMAP, PART_RAGE, PART_BAG, PART_EFFECTS, PART_RUN,
       PARTS };
static struct {
    int on, part;
    int item;                   /* 0 the whole part, or one piece of it (1 on) */
    int page, option;           /* page 1: the options, and the one picked */
    DWORD changed;
} g_layout;
#define LAYOUT_YELLOW 0xFFE63Cu
static wchar_t g_ini_path[MAX_PATH];
static FILETIME g_ini_time;
static ULONGLONG g_ini_checked;

static int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static int ini_int(const wchar_t *section, const wchar_t *key, int fallback, int lo, int hi)
{
    return clamp((int)GetPrivateProfileIntW(section, key, fallback, g_ini_path), lo, hi);
}

/* A colour in hex; "game" (where `game` allows it) for COLOUR_GAME. */
static unsigned ini_colour(const wchar_t *section, const wchar_t *key, unsigned fallback, int game)
{
    wchar_t text[16], *end;
    unsigned long v;

    GetPrivateProfileStringW(section, key, L"", text, 16, g_ini_path);
    if (!text[0])
        return fallback;
    if (lstrcmpiW(text, L"game") == 0)
        return game ? COLOUR_GAME : fallback;
    v = wcstoul(text[0] == L'#' ? text + 1 : text, &end, 16);
    return *end || v > 0xFFFFFF ? fallback : (unsigned)v;
}

static void read_block(const wchar_t *section, struct block_settings *b, const struct block_settings *home)
{
    b->show = ini_int(section, L"show", home->show, 0, 1);
    b->x = ini_int(section, L"x", home->x, 0, 1920);
    b->y = ini_int(section, L"y", home->y, 0, 1080);
    b->align = ini_int(section, L"align", home->align, 0, 2);
    b->size = ini_int(section, L"size", home->size, 40, 300);
    b->colour = ini_colour(section, L"colour", home->colour, 0);
    b->background = ini_int(section, L"background", 0, 0, 1);
}

static void read_settings(void)
{
    struct settings s;
    struct enemy_settings e;
    struct map_settings m;
    struct bow_settings b;
    struct minimap_settings mm;

    s.melee = ini_int(L"durability", L"melee", 1, 0, 1);
    s.guns = ini_int(L"durability", L"guns", 1, 0, 1);
    s.format = ini_int(L"durability", L"format", 0, 0, 2);
    s.position = ini_int(L"durability", L"position", 0, 0, 2);
    s.size = ini_int(L"durability", L"size", 100, 30, 300);
    s.x = ini_int(L"durability", L"x", 0, -300, 300);
    s.y = ini_int(L"durability", L"y", 0, -300, 300);
    s.white = ini_int(L"durability", L"white", 1, 0, 1);
    s.shadow = ini_int(L"durability", L"shadow", 1, 0, 1);
    s.menu = ini_int(L"durability", L"menu", 1, 0, 1);
    s.popup = ini_int(L"durability", L"popup", 1, 0, 1);
    s.loss = ini_int(L"durability", L"loss", 0, 0, 7);
    s.gain = ini_int(L"durability", L"gain", 4, 0, 7);
    s.hide_serial = ini_int(L"screen", L"hide_serial_killer", 0, 0, 1);
    s.colour = ini_colour(L"durability", L"colour", s.white ? 0xFFFFFF : COLOUR_GAME, 1);
    s.warn = ini_int(L"durability", L"warn", 0, 0, 100);
    s.warn_colour = ini_colour(L"durability", L"warn_colour", 0xFFE63C, 0);
    s.low = ini_int(L"durability", L"low", 25, 0, 100);
    s.low_colour = ini_colour(L"durability", L"low_colour", 0xFF3C3C, 0);
    s.flash = ini_int(L"durability", L"flash", 1, 0, 1);
    s.background = ini_int(L"durability", L"background", 0, 0, 1);
    g_background_darkness = ini_int(L"screen", L"background_darkness", 50, 10, 100);
    {
        wchar_t key[24];
        int i;
        for (i = 0; i < 6; i++) {
            swprintf(key, 24, L"%ls_x", SLOT_KEYS[i]);
            s.off_x[i] = ini_int(L"durability", key, 0, -1000, 1000);
            swprintf(key, 24, L"%ls_y", SLOT_KEYS[i]);
            s.off_y[i] = ini_int(L"durability", key, 0, -1000, 1000);
        }
    }
    g_set = s;

    e.show = ini_int(L"enemies", L"show", 1, 0, 1);
    e.total = ini_int(L"enemies", L"total", 1, 0, 1);
    e.x = ini_int(L"enemies", L"x", 1880, 0, 1920);
    e.y = ini_int(L"enemies", L"y", 160, 0, 1080);
    e.align = ini_int(L"enemies", L"align", 2, 0, 2);
    e.size = ini_int(L"enemies", L"size", 100, 40, 300);
    e.colour = ini_colour(L"enemies", L"colour", 0xFFFFFF, 0);
    e.background = ini_int(L"enemies", L"background", 0, 0, 1);
    g_enemies = e;

    m.escalators = ini_int(L"map", L"escalators", 1, 0, 1);
    m.escalator_size = ini_int(L"map", L"escalator_size", 24, 4, 120);
    g_map = m;

    b.arc = ini_int(L"bow", L"arc", 1, 0, 1);
    b.pawns = ini_int(L"bow", L"stop_at_enemies", 1, 0, 1);
    b.enemy_colour = ini_int(L"bow", L"enemy_colour", 0, 0, 5);
    b.headshot = ini_int(L"bow", L"headshot", 0, 0, 1);
    g_bow = b;

    mm.show = ini_int(L"minimap", L"show", 1, 0, 1);
    mm.corner = ini_int(L"minimap", L"corner", 0, 0, 5);
    mm.x = ini_int(L"minimap", L"x", 40, -1000, 2400);
    mm.y = ini_int(L"minimap", L"y", 200, -1500, 1500);
    mm.size = ini_int(L"minimap", L"size", 320, 50, 1000);
    mm.zoom = ini_int(L"minimap", L"zoom", 0, 0, 1);
    mm.zoom_percent = ini_int(L"minimap", L"zoom_percent", 100, 25, 400);
    g_mini = mm;

    {
        struct floor_settings f;
        f.show = ini_int(L"floor", L"show", 1, 0, 1);
        f.x = ini_int(L"floor", L"x", 1880, 0, 1920);
        f.y = ini_int(L"floor", L"y", 200, 0, 1080);
        f.align = ini_int(L"floor", L"align", 2, 0, 2);
        f.size = ini_int(L"floor", L"size", 80, 40, 300);
        f.beasts = ini_int(L"floor", L"beasts", 1, 0, 1);
        f.chests = ini_int(L"floor", L"chests", 1, 0, 1);
        f.materials = ini_int(L"floor", L"materials", 1, 0, 1);
        f.fungus = ini_int(L"floor", L"fun_fungus", 1, 0, 1);
        f.mushrooms = ini_int(L"floor", L"mushrooms", 1, 0, 1);
        f.colour = ini_colour(L"floor", L"colour", 0xFFFFFF, 0);
        f.fungus_colour = ini_colour(L"floor", L"fungus_colour", 0x50FF50, 0);
        f.background = ini_int(L"floor", L"background", 0, 0, 1);
        {
            wchar_t key[24];
            int i;
            for (i = 0; i < FLOOR_LINES; i++) {
                swprintf(key, 24, L"%ls_x", LINE_KEYS[i]);
                f.off_x[i] = ini_int(L"floor", key, 0, -1000, 1000);
                swprintf(key, 24, L"%ls_y", LINE_KEYS[i]);
                f.off_y[i] = ini_int(L"floor", key, 0, -1000, 1000);
            }
        }
        g_floor = f;
    }
    {
        struct rage_settings r;
        r.show = ini_int(L"rage", L"show", 1, 0, 1);
        r.gain = ini_int(L"rage", L"gain", 1, 0, 1);
        r.x = ini_int(L"rage", L"x", 60, 0, 1920);
        r.y = ini_int(L"rage", L"y", 200, 0, 1080);
        r.align = ini_int(L"rage", L"align", 0, 0, 2);
        r.size = ini_int(L"rage", L"size", 80, 40, 300);
        r.colour = ini_colour(L"rage", L"colour", 0xFFFFFF, 0);
        r.background = ini_int(L"rage", L"background", 0, 0, 1);
        g_rage = r;
    }
    read_block(L"bag", &g_bag.b, &BAG_HOME);
    g_bag.full_colour = ini_colour(L"bag", L"full_colour", 0xFF3C3C, 0);
    read_block(L"effects", &g_effects, &EFFECTS_HOME);
    read_block(L"run", &g_run.b, &RUN_HOME);
    g_run.hub = ini_int(L"run", L"hub", 1, 0, 1);
    {
        wchar_t key[32];
        int i;
        for (i = 0; i < RUN_LINES; i++) {
            g_run.lines[i] = ini_int(L"run", RUN_KEYS[i], 1, 0, 1);
            swprintf(key, 32, L"%ls_x", RUN_KEYS[i]);
            g_run.off_x[i] = ini_int(L"run", key, 0, -1000, 1000);
            swprintf(key, 32, L"%ls_y", RUN_KEYS[i]);
            g_run.off_y[i] = ini_int(L"run", key, 0, -1000, 1000);
        }
    }

    {
        wchar_t key[8];
        int n;
        GetPrivateProfileStringW(L"layout", L"key", L"F8", key, 8, g_ini_path);
        n = (key[0] == L'F' || key[0] == L'f') ? _wtoi(key + 1) : 0;
        g_layout_key = n >= 1 && n <= 12 ? (UINT)(VK_F1 + n - 1) : VK_F8;
    }
    {
        struct menu_settings mn;
        mn.text_size = ini_int(L"layout", L"text_size", 100, 80, 200);
        mn.thick = ini_int(L"layout", L"thick_text", 1, 0, 1);
        mn.panel = ini_int(L"layout", L"panel", 1, 0, 1);
        g_menu = mn;
    }
}

/* Read the file again when it has changed; checked at most once a second. */
static void watch_settings(void)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    ULONGLONG now = GetTickCount64();

    if (!g_ini_path[0] || now - g_ini_checked < 1000 || g_layout.on)
        return;
    g_ini_checked = now;
    if (!GetFileAttributesExW(g_ini_path, GetFileExInfoStandard, &a))
        memset(&a.ftLastWriteTime, 0, sizeof a.ftLastWriteTime);
    if (CompareFileTime(&a.ftLastWriteTime, &g_ini_time) == 0)
        return;
    g_ini_time = a.ftLastWriteTime;
    read_settings();
    g_settings_changed = 1;
    log_line("settings: durability melee %d, guns %d, format %d, position %d, size %d%%, x %d, y %d, "
             "colour %06X, shadow %d, menu %d, popup %d (loss %d, gain %d); enemies show %d, total %d, "
             "x %d, y %d, align %d, size %d%%, colour %06X; hide serial killer %d; map escalators %d "
             "(size %d); bow arc %d (stop at enemies %d, enemy colour %d, headshot %d); minimap %d (corner %d, "
             "x %d, y %d, size %d, zoom %d, %d%%)", g_set.melee,
             g_set.guns, g_set.format,
             g_set.position, g_set.size, g_set.x, g_set.y, g_set.colour, g_set.shadow, g_set.menu,
             g_set.popup, g_set.loss, g_set.gain, g_enemies.show, g_enemies.total, g_enemies.x,
             g_enemies.y, g_enemies.align, g_enemies.size, g_enemies.colour, g_set.hide_serial,
             g_map.escalators, g_map.escalator_size, g_bow.arc, g_bow.pawns, g_bow.enemy_colour,
             g_bow.headshot, g_mini.show, g_mini.corner, g_mini.x, g_mini.y, g_mini.size, g_mini.zoom,
             g_mini.zoom_percent);
}

/* ------------------------------------------------------------ helpers */

struct span {
    unsigned char *start;
    size_t size;
};

static int section(unsigned char *base, const char *name, struct span *out)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS64 *nt;
    IMAGE_SECTION_HEADER *sec;
    WORD i;

    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;
    nt = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return 0;
    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (strncmp((const char *)sec->Name, name, IMAGE_SIZEOF_SHORT_NAME) == 0) {
            out->start = base + sec->VirtualAddress;
            out->size = sec->Misc.VirtualSize;
            return 1;
        }
    }
    return 0;
}

static int inside(struct span s, const void *p, size_t n)
{
    const unsigned char *b = p;
    return b >= s.start && n <= s.size && b - s.start <= (ptrdiff_t)(s.size - n);
}

/* Memory that can be read: committed, not a guard page, not no-access. */
static int readable(const void *p, size_t n)
{
    MEMORY_BASIC_INFORMATION mbi;
    const unsigned char *a = p, *end = a + n;

    if ((ULONG_PTR)a < 0x10000 || n == 0 || end < a)
        return 0;
    while (a < end) {
        if (!VirtualQuery(a, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT || !mbi.Protect ||
            (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            return 0;
        a = (const unsigned char *)mbi.BaseAddress + mbi.RegionSize;
    }
    return 1;
}

static int32_t rd32(const unsigned char *p)
{
    int32_t v;
    memcpy(&v, p, 4);
    return v;
}

/* A byte pattern like "48 8B 0D ?? ?? ?? ?? E8", found exactly once in `s`. */
static unsigned char *find_once(struct span s, const char *pattern)
{
    unsigned char bytes[160], mask[160];
    int n = 0, first = -1, k;
    const char *c = pattern;
    unsigned char *p, *end, *hit = NULL;

    while (*c && n < (int)sizeof bytes) {
        while (*c == ' ')
            c++;
        if (!*c)
            break;
        if (c[0] == '?') {
            mask[n] = 0;
            bytes[n] = 0;
        } else {
            unsigned v = 0;
            sscanf(c, "%2x", &v);
            mask[n] = 1;
            bytes[n] = (unsigned char)v;
            if (first < 0)
                first = n;
        }
        n++;
        c += 2;
    }
    if (first < 0 || (size_t)n > s.size)
        return NULL;
    end = s.start + s.size - n;
    for (p = s.start + first; p <= end + first; p++) {
        p = memchr(p, bytes[first], (size_t)(end + first - p) + 1);
        if (!p)
            break;
        for (k = 0; k < n; k++)
            if (mask[k] && p[k - first] != bytes[k])
                break;
        if (k == n) {
            if (hit)
                return NULL;        /* twice: not the code we know */
            hit = p - first;
        }
    }
    return hit;
}

/* -------------------------------------------------------- the game's code */

/* The engine registers each native function with its name, in a table of
 * (name, function) pairs; "UBrgUIFontRenderexecDrawText_UseImgPos" leads to
 * the script-facing wrapper, and the wrapper calls the real function through
 * the object's function table. These are the slots of those calls. */
struct pieces {
    unsigned char *base;
    struct span text, rdata, data;
    LONG_PTR delta;                 /* where the exe is minus where it was built for */

    void **gworld;                  /* the engine's current world */
    void *fname, *find_function;    /* FName::FName, UObject::FindFunction */
    unsigned char *frame_call;      /* the main loop's GetForegroundWindow call */
    void **frame_import;            /* ... and its import slot */

    int vt_text, vt_shadow_text, vt_text_width, vt_gauge;   /* function-table slots */
    int vt_shadow_keep;             /* ... the shadow text in the font's own colour */
    int vt_status_render;           /* the player status panel's RenderProcess */
    int vt_esc_up, vt_esc_down;     /* the area map's escalator icons */
    int vt_sim_projectile;          /* the aiming line's simulation */
    void *line_check;               /* UWorld::SingleLineCheck */
    void *mem_alloc, *mem_free;     /* the engine's own appMalloc, appFree */
    void *find_object, *make_object;    /* StaticFindObject, StaticConstructObject */
    int vt_render_icon;             /* the area map's icon drawing */

    /* the HUD's weapon slot drawing: where it keeps the slot it is on */
    void *slot_fn;                  /* start of that function */
    int slot_no, slot_hand;         /* rbp offsets: slot number, hand (1 right, 2 left) */
    int slot_part, slot_master;     /* rbp offsets: the slot's part, its master data */
    int part_dur, part_lvl;         /* offsets in the part */
    int master_dur, master_dur_c;   /* offsets in the master data */
};

static struct pieces g_pc;

static unsigned char *native_named(struct pieces *pc, const char *name)
{
    size_t n = strlen(name) + 1;
    unsigned char *s = pc->rdata.start, *end = pc->rdata.start + pc->rdata.size;

    while (s && s + n <= end) {
        s = memchr(s, name[0], (size_t)(end - s));
        if (!s || s + n > end)
            break;
        if (memcmp(s, name, n) == 0 && (s == pc->rdata.start || s[-1] == 0)) {
            ULONG_PTR want = (ULONG_PTR)s - pc->delta;  /* as the table stores it */
            unsigned char *q = pc->data.start + ((8 - ((ULONG_PTR)pc->data.start & 7)) & 7);

            for (; q + 16 <= pc->data.start + pc->data.size; q += 8) {
                ULONG_PTR a, f;
                memcpy(&a, q, 8);
                if (a != want)
                    continue;
                memcpy(&f, q + 8, 8);
                f += pc->delta;
                if (inside(pc->text, (void *)f, 1))
                    return (unsigned char *)f;
            }
        }
        s++;
    }
    return NULL;
}

/* The function-table slot a native wrapper calls: its last `call [reg+N]`
 * before the end of the function. */
static int wrapper_slot(struct pieces *pc, const char *name)
{
    unsigned char *f = native_named(pc, name);
    int i, slot = 0;

    if (!f || !inside(pc->text, f, 0x402))
        return 0;
    for (i = 0; i < 0x400; i++) {
        if (f[i] == 0xC3) {
            if (f[i + 1] == 0xCC)
                break;
        } else if (f[i] == 0xFF && f[i + 1] != 0x94 && (f[i + 1] & 0xF8) == 0x90) {
            int32_t d = rd32(f + i + 2);
            if (d >= 0x100 && d < 0x2000)
                slot = d;
        }
    }
    return slot;
}

/* The function-table slot a native wrapper jumps to at its end, as a tail
 * call: `jmp [rax+N]` in its first bytes. */
static int tail_slot(struct pieces *pc, const char *name)
{
    unsigned char *f = native_named(pc, name);
    int i;

    if (!f || !inside(pc->text, f, 0x68))
        return 0;
    for (i = 0; i < 0x60; i++)
        if (f[i] == 0x48 && f[i + 1] == 0xFF && f[i + 2] == 0xA0) {
            int32_t d = rd32(f + i + 3);
            return d >= 0x100 && d < 0x2000 ? d : 0;
        }
    return 0;
}

/* The start of the function `p` is in, from the exe's own table of functions
 * (.pdata), so it works on a mapped copy of the exe too. */
static void *function_start(const struct pieces *pc, const void *p)
{
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(pc->base + ((IMAGE_DOS_HEADER *)pc->base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    RUNTIME_FUNCTION *table = (RUNTIME_FUNCTION *)(pc->base + dir->VirtualAddress), *rf = NULL;
    size_t lo = 0, hi = dir->Size / sizeof *table;
    DWORD rva;
    int hops;

    if ((const unsigned char *)p < pc->base || !dir->Size ||
        (ULONG_PTR)((const unsigned char *)p - pc->base) >= nt->OptionalHeader.SizeOfImage)
        return NULL;
    rva = (DWORD)((const unsigned char *)p - pc->base);
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (rva < table[mid].BeginAddress) {
            hi = mid;
        } else if (rva >= table[mid].EndAddress) {
            lo = mid + 1;
        } else {
            rf = &table[mid];
            break;
        }
    }
    if (!rf)
        return NULL;
    /* a function split in parts chains back to its first part */
    for (hops = 0; hops < 8; hops++) {
        unsigned char *unwind = pc->base + rf->UnwindData;
        if (!(unwind[0] & (UNW_FLAG_CHAININFO << 3)))
            break;
        rf = (RUNTIME_FUNCTION *)(unwind + 4 + ((unwind[2] + 1u) & ~1u) * 2);
    }
    return pc->base + rf->BeginAddress;
}

/* Find everything, without changing anything. `base` is the game's exe in
 * memory (the tests pass a mapped copy instead). */
static void find_pieces(unsigned char *base, struct pieces *pc)
{
    IMAGE_NT_HEADERS64 *nt;
    unsigned char *p, *a, *b, *c;

    memset(pc, 0, sizeof *pc);
    pc->base = base;
    if (!section(base, ".text", &pc->text) || !section(base, ".rdata", &pc->rdata) ||
        !section(base, ".data", &pc->data))
        return;
    nt = (IMAGE_NT_HEADERS64 *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    pc->delta = (LONG_PTR)base - (LONG_PTR)nt->OptionalHeader.ImageBase;

    p = find_once(pc->text, "48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 49 89 86 40 02 00 00 4C 89 B0 7C 02 00 00 49 8B");
    if (p)
        pc->gworld = (void **)(p + 7 + rd32(p + 3));
    pc->fname = find_once(pc->text, "40 53 48 83 EC 30 45 8B C8 45 33 C0 48 8B D9 C7 44 24 20 01 00 00 00 E8 ?? ?? ?? ??");
    pc->find_function = find_once(pc->text,
        "48 89 5C 24 18 48 89 6C 24 20 48 89 54 24 10 56 57 41 56 48 83 EC 30 48 8B 71 20 33");
    p = find_once(pc->text, "FF 15 ?? ?? ?? ?? 48 8B C8 48 8D 94 24 A8 00 00 00 FF 15 ?? ?? ?? ??");
    if (p) {
        pc->frame_call = p;
        pc->frame_import = (void **)(p + 6 + rd32(p + 2));
    }

    pc->vt_text = wrapper_slot(pc, "UBrgUIFontRenderexecDrawText_UseImgPos");
    pc->vt_shadow_text = wrapper_slot(pc, "UBrgUIFontRenderexecDrawShadowText_UseImgPos");
    pc->vt_shadow_keep = wrapper_slot(pc, "UBrgUIFontRenderexecDrawShadowText_UseImgPos_KeepColor");
    pc->vt_text_width = wrapper_slot(pc, "UBrgUIFontRenderexecGetDrawTextWidthWithFontState");
    pc->vt_gauge = wrapper_slot(pc, "ABrgHUDBaseexecDraw_GaugeLRImage");
    pc->vt_status_render = wrapper_slot(pc, "UBrgUIHUD_PlayerStatusexecRenderProcess");
    pc->vt_esc_up = wrapper_slot(pc, "UBrgUIMenuPart_PauseMapexecRenderIconEscalatorUp");
    pc->vt_esc_down = wrapper_slot(pc, "UBrgUIMenuPart_PauseMapexecRenderIconEscalatorDown");
    pc->vt_sim_projectile = tail_slot(pc, "UBrgPredictedProjectileTrajectoryexecSimProjectile");
    pc->line_check = find_once(pc->text,
        "48 8B C4 55 48 8B EC 48 83 EC 70 48 C7 45 D0 FE FF FF FF 48 89 58 08 48 89 70 10 48");
    pc->mem_alloc = find_once(pc->text,
        "48 89 5C 24 08 57 48 83 EC 20 8B F9 48 8B 0D ?? ?? ?? ?? 8B DA 48 85 C9 75 0C E8 ?? ?? ?? ?? "
        "48 8B 0D ?? ?? ?? ?? 48 8B 01 44 8B C3 8B D7 48 8B 5C 24 30 48 83 C4 20 5F 48 FF 60 10");
    pc->find_object = find_once(pc->text,
        "40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 80 00 00 00 48 C7 44 24 50 FE FF FF FF 48 89 9C 24 C0 00 00 00");
    pc->make_object = find_once(pc->text,
        "48 89 5C 24 08 4C 89 44 24 18 57 48 83 EC 40 49 8B D9 48 8B F9 48 83 FA FF 75 0D");
    pc->vt_render_icon = wrapper_slot(pc, "UBrgUIMiniMapRenderexecRenderIcon");
    pc->mem_free = find_once(pc->text,
        "40 53 48 83 EC 20 48 8B D9 48 8B 0D ?? ?? ?? ?? 48 85 C9 75 0C E8 ?? ?? ?? ?? "
        "48 8B 0D ?? ?? ?? ?? 48 8B 01 48 8B D3 48 83 C4 20 5B 48 FF 60 20");

    /* The weapon slot drawing: its loop over the slots, where it sets up a
     * slot's part and master data, and where it reads their durability. All
     * three must be in the same function. */
    a = find_once(pc->text, "8B 5D ?? FF C3 89 5D ?? 48 FF 45 ?? 83 FB 03 F3 0F 10 75 ?? F3 0F 10 7D ?? 48 8B 7D ?? 44 0F B6 A5 ?? ?? ?? ??");
    b = find_once(pc->text, "48 8D B5 ?? ?? ?? ?? 48 8D BD ?? ?? ?? ?? 8B 85");
    c = find_once(pc->text, "44 8B 8F ?? ?? ?? ?? 44 8B 47 ?? 8B 56 ?? 48 8B 89 ?? ?? ?? ?? E8 ?? ?? ?? ?? 66 44 0F 6E 4E ??");
    if (a && b && c) {
        void *fa = function_start(pc, a);
        if (fa && fa == function_start(pc, b) && fa == function_start(pc, c)) {
            pc->slot_fn = fa;
            pc->slot_no = (signed char)a[2];
            pc->slot_hand = rd32(a + 0x21);
            pc->slot_part = rd32(b + 3);
            pc->slot_master = rd32(b + 10);
            pc->master_dur_c = rd32(c + 3);
            pc->master_dur = (signed char)c[10];
            pc->part_lvl = (signed char)c[13];
            pc->part_dur = (signed char)c[0x1f];
        }
    }
}

static int engine_ok(const struct pieces *pc)
{
    return pc->gworld && pc->fname && pc->find_function && pc->frame_import;
}

static int durability_ok(const struct pieces *pc)
{
    return pc->vt_text && pc->vt_text_width && pc->vt_gauge && pc->slot_fn;
}

static int enemies_ok(const struct pieces *pc)
{
    return pc->vt_status_render != 0;
}

static int map_ok(const struct pieces *pc)
{
    return pc->vt_esc_up && pc->vt_esc_down;
}

static int arc_ok(const struct pieces *pc)
{
    return pc->vt_sim_projectile && pc->line_check && pc->mem_alloc && pc->mem_free;
}

static int minimap_ok(const struct pieces *pc)
{
    return pc->find_object && pc->make_object && pc->vt_render_icon && pc->vt_status_render;
}

/* ------------------------------------------------- the engine's objects */

/* Unreal Engine 3 object layout in this game (64-bit). */
#define OBJ_NAME        0x48    /* UObject: name (FName index) */
#define OBJ_CLASS       0x50    /* UObject: class */
#define FIELD_NEXT      0x60    /* UField: next field of the same struct */
#define PROP_DIM        0x68    /* UProperty: array size */
#define PROP_SIZE       0x6C    /* UProperty: size of one element */
#define STRUCT_SUPER    0x78    /* UStruct: parent */
#define STRUCT_FIELDS   0x80    /* UStruct: first field */
#define STRUCT_SIZE     0x88    /* UStruct: size of its properties */
#define PROP_OFFSET     0x94    /* UProperty: offset in the object */
#define PROP_INNER      0xB0    /* UStructProperty: its struct */
#define FUNC_PARMS      0xE2    /* UFunction: size of its parameters */
#define VT_PROCESS_EVENT 0x218  /* UObject function table: ProcessEvent */

typedef void (*fname_fn)(uint64_t *self, const wchar_t *name, int find);
typedef void *(*find_function_fn)(void *obj, uint64_t name, int global);
typedef void (*process_event_fn)(void *obj, void *func, void *parms, void *result);

static uint32_t name_index(const wchar_t *name)
{
    static struct { const wchar_t *name; uint32_t index; } cache[160];
    static int n;
    uint64_t out = 0;
    int i;

    for (i = 0; i < n; i++)
        if (cache[i].name == name)
            return cache[i].index;
    ((fname_fn)g_pc.fname)(&out, name, 0);  /* find only: never adds a name */
    if (out && n < (int)(sizeof cache / sizeof cache[0])) {
        cache[n].name = name;
        cache[n].index = (uint32_t)out;
        n++;
    }
    return (uint32_t)out;
}

/* A field of a struct or class (or of its parents), by name. */
static unsigned char *field_of(unsigned char *strct, const wchar_t *name)
{
    static struct { unsigned char *strct; uint32_t name; unsigned char *field; } cache[512];
    uint32_t idx, h, i;
    unsigned char *s, *f;

    if (!strct || !(idx = name_index(name)))
        return NULL;
    h = ((uint32_t)((ULONG_PTR)strct >> 4) * 433u) ^ idx;
    for (i = 0; i < 8; i++) {
        uint32_t k = (h + i) & 511;
        if (cache[k].strct == strct && cache[k].name == idx)
            return cache[k].field;
        if (!cache[k].strct)
            break;
    }
    f = NULL;
    for (s = strct; s && readable(s, 0x100); s = *(unsigned char **)(s + STRUCT_SUPER)) {
        for (f = *(unsigned char **)(s + STRUCT_FIELDS); f && readable(f, 0xB8);
             f = *(unsigned char **)(f + FIELD_NEXT))
            if (*(uint32_t *)(f + OBJ_NAME) == idx)
                goto found;
        f = NULL;
    }
found:
    for (i = 0; i < 8; i++) {
        uint32_t k = (h + i) & 511;
        if (!cache[k].strct || i == 7) {
            cache[k].strct = strct;
            cache[k].name = idx;
            cache[k].field = f;
            break;
        }
    }
    return f;
}

static unsigned char *class_of(void *obj)
{
    return obj && readable(obj, 0x60) ? *(unsigned char **)((unsigned char *)obj + OBJ_CLASS) : NULL;
}

static uint32_t offset_of(unsigned char *field)
{
    return *(uint32_t *)(field + PROP_OFFSET);
}

/* Where a property of an object is, or NULL. */
static void *value_at(void *obj, const wchar_t *name, size_t size)
{
    unsigned char *f = field_of(class_of(obj), name), *p;

    if (!f)
        return NULL;
    p = (unsigned char *)obj + offset_of(f);
    return readable(p, size) ? p : NULL;
}

/* An object property's value: another object, or NULL. */
static void *object_at(void *obj, const wchar_t *name)
{
    void **p = value_at(obj, name, 8);
    void *o = p ? *p : NULL;
    return o && readable(o, 0x60) ? o : NULL;
}

/* Call a script function of `obj` with `in` as its parameters; `out` gets them
 * back after the call. 0 on success. */
static int call_function(void *obj, const wchar_t *name, const void *in, unsigned in_size,
                         void *out, unsigned out_size)
{
    unsigned char parms[0x400];
    uint32_t idx = name_index(name);
    unsigned char *fn;
    unsigned size;

    if (!idx || !obj)
        return -1;
    fn = ((find_function_fn)g_pc.find_function)(obj, idx, 0);
    if (!fn || !readable(fn, 0x100))
        return -1;
    size = *(uint16_t *)(fn + FUNC_PARMS);
    if (*(uint16_t *)(fn + STRUCT_SIZE) > size)
        size = *(uint16_t *)(fn + STRUCT_SIZE);
    if (in_size > *(uint16_t *)(fn + FUNC_PARMS) || size > sizeof parms)
        return -1;
    memset(parms, 0, size);
    if (in && in_size)
        memcpy(parms, in, in_size);
    (*(process_event_fn *)(*(unsigned char **)obj + VT_PROCESS_EVENT))(obj, fn, parms, NULL);
    if (out && out_size)
        memcpy(out, parms, out_size < size ? out_size : size);
    return 0;
}

/* ----------------------------------------------------------- durability */

/* What the frame hook found this frame. */
static void *g_game;            /* the game (GameInfo) */
static void *g_ui;              /* the UI manager */
static void *g_status;          /* its player status */
static void *g_font;            /* its font renderer */
static void *g_pawn;            /* the player's character, native side */
static void *g_fighter;         /* the pause menu's fighter panel */
static void *g_hud;             /* the HUD (its images draw the game's own HUD) */

/* The game lays its HUD out on a 3840 x 2160 screen and scales that to the
 * real one. The settings are in 1920 x 1080 pixels, so: twice that. */
#define UI_SCALE 2

/* A weapon's maximum durability, by the game's own CalcPartParam(lvl, base_p,
 * const_p) on its database. 0 if it cannot be worked out. */
static int max_durability(int lvl, int base_p, int const_p)
{
    static struct { int lvl, base_p, const_p, max; } seen[16];
    static int n, next;
    unsigned char parms[64], *fn, *f_lvl, *f_base, *f_const, *f_ret;
    void *db;
    uint32_t idx, in_size;
    int i, max;

    for (i = 0; i < n; i++)
        if (seen[i].lvl == lvl && seen[i].base_p == base_p && seen[i].const_p == const_p)
            return seen[i].max;
    db = object_at(object_at(g_ui, L"mGameInfo"), L"mDatabase");
    idx = name_index(L"CalcPartParam");
    if (!db || !idx)
        return 0;
    fn = ((find_function_fn)g_pc.find_function)(db, idx, 0);
    if (!fn || !readable(fn, 0x100))
        return 0;
    f_lvl = field_of(fn, L"lvl");
    f_base = field_of(fn, L"base_p");
    f_const = field_of(fn, L"const_p");
    f_ret = field_of(fn, L"ReturnValue");
    if (!f_lvl || !f_base || !f_const || !f_ret || offset_of(f_lvl) > 60 || offset_of(f_base) > 60 ||
        offset_of(f_const) > 60 || offset_of(f_ret) > 60)
        return 0;
    memset(parms, 0, sizeof parms);
    memcpy(parms + offset_of(f_lvl), &lvl, 4);
    memcpy(parms + offset_of(f_base), &base_p, 4);
    memcpy(parms + offset_of(f_const), &const_p, 4);
    in_size = offset_of(f_lvl);
    if (offset_of(f_base) > in_size)
        in_size = offset_of(f_base);
    if (offset_of(f_const) > in_size)
        in_size = offset_of(f_const);
    if (call_function(db, L"CalcPartParam", parms, in_size + 4, parms, sizeof parms) != 0)
        return 0;
    memcpy(&max, parms + offset_of(f_ret), 4);
    i = n < 16 ? n++ : next++ % 16;
    seen[i].lvl = lvl;
    seen[i].base_p = base_p;
    seen[i].const_p = const_p;
    seen[i].max = max;
    return max;
}

/* Where the character keeps what each hand holds: mEquipPartInfo, an array of
 * structs, and the offsets of the numbers in one of them. */
static struct {
    int state;                  /* 0 not looked yet, 1 found, -1 not as expected */
    uint32_t offset, stride, count;
    uint32_t enable_offset, enable_bit;
    uint32_t dur, lvl;          /* in the part the player owns */
    uint32_t base_p, const_p, capacity;     /* in its master data */
    uint32_t master_capacity;   /* capacity's offset in a master data on its own */
    uint32_t eptid;             /* the part's id (a string), 0 if not found */
    uint32_t part_dur, part_eptid;  /* mDur and mEptid's offsets in a part on its own */
} g_equip;

static int bool_bit(unsigned char *field)
{
    int i;
    for (i = PROP_OFFSET + 4; i < 0xD5; i += 4) {
        uint32_t m = *(uint32_t *)(field + i);
        if (m && !(m & (m - 1)))
            return (int)m;
    }
    return 0;
}

static int int_field(unsigned char *strct, const wchar_t *name, uint32_t *out)
{
    unsigned char *f = field_of(strct, name);
    if (!f || *(uint32_t *)(f + PROP_SIZE) != 4)
        return 0;
    *out = offset_of(f);
    return 1;
}

static int struct_field(unsigned char *strct, const wchar_t *name, uint32_t *offset, unsigned char **inner)
{
    unsigned char *f = field_of(strct, name);
    if (!f || !(*inner = *(unsigned char **)(f + PROP_INNER)) || !readable(*inner, 0x100))
        return 0;
    *offset = offset_of(f);
    return 1;
}

static void learn_equip_layout(void *pawn)
{
    unsigned char *info = field_of(class_of(pawn), L"mEquipPartInfo"), *item, *part, *auto_info, *master, *f;
    uint32_t part_at, auto_at, master_at;

    g_equip.state = -1;
    if (!info || !(item = *(unsigned char **)(info + PROP_INNER)) || !readable(item, 0x100))
        goto bad;
    g_equip.offset = offset_of(info);
    g_equip.stride = *(uint32_t *)(info + PROP_SIZE);
    g_equip.count = *(uint32_t *)(info + PROP_DIM);
    if (!(f = field_of(item, L"mEnable")) || !(g_equip.enable_bit = (uint32_t)bool_bit(f)))
        goto bad;
    g_equip.enable_offset = offset_of(f);
    if (!struct_field(item, L"mDbPsPart", &part_at, &part) ||
        !struct_field(item, L"mDbPsPartAutoInfo", &auto_at, &auto_info) ||
        !struct_field(auto_info, L"mDbPart", &master_at, &master))
        goto bad;
    if (!int_field(part, L"mDur", &g_equip.dur) || !int_field(part, L"mLvl", &g_equip.lvl) ||
        !int_field(master, L"mDur", &g_equip.base_p) || !int_field(master, L"mDur_c", &g_equip.const_p) ||
        !int_field(master, L"mCapacity", &g_equip.capacity))
        goto bad;
    g_equip.master_capacity = g_equip.capacity;
    if ((f = field_of(part, L"mEptid")) && *(uint32_t *)(f + PROP_SIZE) == 16) {
        g_equip.eptid = part_at + offset_of(f);
        g_equip.part_eptid = offset_of(f);
    }
    g_equip.part_dur = g_equip.dur;
    g_equip.dur += part_at;
    g_equip.lvl += part_at;
    g_equip.base_p += auto_at + master_at;
    g_equip.const_p += auto_at + master_at;
    g_equip.capacity += auto_at + master_at;
    if (g_equip.count < 3 || !g_equip.stride)
        goto bad;
    g_equip.state = 1;
    return;
bad:
    log_line("durability: the equipment data is not laid out as expected - numbers stay off");
}

/* What hand 1 (right) or 2 (left) holds: durability now and at most, and
 * whether it is a gun. 1 if known. */
static int held_durability(unsigned hand, int *cur, int *max, int *gun)
{
    unsigned char *e;
    int dur, top;

    if (!g_pawn)
        return 0;
    if (!g_equip.state)
        learn_equip_layout(g_pawn);
    if (g_equip.state != 1 || hand >= g_equip.count)
        return 0;
    e = (unsigned char *)g_pawn + g_equip.offset + hand * g_equip.stride;
    if (!readable(e, g_equip.stride) || !(*(uint32_t *)(e + g_equip.enable_offset) & g_equip.enable_bit))
        return 0;
    dur = *(int *)(e + g_equip.dur);
    top = max_durability(*(int *)(e + g_equip.lvl), *(int *)(e + g_equip.base_p), *(int *)(e + g_equip.const_p));
    if (top <= 0)
        return 0;
    *cur = dur < 0 ? 0 : dur > top ? top : dur;
    *max = top;
    *gun = *(int *)(e + g_equip.capacity) > 0;
    return 1;
}

/* A part id (the string at `at`) as a number. 0 if unknown. */
static uint32_t part_id_at(const unsigned char *at)
{
    struct { const wchar_t *data; int32_t count, max; } id;
    uint32_t h = 0x811C9DC5;
    int i;

    if (!readable(at, sizeof id))
        return 0;
    memcpy(&id, at, sizeof id);
    if (!id.data || id.count < 1 || id.count > 64 || !readable(id.data, (size_t)id.count * 2))
        return 0;
    for (i = 0; i < id.count && id.data[i]; i++)
        h = (h ^ (uint16_t)id.data[i]) * 0x01000193;
    return h ? h : 1;
}

/* What hand 1 or 2 holds, as a number: a hash of its part id. 0 if unknown. */
static uint32_t held_id(unsigned hand)
{
    if (g_equip.state != 1 || !g_equip.eptid || hand >= g_equip.count)
        return 0;
    return part_id_at((unsigned char *)g_pawn + g_equip.offset + hand * g_equip.stride + g_equip.eptid);
}

/* --------------------------------------------------------- drawing text */

struct fstring {
    const wchar_t *data;
    int32_t count, max;
};

typedef void (*draw_text_fn)(void *font, void *state, void *image, struct fstring *text, int unused,
                             float x, float y, int a, int b);
typedef void (*text_width_fn)(void *font, void *state, struct fstring *text, float *w, float *h);

/* The pop-ups' colours: red, orange, yellow, white, green, cyan, blue, magenta. */
static const unsigned char COLOURS[8][3] = {
    {255, 60, 60}, {255, 150, 40}, {255, 230, 60}, {255, 255, 255},
    {80, 255, 80}, {60, 230, 255}, {90, 140, 255}, {255, 90, 255},
};

/* Draw `str` against `image`, at (x, y) from it. `own_colour`: in the colour
 * just put in the font renderer - DrawShadowText_UseImgPos puts the image's own
 * colour back first, its KeepColor twin does not; there is no KeepColor text
 * without a shadow, so for that the shadow is made see-through. */
static void font_draw(void *state, void *image, struct fstring *str, float x, float y, int own_colour, int shadow)
{
    unsigned char *shadow_a = value_at(g_font, L"mShadowColorA", 1), keep_a = 0;
    int slot;

    if (own_colour && g_pc.vt_shadow_keep && (shadow || shadow_a)) {
        if (!shadow) {
            keep_a = *shadow_a;
            *shadow_a = 0;
        }
        ((draw_text_fn)(*(void ***)g_font)[g_pc.vt_shadow_keep / 8])(g_font, state, image, str, 0, x, y, 0, 0);
        if (!shadow)
            *shadow_a = keep_a;
        return;
    }
    slot = shadow && g_pc.vt_shadow_text ? g_pc.vt_shadow_text : g_pc.vt_text;
    ((draw_text_fn)(*(void ***)g_font)[slot / 8])(g_font, state, image, str, 0, x, y, 0, 0);
}

/* Write `text` on the bar `image` that the game just drew at (x, y): in the
 * settings' colour (colour -1) or one of COLOURS, `alpha` opaque; a pop-up
 * goes under the bar (and under the numbers, if they are below it too). */
static uint32_t text_state(unsigned char *state, int size_percent);
static int g_state_for_bars;
static void draw_box(float x1, float y1, float x2, float y2, unsigned colour_rgb, int alpha);

/* Low durability: the colour the number being drawn takes instead of its own
 * (g_bar_low set), and when a flashing one was last drawn. */
static int g_bar_low;
static unsigned g_bar_rgb;
static DWORD g_flashing_at;

static void draw_on_bar(void *image, float x, float y, const wchar_t *text, int colour_index,
                        unsigned char alpha, int popup, int item)
{
    static wchar_t buf[48];
    unsigned char state[256];
    unsigned char *colour[4], keep[4];
    struct fstring s;
    float w = 0, h = 0, tx, ty;
    int *pos_x, *pos_y, *use_w, *use_h, i, own = 1;

    g_state_for_bars = 1;
    i = g_status && g_font && text_state(state, g_set.size);
    g_state_for_bars = 0;
    if (!i)
        return;

    lstrcpynW(buf, text, 48);
    s.data = buf;
    s.count = s.max = lstrlenW(buf) + 1;
    ((text_width_fn)(*(void ***)g_font)[g_pc.vt_text_width / 8])(g_font, state, &s, &w, &h);
    if (h <= 0 || h >= 400)
        h = 16;
    if (w <= 0 || w >= 2000)
        w = 0;

    pos_x = value_at(image, L"mPosX", 4);
    pos_y = value_at(image, L"mPosY", 4);
    use_w = value_at(image, L"mUseSizeX", 4);
    use_h = value_at(image, L"mUseSizeY", 4);
    if (!pos_x || !pos_y || !use_w || !use_h)
        return;
    tx = (*use_w - w) * 0.5f + x;
    if (popup)
        ty = *use_h + y + 2 + (g_set.position == 2 ? h + 1 : 0);
    else if (g_set.position == 1)
        ty = y - h - 2;                         /* above the bar */
    else if (g_set.position == 2)
        ty = *use_h + y + 2;                    /* below it */
    else
        ty = (*use_h - h) * 0.5f + y;           /* on it */
    /* the text is drawn relative to the image, which the game adds itself */
    tx += g_set.x * UI_SCALE - *pos_x;
    ty += g_set.y * UI_SCALE - *pos_y;
    if (item >= 0) {                            /* the slot moved on its own */
        tx += g_set.off_x[item] * UI_SCALE;
        ty += g_set.off_y[item] * UI_SCALE;
    }

    colour[0] = value_at(g_font, L"mColorR", 1);
    colour[1] = value_at(g_font, L"mColorG", 1);
    colour[2] = value_at(g_font, L"mColorB", 1);
    colour[3] = value_at(g_font, L"mColorA", 1);
    for (i = 0; i < 4; i++)
        if (!colour[i])
            return;
    for (i = 0; i < 4; i++)
        keep[i] = *colour[i];
    if (colour_index >= 0) {
        *colour[0] = COLOURS[colour_index][0];
        *colour[1] = COLOURS[colour_index][1];
        *colour[2] = COLOURS[colour_index][2];
    } else if (g_layout.on && !g_layout.page && g_layout.part == PART_DURABILITY &&
               (g_layout.item == 0 || g_layout.item == item + 1)) {
        *colour[0] = 255;                       /* the part being moved */
        *colour[1] = 230;
        *colour[2] = 60;
    } else if (g_bar_low && !popup) {
        *colour[0] = (unsigned char)(g_bar_rgb >> 16);     /* low durability */
        *colour[1] = (unsigned char)(g_bar_rgb >> 8);
        *colour[2] = (unsigned char)g_bar_rgb;
    } else if (g_set.colour != COLOUR_GAME) {
        *colour[0] = (unsigned char)(g_set.colour >> 16);
        *colour[1] = (unsigned char)(g_set.colour >> 8);
        *colour[2] = (unsigned char)g_set.colour;
    } else {
        own = 0;                                /* the slot's own colour */
    }
    *colour[3] = alpha;
    if (g_set.background)                       /* a box behind it, where the text goes on the screen */
        draw_box((tx + *pos_x) / UI_SCALE - 3, (ty + *pos_y) / UI_SCALE - 1, (tx + *pos_x + w) / UI_SCALE + 3,
                 (ty + *pos_y + h) / UI_SCALE + 1, 0x000000, g_background_darkness * 255 / 100 * alpha / 255);
    font_draw(state, image, &s, tx, ty, own, g_set.shadow);
    for (i = 0; i < 4; i++)
        *colour[i] = keep[i];
}

/* ------------------------------------------------------ the gauge hook */

typedef void (*gauge_fn)(void *hud, float x, float y, float ratio, void *image);

static struct { void **table; gauge_fn original; } g_hooked[4];
static int g_nhooked;

/* Set by the stub below on the way in: the caller's frame and stack. Only the
 * stub writes them, so they are volatile: the compiler must read them. */
static unsigned char *volatile g_gauge_rbp __attribute__((used));
static void **volatile g_gauge_rsp __attribute__((used));

static void gauge_hook(void *hud, float x, float y, float ratio, void *image) __attribute__((used));

/* The stub saves the caller's rbp and rsp, then goes on to gauge_hook with
 * every argument untouched. */
__asm__(
    ".text\n"
    ".p2align 4\n"
    "lid_gauge_stub:\n"
    "    movq %rbp, g_gauge_rbp(%rip)\n"
    "    movq %rsp, g_gauge_rsp(%rip)\n"
    "    jmp gauge_hook\n");
void lid_gauge_stub(void);

/* gauge_hook is only called from the stub; taking its address here keeps the
 * compiler from giving it a calling convention of its own. */
static void *volatile g_gauge_hook_address __attribute__((used)) = (void *)gauge_hook;

static gauge_fn original_gauge(void *hud)
{
    void **table = hud ? *(void ***)hud : NULL;
    int i;
    for (i = 0; i < g_nhooked; i++)
        if (g_hooked[i].table == table)
            return g_hooked[i].original;
    return g_nhooked ? g_hooked[0].original : NULL;
}

/* Was the gauge asked for by the HUD's weapon slot drawing? Its return
 * address is somewhere just up the stack. */
static int from_weapon_slot(void **sp)
{
    static struct { void *ret; int yes; } seen[32];
    static int n;
    int i, k;

    if (!readable(sp, 32 * 8))
        return 0;
    for (i = 0; i < 32; i++) {
        void *ret = sp[i];
        if (!inside(g_pc.text, ret, 1))
            continue;
        for (k = 0; k < n; k++)
            if (seen[k].ret == ret)
                break;
        if (k == n) {
            if (n == 32)
                continue;
            seen[n].ret = ret;
            seen[n].yes = function_start(&g_pc, ret) == g_pc.slot_fn;
            n++;
        }
        if (seen[k].yes)
            return 1;
    }
    return 0;
}

static int matches(int cur, int max, float ratio);

/* Durability changes, for the pop-ups: for each hand, what it held last frame
 * and how much it changed. Changes close together add up; the total shows for
 * 1.8 seconds once 0.3 seconds pass without another change (or 1.5 seconds
 * after the first), fading out over the last 0.6. */
static struct {
    uint32_t weapon;
    int cur, max, pending, shown;
    DWORD first, last, shown_at;
} g_pop[3];
static int g_pop_active;

static void request_update(void)
{
    if (g_status)
        call_function(g_status, L"UpdateRequest", NULL, 0, NULL, 0);
}

static void track_changes(void)
{
    DWORD now = GetTickCount();
    unsigned hand;
    int any = 0;

    if (!g_set.popup || !g_pawn) {
        memset(g_pop, 0, sizeof g_pop);
        g_pop_active = 0;
        return;
    }
    for (hand = 1; hand <= 2; hand++) {
        int cur = 0, max = 0, gun;
        uint32_t id = held_durability(hand, &cur, &max, &gun) ? held_id(hand) : 0;

        if (id && id == g_pop[hand].weapon && max == g_pop[hand].max) {
            int delta = cur - g_pop[hand].cur;
            if (delta) {
                if (!g_pop[hand].pending)
                    g_pop[hand].first = now;
                g_pop[hand].pending += delta;
                g_pop[hand].last = now;
            }
        } else if (id != g_pop[hand].weapon) {
            g_pop[hand].pending = 0;    /* another weapon: nothing to show */
            g_pop[hand].shown = 0;
        }
        g_pop[hand].weapon = id;
        g_pop[hand].cur = cur;
        g_pop[hand].max = max;
        if (g_pop[hand].pending && (now - g_pop[hand].last > 299 || now - g_pop[hand].first > 1499)) {
            g_pop[hand].shown = g_pop[hand].pending;
            g_pop[hand].shown_at = now;
            g_pop[hand].pending = 0;
        }
        if (g_pop[hand].shown && now - g_pop[hand].shown_at < 1800)
            any = 1;
    }
    /* the panel only redraws when asked: ask while a pop-up shows, and once
     * after, to take it away */
    if (any || g_pop_active)
        request_update();
    g_pop_active = any;
}

static void draw_popup(unsigned hand, void *image, float x, float y, int item)
{
    DWORD t = GetTickCount() - g_pop[hand].shown_at;
    wchar_t text[16];
    unsigned char alpha = 255;

    if (!g_set.popup || !g_pop[hand].shown || t >= 1800)
        return;
    if (t > 1200)
        alpha = (unsigned char)(255 - (t - 1200) * 255 / 600);
    swprintf(text, 16, g_pop[hand].shown > 0 ? L"+%d" : L"%d", g_pop[hand].shown);
    draw_on_bar(image, x, y, text, g_pop[hand].shown < 0 ? g_set.loss : g_set.gain, alpha, 1, item);
}

/* The number in the chosen format. */
static void format_durability(wchar_t *text, int size, int cur, int max)
{
    if (g_set.format == 1)
        swprintf(text, (size_t)size, L"%d", cur);
    else if (g_set.format == 2)
        swprintf(text, (size_t)size, L"%d%%", cur * 100 / max);
    else
        swprintf(text, (size_t)size, L"%d/%d", cur, max);
}

/* Which of the fighter panel's durability bars `image` is, or -1. Each part
 * is drawn once a frame (g_menu_drawn). */
static unsigned g_menu_drawn;

static int menu_gauge(void *image)
{
    static const wchar_t *names[2] = {L"mEquipDurGauge00Image", L"mEquipDurGauge01Image"};
    int n, i, k;

    for (k = 0; k < 2; k++) {
        unsigned char *f = field_of(class_of(g_fighter), names[k]);
        void **images;
        if (!f || !(n = (int)*(uint32_t *)(f + PROP_DIM)))
            continue;
        if (n > 8)
            n = 8;
        images = (void **)((unsigned char *)g_fighter + offset_of(f));
        if (!readable(images, (size_t)n * 8))
            continue;
        for (i = 0; i < n; i++)
            if (images[i] == image)
                return i;
    }
    return -1;
}

static void label_menu_gauge(void *image, float x, float y, float ratio)
{
    int index, cur, max, gun;
    wchar_t text[32];

    if (!g_set.menu || !g_fighter || (index = menu_gauge(image)) < 0 || (g_menu_drawn >> index & 1))
        return;
    g_menu_drawn |= 1u << index;
    if (held_durability((unsigned)index, &cur, &max, &gun) && matches(cur, max, ratio))
        format_durability(text, 32, cur, max);
    else
        swprintf(text, 32, L"%d%%", (int)(ratio * 1000 + 0.5f) * 100 / 1000);
    draw_on_bar(image, x, y, text, -1, 255, 0, -1);
}

static int matches(int cur, int max, float ratio)
{
    float d;
    if (max < 1 || cur < 0 || cur > max)
        return 0;
    d = (float)cur / (float)max - ratio;
    return (d < 0 ? -d : d) < 0.002f;
}

static void gauge_hook(void *hud, float x, float y, float ratio, void *image)
{
    unsigned char *rbp = g_gauge_rbp, *part, *master;
    gauge_fn original = original_gauge(hud);
    int slot, *equipped, live_cur, live_max, live_gun, live, frame_cur, frame_max, frame_gun;
    int cur, max, gun, from_hand, item;
    unsigned hand;
    wchar_t text[32];
    static int seen_slots, logged_slots;

    if (original)
        original(hud, x, y, ratio, image);
    if (ratio <= 0 || !image || !g_status || !g_pawn)
        return;
    if (!from_weapon_slot(g_gauge_rsp)) {
        label_menu_gauge(image, x, y, ratio);
        return;
    }
    if (!readable(rbp + g_pc.slot_no, 4) || !readable(rbp + g_pc.slot_hand, 1))
        return;
    slot = *(int *)(rbp + g_pc.slot_no);
    hand = *(unsigned char *)(rbp + g_pc.slot_hand);
    if (hand != 1 && hand != 2)
        return;
    /* the hand's slots are 0 to 2 (three on the HUD); written to the log the
     * first time each one turns up */
    item = (int)(hand - 1) * 3 + (slot < 0 ? 0 : slot > 2 ? 2 : slot);
    if (slot >= 0 && slot < 8 && !(seen_slots >> ((hand - 1) * 8 + slot) & 1) && logged_slots < 12) {
        seen_slots |= 1 << ((hand - 1) * 8 + slot);
        logged_slots++;
        log_line("durability: a bar for the %s hand's slot %d", hand == 1 ? "right" : "left", slot);
    }

    /* two ways to know the slot's durability: what the hand holds now, and
     * what the slot drawing has for the slot it is on */
    live = held_durability(hand, &live_cur, &live_max, &live_gun);
    part = rbp + g_pc.slot_part;
    master = rbp + g_pc.slot_master;
    frame_max = 0;
    frame_cur = -1;
    frame_gun = 0;
    if (readable(part, 0x100) && readable(master, 0x100) && g_equip.state == 1) {
        frame_cur = *(int *)(part + g_pc.part_dur);
        frame_max = max_durability(*(int *)(part + g_pc.part_lvl), *(int *)(master + g_pc.master_dur),
                                   *(int *)(master + g_pc.master_dur_c));
        frame_gun = *(int *)(master + g_equip.master_capacity) > 0;
    }
    equipped = value_at(g_pawn, hand == 2 ? L"mEquipWeaponSlotNo_L" : L"mEquipWeaponSlotNo_R", 4);

    /* the slot in hand: what it holds now first; the others: the slot's own */
    if (equipped && *equipped == slot && live && matches(live_cur, live_max, ratio)) {
        cur = live_cur; max = live_max; gun = live_gun; from_hand = 1;
    } else if (matches(frame_cur, frame_max, ratio)) {
        cur = frame_cur; max = frame_max; gun = frame_gun; from_hand = 0;
    } else if (live && matches(live_cur, live_max, ratio)) {
        cur = live_cur; max = live_max; gun = live_gun; from_hand = 1;
    } else {
        return;     /* not a durability bar we can vouch for */
    }
    if (gun ? g_set.guns : g_set.melee) {
        int percent = cur * 100 / max;
        format_durability(text, 32, cur, max);
        g_bar_low = 0;
        if (g_set.low && percent < g_set.low) {
            g_bar_low = 1;
            g_bar_rgb = g_set.low_colour;
            if (g_set.flash) {
                g_flashing_at = GetTickCount();
                if (g_flashing_at / 400 & 1)
                    g_bar_low = 0;              /* the other half of the flash */
            }
        } else if (g_set.warn && percent < g_set.warn) {
            g_bar_low = 1;
            g_bar_rgb = g_set.warn_colour;
        }
        draw_on_bar(image, x, y, text, -1, 255, 0, item);
        g_bar_low = 0;
    }
    if (from_hand)
        draw_popup(hand, image, x, y, item);
}

/* Put the hook in the HUD class's function table, once per table. */
static void hook_gauges(void *hud)
{
    void **table = hud ? *(void ***)hud : NULL, **entry;
    static int failed;
    DWORD old;
    int i;

    if (!table || !readable(table, (size_t)g_pc.vt_gauge + 8))
        return;
    for (i = 0; i < g_nhooked; i++)
        if (g_hooked[i].table == table)
            return;
    if (g_nhooked == 4)
        return;
    entry = (void **)((unsigned char *)table + g_pc.vt_gauge);
    if (*entry == (void *)lid_gauge_stub || !inside(g_pc.text, *entry, 1) ||
        !VirtualProtect(entry, 8, PAGE_READWRITE, &old)) {
        LOG_ONCE(failed, "durability: could not hook the HUD's bars - numbers stay off");
        return;
    }
    g_hooked[g_nhooked].table = table;
    g_hooked[g_nhooked].original = (gauge_fn)*entry;
    g_nhooked++;
    *entry = (void *)lid_gauge_stub;
    VirtualProtect(entry, 8, old, &old);
    log_line("durability: the HUD's bars are hooked");
}

/* --------------------------------------------------------- enemies left */

/* "Enemies left: 12 / 20" on the HUD while you explore a floor. The numbers
 * are the game's own: the living Haters and monsters its AI director counts to
 * judge a floor cleared, and how many the floor started with. Drawn while the
 * player status panel draws - so only while the game shows its HUD - and
 * never in the hub, where the director still holds the last floor's numbers. */

/* Call a script function that returns an int or a bool; `in` are its first
 * parameters. -1 if it cannot be called. */
static int call_returning(void *obj, const wchar_t *name, const void *in, unsigned in_size)
{
    unsigned char parms[64], *fn, *ret;
    uint32_t idx = name_index(name);
    int v;

    if (!obj || !idx)
        return -1;
    fn = ((find_function_fn)g_pc.find_function)(obj, idx, 0);
    if (!fn || !readable(fn, 0x100) || !(ret = field_of(fn, L"ReturnValue")) || offset_of(ret) > 60)
        return -1;
    memset(parms, 0, sizeof parms);
    if (in_size)
        memcpy(parms, in, in_size);
    if (call_function(obj, name, parms, in_size, parms, sizeof parms) != 0)
        return -1;
    memcpy(&v, parms + offset_of(ret), 4);
    return v;
}

/* The words, in the game's language: the HUD mod writes them to master_text
 * (TUB.TXT_LID_HUD_ENEMIES, with the total, and ..._ALIVE without). Asked
 * for once, with "#0" and "#1" handed back as they are, so the numbers can
 * be put in each frame without asking again. */
static wchar_t g_words[2][128];
static void *g_words_ui;

static void load_words(void)
{
    static const wchar_t *keys[2] = {L"TUB.TXT_LID_HUD_ENEMIES_ALIVE", L"TUB.TXT_LID_HUD_ENEMIES"};
    static const wchar_t *english[2] = {L"Enemies left: #0", L"Enemies left: #0 / #1"};
    struct fstring in[3], out;
    unsigned char *fn, *ret;
    uint32_t idx;
    int i;

    g_words_ui = g_ui;
    for (i = 0; i < 2; i++) {
        unsigned char parms[0x100];

        lstrcpynW(g_words[i], english[i], 128);
        idx = name_index(L"GetLocalizeTextST");
        if (!idx || !(fn = ((find_function_fn)g_pc.find_function)(g_ui, idx, 0)) || !readable(fn, 0x100) ||
            !(ret = field_of(fn, L"ReturnValue")) || offset_of(ret) + sizeof out > sizeof parms)
            continue;
        in[0].data = keys[i];
        in[0].count = in[0].max = lstrlenW(keys[i]) + 1;
        in[1].data = L"#0";
        in[2].data = L"#1";
        in[1].count = in[1].max = in[2].count = in[2].max = 3;
        memset(parms, 0, sizeof parms);
        memcpy(parms, in, sizeof in);
        if (call_function(g_ui, L"GetLocalizeTextST", parms, sizeof in, parms, sizeof parms) != 0)
            continue;
        memcpy(&out, parms + offset_of(ret), sizeof out);
        /* the game's own words, if the HUD mod has written them; the string
         * is the engine's, and is kept (once per game, a few bytes) */
        if (out.data && out.count > 1 && out.count < 128 && readable(out.data, (size_t)out.count * 2) &&
            wcsstr(out.data, L"#0") && (i == 0 || wcsstr(out.data, L"#1")))
            lstrcpynW(g_words[i], out.data, 128);
    }
    log_line("enemies left: the words are \"%ls\" and \"%ls\"", g_words[1], g_words[0]);
}

/* `words` with #0 and #1 replaced by the numbers. */
static void fill_words(const wchar_t *words, int alive, int start, wchar_t *out, int size)
{
    int n = 0;

    while (*words && n < size - 12) {
        if (words[0] == L'#' && (words[1] == L'0' || words[1] == L'1')) {
            n += swprintf(out + n, (size_t)(size - n), L"%d", words[1] == L'0' ? alive : start);
            words += 2;
        } else {
            out[n++] = *words++;
        }
    }
    out[n] = 0;
}

/* The player panel's text state is the one the panel last drew with: the game
 * moves (mAddX, mAddY), turns (mRot) and briefly sizes it for its own
 * numbers, and copying that as it is makes our text shake. So those are
 * taken out, and the size is the one it keeps: a new size is only taken once
 * it has stayed for a second. What moved is written to the log, a few times. */
static void steady_state(unsigned char *state, unsigned char *def, unsigned char *f_sx, unsigned char *f_sy)
{
    static float bases[2][2], seens[2][2];  /* the bars' numbers and the other text each keep their own */
    static DWORD seen_ats[2];
    static float told[3];
    static int logged;
    float *base = bases[g_state_for_bars], *seen = seens[g_state_for_bars];
    DWORD *seen_at_p = &seen_ats[g_state_for_bars];
    static const wchar_t *zeroed[3] = {L"mAddX", L"mAddY", L"mRot"};
    float *sx = (float *)(state + offset_of(f_sx)), *sy = (float *)(state + offset_of(f_sy)), moved[3] = {0};
    unsigned char *f;
    DWORD now = GetTickCount();
    int i;

    for (i = 0; i < 3; i++)
        if ((f = field_of(def, zeroed[i]))) {
            moved[i] = *(float *)(state + offset_of(f));
            *(float *)(state + offset_of(f)) = 0;
        }
    if (*sx != seen[0] || *sy != seen[1]) {
        seen[0] = *sx;
        seen[1] = *sy;
        *seen_at_p = now;
    }
    if (!(base[0] > 0) || now - *seen_at_p >= 1000) {
        if (base[0] != seen[0] && logged < 12) {
            logged++;
            log_line("text: the HUD's text size (%s) is %.3f x %.3f", g_state_for_bars ? "bars" : "other text",
                     seen[0], seen[1]);
        }
        base[0] = seen[0];
        base[1] = seen[1];
    }
    if ((moved[0] != told[0] || moved[1] != told[1] || moved[2] != told[2] || *sx != base[0]) && logged < 24) {
        logged++;
        memcpy(told, moved, sizeof told);
        log_line("text: the HUD's text state was moved (%.2f, %.2f), turned %.3f, sized %.3f - not followed",
                 moved[0], moved[1], moved[2], *sx);
    }
    *sx = base[0];
    *sy = base[1];
}

/* The HUD's own text state (font, size, spacing) at `size_percent` of its
 * size, copied into `state`: its size in bytes, 0 if there is none yet. */
static uint32_t text_state(unsigned char *state, int size_percent)
{
    unsigned char *f_state, *def, *f_sx, *f_sy, *f_font;
    uint32_t size;
    float scale = size_percent / 100.0f;

    if (!g_status || !g_font)
        return 0;
    f_state = field_of(class_of(g_status), L"mFont2");
    if (!f_state || !(def = *(unsigned char **)(f_state + PROP_INNER)) || !readable(def, 0x100))
        return 0;
    size = *(uint32_t *)(def + STRUCT_SIZE);
    f_sx = field_of(def, L"mScaleX");
    f_sy = field_of(def, L"mScaleY");
    f_font = field_of(def, L"mFont");
    if (size - 1 > 255 || !f_sx || !f_sy || !f_font ||
        !readable((unsigned char *)g_status + offset_of(f_state), size))
        return 0;
    memcpy(state, (unsigned char *)g_status + offset_of(f_state), size);
    if (!*(void **)(state + offset_of(f_font)))
        return 0;
    steady_state(state, def, f_sx, f_sy);
    *(float *)(state + offset_of(f_sx)) *= scale;
    *(float *)(state + offset_of(f_sy)) *= scale;
    return size;
}

/* How wide `text` is on the 3840 x 2160 layout, at `state`. */
static float measure(unsigned char *state, struct fstring *str)
{
    float w = 0, h = 0;

    ((text_width_fn)(*(void ***)g_font)[g_pc.vt_text_width / 8])(g_font, state, str, &w, &h);
    return w > 0 && w < 8000 ? w : 0;
}

/* How wide `text` is in 1920 x 1080 pixels, at `size_percent`. */
static float text_width(const wchar_t *text, int size_percent)
{
    static wchar_t buf[200];
    unsigned char state[256];
    struct fstring str;

    if (!g_pc.vt_text_width || !text[0] || !text_state(state, size_percent))
        return 0;
    lstrcpynW(buf, text, 200);
    str.data = buf;
    str.count = str.max = lstrlenW(buf) + 1;
    return measure(state, &str) / UI_SCALE;
}

static int g_plain_text;        /* 1: draw_line leaves out the shadow */
static int g_line_box;          /* 1: draw_line puts a dark box behind the text */
static float g_line_gap;        /* ... that tall (1920 x 1080 pixels), so lines under each other meet; 0: the text's own */

/* One line of text on the HUD, in the HUD's own font and shadow, drawn the
 * way the game draws its HUD text - against one of the HUD's images, so it
 * sits on the same 3840 x 2160 layout as everything else and scales with the
 * screen. At (x, y) in 1920 x 1080 pixels: starting there (align 0), centred
 * on it (1) or ending there (2); `size_percent` of the HUD's own size, in
 * `colour_rgb`. The font renderer's colour is put back after. */
static void draw_line(const wchar_t *text, float x, float y, int align, int size_percent, unsigned colour_rgb)
{
    static wchar_t buf[200];
    unsigned char state[256], *colour[4], keep[4];
    void *image;
    struct fstring str;
    float w;
    int *pos_x, *pos_y, i;

    if (!g_status || !g_font || !g_hud || !text[0] || !g_pc.vt_text || !g_pc.vt_text_width)
        return;
    image = object_at(g_hud, L"mWhiteImage");
    if (!(pos_x = value_at(image, L"mPosX", 4)) || !(pos_y = value_at(image, L"mPosY", 4)))
        return;
    if (!text_state(state, size_percent))
        return;

    lstrcpynW(buf, text, 200);
    str.data = buf;
    str.count = str.max = lstrlenW(buf) + 1;
    w = measure(state, &str);
    x = x * UI_SCALE - (align == 2 ? w : align == 1 ? w * 0.5f : 0);
    y *= UI_SCALE;
    if (g_line_box && w > 0) {
        float pad = 6.0f * size_percent / 100, h = g_line_gap > 0 ? g_line_gap : 30.0f * size_percent / 100;
        draw_box(x / UI_SCALE - pad, y / UI_SCALE - 2, (x + w) / UI_SCALE + pad, y / UI_SCALE - 2 + h, 0x000000,
                 g_background_darkness * 255 / 100);
    }

    colour[0] = value_at(g_font, L"mColorR", 1);
    colour[1] = value_at(g_font, L"mColorG", 1);
    colour[2] = value_at(g_font, L"mColorB", 1);
    colour[3] = value_at(g_font, L"mColorA", 1);
    for (i = 0; i < 4; i++)
        if (!colour[i])
            return;
    for (i = 0; i < 4; i++)
        keep[i] = *colour[i];
    *colour[0] = (unsigned char)(colour_rgb >> 16);
    *colour[1] = (unsigned char)(colour_rgb >> 8);
    *colour[2] = (unsigned char)colour_rgb;
    *colour[3] = 255;
    /* the text is drawn relative to the image, which the game adds itself */
    font_draw(state, image, &str, x - *pos_x, y - *pos_y, 1, !g_plain_text);
    for (i = 0; i < 4; i++)
        *colour[i] = keep[i];
}

/* A line of the layout mode's own pages: at its text size, and with thick
 * text drawn a second time a little to the right, without a shadow. */
static void menu_line(const wchar_t *text, float x, float y, int align, int size_percent, unsigned colour_rgb)
{
    int size = size_percent * g_menu.text_size / 100;

    draw_line(text, x, y, align, size, colour_rgb);
    if (g_menu.thick) {
        g_plain_text = 1;
        draw_line(text, x + (size < 100 ? 0.75f : size / 133.0f), y, align, size, colour_rgb);
        g_plain_text = 0;
    }
}

/* Draw a part's lines with a background, if it has one on: around the drawing,
 * `on` (its setting) and how far apart its lines are (0: one line). */
static void background_on(int on, float gap)
{
    g_line_box = on;
    g_line_gap = gap;
}

/* A filled box from (x1, y1) to (x2, y2) in 1920 x 1080 pixels, by the HUD's
 * own Draw_Box, in `colour_rgb` at `alpha` (0 to 255). The HUD's draw colours
 * are put back after. */
static void draw_box(float x1, float y1, float x2, float y2, unsigned colour_rgb, int alpha)
{
    unsigned char *colours, keep[16];
    float *linear, keep_linear[16], v[4];
    int i;

    if (!g_hud || !(colours = value_at(g_hud, L"mDrawColor", 16)) ||
        !(linear = value_at(g_hud, L"mDrawLinearColor", 64)))
        return;
    memcpy(keep, colours, 16);
    memcpy(keep_linear, linear, 64);
    for (i = 0; i < 4; i++) {
        colours[i * 4] = (unsigned char)colour_rgb;               /* blue, green, red, alpha */
        colours[i * 4 + 1] = (unsigned char)(colour_rgb >> 8);
        colours[i * 4 + 2] = (unsigned char)(colour_rgb >> 16);
        colours[i * 4 + 3] = (unsigned char)alpha;
        linear[i * 4] = (unsigned char)(colour_rgb >> 16) / 255.0f;
        linear[i * 4 + 1] = (unsigned char)(colour_rgb >> 8) / 255.0f;
        linear[i * 4 + 2] = (unsigned char)colour_rgb / 255.0f;
        linear[i * 4 + 3] = alpha / 255.0f;
    }
    v[0] = x1 * UI_SCALE;
    v[1] = y1 * UI_SCALE;
    v[2] = x2 * UI_SCALE;
    v[3] = y2 * UI_SCALE;
    call_function(g_hud, L"Draw_Box", v, sizeof v, NULL, 0);
    memcpy(colours, keep, 16);
    memcpy(linear, keep_linear, 64);
}

static void draw_enemies(void)
{
    static wchar_t text[160];
    void *director, *stage;
    int alive = -1, start = 0, shown, in_hub;

    if (!g_enemies.show || !g_ui || !g_font || !g_game)
        return;
    director = object_at(g_game, L"mAIDirector");
    stage = object_at(g_game, L"mStageManager");
    if (director && stage && (start = call_returning(director, L"GetFloorStartEnemyNum", NULL, 0)) > 0) {
        in_hub = call_returning(stage, L"IsInHub", NULL, 0);
        shown = call_returning(g_ui, L"CheckVisibleHUD", NULL, 0);
        if (in_hub == 0 && shown > 0 && (shown & 1))
            alive = call_returning(director, L"GetAliveEnemyNum", NULL, 0);
    }
    if (alive < 0) {
        if (!g_layout.on)
            return;         /* a floor without enemies, or the hub */
        alive = 12;         /* the layout mode: a sample, to place it */
        start = 20;
    }
    if (g_words_ui != g_ui)
        load_words();
    fill_words(g_words[g_enemies.total ? 1 : 0], alive, start, text, 160);
    background_on(g_enemies.background, 0);
    draw_line(text, (float)g_enemies.x, (float)g_enemies.y, g_enemies.align, g_enemies.size,
              g_layout.on && !g_layout.page && g_layout.part == PART_ENEMIES ? LAYOUT_YELLOW : g_enemies.colour);
    background_on(0, 0);
}

/* The player status panel draws every frame the HUD shows; the counter is
 * drawn right after it. */
typedef void (*status_render_fn)(void *panel, void *hud);
static void draw_minimap(void *panel, void *hud);
static void draw_floor(void *panel);
static void draw_rage(void *panel);
static void draw_bag(void *panel);
static void draw_effects(void *panel);
static void draw_run(void *panel);
static void draw_layout_help(void);
static struct { void **table; status_render_fn original; } g_status_hooked[4];
static int g_status_nhooked;

/* The HUD can draw through a transform (mEnableDrawTransformMatrix and
 * mDrawTransformMatrix: moved, turned, sized); a part drawn before ours may
 * leave it on, and our text would move with it. It is switched off while
 * ours draw and put back after; when it was on, that goes to the log with
 * the matrix, as does the image our text is drawn against moving - both to
 * find what makes our text shake. Returns what to put back. */
static uint32_t hud_transform_off(void)
{
    static int logged;
    static int32_t image_at[2];
    unsigned char *f = g_hud ? field_of(class_of(g_hud), L"mEnableDrawTransformMatrix") : NULL;
    uint32_t bit = f ? (uint32_t)bool_bit(f) : 0, *p = bit ? value_at(g_hud, L"mEnableDrawTransformMatrix", 4) : NULL;
    void *image = g_hud ? object_at(g_hud, L"mWhiteImage") : NULL;
    int32_t *pos_x = image ? value_at(image, L"mPosX", 4) : NULL, *pos_y = image ? value_at(image, L"mPosY", 4) : NULL;

    if (pos_x && pos_y && (*pos_x != image_at[0] || *pos_y != image_at[1])) {
        if (logged < 30) {
            logged++;
            log_line("text: the image our text is drawn against is at %d, %d (was %d, %d)", *pos_x, *pos_y,
                     image_at[0], image_at[1]);
        }
        image_at[0] = *pos_x;
        image_at[1] = *pos_y;
    }
    if (!p || !(*p & bit))
        return 0;
    if (logged < 30) {
        float *m = value_at(g_hud, L"mDrawTransformMatrix", 24);
        logged++;
        if (m)
            log_line("text: the HUD's draw transform was on (%.3f %.3f %.3f / %.3f %.3f %.3f) - off for ours", m[0],
                     m[1], m[2], m[3], m[4], m[5]);
    }
    *p &= ~bit;
    return bit;
}

static void hud_transform_back(uint32_t bit)
{
    uint32_t *p = bit && g_hud ? value_at(g_hud, L"mEnableDrawTransformMatrix", 4) : NULL;

    if (p)
        *p |= bit;
}

static void status_render_hook(void *panel, void *hud)
{
    void **table = panel ? *(void ***)panel : NULL;
    status_render_fn original = NULL;
    uint32_t transform;
    int i;

    for (i = 0; i < g_status_nhooked; i++)
        if (g_status_hooked[i].table == table)
            original = g_status_hooked[i].original;
    if (!original && g_status_nhooked)
        original = g_status_hooked[0].original;
    if (original)
        original(panel, hud);
    transform = hud_transform_off();
    if (minimap_ok(&g_pc))
        draw_minimap(panel, hud);
    draw_enemies();
    draw_floor(panel);
    draw_rage(panel);
    draw_bag(panel);
    draw_effects(panel);
    draw_run(panel);
    draw_layout_help();
    hud_transform_back(transform);
}

static void hook_status(void *panel)
{
    void **table = panel ? *(void ***)panel : NULL, **entry;
    static int failed;
    DWORD old;
    int i;

    if (!table || !readable(table, (size_t)g_pc.vt_status_render + 8))
        return;
    for (i = 0; i < g_status_nhooked; i++)
        if (g_status_hooked[i].table == table)
            return;
    if (g_status_nhooked == 4)
        return;
    entry = (void **)((unsigned char *)table + g_pc.vt_status_render);
    if (*entry == (void *)status_render_hook || !inside(g_pc.text, *entry, 1) ||
        !VirtualProtect(entry, 8, PAGE_READWRITE, &old)) {
        LOG_ONCE(failed, "could not hook the HUD's drawing - the enemies counter and minimap stay off");
        return;
    }
    g_status_hooked[g_status_nhooked].table = table;
    g_status_hooked[g_status_nhooked].original = (status_render_fn)*entry;
    g_status_nhooked++;
    *entry = (void *)status_render_hook;
    VirtualProtect(entry, 8, old, &old);
    log_line("the HUD's player status drawing is hooked");
}

/* --------------------------------------------------- the screen effects */

/* The Serial Killer decal's colour-split distortion is a screen effect named
 * DamageBoost in the player's chain of post-process effects. To hide it, it
 * is switched off and its name cleared, so the game cannot find it to switch
 * it back on; switching the setting off puts the name back. */
static void *g_hidden_effect;
static uint32_t g_hidden_name;

static void set_bool(void *obj, const wchar_t *name, int on)
{
    unsigned char *f = field_of(class_of(obj), name);
    uint32_t bit = f ? (uint32_t)bool_bit(f) : 0, *p;

    if (!bit || !(p = value_at(obj, name, 4)))
        return;
    *p = on ? *p | bit : *p & ~bit;
}

static void serial_killer_effect(void)
{
    static unsigned tick;
    struct { void **data; int32_t count, max; } *effects;
    void *chain;
    uint32_t boost, *name;
    int i;

    if (!g_set.hide_serial) {
        /* only if it is still the effect we cleared: after a level change it
         * may be something else */
        if (g_hidden_effect && (name = value_at(g_hidden_effect, L"EffectName", 4)) && *name == 0)
            *name = g_hidden_name;
        g_hidden_effect = NULL;
        return;
    }
    if (++tick % 15 || !g_ui)
        return;
    chain = object_at(object_at(object_at(object_at(object_at(g_ui, L"mPlayerCommonPawn"), L"mPawn"),
                                          L"Controller"), L"Player"), L"PlayerPostProcess");
    effects = value_at(chain, L"Effects", 16);
    boost = name_index(L"DamageBoost");
    if (!effects || !boost || !effects->data || effects->count < 1 || effects->count > 256 ||
        !readable(effects->data, (size_t)effects->count * 8))
        return;
    for (i = 0; i < effects->count; i++) {
        void *effect = effects->data[i];
        if (!effect || !(name = value_at(effect, L"EffectName", 4)) || *name != boost)
            continue;
        if (effect != g_hidden_effect) {
            g_hidden_effect = effect;
            g_hidden_name = boost;
            log_line("screen: the Serial Killer effect is hidden");
        }
        set_bool(effect, L"bShowInGame", 0);
        *name = 0;
    }
}

/* ---------------------------------------------- the area map's escalators */

/* On the pause menu's area map, each escalator icon is drawn on a disc of its
 * line's colour, as the floor map does. The colour is the game's own: the map
 * unit the escalator is in (GetUnitName) and that unit's escalator colour in
 * the current area (GetCurrentAreaEscalatorColor). Both are asked once per
 * escalator and floor; the unit's name is a string the engine makes for us,
 * and is kept (a few bytes per escalator and floor). */
static struct {
    float x, y;
    int ok;
    unsigned char r, g, b;
} g_esc[32];
static int g_nesc;
static uint32_t g_esc_floor;

static uint32_t hash_string(uint32_t h, void *obj, const wchar_t *name)
{
    struct fstring *str = value_at(obj, name, sizeof *str);
    int i;

    if (str && str->data && str->count > 0 && str->count < 256 && readable(str->data, (size_t)str->count * 2))
        for (i = 0; i < str->count && str->data[i]; i++)
            h = (h ^ (uint16_t)str->data[i]) * 0x01000193;
    return (h ^ 0xFF) * 0x01000193;     /* so "ab","c" and "a","bc" differ */
}

static unsigned char *function_of(void *obj, const wchar_t *name)
{
    uint32_t idx = name_index(name);
    unsigned char *fn = obj && idx ? ((find_function_fn)g_pc.find_function)(obj, idx, 0) : NULL;
    return fn && readable(fn, 0x100) ? fn : NULL;
}

static int get_bool(void *obj, const wchar_t *name)
{
    unsigned char *f = field_of(class_of(obj), name);
    uint32_t bit = f ? (uint32_t)bool_bit(f) : 0, *p;

    return bit && (p = value_at(obj, name, 4)) && (*p & bit);
}

static unsigned char clamp_colour(float v)
{
    return (unsigned char)(int)((v > 0 ? v < 1 ? v : 1 : 0) * 255.0f + 0.5f);
}

/* The colour of the escalator at (x, y) on the current floor. 0 if it has
 * none, or it cannot be worked out. */
static int escalator_colour(void *stage, float x, float y, unsigned char rgb[3])
{
    unsigned char parms[0x100], *fn, *f_loc, *f_name, *f_unit, *f_colour, *f_ok;
    void *levels = object_at(g_game, L"mRandomGenerateLevelManager");
    float loc[3] = {x, y, 0}, colour[3];
    struct fstring unit;
    int bit;

    if (!levels || !(fn = function_of(levels, L"GetUnitName")) || !(f_loc = field_of(fn, L"Location")) ||
        !(f_name = field_of(fn, L"ReturnValue")) || offset_of(f_loc) + 12 > sizeof parms ||
        offset_of(f_name) + sizeof unit > sizeof parms)
        return 0;
    memset(parms, 0, sizeof parms);
    memcpy(parms + offset_of(f_loc), loc, 12);
    if (call_function(levels, L"GetUnitName", parms, offset_of(f_loc) + 12, parms, sizeof parms) != 0)
        return 0;
    memcpy(&unit, parms + offset_of(f_name), sizeof unit);
    if (!unit.data || unit.count < 2)
        return 0;

    if (!(fn = function_of(stage, L"GetCurrentAreaEscalatorColor")) || !(f_unit = field_of(fn, L"unitid")) ||
        !(f_colour = field_of(fn, L"escalatorColor")) || !(f_ok = field_of(fn, L"ReturnValue")) ||
        !(bit = bool_bit(f_ok)) || offset_of(f_unit) + sizeof unit > sizeof parms ||
        offset_of(f_colour) + 12 > sizeof parms || offset_of(f_ok) + 4 > sizeof parms)
        return 0;
    memset(parms, 0, sizeof parms);
    memcpy(parms + offset_of(f_unit), &unit, sizeof unit);
    if (call_function(stage, L"GetCurrentAreaEscalatorColor", parms, offset_of(f_unit) + sizeof unit, parms,
                      sizeof parms) != 0 ||
        !(*(uint32_t *)(parms + offset_of(f_ok)) & (uint32_t)bit))
        return 0;
    memcpy(colour, parms + offset_of(f_colour), 12);
    rgb[0] = clamp_colour(colour[0]);
    rgb[1] = clamp_colour(colour[1]);
    rgb[2] = clamp_colour(colour[2]);
    return 1;
}

static void draw_escalator_disc(void *part, void *hud, float x, float y)
{
    static int said;
    unsigned char parms[0x100], *fn, *f[8], *ratio_fn, *f_ratio, rgb[3] = {0, 0, 0};
    static const wchar_t *names[8] = {L"inHUD", L"inDrawX", L"InDrawY", L"inPosX", L"inPosY", L"Inradius",
                                      L"inColorRGB", L"inDrawAlpha"};
    void *stage, *render, *guide, *minimap;
    float draw_x = 0, draw_y = 0, ratio = 0, *map_scale, radius, alpha = 1, v[5];
    uint32_t key;
    int i, ok = 0, *gx, *gy;

    if (!g_map.escalators || !g_game || !g_ui || !(stage = object_at(g_game, L"mStageManager")))
        return;

    /* the colour, worked out once per escalator and floor */
    key = hash_string(hash_string((uint32_t)(ULONG_PTR)*g_pc.gworld, stage, L"mCurrentAreaId"), stage,
                      L"mCurrentFloorId");
    if (key != g_esc_floor) {
        g_esc_floor = key;
        g_nesc = 0;
    }
    for (i = 0; i < g_nesc; i++)
        if (g_esc[i].x == x && g_esc[i].y == y)
            break;
    if (i < g_nesc) {
        ok = g_esc[i].ok;
        rgb[0] = g_esc[i].r;
        rgb[1] = g_esc[i].g;
        rgb[2] = g_esc[i].b;
    } else {
        ok = escalator_colour(stage, x, y, rgb);
        if (g_nesc < 32) {
            g_esc[g_nesc].x = x;
            g_esc[g_nesc].y = y;
            g_esc[g_nesc].ok = ok;
            g_esc[g_nesc].r = rgb[0];
            g_esc[g_nesc].g = rgb[1];
            g_esc[g_nesc].b = rgb[2];
            g_nesc++;
        }
        if (g_nesc <= 8)
            log_line("map: escalator at %d,%d: %s (colour %d %d %d)", (int)x, (int)y,
                     ok ? "has a colour" : "no colour", rgb[0], rgb[1], rgb[2]);
    }
    if (!ok)
        return;

    /* drawn by the map's own disc drawing, under the icon */
    render = object_at(part, L"mMiniMapRender");
    if (!(fn = function_of(render, L"RenderCircleBase"))) {
        LOG_ONCE(said, "map: the area map cannot draw discs - escalator colours stay off");
        return;
    }
    for (i = 0; i < 8; i++)
        if (!(f[i] = field_of(fn, names[i])) || offset_of(f[i]) + 8 > sizeof parms)
            return;
    /* the map's place on screen, when the game draws its icons over it */
    if (get_bool(render, L"mbRenderIconOverCover") && (guide = object_at(render, L"mMapGuide")) &&
        (gx = value_at(guide, L"mPosX", 4)) && (gy = value_at(guide, L"mPosY", 4))) {
        draw_x = (float)*gx;
        draw_y = (float)*gy;
    }
    /* the radius is in the world's units; the same size on screen at any zoom */
    minimap = object_at(g_ui, L"mMiniMapManager");
    if ((ratio_fn = function_of(minimap, L"GetWorldToMiniMapRatio")) &&
        (f_ratio = field_of(ratio_fn, L"ReturnValue")) && offset_of(f_ratio) + 4 <= sizeof parms) {
        memset(parms, 0, sizeof parms);
        if (call_function(minimap, L"GetWorldToMiniMapRatio", NULL, 0, parms, sizeof parms) == 0)
            memcpy(&ratio, parms + offset_of(f_ratio), 4);
    }
    map_scale = value_at(render, L"mMapScale", 4);
    radius = 0;
    if (ratio > 0 && map_scale && *map_scale > 0)
        radius = g_map.escalator_size / (ratio * *map_scale);
    if (!(radius > 0 && radius < 1e7f))
        radius = 250;

    memset(parms, 0, sizeof parms);
    memcpy(parms + offset_of(f[0]), &hud, 8);
    v[0] = draw_x;
    v[1] = draw_y;
    v[2] = x;
    v[3] = y;
    v[4] = radius;
    for (i = 0; i < 5; i++)
        memcpy(parms + offset_of(f[i + 1]), &v[i], 4);
    parms[offset_of(f[6])] = rgb[2];            /* a colour is blue, green, red, alpha */
    parms[offset_of(f[6]) + 1] = rgb[1];
    parms[offset_of(f[6]) + 2] = rgb[0];
    parms[offset_of(f[6]) + 3] = 255;
    memcpy(parms + offset_of(f[7]), &alpha, 4);
    call_function(render, L"RenderCircleBase", parms, offset_of(f[7]) + 4, NULL, 0);
}

/* The area map draws each escalator icon through its function table; the disc
 * goes down first, then the game's icon on top. */
typedef void (*escalator_fn)(void *part, void *hud, float x, float y, float turn);
static struct { void **table; escalator_fn original[2]; } g_map_hooked[4];
static int g_map_nhooked;

static escalator_fn map_original(void *part, int down)
{
    void **table = part ? *(void ***)part : NULL;
    int i;

    for (i = 0; i < g_map_nhooked; i++)
        if (g_map_hooked[i].table == table)
            return g_map_hooked[i].original[down];
    return g_map_nhooked ? g_map_hooked[0].original[down] : NULL;
}

static void escalator_up_hook(void *part, void *hud, float x, float y, float turn)
{
    escalator_fn original = map_original(part, 0);

    draw_escalator_disc(part, hud, x, y);
    if (original)
        original(part, hud, x, y, turn);
}

static void escalator_down_hook(void *part, void *hud, float x, float y, float turn)
{
    escalator_fn original = map_original(part, 1);

    draw_escalator_disc(part, hud, x, y);
    if (original)
        original(part, hud, x, y, turn);
}

static void hook_map(void *part)
{
    void **table = part ? *(void ***)part : NULL, **up, **down;
    static int failed;
    int end = g_pc.vt_esc_up > g_pc.vt_esc_down ? g_pc.vt_esc_up : g_pc.vt_esc_down, i;
    DWORD old;

    if (!table || !readable(table, (size_t)end + 8))
        return;
    for (i = 0; i < g_map_nhooked; i++)
        if (g_map_hooked[i].table == table)
            return;
    if (g_map_nhooked == 4)
        return;
    up = (void **)((unsigned char *)table + g_pc.vt_esc_up);
    down = (void **)((unsigned char *)table + g_pc.vt_esc_down);
    if (*up == (void *)escalator_up_hook || *down == (void *)escalator_down_hook ||
        !inside(g_pc.text, *up, 1) || !inside(g_pc.text, *down, 1) ||
        !VirtualProtect(table, (size_t)end + 8, PAGE_READWRITE, &old)) {
        LOG_ONCE(failed, "map: could not hook the area map's escalator icons - escalator colours stay off");
        return;
    }
    g_map_hooked[g_map_nhooked].table = table;
    g_map_hooked[g_map_nhooked].original[0] = (escalator_fn)*up;
    g_map_hooked[g_map_nhooked].original[1] = (escalator_fn)*down;
    g_map_nhooked++;
    *up = (void *)escalator_up_hook;
    *down = (void *)escalator_down_hook;
    VirtualProtect(table, (size_t)end + 8, old, &old);
    log_line("map: the area map's escalator icons are hooked");
}

/* ------------------------------------------------------------ the bow arc */

/* While a bow is drawn, the line the game draws for a thrown item shows where
 * the arrow will fly. Once a frame the plugin works out the arrow's flight -
 * from the bow's fire socket, the game's own aim, the draw's charge - and
 * checks it against the world with the engine's own line check, then switches
 * the game's line on (EnableThrowItemLine) and answers its simulation
 * (SimProjectile) with that flight instead of a thrown item's. The speed,
 * gravity and steps are the arrow's own (5500 x charge + 270 a second, 22 a
 * step, 60 steps a second, as hud.dll measured them). Holding an item to
 * throw, or not drawing a bow, leaves the game's line to the game. */
#define ARC_POINTS 200

typedef uint32_t (*line_check_fn)(void *world, void *hit, void *source, const float *end, const float *start,
                                  uint32_t flags, const float *extent, void *light);
typedef void *(*mem_alloc_fn)(uint32_t size, uint32_t align);
typedef void (*mem_free_fn)(void *p);
typedef void (*sim_fn)(void *ppt);

static struct {
    int armed;                  /* the path below is this frame's */
    int line_on;                /* the plugin switched the game's line on */
    void *ppt;                  /* the line */
    int n;
    float points[ARC_POINTS][3];
    int hit, hit_pawn, hit_head;
    float hit_at[3], hit_normal[3];
} g_arc;

/* An object in an object array property, or NULL. */
static void *array_object(void *obj, const wchar_t *name, int i)
{
    struct { void **data; int32_t count, max; } *a = value_at(obj, name, 16);
    void *o;

    if (!a || i < 0 || i >= a->count || !a->data || !readable(a->data + i, 8))
        return NULL;
    o = a->data[i];
    return o && readable(o, 0x60) ? o : NULL;
}

/* Is the player drawing a bow? Then its charge, 0 to 1, and the weapon. */
static int bow_drawn(void *pcp, void **weapon_out, float *charge)
{
    void *pawn = object_at(pcp, L"mPawn"), *state, *weapon;
    uint32_t archery = name_index(L"ArcheryAtkState"), *ctrl, *anim;
    float *count, *max, c;
    int use, reload;

    if (!pawn || !archery || !(ctrl = value_at(pawn, L"mCtrlStateName", 4)) || *ctrl != archery ||
        object_at(pcp, L"mThrowItem"))
        return 0;
    state = object_at(pcp, L"mActionState");
    if (!(weapon = object_at(state, L"mCurrentStateWeapon")))
        return 0;
    if (((anim = value_at(state, L"mShotReloadAnimName", 4)) && *anim) ||
        ((anim = value_at(state, L"mShotDirectAtkAnimName", 4)) && *anim))
        return 0;                               /* reloading or shooting */
    if (!get_bool(state, L"mbChargeShot") || get_bool(state, L"mbShot"))
        return 0;
    use = call_returning(weapon, L"IsCanUseState", NULL, 0);
    reload = call_returning(weapon, L"IsCanReload", NULL, 0);
    if (!(use & 1) && !(reload & 1))
        return 0;
    count = value_at(state, L"mChargeTimeCnt", 4);
    max = value_at(state, L"mChargeTimeMax", 4);
    if (!count || !max || !(*max > 0))
        return 0;
    c = (*count > 0 ? *count : 0) / *max;
    *charge = c < 1 ? c : 1;
    *weapon_out = weapon;
    return 1;
}

/* Where the arrow leaves the bow, and which way: the bow's fire socket, aimed
 * as the game aims it. */
static int arrow_start(void *weapon, float at[3], int32_t *pitch, int32_t *yaw)
{
    unsigned char parms[0x100], *fn, *f_socket, *f_loc, *f_rot, *f_pos, *f_aim;
    void *mesh = object_at(weapon, L"mMeshCompRef");
    void *socket = value_at(weapon, L"mFireSocketName", 8);
    int32_t rot[3];
    uint32_t end;

    if (!mesh || !socket || !(fn = function_of(mesh, L"GetSocketWorldLocationAndRotation")) ||
        !(f_socket = field_of(fn, L"InSocketName")) || !(f_loc = field_of(fn, L"OutLocation")) ||
        !(f_rot = field_of(fn, L"OutRotation")) || offset_of(f_socket) > 0x20 || offset_of(f_loc) > 100 ||
        offset_of(f_rot) > 100)
        return 0;
    memset(parms, 0, sizeof parms);
    memcpy(parms + offset_of(f_socket), socket, 8);
    if (call_function(mesh, L"GetSocketWorldLocationAndRotation", parms, offset_of(f_socket) + 8, parms,
                      sizeof parms) != 0)
        return 0;
    memcpy(at, parms + offset_of(f_loc), 12);
    memcpy(rot, parms + offset_of(f_rot), 12);

    /* the game's aim assist, when the bow has it */
    if ((call_returning(weapon, L"IsAdjustAim", NULL, 0) & 1) == 1 && (fn = function_of(weapon, L"AdjustAim")) &&
        (f_pos = field_of(fn, L"pos")) && (f_aim = field_of(fn, L"Rot")) && offset_of(f_pos) < 0x30 &&
        offset_of(f_aim) < 0x30) {
        memset(parms, 0, sizeof parms);
        memcpy(parms + offset_of(f_pos), at, 12);
        memcpy(parms + offset_of(f_aim), rot, 12);
        end = (offset_of(f_pos) > offset_of(f_aim) ? offset_of(f_pos) : offset_of(f_aim)) + 12;
        if (call_function(weapon, L"AdjustAim", parms, end, parms, sizeof parms) == 0)
            memcpy(rot, parms + offset_of(f_aim), 8);
    }
    *pitch = rot[0];
    *yaw = rot[1] + 0x36;       /* the arrow flies a little right of the socket (hud.dll's measure) */
    return 1;
}

/* The arrow's flight, step by step, checked against the world every 4 steps. */
static int fly_arrow(void *pawn, void *weapon, float charge)
{
    static const float zero[3] = {0, 0, 0};
    unsigned char hit[0x100];
    float start[3], from[3], to[3], step[2], climb, fall = 0, speed;
    uint32_t flags, head = name_index(L"HeadJT"), neck = name_index(L"neckJT");
    int32_t pitch, yaw;
    void *world, *actor;
    double p, y;
    int i, k;

    if (!arrow_start(weapon, start, &pitch, &yaw) || !readable(g_pc.gworld, 8) || !(world = *g_pc.gworld) ||
        !readable(world, 0x100))
        return 0;
    p = pitch * (3.14159265358979323846 / 32768);
    y = yaw * (3.14159265358979323846 / 32768);
    speed = 5500 * charge + 270;
    step[0] = (float)(cos(p) * cos(y)) * speed / 60;
    step[1] = (float)(cos(p) * sin(y)) * speed / 60;
    climb = (float)sin(p) * speed;
    flags = g_bow.pawns ? 0x20BF : 0x2086;      /* pawns too, or only the level */
    if (g_bow.pawns && g_bow.headshot)
        flags |= 0x20000;                       /* down to the bones, for the head */

    g_arc.hit = g_arc.hit_pawn = g_arc.hit_head = 0;
    memcpy(g_arc.points[0], start, 12);
    memcpy(from, start, 12);
    memcpy(to, start, 12);
    g_arc.n = 1;
    for (i = 1; i <= 180 && g_arc.n < ARC_POINTS; i++) {
        to[0] += step[0];
        to[1] += step[1];
        fall += 22;
        to[2] += (climb - fall) / 60;
        if (i & 3)
            continue;
        memset(hit, 0, sizeof hit);
        *(float *)(hit + 0x28) = 1;             /* the hit's time: none yet */
        *(int32_t *)(hit + 0x2C) = -1;
        *(int32_t *)(hit + 0x60) = -1;
        ((line_check_fn)g_pc.line_check)(world, hit, pawn, to, from, flags, zero, NULL);
        memcpy(&actor, hit + 8, 8);
        if (actor) {
            g_arc.hit = 1;
            memcpy(g_arc.hit_at, hit + 0x10, 12);
            memcpy(g_arc.hit_normal, hit + 0x1C, 12);
            g_arc.hit_pawn = readable(actor, 0x60) && field_of(class_of(actor), L"Health") != NULL;
            for (k = 0x30; k <= 0x9C; k += 4) {
                int32_t v = *(int32_t *)(hit + k);
                if (v && ((uint32_t)v == head || (uint32_t)v == neck)) {
                    g_arc.hit_head = 1;
                    break;
                }
            }
            memcpy(g_arc.points[g_arc.n++], g_arc.hit_at, 12);
            return 1;
        }
        memcpy(g_arc.points[g_arc.n++], to, 12);
        memcpy(from, to, 12);
    }
    return 1;
}

/* Hand the game's line the arrow's flight. 0 if it cannot be. */
static int fill_line(void *ppt)
{
    struct { float *data; int32_t count, max; } *line = value_at(ppt, L"mPredictedLocations", 16);
    float *at, *normal;

    if (!line || g_arc.n < 1)
        return 0;
    if (line->max < g_arc.n || !line->data) {
        float *room = ((mem_alloc_fn)g_pc.mem_alloc)(ARC_POINTS * 12, 8);
        if (!room)
            return 0;
        if (line->data)
            ((mem_free_fn)g_pc.mem_free)(line->data);
        line->data = room;
        line->max = ARC_POINTS;
    }
    memcpy(line->data, g_arc.points, (size_t)g_arc.n * 12);
    line->count = g_arc.n;
    set_bool(ppt, L"mbHit", g_arc.hit);
    set_bool(ppt, L"mbHitToPawn", g_arc.hit_pawn);
    if (g_arc.hit && (at = value_at(ppt, L"mHitLocation", 12)) && (normal = value_at(ppt, L"mHitNormal", 12))) {
        memcpy(at, g_arc.hit_at, 12);
        memcpy(normal, g_arc.hit_normal, 12);
    }
    return 1;
}

static struct { void **table; sim_fn original; } g_sim_hooked[4];
static int g_sim_nhooked;

static void sim_projectile_hook(void *ppt)
{
    void **table = ppt ? *(void ***)ppt : NULL;
    sim_fn original = NULL;
    int i;

    if (g_arc.armed && ppt == g_arc.ppt && fill_line(ppt))
        return;
    for (i = 0; i < g_sim_nhooked; i++)
        if (g_sim_hooked[i].table == table)
            original = g_sim_hooked[i].original;
    if (!original && g_sim_nhooked)
        original = g_sim_hooked[0].original;
    if (original)
        original(ppt);
}

static int hook_line(void *ppt)
{
    void **table = ppt ? *(void ***)ppt : NULL, **entry;
    static int failed;
    DWORD old;
    int i;

    if (!table || !readable(table, (size_t)g_pc.vt_sim_projectile + 8))
        return 0;
    for (i = 0; i < g_sim_nhooked; i++)
        if (g_sim_hooked[i].table == table)
            return 1;
    if (g_sim_nhooked == 4)
        return 0;
    entry = (void **)((unsigned char *)table + g_pc.vt_sim_projectile);
    if (*entry == (void *)sim_projectile_hook || !inside(g_pc.text, *entry, 1) ||
        !VirtualProtect(entry, 8, PAGE_READWRITE, &old)) {
        LOG_ONCE(failed, "bow arc: could not hook the aiming line - the arc stays off");
        return 0;
    }
    g_sim_hooked[g_sim_nhooked].table = table;
    g_sim_hooked[g_sim_nhooked].original = (sim_fn)*entry;
    g_sim_nhooked++;
    *entry = (void *)sim_projectile_hook;
    VirtualProtect(entry, 8, old, &old);
    log_line("bow arc: the aiming line is hooked");
    return 1;
}

/* The arc's colour when it ends on an enemy, or on a head. The beam's colour
 * is a table of colours over its life, in the beam's template: the plugin
 * keeps the game's own and writes the chosen colour over it, as bright as the
 * game's was, while it is wanted - and puts the game's back after. */
static const float ARC_COLOURS[6][3] = {
    {1, 1, 1}, {1, 0.1f, 0.1f}, {1, 0.5f, 0.05f}, {1, 0.9f, 0.1f}, {0.2f, 1, 0.2f}, {0.1f, 0.9f, 1},
};
static struct { float *table; int n; float game[48]; } g_beam[4];
static int g_nbeam, g_beam_looked, g_beam_key = -1;

static void learn_beam(void *psc)
{
    void *template = object_at(psc, L"Template"), *emitter, *lod, *module;
    uint32_t colour_class = name_index(L"ParticleModuleColorOverLife");
    unsigned char *f, *inner, *f_table, *f_chunk, *f_count, *base, *cls;
    struct { float *data; int32_t count, max; } table;
    int e, m, i, chunk, elems;

    for (e = 0; e < 8 && (emitter = array_object(template, L"Emitters", e)); e++) {
        if (!(lod = array_object(emitter, L"LODLevels", 0)))
            continue;
        for (m = 0; m < 32 && (module = array_object(lod, L"Modules", m)); m++) {
            if (!colour_class || !(cls = class_of(module)) || !readable(cls, 0x60) ||
                *(uint32_t *)(cls + OBJ_NAME) != colour_class)
                continue;
            if (!(f = field_of(cls, L"ColorOverLife")) || !(inner = *(unsigned char **)(f + PROP_INNER)) ||
                !readable(inner, 0x100) || !(f_table = field_of(inner, L"LookupTable")) ||
                !(f_chunk = field_of(inner, L"LookupTableChunkSize")) ||
                !(f_count = field_of(inner, L"LookupTableNumElements")))
                continue;
            base = (unsigned char *)module + offset_of(f);
            if (!readable(base + offset_of(f_table), 16) || !readable(base + offset_of(f_chunk), 1) ||
                !readable(base + offset_of(f_count), 1))
                continue;
            memcpy(&table, base + offset_of(f_table), 16);
            chunk = base[offset_of(f_chunk)];
            elems = base[offset_of(f_count)];
            /* a plain table of colours: two numbers, then red, green, blue */
            if (!table.data || table.count < 5 || table.count > 48 || (table.count - 2) % 3 ||
                !(chunk == 3 || (chunk == 6 && elems)) || chunk * elems > table.count ||
                !readable(table.data, (size_t)table.count * 4))
                continue;
            for (i = 0; i < g_nbeam; i++)
                if (g_beam[i].table == table.data + 2)
                    break;
            if (i < g_nbeam || g_nbeam == 4)
                continue;
            g_beam[g_nbeam].table = table.data + 2;
            g_beam[g_nbeam].n = table.count - 2;
            memcpy(g_beam[g_nbeam].game, table.data + 2, (size_t)(table.count - 2) * 4);
            g_nbeam++;
        }
    }
}

/* The beam's particles, made again so they take the new colour. */
static void refresh_beam(void)
{
    void *psc;
    int i;

    for (i = 0; i < 256 && (psc = array_object(g_arc.ppt, L"mLinePSCs", i)); i++)
        call_function(psc, L"KillParticlesForced", NULL, 0, NULL, 0);
}

static void game_beam_colour(void)
{
    int i;

    if (g_beam_key < 0)
        return;
    for (i = 0; i < g_nbeam; i++)
        if (readable(g_beam[i].table, (size_t)g_beam[i].n * 4))
            memcpy(g_beam[i].table, g_beam[i].game, (size_t)g_beam[i].n * 4);
    if (g_beam_key > 0 && g_arc.ppt)
        refresh_beam();
    g_beam_key = -1;
}

static void arc_colour(void)
{
    struct { void **data; int32_t count, max; } *pscs;
    int mode, colour = g_bow.enemy_colour, i, k;

    if (!colour && !g_bow.headshot) {
        game_beam_colour();
        return;
    }
    if (!g_beam_looked) {
        g_beam_looked = 1;
        pscs = value_at(g_arc.ppt, L"mLinePSCs", 16);
        learn_beam(array_object(g_arc.ppt, L"mLinePSCs", 0));
        if (pscs && pscs->count > 1)
            learn_beam(array_object(g_arc.ppt, L"mLinePSCs", pscs->count - 1));
        if (g_nbeam)
            log_line("bow arc: %d colour table(s) found in the beam", g_nbeam);
        else
            log_line("bow arc: the beam's colour was not found - arc colours stay off");
    }
    mode = g_arc.hit_head ? 2 : g_arc.hit_pawn ? 1 : 0;     /* 0 the game's, 1 enemy, 2 head */
    if (mode == 1 && !colour)
        mode = 0;
    if (mode == 2 && !g_bow.headshot)
        mode = colour ? 1 : 0;
    if (mode + colour * 4 == g_beam_key)
        return;
    for (i = 0; i < g_nbeam; i++) {
        float *t = g_beam[i].table, *game = g_beam[i].game;
        if (!readable(t, (size_t)g_beam[i].n * 4))
            continue;
        for (k = 0; k + 2 < g_beam[i].n; k += 3) {
            float bright = (game[k] + game[k + 1] + game[k + 2]) / 3;
            if (mode == 0) {
                memcpy(t + k, game + k, 12);
                continue;
            }
            if (!(bright > 0))
                bright = 1;
            if (mode == 2)
                bright *= 3;
            t[k] = bright * ARC_COLOURS[colour][0];
            t[k + 1] = bright * ARC_COLOURS[colour][1];
            t[k + 2] = bright * ARC_COLOURS[colour][2];
        }
    }
    if (mode != 0 || g_beam_key != -1)
        refresh_beam();
    g_beam_key = mode + colour * 4;
}

static void bow_arc(void)
{
    static const uint32_t no = 0;
    void *ppt, *pcp, *weapon = NULL;
    float charge = 0;

    if (!g_bow.arc || !g_ui || !(ppt = object_at(g_ui, L"mPPT")) ||
        !(pcp = object_at(g_ui, L"mPlayerCommonPawn")) || !bow_drawn(pcp, &weapon, &charge) ||
        !hook_line(ppt) || !fly_arrow(object_at(pcp, L"mPawn"), weapon, charge)) {
        if (g_arc.armed || g_arc.line_on) {
            g_arc.armed = 0;
            if (g_arc.line_on && g_ui)
                call_function(g_ui, L"DisableThrowItemLine", &no, 4, NULL, 0);
            g_arc.line_on = 0;
        }
        game_beam_colour();
        return;
    }
    g_arc.ppt = ppt;
    g_arc.armed = 1;
    if (!get_bool(ppt, L"mEnable")) {
        call_function(g_ui, L"EnableThrowItemLine", &no, 4, NULL, 0);
        g_arc.line_on = 1;
    }
    arc_colour();
}

/* -------------------------------------------------------------- minimap */

/* The area map, always on, in a corner of the screen. The plugin makes its own
 * copy of the pause menu's area map (BrgUIMenuPart_PauseMap) with the game's
 * own resources, sized for the corner, and keeps it from the garbage
 * collector. Each frame the HUD shows, right after the player status panel,
 * it puts the copy on the player, in the corner, at the zoom chosen, and has
 * it draw itself; icons outside its square are left out. It is not drawn while
 * the pause menu's own map is open. Positions are in the game's 3840 x 2160
 * UI space: the settings' 1920 x 1080 pixels, doubled. */
#define RF_ROOT_SET 0x4000ull   /* UObject flags: never garbage collected */
#define OBJ_FLAGS   0x10

typedef void *(*find_object_fn)(void *cls, void *outer, const wchar_t *name, int exact);
typedef void *(*make_object_fn)(void *cls, void *outer, uint64_t name, uint64_t flags, void *template,
                                void *subobject_root, void *instance_graph);
typedef void (*render_icon_fn)(void *render, void *hud, void *image, float x, float y, float turn, float scale,
                               float alpha);

static struct {
    void *map;                  /* the plugin's area map, or NULL */
    void *ui;                   /* the UI manager it was made for */
    int size;                   /* its size, in 3840 x 2160 pixels */
    int failed;                 /* could not be made for this UI manager */
    DWORD ready_since;          /* when the game's map resources were first ready */
    float scale[2];             /* the map's own display scale, before zoom_percent */
    float centre[2], cull;      /* icons further than this from the player are left out */
} g_mm;

static void set_rooted(void *obj, int on)
{
    uint64_t *flags = obj && readable(obj, 0x60) ? (uint64_t *)((unsigned char *)obj + OBJ_FLAGS) : NULL;

    if (flags)
        *flags = on ? *flags | RF_ROOT_SET : *flags & ~RF_ROOT_SET;
}

static void drop_minimap(void)
{
    if (g_mm.map)
        set_rooted(g_mm.map, 0);    /* the garbage collector takes it from here */
    g_mm.map = NULL;
    g_mm.cull = 0;
}

/* A script function's object return value, or NULL. */
static void *call_object(void *obj, const wchar_t *name)
{
    unsigned char parms[0x40], *fn, *ret;
    void *o;

    if (!(fn = function_of(obj, name)) || !(ret = field_of(fn, L"ReturnValue")) || offset_of(ret) + 8 > sizeof parms)
        return NULL;
    memset(parms, 0, sizeof parms);
    if (call_function(obj, name, NULL, 0, parms, sizeof parms) != 0)
        return NULL;
    memcpy(&o, parms + offset_of(ret), 8);
    return o && readable(o, 0x60) ? o : NULL;
}

/* Call a script function whose one parameter is an object. */
static void call_with_object(void *obj, const wchar_t *function, const wchar_t *parameter, void *value)
{
    unsigned char parms[0x40], *fn, *f;

    if (!(fn = function_of(obj, function)) || !(f = field_of(fn, parameter)) || offset_of(f) + 8 > sizeof parms)
        return;
    memset(parms, 0, sizeof parms);
    memcpy(parms + offset_of(f), &value, 8);
    call_function(obj, function, parms, offset_of(f) + 8, NULL, 0);
}

/* The guide image's place and size: mPosX, mPosY, mUseSizeX, mUseSizeY. */
static int guide_box(void *guide, int32_t *box[4])
{
    return (box[0] = value_at(guide, L"mPosX", 4)) && (box[1] = value_at(guide, L"mPosY", 4)) &&
           (box[2] = value_at(guide, L"mUseSizeX", 4)) && (box[3] = value_at(guide, L"mUseSizeY", 4));
}

static void *make_minimap(void *ui, int size)
{
    void *cls, *resource, *map, *guide, *textures[2], **slot;
    int32_t *box[4], keep[4];
    int i, ok;

    cls = ((find_object_fn)g_pc.find_object)(NULL, (void *)(LONG_PTR)-1, L"BrgGame.BrgUIMenuPart_PauseMap", 0);
    resource = object_at(object_at(ui, L"mOnMemoryResource"), L"mAreaMapResource");
    if (!cls || !resource) {
        log_line("minimap: %s", cls ? "the area map's resources are missing" : "the area map class was not found");
        return NULL;
    }
    map = ((make_object_fn)g_pc.make_object)(cls, ui, 0, 0, NULL, NULL, NULL);
    if (!map || !readable(map, 0x60) || !(slot = value_at(map, L"mResource", 8))) {
        log_line("minimap: could not make an area map");
        return NULL;
    }
    set_rooted(map, 1);
    *slot = resource;
    if (!(guide = call_object(map, L"GetMapGuide")) || !guide_box(guide, box)) {
        log_line("minimap: the area map has no guide image");
        set_rooted(map, 0);
        return NULL;
    }
    call_with_object(ui, L"CreateAndGetMapRenderTarget", L"mapGuide", guide);

    /* made at the minimap's size; the UI manager's own map textures are kept
     * out of it, and put back */
    textures[0] = call_object(ui, L"GetMinimap_MapTexture");
    textures[1] = call_object(ui, L"GetMinimap_RenderTargetTexture");
    call_with_object(ui, L"SetMinimap_MapTexture", L"NewRT", NULL);
    call_with_object(ui, L"SetMinimap_RenderTargetTexture", L"NewRT", NULL);
    for (i = 0; i < 4; i++)
        keep[i] = *box[i];
    log_line("minimap: the area map's guide is %d,%d %dx%d", keep[0], keep[1], keep[2], keep[3]);
    *box[2] = *box[3] = size;
    ok = call_function(map, L"Initialize", NULL, 0, NULL, 0) == 0;
    for (i = 0; i < 4; i++)
        *box[i] = keep[i];
    call_with_object(ui, L"SetMinimap_MapTexture", L"NewRT", textures[0]);
    call_with_object(ui, L"SetMinimap_RenderTargetTexture", L"NewRT", textures[1]);
    if (!ok) {
        log_line("minimap: the area map would not initialise");
        set_rooted(map, 0);
        return NULL;
    }
    log_line("minimap: made (%d x %d)", size, size);
    return map;
}

/* Each frame: make the map once the game's own map resources are loaded (and
 * have been for 3 seconds), make it again for a new size or UI manager. */
static void keep_minimap(void)
{
    void *res;
    int size = g_mini.size * 2, loaded;

    if (!g_mini.show || !g_ui) {
        drop_minimap();
        return;
    }
    if (g_mm.ui != g_ui) {
        drop_minimap();
        g_mm.ui = g_ui;
        g_mm.failed = 0;
        g_mm.ready_since = 0;
    }
    if (g_mm.map && g_mm.size != size && GetTickCount() - g_layout.changed > 400)
        drop_minimap();
    if (g_mm.map || g_mm.failed || object_at(g_ui, L"mPauseMap"))
        return;
    res = object_at(g_ui, L"mOnMemoryResource");
    loaded = res && object_at(res, L"mAreaMapResource") && object_at(res, L"mIcon_MiniMapResource") &&
             object_at(object_at(object_at(g_ui, L"mPlayerCommonPawn"), L"mPawn"), L"Controller") &&
             (call_returning(res, L"CheckLoadEnd", NULL, 0) & 1) == 1;
    if (!loaded) {
        g_mm.ready_since = 0;
        return;
    }
    if (!g_mm.ready_since)
        g_mm.ready_since = GetTickCount() | 1;
    if (GetTickCount() - g_mm.ready_since < 3000)
        return;
    g_mm.map = make_minimap(g_ui, size);
    g_mm.size = size;
    g_mm.scale[0] = g_mm.scale[1] = 0;
    g_mm.failed = g_mm.map == NULL;
}

/* A value in a struct property of `obj`: struct field `outer`, its `inner`. */
static void *struct_value(void *obj, const wchar_t *outer, const wchar_t *inner, size_t size)
{
    unsigned char *f = field_of(class_of(obj), outer), *def, *g, *p;

    if (!f || !(def = *(unsigned char **)(f + PROP_INNER)) || !readable(def, 0x100) || !(g = field_of(def, inner)))
        return NULL;
    p = (unsigned char *)obj + offset_of(f) + offset_of(g);
    return readable(p, size) ? p : NULL;
}

static float *struct_float(void *obj, const wchar_t *outer, const wchar_t *inner)
{
    return struct_value(obj, outer, inner, 4);
}

static void call_with_hud(void *obj, const wchar_t *name, void *hud)
{
    call_with_object(obj, name, L"inHUD", hud);
}

static void draw_minimap(void *panel, void *hud)
{
    float *alpha, *map_alpha, *where, *pos, *map_scale, ratio = 0;
    void *pawn, *guide, *render, *minimap;
    int32_t *box[4], keep[4], *zoom, size = g_mm.size, x, y;
    unsigned char parms[0x40], *fn, *f_ratio;
    int i;

    g_mm.cull = 0;
    if (!g_mini.show || !g_mm.map || !g_ui || object_at(g_ui, L"mPauseMap"))
        return;
    if (!(alpha = struct_float(panel, L"mVisibleAlpha", L"mNowParam")) || !(*alpha > 0.01f))
        return;                                 /* the HUD is faded out */
    pawn = object_at(object_at(g_ui, L"mPlayerCommonPawn"), L"mPawn");
    if (!(where = value_at(pawn, L"Location", 12)) || !(pos = value_at(g_mm.map, L"mCurrentPos", 12)))
        return;
    memcpy(pos, where, 12);                     /* the map follows the player */
    if ((map_alpha = struct_float(g_mm.map, L"mNowVisibleAlpha", L"mNowParam")))
        *map_alpha = *alpha < 1 ? *alpha : 1;
    if ((zoom = value_at(g_mm.map, L"mDispScaleIndex", 4)))
        *zoom = g_mini.zoom;
    if ((map_scale = value_at(g_mm.map, L"mDispScale", 8))) {
        if (!(g_mm.scale[0] > 0))
            memcpy(g_mm.scale, map_scale, 8);
        if (g_mm.scale[0] > 0) {
            map_scale[0] = g_mm.scale[0] * g_mini.zoom_percent / 100.0f;
            map_scale[1] = g_mm.scale[1] * g_mini.zoom_percent / 100.0f;
        }
    }
    if (!(guide = call_object(g_mm.map, L"GetMapGuide")) || !guide_box(guide, box))
        return;
    for (i = 0; i < 4; i++)
        keep[i] = *box[i];

    /* the corner, in the 3840 x 2160 UI space */
    x = g_mini.corner == 1 || g_mini.corner == 3 || g_mini.corner == 5 ? g_mini.x * 2 : 3840 - g_mini.x * 2 - size;
    if (g_mini.corner >= 4)
        y = g_mini.y * 2 + (2160 - size) / 2;
    else if (g_mini.corner >= 2)
        y = 2160 - g_mini.y * 2 - size;
    else
        y = g_mini.y * 2;
    *box[0] = x;
    *box[1] = y;
    *box[2] = *box[3] = size;

    /* icons further than half the map from the player are left out */
    minimap = object_at(g_ui, L"mMiniMapManager");
    render = object_at(g_mm.map, L"mMiniMapRender");
    if ((fn = function_of(minimap, L"GetWorldToMiniMapRatio")) && (f_ratio = field_of(fn, L"ReturnValue")) &&
        offset_of(f_ratio) + 4 <= sizeof parms) {
        memset(parms, 0, sizeof parms);
        if (call_function(minimap, L"GetWorldToMiniMapRatio", NULL, 0, parms, sizeof parms) == 0)
            memcpy(&ratio, parms + offset_of(f_ratio), 4);
    }
    map_scale = value_at(render, L"mMapScale", 4);
    if (ratio > 0 && map_scale && *map_scale > 0) {
        g_mm.cull = size * 0.5f / (ratio * *map_scale);
        g_mm.centre[0] = where[0];
        g_mm.centre[1] = where[1];
    }

    call_with_hud(g_mm.map, L"RenderBegin", hud);
    call_with_hud(g_mm.map, L"RenderMain", hud);
    call_with_hud(g_mm.map, L"RenderEnd", hud);
    g_mm.cull = 0;
    for (i = 0; i < 4; i++)
        *box[i] = keep[i];
    if (g_layout.on && !g_layout.page && g_layout.part == PART_MINIMAP)
        draw_line(L"Minimap", (x + size / 2) / 2.0f, y / 2.0f - 30, 1, 90, LAYOUT_YELLOW);
}

/* The area map draws each icon through its function table: on the minimap,
 * icons outside its square are left out. */
static struct { void **table; render_icon_fn original; } g_icon_hooked[4];
static int g_icon_nhooked;

static void render_icon_hook(void *render, void *hud, void *image, float x, float y, float turn, float scale,
                             float alpha)
{
    void **table = render ? *(void ***)render : NULL;
    render_icon_fn original = NULL;
    float reach = g_mm.cull * 0.94f;
    int i;

    if (reach > 0 && g_mm.map && render == object_at(g_mm.map, L"mMiniMapRender") &&
        (x < g_mm.centre[0] - reach || x > g_mm.centre[0] + reach || y < g_mm.centre[1] - reach ||
         y > g_mm.centre[1] + reach))
        return;
    for (i = 0; i < g_icon_nhooked; i++)
        if (g_icon_hooked[i].table == table)
            original = g_icon_hooked[i].original;
    if (!original && g_icon_nhooked)
        original = g_icon_hooked[0].original;
    if (original)
        original(render, hud, image, x, y, turn, scale, alpha);
}

static void hook_icons(void *render)
{
    void **table = render ? *(void ***)render : NULL, **entry;
    static int failed;
    DWORD old;
    int i;

    if (!table || !readable(table, (size_t)g_pc.vt_render_icon + 8))
        return;
    for (i = 0; i < g_icon_nhooked; i++)
        if (g_icon_hooked[i].table == table)
            return;
    if (g_icon_nhooked == 4)
        return;
    entry = (void **)((unsigned char *)table + g_pc.vt_render_icon);
    if (*entry == (void *)render_icon_hook || !inside(g_pc.text, *entry, 1) ||
        !VirtualProtect(entry, 8, PAGE_READWRITE, &old)) {
        LOG_ONCE(failed, "minimap: could not hook the map's icons - every icon shows");
        return;
    }
    g_icon_hooked[g_icon_nhooked].table = table;
    g_icon_hooked[g_icon_nhooked].original = (render_icon_fn)*entry;
    g_icon_nhooked++;
    *entry = (void *)render_icon_hook;
    VirtualProtect(entry, 8, old, &old);
    log_line("minimap: the map's icons are hooked");
}

/* ------------------------------------------------------------ floor info */

/* What is left on this floor, a line each: beasts alive, mushrooms and
 * materials lying on the floor, chests not yet opened, and whether there is a
 * Fun Fungus. Chests and pickups are the level's treasure actors
 * (BrgActor_Treasure): a chest is a BrgActor_Treasure_Box, a pickup says what
 * it is in mTreasureType (8 material, 1 mushroom - the Fun Fungus is
 * MSR_022). Blueprints are not counted: one only turns up when a chest is
 * opened or an enemy dies. Counted twice a second, never in the Waiting Room. */
enum { TREASURE_MUSHROOM = 1, TREASURE_MATERIAL = 8, TREASURE_FADING = 3 };
#define FUN_FUNGUS L"MSR_022"

static struct {
    int valid;                  /* counted on a floor, just now */
    int beasts, mushrooms, chests, materials, fungus;
    DWORD at;
    uint32_t floor;             /* the floor last written to the log */
} g_count;

/* What kind of actor a class makes: 1 nothing counted, 2 a treasure, 3 a
 * chest, 4 a beast. */
enum { KIND_OTHER = 1, KIND_TREASURE, KIND_CHEST, KIND_BEAST };

static int actor_kind(unsigned char *cls)
{
    static struct { unsigned char *cls; int kind; } seen[512];
    static int n;
    uint32_t treasure = name_index(L"BrgActor_Treasure"), chest = name_index(L"BrgActor_Treasure_Box");
    uint32_t beast = name_index(L"BrgPawn_MushBeastBase");
    unsigned char *c;
    int i, kind = 1, depth;

    for (i = 0; i < n; i++)
        if (seen[i].cls == cls)
            return seen[i].kind;
    for (c = cls, depth = 0; c && depth < 16 && readable(c, 0x100); c = *(unsigned char **)(c + STRUCT_SUPER), depth++) {
        uint32_t name = *(uint32_t *)(c + OBJ_NAME);
        if (chest && name == chest) {
            kind = 3;
            break;
        }
        if (treasure && name == treasure) {
            kind = 2;
            break;
        }
        if (beast && name == beast) {
            kind = 4;
            break;
        }
    }
    if (n < 512) {
        seen[n].cls = cls;
        seen[n].kind = kind;
        n++;
    }
    return kind;
}

static int is_fun_fungus(void *actor)
{
    struct fstring *id = struct_value(actor, L"mDbMushroom", L"Mid", sizeof *id);

    return id && id->data && id->count == 8 && readable(id->data, 16) && wcscmp(id->data, FUN_FUNGUS) == 0;
}

/* A treasure the game is done with: taken, going, or let go. */
static int treasure_gone(void *a, unsigned char state)
{
    return state >= TREASURE_FADING || get_bool(a, L"bDeleteMe") || get_bool(a, L"mDeleteRequest") ||
           get_bool(a, L"mDeleteComp") || get_bool(a, L"mUnregist");
}

static void count_floor(void)
{
    struct { void **data; int32_t count, max; } *actors;
    unsigned char *world, *level, *state, *type;
    void *stage;
    DWORD now = GetTickCount();
    int i, game_beasts, game_mushrooms, in_hub, beasts = 0, mushrooms = 0, chests = 0, materials = 0;
    int fungus = 0;
    uint32_t floor;

    if (now - g_count.at < 500)
        return;
    g_count.at = now;
    g_count.valid = 0;
    if (!g_floor.show || !g_game || !(stage = object_at(g_game, L"mStageManager")))
        return;
    in_hub = call_returning(stage, L"IsInHub", NULL, 0);
    if (in_hub != 0)
        return;
    floor = hash_string(0x811C9DC5, stage, L"mCurrentFloorId");
    game_beasts = call_returning(stage, L"GetCurrentFloorBeastCount", NULL, 0);
    game_mushrooms = call_returning(stage, L"GetCurrentFloorMushroomCount", NULL, 0);
    if (!readable(g_pc.gworld, 8) || !(world = *g_pc.gworld) || !readable(world, 0x100) ||
        !(level = *(unsigned char **)(world + 0x80)) || !readable(level, 0x70))
        return;
    actors = (void *)(level + 0x60);
    if (!actors->data || actors->count < 1 || actors->count > 200000 ||
        !readable(actors->data, (size_t)actors->count * 8))
        return;
    for (i = 0; i < actors->count; i++) {
        void *a = actors->data[i];
        int kind, *health;
        if (!a || !readable(a, 0x60) || (kind = actor_kind(class_of(a))) == KIND_OTHER)
            continue;
        if (kind == KIND_BEAST) {
            /* alive: still has health, and not being removed */
            if (!get_bool(a, L"bDeleteMe") && (health = value_at(a, L"Health", 4)) && *health > 0)
                beasts++;
            continue;
        }
        if (!(state = value_at(a, L"mState", 1)))
            continue;
        if (kind == KIND_CHEST) {
            if (!treasure_gone(a, *state) && !get_bool(a, L"mOpen"))
                chests++;
            continue;
        }
        if (!(type = value_at(a, L"mTreasureType", 1)))
            continue;
        if (*type != TREASURE_MATERIAL && *type != TREASURE_MUSHROOM)
            continue;
        if (treasure_gone(a, *state))
            continue;
        if (*type == TREASURE_MATERIAL)
            materials++;
        else {
            mushrooms++;
            if (is_fun_fungus(a))
                fungus++;
        }
    }
    g_count.beasts = beasts;
    g_count.mushrooms = mushrooms;
    g_count.chests = chests;
    g_count.materials = materials;
    g_count.fungus = fungus;
    g_count.valid = 1;
    if (floor != g_count.floor) {
        g_count.floor = floor;
        log_line("floor info: beasts %d (the game counts %d), mushrooms %d (the game counts %d), chests %d, "
                 "materials %d, fun fungus %d (%d actors)", beasts, game_beasts, mushrooms,
                 game_mushrooms, chests, materials, fungus, actors->count);
    }
}

/* Is the HUD on screen? The game's own answer, and the player panel not faded
 * out - so not in films or loading screens. */
static int hud_visible(void *panel)
{
    float *alpha = struct_float(panel, L"mVisibleAlpha", L"mNowParam");
    int shown = call_returning(g_ui, L"CheckVisibleHUD", NULL, 0);

    return alpha && *alpha > 0.01f && shown > 0 && (shown & 1);
}

static void draw_floor(void *panel)
{
    wchar_t lines[6][64];
    unsigned colours[6];
    int kinds[6];
    int n = 0, i, beasts, mushrooms, chests, materials, fungus;
    int selected = g_layout.on && !g_layout.page && g_layout.part == PART_FLOOR;

    if (!g_floor.show || !g_ui || (!g_layout.on && !hud_visible(panel)))
        return;
    if (g_count.valid) {
        beasts = g_count.beasts;
        mushrooms = g_count.mushrooms;
        chests = g_count.chests;
        materials = g_count.materials;
        fungus = g_count.fungus;
    } else if (g_layout.on) {
        beasts = 4;             /* the layout mode: a sample, to place it */
        mushrooms = 9;
        chests = 2;
        materials = 7;
        fungus = 1;
    } else {
        return;
    }
    if (g_floor.beasts && beasts >= 0) {
        swprintf(lines[n], 64, L"Beasts: %d", beasts);
        kinds[n] = LINE_BEASTS;
        colours[n++] = g_floor.colour;
    }
    if (g_floor.mushrooms && mushrooms >= 0) {
        swprintf(lines[n], 64, L"Mushrooms: %d", mushrooms);
        kinds[n] = LINE_MUSHROOMS;
        colours[n++] = g_floor.colour;
    }
    if (g_floor.chests) {
        swprintf(lines[n], 64, L"Chests: %d", chests);
        kinds[n] = LINE_CHESTS;
        colours[n++] = g_floor.colour;
    }
    if (g_floor.materials) {
        swprintf(lines[n], 64, L"Materials: %d", materials);
        kinds[n] = LINE_MATERIALS;
        colours[n++] = g_floor.colour;
    }
    /* the Fun Fungus line: when there is one, or to place it */
    if (g_floor.fungus && (fungus > 0 || selected)) {
        lstrcpynW(lines[n], L"Fun Fungus on this floor", 64);
        kinds[n] = LINE_FUNGUS;
        colours[n++] = g_floor.fungus_colour;
    }
    /* a line under the other, each moved on its own on top of that */
    background_on(g_floor.background, 32.0f * g_floor.size / 100);
    for (i = 0; i < n; i++)
        draw_line(lines[i], (float)(g_floor.x + g_floor.off_x[kinds[i]]),
                  g_floor.y + g_floor.off_y[kinds[i]] + i * 32.0f * g_floor.size / 100, g_floor.align, g_floor.size,
                  selected && (g_layout.item == 0 || g_layout.item == kinds[i] + 1) ? LAYOUT_YELLOW : colours[i]);
    background_on(0, 0);
}

/* ------------------------------------------------------------------ rage */

/* How full the rage gauge is, read from the game's own gauge on the player
 * status panel (mRageGaugeNewRate, 0 to 1): "Rage: 63%" is how far it is to
 * the next gauge. When it goes up, "+4%" shows for 3 seconds - what that hit
 * gave. The game flashes a number when the gauge goes up a level
 * (mRageGaugeUpNumber); when that changes, the gain counts the rest of the old
 * gauge too. The first changes go to the log, to check what the numbers mean. */
static struct {
    float rate, gain;
    int known;
    DWORD gain_at;
    wchar_t level[16];
    int logged;
} g_rg;

static void draw_rage(void *panel)
{
    wchar_t text[64], level[16] = L"";
    struct fstring *up;
    float *rate, r, gain = 0;
    void *stage;
    int selected = g_layout.on && !g_layout.page && g_layout.part == PART_RAGE, in_hub = 0;
    DWORD now = GetTickCount();

    if (!g_rage.show || !g_status || (!g_layout.on && !hud_visible(panel)))
        return;
    if ((stage = object_at(g_game, L"mStageManager")))
        in_hub = call_returning(stage, L"IsInHub", NULL, 0) == 1;
    rate = value_at(g_status, L"mRageGaugeNewRate", 4);
    if ((up = value_at(g_status, L"mRageGaugeUpNumber", sizeof *up)) && up->data && up->count > 0 &&
        up->count < 16 && readable(up->data, (size_t)up->count * 2))
        lstrcpynW(level, up->data, 16);
    if (rate && *rate == *rate) {           /* not NaN */
        r = *rate < 0 ? 0 : *rate > 1 ? 1 : *rate;
        if (g_rg.known) {
            if (wcscmp(level, g_rg.level) != 0 && level[0])
                gain = (1 - g_rg.rate) + r;     /* a new level: the rest of the old gauge, then this */
            else if (r > g_rg.rate + 0.0005f)
                gain = r - g_rg.rate;
            if (gain > 0) {
                g_rg.gain = gain;
                g_rg.gain_at = now;
            }
            if ((gain > 0 || r < g_rg.rate - 0.0005f) && g_rg.logged < 20) {
                g_rg.logged++;
                log_line("rage: %.3f -> %.3f (level text \"%ls\")", g_rg.rate, r, level);
            }
        }
        g_rg.rate = r;
        g_rg.known = 1;
        lstrcpynW(g_rg.level, level, 16);
    } else if (!g_layout.on) {
        return;
    }
    if (in_hub && !g_layout.on)
        return;
    if (rate && !in_hub) {
        r = g_rg.rate;
        gain = g_rage.gain && now - g_rg.gain_at < 3000 ? g_rg.gain : 0;
    } else {
        r = 0.63f;              /* the layout mode: a sample, to place it */
        gain = g_rage.gain ? 0.04f : 0;
    }
    if (gain > 0)
        swprintf(text, 64, gain < 0.1f ? L"Rage: %d%%   +%.1f%%" : L"Rage: %d%%   +%.0f%%", (int)(r * 100),
                 gain * 100);
    else
        swprintf(text, 64, L"Rage: %d%%", (int)(r * 100));
    background_on(g_rage.background, 0);
    draw_line(text, (float)g_rage.x, (float)g_rage.y, g_rage.align, g_rage.size,
              selected ? LAYOUT_YELLOW : g_rage.colour);
    background_on(0, 0);
}

/* ------------------------------------------------------ more text parts */

static const wchar_t *RUN_NAMES[RUN_LINES] = {L"Time", L"Kill Coins", L"SP", L"Bloodnium", L"EXP", L"Death Metals",
                                              L"Deaths", L"Continues"};

/* `v` with a sign and thousands commas: "+12,345". */
static void signed_number(wchar_t *out, int size, long long v)
{
    wchar_t digits[32];
    int n, i, k = 0;

    swprintf(digits, 32, L"%lld", v < 0 ? -v : v);
    n = lstrlenW(digits);
    if (size < 2)
        return;
    out[k++] = v < 0 ? L'-' : L'+';
    for (i = 0; i < n && k < size - 2; i++) {
        if (i && (n - i) % 3 == 0)
            out[k++] = L',';
        out[k++] = digits[i];
    }
    out[k] = 0;
}

/* Lines of a text part, one under the other, at `b`; `kinds` say which of
 * the part's pieces each is (moved by off_x/off_y, if given), `colours` their
 * colour. The part being moved is yellow (`part`, and the piece picked). */
static void draw_block(const struct block_settings *b, int part, wchar_t lines[][80], const int *kinds,
                       const unsigned *colours, int n, const int *off_x, const int *off_y)
{
    int i, selected = g_layout.on && !g_layout.page && g_layout.part == part;

    background_on(b->background, 32.0f * b->size / 100);
    for (i = 0; i < n; i++) {
        int k = kinds ? kinds[i] : 0;
        draw_line(lines[i], (float)(b->x + (off_x ? off_x[k] : 0)),
                  b->y + (off_y ? off_y[k] : 0) + i * 32.0f * b->size / 100, b->align, b->size,
                  selected && (g_layout.item == 0 || (kinds && g_layout.item == k + 1)) ? LAYOUT_YELLOW
                                                                                        : colours[i]);
    }
    background_on(0, 0);
}

/* ---- the bag: "Bag: 12 / 20" - the fighter's bag and its size, by the
 * character's GetDeathBagItemNum and GetDeathBagSize for the fighter being
 * played (the user data's GetCurrentCharaIndex; 0 is the first fighter, not
 * "this one" - 0.10.x read fighter 0's bag). Read twice a second; what they
 * say goes to the log when it changes, beside the free space and the user
 * data's slot count. */
static void draw_bag(void *panel)
{
    static DWORD at;
    static int items = -1, size = -1, logged;
    wchar_t lines[1][80];
    unsigned colour;
    DWORD now = GetTickCount();

    if (!g_bag.b.show || !g_ui || (!g_layout.on && !hud_visible(panel)))
        return;
    if (now - at >= 500 && g_pawn) {
        void *user = object_at(g_game, L"mUserData");
        int chara = user ? call_returning(user, L"GetCurrentCharaIndex", NULL, 0) : -1, n = -1, max = -1;
        at = now;
        if (chara >= 0) {
            n = call_returning(g_pawn, L"GetDeathBagItemNum", &chara, 4);
            max = call_returning(g_pawn, L"GetDeathBagSize", &chara, 4);
        }
        if ((n != items || max != size) && logged < 40) {
            logged++;
            log_line("bag: fighter %d has %d of %d (free %d; the user data's slot count %d)", chara, n, max,
                     chara >= 0 ? call_returning(g_pawn, L"GetDeathBagSpace", &chara, 4) : -1,
                     chara >= 0 && user ? call_returning(user, L"GetDeathBagSlotCount", &chara, 4) : -1);
        }
        items = n;
        size = max;
    }
    if (items < 0 || size <= 0) {
        if (!g_layout.on)
            return;
        swprintf(lines[0], 80, L"Bag: 12 / 20");       /* the layout mode: a sample */
        colour = g_bag.b.colour;
    } else {
        swprintf(lines[0], 80, L"Bag: %d / %d", items, size);
        colour = items >= size ? g_bag.full_colour : g_bag.b.colour;
    }
    draw_block(&g_bag.b, PART_BAG, lines, NULL, &colour, 1, NULL, NULL);
}

/* ---- mushroom effects: each status effect running on the player (the pawn's
 * mStatusAilmentArray) with its time left. Its name is the game's own, in its
 * language: the effect's master data (the database's GetMushroomEfc) gives
 * its text id, GetLocalizeTextST the words, "#0" its value. Kept once found. */
static struct {
    wchar_t id[48];
    wchar_t words[64];
} g_efc_names[48];
static int g_nefc_names;

static void effect_words(const wchar_t *id, const wchar_t *fallback, wchar_t *out, int size)
{
    unsigned char parms[0x400], *fn, *f_id, *f_out, *def, *f_name, *f_ret;
    struct fstring in[3], key, got;
    void *db;
    int i;

    for (i = 0; i < g_nefc_names; i++)
        if (wcscmp(g_efc_names[i].id, id) == 0) {
            lstrcpynW(out, g_efc_names[i].words, size);
            return;
        }
    lstrcpynW(out, fallback && fallback[0] ? fallback : id, size);
    db = object_at(g_game, L"mDatabase");
    if (db && (fn = function_of(db, L"GetMushroomEfc")) && (f_id = field_of(fn, L"Id")) &&
        (f_out = field_of(fn, L"outMsrefc")) && (def = *(unsigned char **)(f_out + PROP_INNER)) &&
        readable(def, 0x100) && (f_name = field_of(def, L"mName")) &&
        *(uint16_t *)(fn + FUNC_PARMS) <= sizeof parms) {
        memset(parms, 0, sizeof parms);
        key.data = (wchar_t *)id;
        key.count = key.max = lstrlenW(id) + 1;
        memcpy(parms + offset_of(f_id), &key, sizeof key);
        if (call_function(db, L"GetMushroomEfc", parms, *(uint16_t *)(fn + FUNC_PARMS), parms, sizeof parms) == 0) {
            memcpy(&key, parms + offset_of(f_out) + offset_of(f_name), sizeof key);
            /* the words for that text id */
            if (key.data && key.count > 1 && key.count < 96 && readable(key.data, (size_t)key.count * 2) &&
                (fn = function_of(g_ui, L"GetLocalizeTextST")) && (f_ret = field_of(fn, L"ReturnValue")) &&
                offset_of(f_ret) + sizeof got <= sizeof parms) {
                in[0] = key;
                in[1].data = L"#0";
                in[2].data = L"#1";
                in[1].count = in[1].max = in[2].count = in[2].max = 3;
                memset(parms, 0, sizeof parms);
                memcpy(parms, in, sizeof in);
                if (call_function(g_ui, L"GetLocalizeTextST", parms, sizeof in, parms, sizeof parms) == 0) {
                    memcpy(&got, parms + offset_of(f_ret), sizeof got);
                    if (got.data && got.count > 1 && got.count < 64 && readable(got.data, (size_t)got.count * 2))
                        lstrcpynW(out, got.data, size);
                }
            }
        }
    }
    if (g_nefc_names < 48) {
        lstrcpynW(g_efc_names[g_nefc_names].id, id, 48);
        lstrcpynW(g_efc_names[g_nefc_names].words, out, 64);
        g_nefc_names++;
    }
    log_line("effects: %ls is \"%ls\"", id, out);
}

/* `words` with "#0" made `value`. */
static void fill_value(const wchar_t *words, float value, wchar_t *out, int size)
{
    const wchar_t *at = wcsstr(words, L"#0");
    wchar_t number[16];

    if (!at) {
        lstrcpynW(out, words, size);
        return;
    }
    swprintf(number, 16, L"%d", (int)(value + (value < 0 ? -0.5f : 0.5f)));
    swprintf(out, (size_t)size, L"%.*ls%ls%ls", (int)(at - words), words, number, at + 2);
}

static void draw_effects(void *panel)
{
    static unsigned char *f_id, *f_name, *f_value, *f_left;
    static uint32_t stride;
    static void *known_class;
    wchar_t lines[8][80], words[64], name[64];
    unsigned colours[8];
    struct { unsigned char *data; int32_t count, max; } *list;
    void *pawn;
    int i, n = 0;

    if (!g_effects.show || !g_ui || (!g_layout.on && !hud_visible(panel)))
        return;
    pawn = object_at(object_at(g_ui, L"mPlayerCommonPawn"), L"mPawn");
    if (pawn && class_of(pawn) != known_class) {
        /* where the numbers are in one effect */
        unsigned char *array = field_of(class_of(pawn), L"mStatusAilmentArray"), *inner, *def;
        known_class = class_of(pawn);
        f_id = NULL;
        if (array && (inner = *(unsigned char **)(array + PROP_INNER)) && readable(inner, 0x100) &&
            (def = *(unsigned char **)(inner + PROP_INNER)) && readable(def, 0x100)) {
            stride = *(uint32_t *)(inner + PROP_SIZE);
            f_id = field_of(def, L"DbMsrEfcId");
            f_name = field_of(def, L"EffectName");
            f_value = field_of(def, L"Value");
            f_left = field_of(def, L"KeepTime");
            if (!f_name || !f_value || !f_left || stride < 0x34 || stride > 0x100)
                f_id = NULL;
        }
        log_line("effects: %s (each %u bytes)", f_id ? "the player's effects are found" : "no effects list", stride);
    }
    list = f_id ? value_at(pawn, L"mStatusAilmentArray", 16) : NULL;
    if (list && list->data && list->count > 0 && list->count < 64 && readable(list->data, (size_t)list->count * stride))
        for (i = 0; i < list->count && n < 8; i++) {
            unsigned char *e = list->data + i * stride;
            struct fstring *id = (void *)(e + offset_of(f_id)), *nm = (void *)(e + offset_of(f_name));
            float left = *(float *)(e + offset_of(f_left));
            int secs;
            if (!(left > 0.05f) || !id->data || id->count < 2 || id->count > 48 ||
                !readable(id->data, (size_t)id->count * 2))
                continue;
            name[0] = 0;
            if (nm->data && nm->count > 1 && nm->count < 64 && readable(nm->data, (size_t)nm->count * 2))
                lstrcpynW(name, nm->data, 64);
            effect_words(id->data, name, words, 64);
            fill_value(words, *(float *)(e + offset_of(f_value)), name, 64);
            secs = (int)(left + 0.99f);
            swprintf(lines[n], 80, L"%ls   %d:%02d", name, secs / 60, secs % 60);
            colours[n++] = g_effects.colour;
        }
    if (!n && g_layout.on) {
        lstrcpynW(lines[n], L"Attack Up 30%   0:24", 80);   /* the layout mode: a sample */
        colours[n++] = g_effects.colour;
        lstrcpynW(lines[n], L"Infinite Ammo   0:08", 80);
        colours[n++] = g_effects.colour;
    }
    draw_block(&g_effects, PART_EFFECTS, lines, NULL, colours, n, NULL, NULL);
}

/* ---- run totals: a trip into the tower, from leaving the Waiting Room until
 * coming back - its time, and what the fighter's Kill Coins, SP, Bloodnium
 * and EXP and the account's Death Metals went up (or down) by. Read twice a
 * second from the game's user data; the first changes go to the log.
 * Deaths: the game-over screen opening (the UI manager's mGameOverNew
 * leaving its idle state); continues: that screen's mContinue set (whatever
 * paid for it). A last death without a continue ends the trip in the Waiting
 * Room. (Health never reads 0 at a death: 0.11.0 counted none.) */
enum { VALUE_KC, VALUE_SP, VALUE_BLOOD, VALUE_EXP, VALUE_GAIN_EXP, VALUE_DM, VALUES };
static struct {
    int on, done;               /* a trip going on; one ended (in the Waiting Room) */
    ULONGLONG start, end;
    int chara;
    long long first[VALUES], last[VALUES];
    DWORD at;
    int logged;
    int deaths, continues;
    int dead, continued;        /* the game-over screen is open; it was a continue */
} g_trip;

/* The game-over screen: opening is a death, its mContinue a continue (once
 * per screen). Each frame while a trip is on; what the screen does goes to
 * the log, to check it. */
static void watch_deaths(void)
{
    static int logged, last_state = -1;
    static uint32_t last_flags;
    void *menu = object_at(g_ui, L"mGameOverNew");
    int32_t *state = menu ? value_at(menu, L"mState", 4) : NULL;
    uint32_t *flags = menu ? value_at(menu, L"mContinue", 4) : NULL;
    int open = state && *state != 0 && *state != 28;   /* not idle, not ended */

    if (state && flags && (*state != last_state || *flags != last_flags) && logged < 60) {
        logged++;
        log_line("run: the game-over screen is in state %d (flags %08X, continue %d)", *state, *flags,
                 get_bool(menu, L"mContinue"));
    }
    last_state = state ? *state : -1;
    last_flags = flags ? *flags : 0;
    if (open && !g_trip.dead) {
        g_trip.dead = 1;
        g_trip.continued = 0;
        g_trip.deaths++;
        log_line("run: the fighter died (%d so far)", g_trip.deaths);
    }
    if (open && g_trip.dead && !g_trip.continued && get_bool(menu, L"mContinue")) {
        g_trip.continued = 1;
        g_trip.continues++;
        log_line("run: continued (%d so far)", g_trip.continues);
    }
    if (!open)
        g_trip.dead = 0;
}

static int read_values(long long *v, int *chara)
{
    static const wchar_t *getters[VALUES] = {L"GetMoney", L"GetSpirit", L"GetBloodnium", L"GetExp", L"GetGainExp",
                                             L"GetMedal"};
    void *user = object_at(g_game, L"mUserData");
    int i, c;

    if (!user || (c = call_returning(user, L"GetCurrentCharaIndex", NULL, 0)) < 0)
        return 0;
    for (i = 0; i < VALUES; i++) {
        int got = call_returning(user, getters[i], &c, i == VALUE_DM ? 0 : 4);
        if (got < 0 && i != VALUE_GAIN_EXP)
            return 0;
        v[i] = got < 0 ? 0 : got;
    }
    *chara = c;
    return 1;
}

static void run_tick(void)
{
    long long v[VALUES];
    void *stage;
    int in_hub, chara, i;
    DWORD now = GetTickCount();

    if (now - g_trip.at < 500 || !g_game || !(stage = object_at(g_game, L"mStageManager")))
        return;
    g_trip.at = now;
    if ((in_hub = call_returning(stage, L"IsInHub", NULL, 0)) < 0 || !read_values(v, &chara))
        return;
    if (in_hub || (g_trip.on && chara != g_trip.chara)) {
        if (g_trip.on) {                        /* back in the Waiting Room: the trip is over */
            g_trip.on = 0;
            g_trip.done = 1;
            g_trip.end = GetTickCount64();
            log_line("run: back after %llu s - kill coins %lld, sp %lld, bloodnium %lld, exp %lld + %lld, death "
                     "metals %lld (they were %lld, %lld, %lld, %lld + %lld, %lld); %d deaths, %d continues",
                     (g_trip.end - g_trip.start) / 1000, g_trip.last[0], g_trip.last[1], g_trip.last[2],
                     g_trip.last[3], g_trip.last[4], g_trip.last[5], g_trip.first[0], g_trip.first[1],
                     g_trip.first[2], g_trip.first[3], g_trip.first[4], g_trip.first[5], g_trip.deaths,
                     g_trip.continues);
        }
        if (in_hub)
            return;
    }
    if (!g_trip.on) {                           /* into the tower: a new trip */
        g_trip.on = 1;
        g_trip.done = 0;
        g_trip.start = GetTickCount64();
        g_trip.chara = chara;
        memcpy(g_trip.first, v, sizeof v);
        g_trip.logged = 0;
        g_trip.deaths = g_trip.continues = g_trip.dead = g_trip.continued = 0;
        log_line("run: into the tower (fighter %d) - kill coins %lld, sp %lld, bloodnium %lld, exp %lld + %lld, "
                 "death metals %lld", chara, v[0], v[1], v[2], v[3], v[4], v[5]);
    }
    for (i = 0; i < VALUES; i++)
        if (v[i] != g_trip.last[i] && g_trip.logged < 20) {
            g_trip.logged++;
            log_line("run: %ls %lld -> %lld", (const wchar_t *[]){L"kill coins", L"sp", L"bloodnium", L"exp",
                     L"exp gained", L"death metals"}[i], g_trip.last[i], v[i]);
        }
    memcpy(g_trip.last, v, sizeof v);
}

static void draw_run(void *panel)
{
    wchar_t lines[RUN_LINES][80], number[32];
    unsigned colours[RUN_LINES];
    int kinds[RUN_LINES], n = 0, i, sample = 0;
    long long secs, d[RUN_LINES] = {0};

    if (!g_run.b.show || !g_ui || (!g_layout.on && !hud_visible(panel)))
        return;
    if (g_trip.on || (g_trip.done && g_run.hub)) {
        secs = (long long)(((g_trip.on ? GetTickCount64() : g_trip.end) - g_trip.start) / 1000);
        d[RUN_KC] = g_trip.last[VALUE_KC] - g_trip.first[VALUE_KC];
        d[RUN_SP] = g_trip.last[VALUE_SP] - g_trip.first[VALUE_SP];
        d[RUN_BLOOD] = g_trip.last[VALUE_BLOOD] - g_trip.first[VALUE_BLOOD];
        d[RUN_EXP] = g_trip.last[VALUE_EXP] + g_trip.last[VALUE_GAIN_EXP] - g_trip.first[VALUE_EXP] -
                     g_trip.first[VALUE_GAIN_EXP];
        d[RUN_DM] = g_trip.last[VALUE_DM] - g_trip.first[VALUE_DM];
        d[RUN_DEATHS] = g_trip.deaths;
        d[RUN_CONTINUES] = g_trip.continues;
    } else if (g_layout.on) {
        sample = 1;                             /* the layout mode: a sample */
        secs = 754;
        d[RUN_KC] = 12345;
        d[RUN_SP] = 120;
        d[RUN_BLOOD] = 3;
        d[RUN_EXP] = 4500;
        d[RUN_DM] = 1;
        d[RUN_DEATHS] = 2;
        d[RUN_CONTINUES] = 1;
    } else {
        return;
    }
    for (i = 0; i < RUN_LINES; i++) {
        if (!g_run.lines[i])
            continue;
        if (i == RUN_TIME && secs >= 3600)
            swprintf(lines[n], 80, L"%ls: %lld:%02lld:%02lld", !g_trip.on && !sample ? L"Last run" : L"Time",
                     secs / 3600, secs / 60 % 60, secs % 60);
        else if (i == RUN_TIME)
            swprintf(lines[n], 80, L"%ls: %lld:%02lld", !g_trip.on && !sample ? L"Last run" : L"Time", secs / 60,
                     secs % 60);
        else if (i == RUN_DEATHS || i == RUN_CONTINUES)
            swprintf(lines[n], 80, L"%ls: %lld", RUN_NAMES[i], d[i]);
        else {
            signed_number(number, 32, d[i]);
            swprintf(lines[n], 80, L"%ls: %ls", RUN_NAMES[i], number);
        }
        kinds[n] = i;
        colours[n++] = g_run.b.colour;
    }
    draw_block(&g_run.b, PART_RUN, lines, kinds, colours, n, g_run.off_x, g_run.off_y);
}

/* ----------------------------------------------------------- layout mode */

/* F8 in the game (or the key in [layout]) opens the layout mode: Tab picks a
 * part - durability numbers, the enemies counter, floor info, the minimap,
 * rage - drawn in
 * yellow; the arrow keys move it (Shift: 10 at a time), + and - size it, C
 * changes its place (on/above/below the bar; the minimap's corner), H shows or
 * hides it, R puts it back as it came. O opens the options page instead -
 * every other setting, in groups: Up and Down to pick one, Left and Right to
 * change it, Tab for the next group, R for its default - and O or Backspace
 * goes back. F8, Enter or Esc closes it
 * and writes lid-hud.ini. The keys come from the game window's own key messages: while
 * the layout mode is open, those keys - and typing - are kept from the game;
 * every other key still reaches it. */
static const wchar_t *PART_NAMES[PARTS] = {L"Durability numbers", L"Enemies counter", L"Floor info", L"Minimap",
                                           L"Rage", L"Bag space", L"Mushroom effects", L"Run totals"};

/* The text parts that are a block_settings, and where they go back to. */
static struct block_settings *part_block(int part, const struct block_settings **home)
{
    switch (part) {
    case PART_BAG: *home = &BAG_HOME; return &g_bag.b;
    case PART_EFFECTS: *home = &EFFECTS_HOME; return &g_effects;
    case PART_RUN: *home = &RUN_HOME; return &g_run.b;
    default: return NULL;
    }
}
static const wchar_t *ALIGNS[3] = {L"starting at x", L"centred on x", L"ending at x"};
static const wchar_t *PLACES[3] = {L"on the bar", L"above the bar", L"below the bar"};
static const wchar_t *SLOT_NAMES[6] = {L"right hand, slot 1", L"right hand, slot 2", L"right hand, slot 3",
                                       L"left hand, slot 1", L"left hand, slot 2", L"left hand, slot 3"};
static const wchar_t *LINE_NAMES[FLOOR_LINES] = {L"Beasts", L"Mushrooms", L"Chests", L"Materials", L"Fun Fungus"};

/* How many pieces a part has that move on their own (0: none). */
static int part_items(int part)
{
    return part == PART_DURABILITY ? 6 : part == PART_FLOOR ? FLOOR_LINES : part == PART_RUN ? RUN_LINES : 0;
}

static int *floor_line_switch(int line);

/* A piece's own move, across (axis 0) or down (1). */
static int *piece_offsets(int part, int axis)
{
    switch (part) {
    case PART_DURABILITY: return axis ? g_set.off_y : g_set.off_x;
    case PART_FLOOR: return axis ? g_floor.off_y : g_floor.off_x;
    case PART_RUN: return axis ? g_run.off_y : g_run.off_x;
    default: return NULL;
    }
}

/* The switch of a piece H shows or hides: a floor info or run line. */
static int *piece_switch(int part, int k)
{
    return part == PART_FLOOR ? floor_line_switch(k) : part == PART_RUN ? &g_run.lines[k] : NULL;
}

/* The floor info line the layout mode's H shows or hides. */
static int *floor_line_switch(int line)
{
    switch (line) {
    case LINE_BEASTS: return &g_floor.beasts;
    case LINE_MUSHROOMS: return &g_floor.mushrooms;
    case LINE_CHESTS: return &g_floor.chests;
    case LINE_MATERIALS: return &g_floor.materials;
    default: return &g_floor.fungus;
    }
}
static const wchar_t *CORNERS[6] = {L"top right", L"top left", L"bottom right", L"bottom left",
                                    L"right middle", L"left middle"};

static void ini_write(const wchar_t *section, const wchar_t *key, int value)
{
    wchar_t text[16];

    swprintf(text, 16, L"%d", value);
    WritePrivateProfileStringW(section, key, text, g_ini_path);
}

static void layout_changed(void);

/* The options page: every setting that is not a place or a size. */
static const wchar_t *OFF_ON[2] = {L"off", L"on"};
static const wchar_t *FORMATS[3] = {L"current / max", L"current", L"percent left"};
static const wchar_t *COLOUR_NAMES[8] = {L"red", L"orange", L"yellow", L"white", L"green", L"cyan", L"blue",
                                         L"magenta"};
/* the same, after the slot's own colour (COLOUR_GAME) */
static const wchar_t *GAME_COLOUR_NAMES[9] = {L"the slot's own", L"red", L"orange", L"yellow", L"white", L"green",
                                              L"cyan", L"blue", L"magenta"};
static const wchar_t *ARC_COLOUR_NAMES[6] = {L"the game's own", L"red", L"orange", L"yellow", L"green", L"cyan"};
static const wchar_t *ZOOMS[2] = {L"near", L"far"};

struct option {
    const wchar_t *group;           /* its heading on the page */
    const wchar_t *section, *key, *label;
    int *value;
    int lo, hi, step, fallback;
    const wchar_t *const *names;    /* NULL: a number */
    unsigned *colour;               /* a colour (and `value` NULL): `names` are COLOURS */
};

static const struct option OPTIONS[] = {
    {L"Durability numbers", L"durability", L"melee", L"Melee weapons", &g_set.melee, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Durability numbers", L"durability", L"guns", L"Guns and bows", &g_set.guns, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Durability numbers", L"durability", L"format", L"Shows", &g_set.format, 0, 2, 1, 0, FORMATS, NULL},
    {L"Durability numbers", L"durability", L"colour", L"Text colour", NULL, 0, 8, 1, 4, GAME_COLOUR_NAMES,
     &g_set.colour},
    {L"Durability numbers", L"durability", L"shadow", L"Outline", &g_set.shadow, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Durability numbers", L"durability", L"menu", L"In the pause menu", &g_set.menu, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Durability numbers", L"durability", L"warn", L"Warning colour below (%)", &g_set.warn, 0, 100, 5, 0, NULL,
     NULL},
    {L"Durability numbers", L"durability", L"warn_colour", L"Warning colour", NULL, 0, 7, 1, 2, COLOUR_NAMES,
     &g_set.warn_colour},
    {L"Durability numbers", L"durability", L"low", L"Low colour below (%)", &g_set.low, 0, 100, 5, 25, NULL, NULL},
    {L"Durability numbers", L"durability", L"low_colour", L"Low colour", NULL, 0, 7, 1, 0, COLOUR_NAMES,
     &g_set.low_colour},
    {L"Durability numbers", L"durability", L"flash", L"Flash when low", &g_set.flash, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Durability numbers", L"durability", L"popup", L"Pop-ups when it changes", &g_set.popup, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Durability numbers", L"durability", L"loss", L"Pop-up colour for a loss", &g_set.loss, 0, 7, 1, 0,
     COLOUR_NAMES, NULL},
    {L"Durability numbers", L"durability", L"gain", L"Pop-up colour for a gain", &g_set.gain, 0, 7, 1, 4,
     COLOUR_NAMES, NULL},
    {L"Durability numbers", L"durability", L"background", L"Background", &g_set.background, 0, 1, 1, 0, OFF_ON, NULL},
    {L"Enemies counter", L"enemies", L"show", L"Show", &g_enemies.show, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Enemies counter", L"enemies", L"total", L"The floor's total too", &g_enemies.total, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Enemies counter", L"enemies", L"colour", L"Text colour", NULL, 0, 7, 1, 3, COLOUR_NAMES, &g_enemies.colour},
    {L"Enemies counter", L"enemies", L"background", L"Background", &g_enemies.background, 0, 1, 1, 0, OFF_ON, NULL},
    {L"Floor info", L"floor", L"show", L"Show", &g_floor.show, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Floor info", L"floor", L"beasts", L"Beasts left", &g_floor.beasts, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Floor info", L"floor", L"chests", L"Chests left", &g_floor.chests, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Floor info", L"floor", L"materials", L"Materials left", &g_floor.materials, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Floor info", L"floor", L"fun_fungus", L"A Fun Fungus on the floor", &g_floor.fungus, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Floor info", L"floor", L"mushrooms", L"Mushrooms left", &g_floor.mushrooms, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Floor info", L"floor", L"colour", L"Text colour", NULL, 0, 7, 1, 3, COLOUR_NAMES, &g_floor.colour},
    {L"Floor info", L"floor", L"fungus_colour", L"Fun Fungus line colour", NULL, 0, 7, 1, 4, COLOUR_NAMES,
     &g_floor.fungus_colour},
    {L"Floor info", L"floor", L"background", L"Background", &g_floor.background, 0, 1, 1, 0, OFF_ON, NULL},
    {L"Rage", L"rage", L"show", L"Show how full the gauge is", &g_rage.show, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Rage", L"rage", L"gain", L"What the last hit gave", &g_rage.gain, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Rage", L"rage", L"colour", L"Text colour", NULL, 0, 7, 1, 3, COLOUR_NAMES, &g_rage.colour},
    {L"Rage", L"rage", L"background", L"Background", &g_rage.background, 0, 1, 1, 0, OFF_ON, NULL},
    {L"Bag space", L"bag", L"show", L"Show", &g_bag.b.show, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Bag space", L"bag", L"colour", L"Text colour", NULL, 0, 7, 1, 3, COLOUR_NAMES, &g_bag.b.colour},
    {L"Bag space", L"bag", L"full_colour", L"Colour when full", NULL, 0, 7, 1, 0, COLOUR_NAMES, &g_bag.full_colour},
    {L"Bag space", L"bag", L"background", L"Background", &g_bag.b.background, 0, 1, 1, 0, OFF_ON, NULL},
    {L"Mushroom effects", L"effects", L"show", L"Show", &g_effects.show, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Mushroom effects", L"effects", L"colour", L"Text colour", NULL, 0, 7, 1, 3, COLOUR_NAMES, &g_effects.colour},
    {L"Mushroom effects", L"effects", L"background", L"Background", &g_effects.background, 0, 1, 1, 0, OFF_ON, NULL},
    {L"Run totals", L"run", L"show", L"Show", &g_run.b.show, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Run totals", L"run", L"time", L"Time in the tower", &g_run.lines[RUN_TIME], 0, 1, 1, 1, OFF_ON, NULL},
    {L"Run totals", L"run", L"kill_coins", L"Kill Coins", &g_run.lines[RUN_KC], 0, 1, 1, 1, OFF_ON, NULL},
    {L"Run totals", L"run", L"sp", L"SP", &g_run.lines[RUN_SP], 0, 1, 1, 1, OFF_ON, NULL},
    {L"Run totals", L"run", L"bloodnium", L"Bloodnium", &g_run.lines[RUN_BLOOD], 0, 1, 1, 1, OFF_ON, NULL},
    {L"Run totals", L"run", L"exp", L"EXP", &g_run.lines[RUN_EXP], 0, 1, 1, 1, OFF_ON, NULL},
    {L"Run totals", L"run", L"death_metals", L"Death Metals", &g_run.lines[RUN_DM], 0, 1, 1, 1, OFF_ON, NULL},
    {L"Run totals", L"run", L"deaths", L"Deaths", &g_run.lines[RUN_DEATHS], 0, 1, 1, 1, OFF_ON, NULL},
    {L"Run totals", L"run", L"continues", L"Continues", &g_run.lines[RUN_CONTINUES], 0, 1, 1, 1, OFF_ON, NULL},
    {L"Run totals", L"run", L"hub", L"The last run in the Waiting Room", &g_run.hub, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Run totals", L"run", L"colour", L"Text colour", NULL, 0, 7, 1, 3, COLOUR_NAMES, &g_run.b.colour},
    {L"Run totals", L"run", L"background", L"Background", &g_run.b.background, 0, 1, 1, 0, OFF_ON, NULL},
    {L"Minimap", L"minimap", L"show", L"Show", &g_mini.show, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Minimap", L"minimap", L"zoom", L"Zoom", &g_mini.zoom, 0, 1, 1, 0, ZOOMS, NULL},
    {L"Minimap", L"minimap", L"zoom_percent", L"Fine zoom (%)", &g_mini.zoom_percent, 25, 400, 5, 100, NULL, NULL},
    {L"Area map (pause menu)", L"map", L"escalators", L"Escalator colours", &g_map.escalators, 0, 1, 1, 1,
     OFF_ON, NULL},
    {L"Area map (pause menu)", L"map", L"escalator_size", L"Escalator disc size", &g_map.escalator_size, 4, 120,
     2, 24, NULL, NULL},
    {L"Bow arc", L"bow", L"arc", L"Show", &g_bow.arc, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Bow arc", L"bow", L"stop_at_enemies", L"Stops at enemies", &g_bow.pawns, 0, 1, 1, 1, OFF_ON, NULL},
    {L"Bow arc", L"bow", L"enemy_colour", L"Colour when it ends on an enemy", &g_bow.enemy_colour, 0, 5, 1, 0,
     ARC_COLOUR_NAMES, NULL},
    {L"Bow arc", L"bow", L"headshot", L"Brighter on headshots", &g_bow.headshot, 0, 1, 1, 0, OFF_ON, NULL},
    {L"Screen", L"screen", L"hide_serial_killer", L"Hide the Serial Killer distortion", &g_set.hide_serial, 0, 1,
     1, 0, OFF_ON, NULL},
    {L"Screen", L"screen", L"background_darkness", L"Background darkness (%)", &g_background_darkness, 10, 100, 10,
     50, NULL, NULL},
    {L"This menu", L"layout", L"text_size", L"Text size (%)", &g_menu.text_size, 80, 200, 10, 100, NULL, NULL},
    {L"This menu", L"layout", L"thick_text", L"Thicker text", &g_menu.thick, 0, 1, 1, 1, OFF_ON, NULL},
    {L"This menu", L"layout", L"panel", L"Dark panel behind it (B)", &g_menu.panel, 0, 1, 1, 1, OFF_ON, NULL},
};
#define OPTION_COUNT ((int)(sizeof OPTIONS / sizeof OPTIONS[0]))

static unsigned colour_rgb(int i)
{
    return (unsigned)COLOURS[i][0] << 16 | (unsigned)COLOURS[i][1] << 8 | COLOURS[i][2];
}

/* A colour option's place in its list (after "the slot's own", where there is
 * one), or -1 for a colour of the file's own. */
static int colour_index(const struct option *o)
{
    int own = o->names == GAME_COLOUR_NAMES, i;

    if (own && *o->colour == COLOUR_GAME)
        return 0;
    for (i = 0; i < 8; i++)
        if (colour_rgb(i) == *o->colour)
            return i + own;
    return -1;
}

static int option_value(const struct option *o)
{
    return o->colour ? colour_index(o) : *o->value;
}

static void set_option(const struct option *o, int v)
{
    int own = o->names == GAME_COLOUR_NAMES;

    if (o->colour)
        *o->colour = own && v == 0 ? COLOUR_GAME : colour_rgb(v - own);
    else
        *o->value = v;
}

/* Left or right on an option: lists go round, numbers stop at their ends. */
static void change_option(int by, int fast)
{
    const struct option *o = &OPTIONS[g_layout.option];
    int v = option_value(o), n = o->hi - o->lo + 1;

    if (o->names)
        v = v < o->lo ? (by > 0 ? o->lo : o->hi) : o->lo + ((v - o->lo + by) % n + n) % n;
    else
        v = clamp(v + by * o->step * (fast ? 5 : 1), o->lo, o->hi);
    set_option(o, v);
    layout_changed();
}

static int same_group(int a, int b)
{
    return wcscmp(OPTIONS[a].group, OPTIONS[b].group) == 0;
}

/* Tab on the options page: the first option of the next (or, back, the
 * previous) group. */
static void next_group(int way)
{
    int i = g_layout.option, n;

    for (n = 0; n < OPTION_COUNT; n++) {
        int prev;
        i = (i + way + OPTION_COUNT) % OPTION_COUNT;
        prev = (i + OPTION_COUNT - 1) % OPTION_COUNT;
        if (i == 0 || !same_group(i, prev)) {
            if (way < 0 && same_group(i, g_layout.option))
                continue;       /* back: past the start of its own group */
            break;
        }
    }
    g_layout.option = i;
}

static void layout_save(void)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    wchar_t hex[8];
    int i;

    if (!g_ini_path[0])
        return;
    for (i = 0; i < OPTION_COUNT; i++) {
        const struct option *o = &OPTIONS[i];
        if (o->value)
            ini_write(o->section, o->key, *o->value);
        else if (o->colour) {
            if (*o->colour == COLOUR_GAME)
                lstrcpyW(hex, L"game");
            else
                swprintf(hex, 8, L"%06X", *o->colour & 0xFFFFFF);
            WritePrivateProfileStringW(o->section, o->key, hex, g_ini_path);
        }
    }
    ini_write(L"durability", L"position", g_set.position);
    ini_write(L"durability", L"size", g_set.size);
    ini_write(L"durability", L"x", g_set.x);
    ini_write(L"durability", L"y", g_set.y);
    for (i = 0; i < 6; i++) {
        wchar_t key[24];
        swprintf(key, 24, L"%ls_x", SLOT_KEYS[i]);
        ini_write(L"durability", key, g_set.off_x[i]);
        swprintf(key, 24, L"%ls_y", SLOT_KEYS[i]);
        ini_write(L"durability", key, g_set.off_y[i]);
    }
    for (i = 0; i < FLOOR_LINES; i++) {
        wchar_t key[24];
        swprintf(key, 24, L"%ls_x", LINE_KEYS[i]);
        ini_write(L"floor", key, g_floor.off_x[i]);
        swprintf(key, 24, L"%ls_y", LINE_KEYS[i]);
        ini_write(L"floor", key, g_floor.off_y[i]);
    }
    ini_write(L"enemies", L"x", g_enemies.x);
    ini_write(L"enemies", L"y", g_enemies.y);
    ini_write(L"enemies", L"size", g_enemies.size);
    ini_write(L"floor", L"x", g_floor.x);
    ini_write(L"floor", L"y", g_floor.y);
    ini_write(L"floor", L"size", g_floor.size);
    ini_write(L"floor", L"align", g_floor.align);
    ini_write(L"rage", L"x", g_rage.x);
    ini_write(L"rage", L"y", g_rage.y);
    ini_write(L"rage", L"size", g_rage.size);
    ini_write(L"rage", L"align", g_rage.align);
    {
        static const wchar_t *sections[3] = {L"bag", L"effects", L"run"};
        struct block_settings *blocks[3] = {&g_bag.b, &g_effects, &g_run.b};
        for (i = 0; i < 3; i++) {
            ini_write(sections[i], L"x", blocks[i]->x);
            ini_write(sections[i], L"y", blocks[i]->y);
            ini_write(sections[i], L"size", blocks[i]->size);
            ini_write(sections[i], L"align", blocks[i]->align);
        }
        for (i = 0; i < RUN_LINES; i++) {
            wchar_t key[32];
            swprintf(key, 32, L"%ls_x", RUN_KEYS[i]);
            ini_write(L"run", key, g_run.off_x[i]);
            swprintf(key, 32, L"%ls_y", RUN_KEYS[i]);
            ini_write(L"run", key, g_run.off_y[i]);
        }
    }
    ini_write(L"minimap", L"corner", g_mini.corner);
    ini_write(L"minimap", L"x", g_mini.x);
    ini_write(L"minimap", L"y", g_mini.y);
    ini_write(L"minimap", L"size", g_mini.size);
    WritePrivateProfileStringW(NULL, NULL, NULL, g_ini_path);      /* flush */
    /* the file now says what is in use: no need to read it again */
    if (GetFileAttributesExW(g_ini_path, GetFileExInfoStandard, &a))
        g_ini_time = a.ftLastWriteTime;
    log_line("layout: saved to lid-hud.ini (and %d options) - durability x %d, y %d, size %d%%, place %d; enemies x %d, y %d, "
             "size %d%%; minimap corner %d, x %d, y %d, size %d", OPTION_COUNT, g_set.x, g_set.y, g_set.size, g_set.position,
             g_enemies.x, g_enemies.y, g_enemies.size, g_mini.corner, g_mini.x, g_mini.y, g_mini.size);
}

static void layout_changed(void)
{
    g_layout.changed = GetTickCount();
    g_settings_changed = 1;         /* the HUD draws its bars again at once */
}

static void layout_move(int dx, int dy)
{
    int k = g_layout.item - 1;

    const struct block_settings *home;
    struct block_settings *b = part_block(g_layout.part, &home);

    if (k >= 0 && piece_offsets(g_layout.part, 0)) {        /* one piece on its own */
        int *ox = piece_offsets(g_layout.part, 0), *oy = piece_offsets(g_layout.part, 1);
        ox[k] = clamp(ox[k] + dx, -1000, 1000);
        oy[k] = clamp(oy[k] + dy, -1000, 1000);
        layout_changed();
        return;
    }
    if (b) {
        b->x = clamp(b->x + dx, 0, 1920);
        b->y = clamp(b->y + dy, 0, 1080);
        layout_changed();
        return;
    }
    switch (g_layout.part) {
    case PART_DURABILITY:
        g_set.x = clamp(g_set.x + dx, -300, 300);
        g_set.y = clamp(g_set.y + dy, -300, 300);
        break;
    case PART_ENEMIES:
        g_enemies.x = clamp(g_enemies.x + dx, 0, 1920);
        g_enemies.y = clamp(g_enemies.y + dy, 0, 1080);
        break;
    case PART_FLOOR:
        g_floor.x = clamp(g_floor.x + dx, 0, 1920);
        g_floor.y = clamp(g_floor.y + dy, 0, 1080);
        break;
    case PART_RAGE:
        g_rage.x = clamp(g_rage.x + dx, 0, 1920);
        g_rage.y = clamp(g_rage.y + dy, 0, 1080);
        break;
    default:
        /* x and y count from the minimap's corner */
        /* past 0: the map draws a little inside its box, so it can go further
         * to sit right on the screen's edge */
        g_mini.x = clamp(g_mini.x + (g_mini.corner % 2 ? dx : -dx), -1000, 2400);
        g_mini.y = clamp(g_mini.y + (g_mini.corner == 2 || g_mini.corner == 3 ? -dy : dy), -1500, 1500);
        break;
    }
    layout_changed();
}

static void layout_size(int more, int fast)
{
    const struct block_settings *home;
    struct block_settings *b = part_block(g_layout.part, &home);

    if (b) {
        b->size = clamp(b->size + more * (fast ? 20 : 5), 40, 300);
        layout_changed();
        return;
    }
    switch (g_layout.part) {
    case PART_DURABILITY:
        g_set.size = clamp(g_set.size + more * (fast ? 20 : 5), 30, 300);
        break;
    case PART_ENEMIES:
        g_enemies.size = clamp(g_enemies.size + more * (fast ? 20 : 5), 40, 300);
        break;
    case PART_FLOOR:
        g_floor.size = clamp(g_floor.size + more * (fast ? 20 : 5), 40, 300);
        break;
    case PART_RAGE:
        g_rage.size = clamp(g_rage.size + more * (fast ? 20 : 5), 40, 300);
        break;
    default:
        g_mini.size = clamp(g_mini.size + more * (fast ? 50 : 10), 50, 1000);
        break;
    }
    layout_changed();
}

static void layout_other(UINT key)
{
    int k = g_layout.item - 1;

    /* one piece on its own: R puts it back with the rest; H shows or hides a
     * floor info line (a durability slot goes with the whole part) */
    const struct block_settings *home;
    struct block_settings *b = part_block(g_layout.part, &home);

    if (k >= 0 && key == 'R' && piece_offsets(g_layout.part, 0)) {
        piece_offsets(g_layout.part, 0)[k] = piece_offsets(g_layout.part, 1)[k] = 0;
        layout_changed();
        return;
    }
    if (k >= 0 && key == 'H' && piece_switch(g_layout.part, k)) {
        int *on = piece_switch(g_layout.part, k);
        *on = !*on;
        layout_changed();
        return;
    }
    if (b) {
        if (key == 'C')
            b->align = (b->align + 1) % 3;
        else if (key == 'H')
            b->show = !b->show;
        else {
            b->x = home->x;
            b->y = home->y;
            b->size = home->size;
            b->align = home->align;
            if (g_layout.part == PART_RUN) {
                memset(g_run.off_x, 0, sizeof g_run.off_x);
                memset(g_run.off_y, 0, sizeof g_run.off_y);
            }
        }
        layout_changed();
        return;
    }
    switch (g_layout.part) {
    case PART_DURABILITY:
        if (key == 'C')
            g_set.position = (g_set.position + 1) % 3;
        else if (key == 'H')
            g_set.melee = g_set.guns = !(g_set.melee || g_set.guns);
        else {
            g_set.x = g_set.y = g_set.position = 0;
            g_set.size = 100;
            memset(g_set.off_x, 0, sizeof g_set.off_x);
            memset(g_set.off_y, 0, sizeof g_set.off_y);
        }
        break;
    case PART_ENEMIES:
        if (key == 'H')
            g_enemies.show = !g_enemies.show;
        else if (key == 'R') {
            g_enemies.x = 1880;
            g_enemies.y = 160;
            g_enemies.size = 100;
            g_enemies.align = 2;
        }
        break;
    case PART_FLOOR:
        if (key == 'C')
            g_floor.align = (g_floor.align + 1) % 3;
        else if (key == 'H')
            g_floor.show = !g_floor.show;
        else {
            g_floor.x = 1880;
            g_floor.y = 200;
            g_floor.size = 80;
            g_floor.align = 2;
            memset(g_floor.off_x, 0, sizeof g_floor.off_x);
            memset(g_floor.off_y, 0, sizeof g_floor.off_y);
        }
        break;
    case PART_RAGE:
        if (key == 'C')
            g_rage.align = (g_rage.align + 1) % 3;
        else if (key == 'H')
            g_rage.show = !g_rage.show;
        else {
            g_rage.x = 60;
            g_rage.y = 200;
            g_rage.size = 80;
            g_rage.align = 0;
        }
        break;
    default:
        if (key == 'C')
            g_mini.corner = (g_mini.corner + 1) % 6;
        else if (key == 'H')
            g_mini.show = !g_mini.show;
        else {
            g_mini.corner = 0;
            g_mini.x = 40;
            g_mini.y = 200;
            g_mini.size = 320;
        }
        break;
    }
    layout_changed();
}

/* A key went down: 1 if the layout mode took it. */
static int layout_key(UINT key, int repeat)
{
    int shift = GetKeyState(VK_SHIFT) < 0;

    if (key == g_layout_key || (g_layout.on && (key == VK_ESCAPE || key == VK_RETURN))) {
        if (repeat)
            return 1;
        g_layout.on = !g_layout.on;
        g_layout.page = 0;
        if (g_layout.on)
            log_line("layout: open");
        else
            layout_save();
        g_settings_changed = 1;
        return 1;
    }
    if (!g_layout.on)
        return 0;
    if (key == 'B') {
        if (!repeat) {
            g_menu.panel = !g_menu.panel;
            layout_changed();
        }
        return 1;
    }
    if (key == 'O' || (key == VK_BACK && g_layout.page)) {
        if (!repeat)
            g_layout.page = !g_layout.page;
        g_settings_changed = 1;
        return 1;
    }
    if (g_layout.page) {
        switch (key) {
        case VK_UP:
            g_layout.option = (g_layout.option + OPTION_COUNT * 5 - (shift ? 5 : 1)) % OPTION_COUNT;
            return 1;
        case VK_DOWN:
            g_layout.option = (g_layout.option + (shift ? 5 : 1)) % OPTION_COUNT;
            return 1;
        case VK_LEFT:
            change_option(-1, shift);
            return 1;
        case VK_RIGHT:
            change_option(1, shift);
            return 1;
        case 'R':
            if (!repeat) {
                set_option(&OPTIONS[g_layout.option], OPTIONS[g_layout.option].fallback);
                layout_changed();
            }
            return 1;
        case VK_TAB:
            if (!repeat)
                next_group(shift ? -1 : 1);
            return 1;
        case VK_OEM_PLUS:
        case VK_ADD:
        case VK_OEM_MINUS:
        case VK_SUBTRACT:
        case 'C':
        case 'H':
            return 1;           /* the layout page's keys: nothing here */
        }
        return 0;
    }
    switch (key) {
    case VK_TAB:
        if (!repeat) {
            g_layout.part = (g_layout.part + (shift ? PARTS - 1 : 1)) % PARTS;
            g_layout.item = 0;
        }
        g_settings_changed = 1;
        return 1;
    case VK_NEXT:               /* Page Down, Page Up: one piece of the part */
    case VK_PRIOR:
        if (part_items(g_layout.part)) {
            int n = part_items(g_layout.part) + 1;
            g_layout.item = (g_layout.item + (key == VK_NEXT ? 1 : n - 1)) % n;
        }
        g_settings_changed = 1;
        return 1;
    case VK_LEFT:
        layout_move(shift ? -10 : -1, 0);
        return 1;
    case VK_RIGHT:
        layout_move(shift ? 10 : 1, 0);
        return 1;
    case VK_UP:
        layout_move(0, shift ? -10 : -1);
        return 1;
    case VK_DOWN:
        layout_move(0, shift ? 10 : 1);
        return 1;
    case VK_OEM_PLUS:
    case VK_ADD:
        layout_size(1, shift);
        return 1;
    case VK_OEM_MINUS:
    case VK_SUBTRACT:
        layout_size(-1, shift);
        return 1;
    case 'C':
    case 'H':
    case 'R':
        if (!repeat)
            layout_other(key);
        return 1;
    }
    return 0;
}

#define MENU_PANEL 0x101010u
#define MENU_PICKED 0x4A4410u
#define MENU_VALUE 0xA8DCFFu
#define MENU_HEADING 0x8CC8FFu

/* The options page: a heading per group, a setting's name on the left and its
 * value on the right, as many as fit, around the one picked. */
static void draw_options(void)
{
    struct { int option; } rows[OPTION_COUNT * 2];  /* -1 - k: the heading of option k's group */
    wchar_t value[64];
    const struct option *o;
    float k = g_menu.text_size / 100.0f, row_h = 40 * k, head_h = 46 * k, help_h = 32 * k;
    float top = 30, y, left, right, half = 0, w;
    int n = 0, at = 0, first, i, v, shown, picked;

    for (i = 0; i < OPTION_COUNT; i++) {
        if (i == 0 || !same_group(i, i - 1))
            rows[n++].option = -1 - i;
        if (i == g_layout.option)
            at = n;
        rows[n++].option = i;
    }
    /* as many rows as fit between the title and the help at the bottom */
    shown = clamp((int)((1080 - 2 * top - 56 * k - 2 * help_h - 30 * k) / head_h), 5, n);
    first = clamp(at - shown / 2, 0, n > shown ? n - shown : 0);
    if (first > 0 && rows[first - 1].option < 0)
        first--;                                /* keep a group's heading with it */

    /* the panel: as wide as the widest row */
    for (i = 0; i < OPTION_COUNT; i++) {
        o = &OPTIONS[i];
        w = text_width(o->label, 85 * g_menu.text_size / 100) + 40 * k + text_width(L"<  your own  >", 85 *
                                                                                   g_menu.text_size / 100);
        if (w > half * 2)
            half = w / 2;
    }
    w = text_width(L"Left / Right: change (Shift: faster)    Tab: next group    R: default", 70 * g_menu.text_size
                   / 100);
    if (w > half * 2)
        half = w / 2;
    half = half < 300 * k ? 300 * k : half + 40 * k;
    if (half > 940)
        half = 940;
    left = 960 - half + 30 * k;
    right = 960 + half - 30 * k;

    y = top + 56 * k;
    for (i = first; i < n && i < first + shown; i++)
        y += rows[i].option < 0 ? head_h + (i > first ? 8 * k : 0) : row_h;
    if (g_menu.panel)
        draw_box(960 - half, top - 12 * k, 960 + half, y + 14 * k + 2 * help_h + 10 * k, MENU_PANEL, 190);

    menu_line(L"HUD options", 960, top, 1, 95, 0xFFFFFF);
    y = top + 56 * k;
    for (i = first; i < n && i < first + shown; i++) {
        if (rows[i].option < 0) {
            if (i > first)
                y += 8 * k;                     /* a little room before each group */
            menu_line(OPTIONS[-1 - rows[i].option].group, 960, y + 4 * k, 1, 90, MENU_HEADING);
            y += head_h;
            continue;
        }
        o = &OPTIONS[rows[i].option];
        picked = rows[i].option == g_layout.option;
        v = option_value(o);
        if (o->names)
            swprintf(value, 64, picked ? L"<  %ls  >" : L"%ls", v >= o->lo && v <= o->hi ? o->names[v - o->lo]
                                                                                       : L"your own");
        else
            swprintf(value, 64, picked ? L"<  %d  >" : L"%d", v);
        if (picked && g_menu.panel)
            draw_box(960 - half + 8 * k, y - 3 * k, 960 + half - 8 * k, y + row_h - 5 * k, MENU_PICKED, 220);
        menu_line(o->label, left, y, 0, 85, picked ? LAYOUT_YELLOW : 0xFFFFFF);
        menu_line(value, right, y, 2, 85, picked ? LAYOUT_YELLOW : MENU_VALUE);
        y += row_h;
    }
    y += 14 * k;
    menu_line(L"Up / Down: pick    Left / Right: change (Shift: faster)    Tab: next group    R: default", 960, y, 1,
              70, 0xFFFFFF);
    menu_line(L"O: back to the layout    B: the panel    F8: save and close", 960, y + help_h, 1, 70, 0xFFFFFF);
}

static void draw_layout_help(void)
{
    static const wchar_t *HELP[2] = {
        L"Tab: next part    Page Up / Down: one piece of it    Arrows: move (Shift: faster)    + / -: size",
        L"C: place    H: show / hide    R: reset    O: options    B: the panel    F8: save and close"};
    wchar_t what[160];
    float k = g_menu.text_size / 100.0f, half, w;
    int i, piece;

    if (!g_layout.on)
        return;
    if (g_layout.page) {
        draw_options();
        return;
    }
    switch (g_layout.item ? -1 : g_layout.part) {
    case -1:                    /* one piece on its own */
        piece = g_layout.item - 1;
        if (g_layout.part == PART_DURABILITY)
            swprintf(what, 160, L"%ls, %ls: x %+d, y %+d from the rest", PART_NAMES[0], SLOT_NAMES[piece],
                     g_set.off_x[piece], g_set.off_y[piece]);
        else
            swprintf(what, 160, L"%ls, %ls%ls: x %+d, y %+d from the rest", PART_NAMES[g_layout.part],
                     g_layout.part == PART_RUN ? RUN_NAMES[piece] : LINE_NAMES[piece],
                     *piece_switch(g_layout.part, piece) ? L"" : L" (hidden)",
                     piece_offsets(g_layout.part, 0)[piece], piece_offsets(g_layout.part, 1)[piece]);
        break;
    case PART_BAG:
    case PART_EFFECTS:
    case PART_RUN: {
        const struct block_settings *home;
        struct block_settings *b = part_block(g_layout.part, &home);
        swprintf(what, 160, L"%ls%ls: x %d, y %d, size %d%%, %ls", PART_NAMES[g_layout.part],
                 b->show ? L"" : L" (hidden)", b->x, b->y, b->size, ALIGNS[b->align]);
        break;
    }
    case PART_DURABILITY:
        swprintf(what, 160, L"%ls%ls: x %d, y %d, size %d%%, %ls", PART_NAMES[0],
                 g_set.melee || g_set.guns ? L"" : L" (hidden)", g_set.x, g_set.y, g_set.size,
                 PLACES[g_set.position]);
        break;
    case PART_ENEMIES:
        swprintf(what, 160, L"%ls%ls: x %d, y %d, size %d%%", PART_NAMES[1], g_enemies.show ? L"" : L" (hidden)",
                 g_enemies.x, g_enemies.y, g_enemies.size);
        break;
    case PART_FLOOR:
        swprintf(what, 160, L"%ls%ls: x %d, y %d, size %d%%, %ls", PART_NAMES[2], g_floor.show ? L"" : L" (hidden)",
                 g_floor.x, g_floor.y, g_floor.size, ALIGNS[g_floor.align]);
        break;
    case PART_RAGE:
        swprintf(what, 160, L"%ls%ls: x %d, y %d, size %d%%, %ls", PART_NAMES[4], g_rage.show ? L"" : L" (hidden)",
                 g_rage.x, g_rage.y, g_rage.size, ALIGNS[g_rage.align]);
        break;
    default:
        swprintf(what, 160, L"%ls%ls: %ls, x %d, y %d, size %d", PART_NAMES[3], g_mini.show ? L"" : L" (hidden)",
                 CORNERS[g_mini.corner], g_mini.x, g_mini.y, g_mini.size);
        break;
    }
    if (g_menu.panel) {
        half = text_width(what, 90 * g_menu.text_size / 100) / 2;
        for (i = 0; i < 2; i++)
            if ((w = text_width(HELP[i], 70 * g_menu.text_size / 100) / 2) > half)
                half = w;
        half = half < 300 * k ? 300 * k : half + 40 * k;
        if (half > 940)
            half = 940;
        draw_box(960 - half, 12, 960 + half, 36 + 150 * k, MENU_PANEL, 190);
    }
    menu_line(L"HUD layout", 960, 24, 1, 95, 0xFFFFFF);
    menu_line(what, 960, 24 + 44 * k, 1, 90, LAYOUT_YELLOW);
    for (i = 0; i < 2; i++)
        menu_line(HELP[i], 960, 24 + (88 + 32 * i) * k, 1, 70, 0xFFFFFF);
}

/* The game window's key messages pass here first. */
static WNDPROC g_old_window_proc;
static HWND g_window;
static int g_window_unicode;

static LRESULT CALLBACK layout_window_proc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && layout_key((UINT)w, (l & 0x40000000) != 0))
        return 0;
    if ((msg == WM_CHAR || msg == WM_SYSCHAR) && g_layout.on)
        return 0;
    return g_window_unicode ? CallWindowProcW(g_old_window_proc, h, msg, w, l)
                            : CallWindowProcA(g_old_window_proc, h, msg, w, l);
}

/* `h` is the window in front: when it is the game's own, on this thread, its
 * key messages are watched (once per window). */
static void watch_window(HWND h)
{
    wchar_t cls[64];
    DWORD pid = 0;
    LONG_PTR old;

    if (g_window && IsWindow(g_window))
        return;
    g_window = NULL;
    if (!h || GetWindowThreadProcessId(h, &pid) != GetCurrentThreadId() || pid != GetCurrentProcessId())
        return;
    g_window_unicode = IsWindowUnicode(h);
    old = g_window_unicode ? SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)layout_window_proc)
                           : SetWindowLongPtrA(h, GWLP_WNDPROC, (LONG_PTR)layout_window_proc);
    if (!old)
        return;
    g_old_window_proc = (WNDPROC)old;
    g_window = h;
    if (!GetClassNameW(h, cls, 64))
        cls[0] = 0;
    log_line("layout: F%d in the game opens the layout mode (window \"%ls\")", (int)(g_layout_key - VK_F1 + 1),
             cls);
}

/* ----------------------------------------------------------- each frame */

static void *world_game(void)
{
    unsigned char *world, *level;
    void **actors;

    if (!readable(g_pc.gworld, 8) || !(world = *g_pc.gworld) || !readable(world, 0x100))
        return NULL;
    level = *(unsigned char **)(world + 0x80);
    if (!level || !readable(level, 0x68))
        return NULL;
    actors = *(void ***)(level + 0x60);
    if (!actors || !readable(actors, 8) || !actors[0])
        return NULL;
    return object_at(actors[0], L"Game");      /* actor 0 is the WorldInfo */
}

static void on_frame(HWND front)
{
    void *ui;

    watch_settings();
    watch_window(front);
    g_game = world_game();
    ui = object_at(g_game, L"mUIManager");
    g_ui = ui;
    g_status = object_at(ui, L"mPlayerStatus");
    g_font = object_at(ui, L"mFontRender");
    g_pawn = object_at(ui, L"mPlayerCommonPawnNative");
    g_fighter = object_at(ui, L"mCommonFighterStatus");
    g_hud = object_at(ui, L"mHUD");
    g_menu_drawn = 0;
    if (durability_ok(&g_pc) && ui) {
        hook_gauges(g_hud);
        track_changes();
        if (g_settings_changed && g_status) {
            g_settings_changed = 0;
            request_update();       /* show new settings at once */
        }
        {
            /* a low number flashing: the bars are drawn again as it turns */
            static DWORD half;
            DWORD now = GetTickCount();
            if (g_set.flash && now - g_flashing_at < 1000 && now / 400 != half) {
                half = now / 400;               /* the flash turned: draw the bars again */
                request_update();
            }
        }
    }
    if ((enemies_ok(&g_pc) || minimap_ok(&g_pc)) && g_status)
        hook_status(g_status);
    serial_killer_effect();
    if (map_ok(&g_pc) && ui)
        hook_map(object_at(ui, L"mPauseMap"));
    if (arc_ok(&g_pc))
        bow_arc();
    count_floor();
    run_tick();
    if (g_trip.on)
        watch_deaths();
    if (minimap_ok(&g_pc)) {
        keep_minimap();
        if (g_mm.map)
            hook_icons(object_at(g_mm.map, L"mMiniMapRender"));
    }
}

typedef HWND (WINAPI *foreground_fn)(void);
static foreground_fn g_real_foreground;

/* Stands in for GetForegroundWindow. The game's main loop calls it once a
 * frame; that call - and only that one - also runs the plugin's frame work. */
static HWND WINAPI foreground_hook(void)
{
    HWND h = g_real_foreground();
    void **sp = __builtin_frame_address(0);
    void *want = g_pc.frame_call + 6;
    int i;

    if (__builtin_return_address(0) == want) {
        on_frame(h);
    } else if (readable(sp, 64 * 8)) {
        /* called through another plugin's hook of the same function */
        for (i = 0; i < 64; i++)
            if (sp[i] == want) {
                on_frame(h);
                break;
            }
    }
    return h;
}

static void patch_game(void)
{
    unsigned char *base = (unsigned char *)GetModuleHandleW(NULL);
    DWORD old;

    find_pieces(base, &g_pc);
    if (!engine_ok(&g_pc)) {
        log_line("the engine code it needs was not found (world %d, names %d, functions %d, frame %d) "
                 "- nothing changed. A game update may have moved it.",
                 g_pc.gworld != NULL, g_pc.fname != NULL, g_pc.find_function != NULL,
                 g_pc.frame_import != NULL);
        return;
    }
    if (!durability_ok(&g_pc))
        log_line("durability: the HUD drawing code was not found (text %d, width %d, gauge %d, slots %d) "
                 "- numbers stay off", g_pc.vt_text != 0, g_pc.vt_text_width != 0, g_pc.vt_gauge != 0,
                 g_pc.slot_fn != NULL);
    if (!readable(g_pc.frame_import, 8) || !*g_pc.frame_import ||
        !VirtualProtect(g_pc.frame_import, 8, PAGE_READWRITE, &old)) {
        log_line("could not hook the frame loop - nothing changed");
        return;
    }
    g_real_foreground = (foreground_fn)*g_pc.frame_import;
    *g_pc.frame_import = (void *)foreground_hook;
    VirtualProtect(g_pc.frame_import, 8, old, &old);
    if (!enemies_ok(&g_pc))
        log_line("enemies left: the HUD's drawing was not found - the counter stays off");
    if (!map_ok(&g_pc))
        log_line("map: the area map's escalator code was not found - escalator colours stay off");
    if (!arc_ok(&g_pc))
        log_line("bow arc: the engine code it needs was not found (line %d, check %d, memory %d %d) "
                 "- the arc stays off", g_pc.vt_sim_projectile != 0, g_pc.line_check != NULL,
                 g_pc.mem_alloc != NULL, g_pc.mem_free != NULL);
    if (!minimap_ok(&g_pc))
        log_line("minimap: the engine code it needs was not found (find %d, make %d, icons %d) "
                 "- the minimap stays off", g_pc.find_object != NULL, g_pc.make_object != NULL,
                 g_pc.vt_render_icon != 0);
    log_line("ready: the frame loop is hooked%s%s%s%s%s", durability_ok(&g_pc) ? "; durability numbers on" : "",
             enemies_ok(&g_pc) ? "; enemies left on" : "", map_ok(&g_pc) ? "; map escalators on" : "",
             arc_ok(&g_pc) ? "; bow arc on" : "", minimap_ok(&g_pc) ? "; minimap on" : "");
}

/* ---------------------------------------------------------------- start */

static int running_in_the_game(void)
{
    wchar_t exe[MAX_PATH];
    const wchar_t *name;
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);

    if (n == 0 || n >= MAX_PATH)
        return 0;
    name = wcsrchr(exe, L'\\');
    name = name ? name + 1 : exe;
    return _wcsicmp(name, L"BrgGame-Steam.exe") == 0;
}

/* g_log_path and g_ini_path: the plugin's own name with .log and .ini */
static void own_files(HINSTANCE inst)
{
    DWORD n = GetModuleFileNameW(inst, g_log_path, MAX_PATH);
    wchar_t *slash = (n && n < MAX_PATH - 16) ? wcsrchr(g_log_path, L'\\') : NULL;

    if (!slash) {
        g_log_path[0] = 0;
        return;
    }
    wcscpy(slash + 1, L"lid-hud.ini");
    wcscpy(g_ini_path, g_log_path);
    wcscpy(slash + 1, L"lid-hud.log");
}

#ifndef LID_HUD_NO_DLLMAIN
BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        own_files(inst);
        if (!running_in_the_game())
            return TRUE;    /* some other program - change nothing */
        log_line("LiD HUD lid-hud.dll " LID_DLL_VERSION " loaded");
        watch_settings();
        patch_game();
    }
    return TRUE;
}
#endif
