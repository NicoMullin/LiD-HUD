/* The layout mode, without the game: keys go to the same function the game
 * window's key messages go to, and closing it must write lid-hud.ini so that
 * reading the file gives back what was set. */
#define LID_HUD_NO_DLLMAIN
#include <windows.h>

/* The keys are the test's own: Shift is never down, whatever the keyboard
 * the test runs on is doing. */
static SHORT test_key_state(int key)
{
    (void)key;
    return 0;
}
#define GetKeyState test_key_state

#include "../dll/lid_hud.c"

static int fails;

static void check(const char *what, int got, int want)
{
    printf("  %-44s %d%s\n", what, got, got == want ? "" : "  WRONG");
    if (got != want) {
        printf("    (wanted %d)\n", want);
        fails++;
    }
}

static int press(UINT key)
{
    return layout_key(key, 0);
}

int main(void)
{
    DWORD n = GetCurrentDirectoryW(MAX_PATH - 16, g_ini_path);
    int i;

    if (!n || n >= MAX_PATH - 16) { puts("no folder"); return 2; }
    wcscat(g_ini_path, L"\\lid-hud.ini");
    DeleteFileW(g_ini_path);
    read_settings();

    puts("closed:");
    check("Tab is left to the game", press(VK_TAB), 0);
    check("an arrow is left to the game", press(VK_LEFT), 0);
    check("F8 opens it", press(VK_F8), 1);
    check("... and it is open", g_layout.on, 1);
    check("F8 held down does nothing more", layout_key(VK_F8, 1), 1);
    check("... still open", g_layout.on, 1);

    puts("durability numbers:");
    check("the first part", g_layout.part, PART_DURABILITY);
    press(VK_RIGHT);
    press(VK_RIGHT);
    press(VK_UP);
    check("x", g_set.x, 2);
    check("y", g_set.y, -1);
    press(VK_OEM_PLUS);
    check("size", g_set.size, 105);
    press('C');
    check("place (above the bar)", g_set.position, 1);
    press(VK_NEXT);
    press(VK_NEXT);
    check("Page Down twice: the right hand's slot 2", g_layout.item, 2);
    press(VK_LEFT);
    press(VK_DOWN);
    press(VK_DOWN);
    check("... moved on its own: x", g_set.off_x[1], -1);
    check("... y", g_set.off_y[1], 2);
    check("... the rest stays", g_set.x, 2);
    press(VK_PRIOR);
    press(VK_PRIOR);
    press(VK_PRIOR);
    check("Page Up past the whole part: the left hand's slot 3", g_layout.item, 6);
    press(VK_RIGHT);
    press('R');
    check("R on one slot: back with the rest", g_set.off_x[5], 0);
    check("... the other slot keeps its place", g_set.off_x[1], -1);
    press(VK_NEXT);
    check("Page Down after the last: the whole part", g_layout.item, 0);
    check("a letter it does not use is left to the game", press('W'), 0);

    puts("enemies counter:");
    press(VK_TAB);
    check("Tab: the next part", g_layout.part, PART_ENEMIES);
    press(VK_LEFT);
    press(VK_DOWN);
    press(VK_SUBTRACT);
    check("x", g_enemies.x, 1879);
    check("y", g_enemies.y, 161);
    check("size", g_enemies.size, 95);

    puts("floor info:");
    press(VK_TAB);
    check("Tab: the next part", g_layout.part, PART_FLOOR);
    press(VK_LEFT);
    press(VK_UP);
    press(VK_OEM_PLUS);
    press('C');
    check("x", g_floor.x, 1879);
    check("y", g_floor.y, 199);
    check("size", g_floor.size, 85);
    check("C: its alignment goes round (right, then left)", g_floor.align, 0);
    press(VK_NEXT);
    press(VK_NEXT);
    check("Page Down twice: the mushrooms line", g_layout.item, 1 + LINE_MUSHROOMS);
    press(VK_UP);
    press(VK_RIGHT);
    check("... moved on its own", g_floor.off_x[LINE_MUSHROOMS] == 1 && g_floor.off_y[LINE_MUSHROOMS] == -1, 1);
    press('H');
    check("H: the mushrooms line off", g_floor.mushrooms, 0);
    press('H');
    check("H again: on", g_floor.mushrooms, 1);
    check("the block stays", g_floor.x, 1879);

    puts("minimap (top right: x counts from the right):");
    press(VK_TAB);
    check("Tab: the next part", g_layout.part, PART_MINIMAP);
    check("... and the whole of it", g_layout.item, 0);
    press(VK_NEXT);
    check("Page Down: the minimap has no pieces", g_layout.item, 0);
    press(VK_RIGHT);
    press(VK_DOWN);
    check("right moves it right: x from the right edge", g_mini.x, 39);
    check("down moves it down", g_mini.y, 201);
    press('C');
    press('C');
    check("C twice: bottom right", g_mini.corner, 2);
    press(VK_DOWN);
    check("down from the bottom: y smaller", g_mini.y, 200);
    press(VK_ADD);
    check("size", g_mini.size, 330);
    press('H');
    check("H hides it", g_mini.show, 0);
    press('H');
    check("H shows it again", g_mini.show, 1);

    puts("rage:");
    press(VK_TAB);
    check("Tab: the next part", g_layout.part, PART_RAGE);
    press(VK_RIGHT);
    press(VK_DOWN);
    press('C');
    check("x", g_rage.x, 61);
    check("y", g_rage.y, 201);
    check("C: centred", g_rage.align, 1);

    puts("bag, mushroom effects, run totals:");
    press(VK_TAB);
    check("Tab: the bag", g_layout.part, PART_BAG);
    press(VK_LEFT);
    press(VK_OEM_PLUS);
    check("bag x", g_bag.b.x, 1879);
    check("bag size", g_bag.b.size, 85);
    press(VK_TAB);
    check("Tab: the mushroom effects", g_layout.part, PART_EFFECTS);
    press(VK_DOWN);
    press('H');
    check("effects y", g_effects.y, 241);
    check("H hides them", g_effects.show, 0);
    press('H');
    press(VK_TAB);
    check("Tab: the run totals", g_layout.part, PART_RUN);
    press(VK_NEXT);
    press(VK_NEXT);
    check("Page Down twice: the Kill Coins line", g_layout.item, 1 + RUN_KC);
    press(VK_RIGHT);
    press('H');
    check("... moved on its own", g_run.off_x[RUN_KC], 1);
    check("... and H hides that line", g_run.lines[RUN_KC], 0);
    press('H');
    for (i = 0; i < RUN_CONTINUES - RUN_KC; i++)
        press(VK_NEXT);
    check("on to the continues line", g_layout.item, 1 + RUN_CONTINUES);
    press('H');
    check("... H hides it", g_run.lines[RUN_CONTINUES], 0);
    for (i = 0; i < 1 + RUN_CONTINUES; i++)
        press(VK_PRIOR);
    check("Page Up back to the whole part", g_layout.item, 0);
    press('C');
    check("the whole part: C centres it", g_run.b.align, 1);

    puts("the game's own parts:");
    press(VK_TAB);
    check("Tab: the game's health and rage", g_layout.part, PART_GAME + GP_HEALTH);
    press(VK_RIGHT);
    press(VK_DOWN);
    press(VK_DOWN);
    press(VK_OEM_MINUS);
    check("x", g_gp[GP_HEALTH].x, 1);
    check("y", g_gp[GP_HEALTH].y, 2);
    check("size", g_gp[GP_HEALTH].size, 95);
    check("Page Down: no pieces of its own", (press(VK_NEXT), g_layout.item), 0);
    check("C does nothing to it", (press('C'), g_gp[GP_HEALTH].x), 1);
    press(VK_TAB);
    check("Tab: the status notices", g_layout.part, PART_GAME + GP_NOTICES);
    press(VK_TAB);
    check("Tab: the left hand's weapons", g_layout.part, PART_GAME + GP_LEFT);
    press(VK_LEFT);
    check("... moved on their own", g_gp[GP_LEFT].x, -1);
    check("... health stays", g_gp[GP_HEALTH].x, 1);
    press(VK_TAB);
    check("Tab: the right hand's weapons", g_layout.part, PART_GAME + GP_RIGHT);
    press(VK_TAB);
    check("Tab: the decals", g_layout.part, PART_GAME + GP_DECALS);
    press(VK_TAB);
    check("Tab: the quick items", g_layout.part, PART_GAME + GP_QUICK);
    press('H');
    check("H hides them", g_gp[GP_QUICK].show, 0);
    press(VK_LEFT);
    press('R');
    check("R puts them back: shown", g_gp[GP_QUICK].show, 1);
    check("... where the game puts them", g_gp[GP_QUICK].x, 0);
    for (i = GP_QUICK; i < GAME_PARTS - 1; i++)
        press(VK_TAB);
    check("on to the last of them, the button guide", g_layout.part, PART_GAME + GP_BUTTONS);
    press('H');
    press(VK_TAB);
    check("Tab after the last part: the first again", g_layout.part, PART_DURABILITY);

    puts("minimap to the very edge:");
    press(VK_TAB);
    press(VK_TAB);
    press(VK_TAB);
    g_mini.x = 0;
    press(VK_LEFT);            /* bottom right: left is away from the right edge */
    press(VK_RIGHT);
    press(VK_RIGHT);
    check("past 0, to sit on the edge", g_mini.x, -1);
    while (g_layout.part != PART_DURABILITY)
        press(VK_TAB);

    puts("options page:");
    check("O opens it", press('O'), 1);
    check("... on the options page", g_layout.page, 1);
    check("the first option: melee on", OPTIONS[g_layout.option].value == &g_set.melee && g_set.melee, 1);
    press(VK_RIGHT);
    check("Right: melee off", g_set.melee, 0);
    press(VK_RIGHT);
    press(VK_RIGHT);
    check("on/off goes round: off again after two more", g_set.melee, 0);
    press(VK_DOWN);
    press(VK_DOWN);
    press(VK_LEFT);
    check("format: Left from current/max goes round to percent", g_set.format, 2);
    press(VK_UP);
    press(VK_UP);
    press(VK_UP);
    check("Up from the first: the last option", g_layout.option, OPTION_COUNT - 1);
    while (wcscmp(OPTIONS[g_layout.option].key, L"colour") != 0)
        press(VK_DOWN);
    check("durability colour: white", (int)g_set.colour, 0xFFFFFF);
    press(VK_LEFT);
    press(VK_LEFT);
    press(VK_LEFT);
    press(VK_LEFT);
    check("... Left past red: the slot's own", g_set.colour == COLOUR_GAME, 1);
    press(VK_LEFT);
    check("... and round to magenta", (int)g_set.colour, 0xFF5AFF);
    press('R');
    check("R: white again", (int)g_set.colour, 0xFFFFFF);
    press(VK_LEFT);
    press(VK_LEFT);
    press(VK_LEFT);
    press(VK_LEFT);
    press(VK_DOWN);
    while (wcscmp(OPTIONS[g_layout.option].key, L"colour") != 0)
        press(VK_DOWN);
    press(VK_RIGHT);
    check("counter colour: white, then green", (int)g_enemies.colour, 0x50FF50);
    while (wcscmp(OPTIONS[g_layout.option].key, L"fungus_colour") != 0)
        press(VK_DOWN);
    check("Fun Fungus line: green", (int)g_floor.fungus_colour, 0x50FF50);
    press(VK_RIGHT);
    check("... then cyan", (int)g_floor.fungus_colour, 0x3CE6FF);
    while (wcscmp(OPTIONS[g_layout.option].section, L"rage") != 0 ||
           wcscmp(OPTIONS[g_layout.option].key, L"colour") != 0)
        press(VK_DOWN);
    press(VK_LEFT);
    check("rage colour: white, then yellow", (int)g_rage.colour, 0xFFE63C);
    while (wcscmp(OPTIONS[g_layout.option].key, L"zoom_percent") != 0)
        press(VK_DOWN);
    press(VK_RIGHT);
    check("fine zoom: a step of 5", g_mini.zoom_percent, 105);
    press('R');
    check("R: its default", g_mini.zoom_percent, 100);
    press(VK_RIGHT);
    check("... and up again", g_mini.zoom_percent, 105);
    check("arrows here leave the layout alone", g_mini.x, -1);
    press(VK_TAB);
    check("Tab: the next group, the area map's first", wcscmp(OPTIONS[g_layout.option].key, L"escalators"), 0);
    press(VK_TAB);
    check("Tab: then the bow arc's first", wcscmp(OPTIONS[g_layout.option].key, L"arc"), 0);
    press(VK_TAB);
    check("Tab: then the screen's", wcscmp(OPTIONS[g_layout.option].key, L"hide_serial_killer"), 0);
    press(VK_TAB);
    check("Tab: then this menu's own, its text size", g_menu.text_size == 100 &&
          wcscmp(OPTIONS[g_layout.option].key, L"text_size") == 0, 1);
    press(VK_RIGHT);
    check("text size: a step of 10", g_menu.text_size, 110);
    press(VK_TAB);
    check("Tab after the last group: the first", g_layout.option, 0);
    check("B: the panel off", press('B') && !g_menu.panel, 1);
    check("arrows still on the options page", g_layout.page, 1);
    check("O goes back", press('O'), 1);
    check("... to the layout page", g_layout.page, 0);

    puts("closing writes lid-hud.ini:");
    check("Esc closes it", press(VK_ESCAPE), 1);
    check("... and it is closed", g_layout.on, 0);
    check("the file is there", GetFileAttributesW(g_ini_path) != INVALID_FILE_ATTRIBUTES, 1);

    /* forget everything, then read the file back */
    g_set.x = g_set.y = g_set.position = 0;
    g_set.size = 100;
    g_enemies.x = g_enemies.y = g_enemies.size = 0;
    g_mini.x = g_mini.y = g_mini.corner = g_mini.size = 0;
    g_floor.x = g_floor.y = g_floor.size = 0;
    g_rage.x = g_rage.y = g_rage.align = 0;
    g_floor.align = 2;
    g_set.melee = 1;
    g_set.format = 0;
    g_enemies.colour = 0;
    g_mini.zoom_percent = 0;
    g_menu.text_size = 0;
    g_bag.b.x = g_bag.b.size = g_effects.y = g_run.b.align = 0;
    memset(g_run.off_x, 0, sizeof g_run.off_x);
    g_run.lines[RUN_CONTINUES] = 1;
    memset(g_set.off_x, 0, sizeof g_set.off_x);
    memset(g_set.off_y, 0, sizeof g_set.off_y);
    memset(g_floor.off_x, 0, sizeof g_floor.off_x);
    g_set.colour = 0xFFFFFF;
    g_floor.fungus_colour = g_rage.colour = 0;
    g_menu.panel = 1;
    for (i = 0; i < GAME_PARTS; i++)
        g_gp[i].show = 1, g_gp[i].x = g_gp[i].y = 0, g_gp[i].size = 100;
    read_settings();
    check("durability x", g_set.x, 2);
    check("durability y", g_set.y, -1);
    check("durability size", g_set.size, 105);
    check("durability place", g_set.position, 1);
    check("enemies x", g_enemies.x, 1879);
    check("enemies y", g_enemies.y, 161);
    check("enemies size", g_enemies.size, 95);
    check("minimap corner", g_mini.corner, 2);
    check("minimap x", g_mini.x, -1);
    check("minimap y", g_mini.y, 200);
    check("minimap size", g_mini.size, 330);
    check("minimap shown", g_mini.show, 1);
    check("floor info x", g_floor.x, 1879);
    check("floor info y", g_floor.y, 199);
    check("floor info size", g_floor.size, 85);
    check("floor info alignment", g_floor.align, 0);
    check("rage x", g_rage.x, 61);
    check("rage y", g_rage.y, 201);
    check("rage alignment", g_rage.align, 1);
    check("melee numbers (an option)", g_set.melee, 0);
    check("format (an option)", g_set.format, 2);
    check("counter colour (an option)", (int)g_enemies.colour, 0x50FF50);
    check("fine zoom (an option)", g_mini.zoom_percent, 105);
    check("menu text size (an option)", g_menu.text_size, 110);
    check("bag x", g_bag.b.x, 1879);
    check("bag size", g_bag.b.size, 85);
    check("effects y", g_effects.y, 241);
    check("run totals centred", g_run.b.align, 1);
    check("Kill Coins line x", g_run.off_x[RUN_KC], 1);
    check("Kill Coins line shown", g_run.lines[RUN_KC], 1);
    check("continues line hidden", g_run.lines[RUN_CONTINUES], 0);
    check("deaths line shown", g_run.lines[RUN_DEATHS], 1);
    check("durability right slot 2 x", g_set.off_x[1], -1);
    check("durability right slot 2 y", g_set.off_y[1], 2);
    check("mushrooms line x", g_floor.off_x[LINE_MUSHROOMS], 1);
    check("mushrooms line y", g_floor.off_y[LINE_MUSHROOMS], -1);
    check("durability colour: the slot's own (\"game\")", g_set.colour == COLOUR_GAME, 1);
    check("Fun Fungus line colour", (int)g_floor.fungus_colour, 0x3CE6FF);
    check("rage colour", (int)g_rage.colour, 0xFFE63C);
    check("menu panel (B)", g_menu.panel, 0);
    check("game's health and rage x", g_gp[GP_HEALTH].x, 1);
    check("game's health and rage y", g_gp[GP_HEALTH].y, 2);
    check("game's health and rage size", g_gp[GP_HEALTH].size, 95);
    check("game's left-hand weapons x", g_gp[GP_LEFT].x, -1);
    check("game's quick items shown", g_gp[GP_QUICK].show, 1);
    check("game's button guide hidden", g_gp[GP_BUTTONS].show, 0);
    check("game's timer untouched", g_gp[GP_TIMER].size, 100);

    puts("the status panel's four parts, by corner (3840 x 2160):");
    check("top left: health and rage", status_group(200, 150), GP_HEALTH);
    check("top right: notices", status_group(3600, 150), GP_NOTICES);
    check("bottom left: left hand", status_group(300, 1900), GP_LEFT);
    check("bottom right: right hand", status_group(3500, 1900), GP_RIGHT);
    check("bottom middle: the decals", status_group(1500, 1950), GP_DECALS);
    puts("the quick use panel's three parts, by corner:");
    check("top: the item box", quick_use_group(3600, 300), GP_QUICK);
    check("bottom left: left-hand weapons", quick_use_group(300, 1900), GP_LEFT);
    check("bottom right: right-hand weapons", quick_use_group(3500, 1900), GP_RIGHT);
    check("bottom middle: the decals", quick_use_group(2500, 1950), GP_DECALS);
    check("the left box's text (seen at 622) stays with it", quick_use_group(622, 2073), GP_LEFT);
    check("the right box's text (seen at 3112) stays with it", quick_use_group(3112, 1822), GP_RIGHT);

    puts("a part's move on top of a transform the game set (3840 x 2160):");
    {
        struct game_part_settings keep = g_gp[GP_LEFT];
        const float game[6] = {0.5f, 0, 300, 0, 0.5f, 1800};   /* the game's: half size, at 300, 1800 */
        float m[6];
        g_gp[GP_LEFT].x = 40;
        g_gp[GP_LEFT].y = -10;
        g_gp[GP_LEFT].size = 100;
        part_matrix(GP_LEFT, game, m);
        check("x of the game's (100, 100), then moved 40 (80 in its space)", (int)(m[0] * 100 + m[1] * 100 + m[2]), 430);
        check("... y, moved -10 (-20)", (int)(m[3] * 100 + m[4] * 100 + m[5]), 1830);
        part_matrix(GP_LEFT, NULL, m);
        check("no transform of the game's: just the move", (int)m[2], 80);
        g_gp[GP_LEFT].x = g_gp[GP_LEFT].y = 0;
        g_gp[GP_LEFT].size = 200;
        part_matrix(GP_LEFT, game, m);
        check("double size, around the middle: x", (int)(m[0] * 100 + m[1] * 100 + m[2]), -1220);
        g_gp[GP_LEFT] = keep;
    }

    puts("stand-ins: where a part's box goes (1920 x 1080):");
    {
        static const float r[4] = {100, 900, 300, 1000};
        float m[4];
        struct game_part_settings keep = g_gp[GP_RIGHT];
        g_gp[GP_RIGHT].x = 50;
        g_gp[GP_RIGHT].y = -20;
        g_gp[GP_RIGHT].size = 100;
        moved_rect(GP_RIGHT, r, m);
        check("moved: left edge", (int)m[0], 150);
        check("moved: top edge", (int)m[1], 880);
        g_gp[GP_RIGHT].x = g_gp[GP_RIGHT].y = 0;
        g_gp[GP_RIGHT].size = 50;
        moved_rect(GP_RIGHT, r, m);
        check("half size, around the middle: left edge", (int)m[0], 530);
        check("... bottom edge", (int)m[3], 770);
        g_gp[GP_RIGHT] = keep;
    }

    puts("where parts were seen is kept in lid-hud.ini:");
    g_area[GP_TIMER].x1 = 800; g_area[GP_TIMER].y1 = 30; g_area[GP_TIMER].x2 = 1100; g_area[GP_TIMER].y2 = 90;
    g_area[GP_TIMER].seen = 1;
    save_areas();
    memset(g_area, 0, sizeof g_area);
    read_settings();
    check("timer seen", g_area[GP_TIMER].seen, 1);
    check("timer's area: left", (int)g_area[GP_TIMER].x1, 800);
    check("timer's area: bottom", (int)g_area[GP_TIMER].y2, 90);
    check("boss health not seen yet", g_area[GP_BOSS].seen, 0);

    puts("R puts a part back:");
    press(VK_F8);
    press(VK_TAB);
    press(VK_TAB);
    press(VK_TAB);
    press('R');
    check("minimap corner", g_mini.corner, 0);
    check("minimap size", g_mini.size, 320);
    press(VK_F8);

    DeleteFileW(g_ini_path);
    printf(fails ? "\n%d WRONG\n" : "\nall passed\n", fails);
    return fails != 0;
}
