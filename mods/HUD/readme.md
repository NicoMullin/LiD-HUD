# HUD

The manager half of [LiD HUD](https://github.com/NicoMullin/LiD-HUD). The HUD
additions themselves are drawn by its plugin, `lid-hud.dll`:

- **Durability numbers** on the weapon bars: `45/60`, `45` or `75%`.
- **Enemies left**: "Enemies left: 12 / 20" while you explore a floor - the
  living Haters and monsters the game counts to judge a floor cleared, and how
  many the floor started with. Never in the Waiting Room, and only while the
  game shows its HUD.

This mod writes the counter's words in every language the game has, to
`master_text`. Without it the counter is in English.

## You also need the plugin

`lid-hud.dll` in `Binaries\Win64\lid-plugins`, with the
[LiD Plugin Loader](https://github.com/NicoMullin/LiD-Plugin-Loader). In LiD DB
Editor, **Get mods...** installs both.

Where things go, their size and colour are set in `lid-hud.ini` next to the
plugin; see the [LiD HUD page](https://github.com/NicoMullin/LiD-HUD#settings).

**If you use the manager's own Enemies Left mod, switch it off**: the plugin
draws the same counter, and you would see two.

## Undo

Untick the mod and save. Delete `lid-hud.dll` to take the HUD additions away.

## Switched with this mod

From LiD DB Editor 0.12.8, saving with this mod ticked switches `lid-hud.dll`
on, and saving with it unticked switches it off: renamed to `.dll.off`, which
the Plugin Loader skips. Nothing is deleted, and ticking the mod again brings
it back. Close the game first. Before, this mod was only the counter's words
and the plugin ran on its own; now ticking this mod is what keeps the HUD on.
