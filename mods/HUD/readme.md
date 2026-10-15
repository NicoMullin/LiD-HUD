# HUD

The manager half of [LiD HUD](https://github.com/NicoMullin/LiD-HUD). It does
two things:

- **It is the plugin's switch.** Saving with this mod ticked switches
  `lid-hud.dll` on; saving with it unticked switches it off.
- **It writes the Enemies Left counter's words** in every language the game
  has, to `master_text`. Without them the counter is in English.

## What the plugin adds

All of it is drawn by `lid-hud.dll`, in the HUD's own font, and only while the
game shows its HUD.

- **Durability numbers** on the weapon bars: `45/60`, `45` or `75%`, for melee
  weapons and guns separately. Also on the pause menu's fighter panel, with a
  short pop-up (`-3`, `+20`) when a weapon loses or gets back durability.
- **Low durability:** a number turns a warning colour below one % and a low
  colour below another, and can flash.
- **Enemies left:** "Enemies left: 12 / 20" while you explore a floor - the
  living Haters and monsters the game counts to judge a floor cleared, and how
  many the floor started with.
- **Floor info:** the beasts, mushrooms, unopened chests and materials left on
  the floor, and a line when the floor has a Fun Fungus.
- **Rage:** "Rage: 63%", how far to the next rage gauge, and what the last hit
  gave.
- **Bag space:** "Bag: 12 / 20", red when the bag is full.
- **Mushroom effects:** each one running on you, with its time left.
- **Run totals:** a trip into the tower, from the Waiting Room and back - its
  time, the Kill Coins, SP, Bloodnium, EXP and Death Metals it brought, and how
  many times the fighter died and was continued. The last trip stays up in the
  Waiting Room.
- **Minimap:** the area map, always on, in a corner of the screen.
- **Bow arc:** while you draw a bow, a line shows where the arrow will fly and
  land.
- **Escalator colours** on the pause menu's area map.
- **Hide the Serial Killer distortion** (off unless you switch it on).

Every part can be switched off, moved and resized on its own, and given a text
colour and a dark background.

## Setting it up in the game

Press **F8** while playing to open the layout mode: Tab picks a part, the arrow
keys move it, + and - size it, and **O** opens every other setting. F8 again
saves to `lid-hud.ini` next to the plugin. The
[LiD HUD page](https://github.com/NicoMullin/LiD-HUD#moving-things-in-the-game)
has all the keys.

## You also need the plugin

`lid-hud.dll` in `Binaries\Win64\lid-plugins`, with the
[LiD Plugin Loader](https://github.com/NicoMullin/LiD-Plugin-Loader). In LiD DB
Editor, **Get mods...** installs both.

**If you use the manager's own Enemies Left mod, switch it off**: the plugin
draws the same counter, and you would see two.

## Switched with this mod

From LiD DB Editor 0.12.8, saving with this mod ticked switches `lid-hud.dll`
on, and saving with it unticked switches it off: renamed to `.dll.off`, which
the Plugin Loader skips. Nothing is deleted, and ticking the mod again brings
it back. Close the game first. Before, this mod was only the counter's words
and the plugin ran on its own; now ticking this mod is what keeps the HUD on.

## Undo

Untick the mod and save: the words go and the plugin is switched off.
