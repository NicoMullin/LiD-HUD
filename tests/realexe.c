/* The plugin's search, run on the real BrgGame-Steam.exe (mapped as a file,
 * nothing runs). It must find every piece. Only run by `build.py --exe PATH`:
 * the game is not part of this repository. */
#define LID_HUD_NO_DLLMAIN
#include "../dll/lid_hud.c"

static int fails;

static void show(const char *what, const void *at, const unsigned char *base)
{
    if (at)
        printf("  %-46s game+0x%llX\n", what, (unsigned long long)((const unsigned char *)at - base));
    else {
        printf("  %-46s NOT FOUND\n", what);
        fails++;
    }
}

static void show_int(const char *what, int v, int want)
{
    printf("  %-46s %d%s\n", what, v, v == want ? "" : "  (1.91 has a different one)");
    if (!v)
        fails++;
}

int main(int argc, char **argv)
{
    struct pieces pc;
    unsigned char *base;
    HMODULE m;

    if (argc < 2) { puts("usage: realexe PATH-TO-BrgGame-Steam.exe"); return 2; }
    m = LoadLibraryExA(argv[1], NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);
    if (!m) { printf("could not map %s (error %lu)\n", argv[1], GetLastError()); return 2; }
    base = (unsigned char *)((ULONG_PTR)m & ~(ULONG_PTR)3);

    find_pieces(base, &pc);
    puts("engine:");
    show("GWorld", pc.gworld, base);
    show("FName::FName", pc.fname, base);
    show("UObject::FindFunction", pc.find_function, base);
    show("main loop's GetForegroundWindow call", pc.frame_call, base);
    show("... its import slot", pc.frame_import, base);
    puts("function-table slots:");
    show_int("DrawText_UseImgPos", pc.vt_text, pc.vt_text);
    show_int("DrawShadowText_UseImgPos", pc.vt_shadow_text, pc.vt_shadow_text);
    show_int("... its KeepColor twin", pc.vt_shadow_keep, pc.vt_shadow_keep);
    show_int("GetDrawTextWidthWithFontState", pc.vt_text_width, pc.vt_text_width);
    show_int("Draw_GaugeLRImage", pc.vt_gauge, pc.vt_gauge);
    show_int("PlayerStatus RenderProcess", pc.vt_status_render, pc.vt_status_render);
    show_int("PauseMap RenderIconEscalatorUp", pc.vt_esc_up, pc.vt_esc_up);
    show_int("PauseMap RenderIconEscalatorDown", pc.vt_esc_down, pc.vt_esc_down);
    show_int("PredictedProjectileTrajectory SimProjectile", pc.vt_sim_projectile, pc.vt_sim_projectile);
    show("UWorld::SingleLineCheck", pc.line_check, base);
    show("appMalloc", pc.mem_alloc, base);
    show("appFree", pc.mem_free, base);
    show("StaticFindObject", pc.find_object, base);
    show("StaticConstructObject", pc.make_object, base);
    show_int("MiniMapRender RenderIcon", pc.vt_render_icon, pc.vt_render_icon);
    puts("the HUD's weapon slot drawing:");
    show("function", pc.slot_fn, base);
    printf("  slot no at rbp%+d, hand at rbp%+d, part at rbp%+d, master at rbp%+d\n",
           pc.slot_no, pc.slot_hand, pc.slot_part, pc.slot_master);
    printf("  part: dur +0x%X, lvl +0x%X; master: dur +0x%X, dur_c +0x%X\n",
           pc.part_dur, pc.part_lvl, pc.master_dur, pc.master_dur_c);
    if (pc.slot_fn && (pc.slot_no != -0x40 || pc.slot_hand != 0x1D30 || pc.slot_part != 0x1C8 ||
                       pc.slot_master != 0x270 || pc.part_dur != 0x30 || pc.part_lvl != 0x34 ||
                       pc.master_dur != 0x7C || pc.master_dur_c != 0x80))
        puts("  (not what 1.91 has: rbp-0x40, +0x1D30, +0x1C8, +0x270; 0x30, 0x34; 0x7C, 0x80)");
    if (!engine_ok(&pc) || !durability_ok(&pc) || !enemies_ok(&pc) || !map_ok(&pc) || !arc_ok(&pc) || !minimap_ok(&pc))
        fails++;
    printf(fails ? "\n%d MISSING\n" : "\nall found\n", fails);
    return fails != 0;
}
