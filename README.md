# LiD HUD

Additions to the HUD of the offline version of Let It Die (Steam), as a plugin
for the [LiD Plugin Loader](https://github.com/NicoMullin/LiD-Plugin-Loader).

- **Durability numbers on the weapon bars** (0.1.0). Each weapon slot's
  durability bar gets its numbers - `45/60`, `45` or `75%` - in the HUD's own
  font. Melee weapons and guns can be switched separately. Since 0.3.0 also on
  the pause menu's fighter panel, with a short pop-up (`-3`, `+20`) when a
  weapon loses or gets back durability.
- **Enemies left** (0.2.0). "Enemies left: 12 / 20" while you explore a floor:
  the living Haters and monsters the game counts to judge a floor cleared, and
  how many the floor started with. Only while the game shows its HUD, never in
  the Waiting Room.
- **Hide the Serial Killer distortion** (0.3.0, off unless you switch it on).
- **Escalator colours on the area map** (0.4.0). In the pause menu's area map,
  each escalator sits on a disc of its line's colour, as on the floor map.
- **Bow arc** (0.5.0). While you draw a bow, the game's aiming line for thrown
  items shows where the arrow will fly and land, stopping at walls, the floor
  and enemies. It can take a colour of your choice when it ends on an enemy,
  and get brighter when the arrow would hit the head or neck.

- **Minimap** (0.6.0). The area map, always on, in a corner of the screen,
  following you, at the zoom you choose. Hidden while the pause menu's own map
  is open, and when the game hides its HUD.

- **Floor info** (0.9.0). Under the enemies counter: the beasts, mushrooms,
  unopened chests and materials left on the floor, and a line when the floor has a
  Fun Fungus. Each line can be switched off; never in the Waiting Room.
- **Low durability** (0.10.0). A durability number turns a warning colour
  below one % and a low colour below another, and can flash when low.
- **Bag space** (0.10.0). "Bag: 12 / 20" - turns red when the bag is full.
- **Mushroom effects** (0.10.0). Each mushroom effect running on you, in the
  game's own words, with its time left: "Attack Up 30%   0:24".
- **Run totals** (0.10.0). A trip into the tower, from leaving the Waiting Room
  until coming back: its time, and the Kill Coins, SP, Bloodnium, EXP and Death
  Metals it brought, and how many times the fighter died and was continued
  (0.11.0). The last trip stays up in the Waiting Room. Each line can be
  switched off and moved on its own.
- **Backgrounds** (0.10.1). Any part's text can have a dark box behind it
  (Background, in each part's options); how dark is one setting for all.
- **Colours** (0.9.6). The durability numbers, the enemies counter, floor info
  (and its Fun Fungus line on its own) and rage each take a text colour.
- **Rage** (0.9.0). "Rage: 63%" - how full the rage gauge is, so how far to
  the next one - and "+4%" for a few seconds after a hit, what that hit gave.
  Read from the game's own gauge.
- **Layout mode** (0.7.0). Move and size every part in the game itself, and
  (0.8.0) change every setting there too: see below.
- **The game's own HUD** (0.12.0). Move, size or hide the game's own parts too,
  in the same layout mode: health and rage, the status notices, each hand's
  weapon boxes (with their numbers), the decals, the quick items box, the
  timer, boss health, the floor name, messages, item pick-ups, the reticle,
  subtitles and the button guide. Parts that are not on the screen get a
  stand-in box, so they can be placed too.

## Moving things in the game

Press **F8** while playing (with the HUD on screen). The part being moved is
drawn in yellow, with its numbers at the top of the screen.

| Key            | What it does                                                      |
|----------------|-------------------------------------------------------------------|
| Tab            | Next part (durability, counter, floor info, minimap, rage, ...)   |
| Page Up / Down | One piece of it: a weapon slot's number, a floor info or run line |
| Arrow keys     | Move it (hold Shift to move 10 at a time)                         |
| + and -        | Make it bigger or smaller (Shift: faster)                         |
| C              | Its place: on/above/below the bar; lining up; minimap's corner    |
| H              | Show or hide it (one floor info line on its own too)              |
| R              | Put it back as it came (one piece: back with the rest)            |
| O              | The options page: every other setting (see below)                 |
| B              | The dark panel behind these pages, on or off                      |
| F8, Enter, Esc | Save and close                                                    |

On the options page, Up and Down pick a setting, Left and Right change it
(Shift: faster), R puts it back to its default, and O or Backspace goes back to
moving things. It has everything in the settings file below that is not a place
or a size: durability format and pop-ups, the counter's total, every text
colour, the minimap's zoom, the escalator discs, the bow arc and the Serial
Killer option. Its last group, **This menu**, sets how these pages look: their
text size, thicker text, and the dark panel behind them.

**The game's own parts** come after ours with Tab, their names starting
"Game's own". Arrow keys move one, + and - size it, H hides it (it still shows
while the layout mode is open, to place it) and R puts it back where the game
has it. Each one shows a box where it was last seen, with its name; one that
has not been on the screen yet shows a box at a rough guess, marked "not seen
yet". A part that is on the screen moves with its box, so it can be placed
against the real thing.

Closing writes everything to `lid-hud.ini` (made if it is not there). While
the layout mode is open, its keys are kept from the game; every other key still
works. The enemies counter shows "12 / 20" as a sample where there is nothing
to count, so it can be placed in the Waiting Room too.

## What the plugin does

Everything happens **in memory only**; the exe on disk is never touched.

- Once a frame it finds the game's UI manager through the engine's own object
  data, by name, so a game update that moves fields around is followed.
- It hooks the HUD's "draw a gauge" function. After the game has drawn a gauge
  for the weapon slots, it reads that slot's durability - now, and at most by
  the game's own `CalcPartParam` - and writes the number only if the bar shows
  exactly that much. It never labels any other bar.
- The game's own parts (0.12.0) are moved through the HUD's own draw
  transform: every one of the game's HUD drawing calls passes its corners
  through it. While a part draws, the transform is set to that part's move and
  size, on top of any the part sets itself, and put back after. Its text goes
  through the game's font renderer instead, so each line gets the same move.
  Two of the game's panels draw several parts each, so their drawing goes to
  the part for where on the screen it is: health and rage, the status notices
  and the item box at the top, and along the bottom each hand's weapon boxes
  in their corners with the decals between them.

Safety rules, all in [dll/lid_hud.c](dll/lid_hud.c):

- It only does anything inside `BrgGame-Steam.exe`. Loaded by any other
  program, it changes nothing.
- Every piece of game code is found by its exact bytes and must be found
  exactly once. Anything not found switches that feature off and is named in
  the log; after a game update that moves the code, the plugin does nothing.
- No network, no registry. The only file it writes is `lid-hud.log` next to
  itself.
- Delete `lid-hud.dll` and the game is exactly stock again.

## Settings

Optional. Make `lid-hud.ini` next to the plugin (in `Binaries\Win64\lid-plugins`).
A change shows in the game within a second; no restart needed.
Every place and size is on one 1920 x 1080 scale, whatever your screen size: the
game scales it to your screen the same way it scales its own HUD.

```ini
[durability]
melee=1       ; numbers on melee weapons' bars
guns=1        ; numbers on guns' and bows' bars
format=0      ; 0 current/max, 1 current, 2 percent left
position=0    ; 0 on the bar, 1 above it, 2 below it
size=100      ; text size, % of the HUD's own (30 to 300)
x=0           ; move right (or left, negative), in 1920 x 1080 pixels
y=0           ; move down (or up, negative)
right1_x=0    ; one slot's number moved on its own, on top of x and y:
right1_y=0    ; right1_ to right3_ and left1_ to left3_ (each hand's 3 slots)
warn=0        ; below this % a number turns warn_colour (0: never)
warn_colour=FFE63C
low=25        ; below this %, low_colour (0: never)
low_colour=FF3C3C
flash=1       ; a low number flashes
colour=FFFFFF ; text colour, red, green, blue in hex; "game" keeps the slot's
              ; own colour (red when out of ammo). Older files: white=1 or 0
shadow=1      ; a dark outline behind the text
menu=1        ; also on the equipment bars of the pause menu's fighter panel
popup=1       ; briefly show how much durability a weapon lost or got back
loss=0        ; its colour for a loss, and gain= for a gain: 0 red, 1 orange,
gain=4        ; 2 yellow, 3 white, 4 green, 5 cyan, 6 blue, 7 magenta

; Every part with text - [durability], [enemies], [floor], [rage], [bag],
; [effects], [run] - also takes background=1: a dark box behind its text.

[enemies]
show=1        ; "Enemies left: 12 / 20" while you explore a floor
total=1       ; also how many the floor started with ("/ 20")
x=1880        ; where it goes, on the game's 1920 x 1080 screen
y=160
align=2       ; 0 starting at that point, 1 centred on it, 2 ending at it
size=100      ; text size, % of the game's own HUD text (40 to 300)
colour=FFFFFF ; red, green, blue in hex

[screen]
hide_serial_killer=0  ; take away the Serial Killer decal's colour-split distortion
background_darkness=50 ; how dark the backgrounds are, % (10 to 100)

[map]
escalators=1        ; on the area map, each escalator on a disc of its line's colour
escalator_size=24   ; the disc's radius (4 to 120); the same at any zoom

[bow]
arc=1               ; while you draw a bow, show where the arrow will fly
stop_at_enemies=1   ; 0: the arc only stops at walls and the floor
enemy_colour=0      ; its colour when it ends on an enemy: 0 the game's own,
                    ; 1 red, 2 orange, 3 yellow, 4 green, 5 cyan
headshot=0          ; brighter when the arrow would hit the head or neck

[minimap]
show=1              ; the area map, always on, in a corner of the screen
corner=0            ; 0 top right, 1 top left, 2 bottom right, 3 bottom left,
                    ; 4 right middle, 5 left middle
x=40                ; from that side of the screen, in 1920 x 1080 pixels
                    ; (below 0 to sit right on the edge)
y=200               ; from the top or bottom (from the middle for 4 and 5)
size=320            ; width and height (50 to 1000)
zoom=0              ; the area map's own zoom: 0 near, 1 far
zoom_percent=100    ; finer: over 100 zooms in, under 100 out (25 to 400)

[floor]
show=1              ; what is left on this floor, under the enemies counter
x=1880              ; where it goes, on the 1920 x 1080 screen
y=200
align=2             ; 0 starting at x, 1 centred on it, 2 ending at it
size=80             ; text size, % of the HUD's own (40 to 300)
beasts=1            ; beasts left
chests=1            ; chests not yet opened
materials=1         ; materials lying on the floor
fun_fungus=1        ; "Fun Fungus on this floor", when there is one
mushrooms=1         ; mushrooms left
colour=FFFFFF       ; text colour, red, green, blue in hex
fungus_colour=50FF50  ; the Fun Fungus line's own colour
beasts_x=0          ; one line moved on its own, on top of x and y: beasts_,
beasts_y=0          ; mushrooms_, chests_, materials_ and fungus_

[rage]
show=1              ; "Rage: 63%" - how full the rage gauge is
gain=1              ; and "+4%" for a few seconds - what the last hit gave
x=60                ; where it goes, on the 1920 x 1080 screen
y=200
align=0             ; 0 starting at x, 1 centred on it, 2 ending at it
size=80             ; text size, % of the HUD's own (40 to 300)
colour=FFFFFF       ; text colour, red, green, blue in hex

[bag]
show=1              ; "Bag: 12 / 20" - what is in the bag, and how much fits
x=1880              ; where it goes, on the 1920 x 1080 screen
y=380
align=2             ; 0 starting at x, 1 centred on it, 2 ending at it
size=80             ; text size, % of the HUD's own (40 to 300)
colour=FFFFFF       ; text colour, red, green, blue in hex
full_colour=FF3C3C  ; its colour when the bag is full

[effects]
show=1              ; each mushroom effect running, with its time left
x=60                ; (and y, align, size, colour as for [bag])
y=240
align=0
size=75

[run]
show=1              ; a trip into the tower, from the Waiting Room and back
time=1              ; its time
kill_coins=1        ; and what each went up (or down) by
sp=1
bloodnium=1
exp=1
death_metals=1
deaths=1            ; times the fighter died on this trip
continues=1         ; times it was continued after one (any way of paying)
hub=1               ; the last trip, while in the Waiting Room
x=60                ; (and y, align, size, colour as for [bag])
y=420
time_x=0            ; one line moved on its own: time_, kill_coins_, sp_,
time_y=0            ; bloodnium_, exp_, death_metals_, deaths_, continues_

[layout]
key=F8              ; the key that opens the layout mode (F1 to F12)
text_size=100       ; the layout mode's own text, % (80 to 200)
thick_text=1        ; its text drawn thicker
panel=1             ; a dark panel behind it (B in the layout mode)

[game_hud]
health_show=1       ; the game's own parts: shown (1) or hidden (0),
health_x=0          ; moved from where the game puts it (1920 x 1080 pixels),
health_y=0
health_size=100     ; and sized, % (30 to 300), around the middle of the screen
; the same four for: notices, weapons_left, weapons_right, decals, quick_use,
; timer, boss_health, floor_title, messages, item_info, reticle, subtitles,
; button_guide
```

The plugin also keeps `<part>_seen` lines and a `seen_version` in
`[game_hud]`: where each part was seen, for the layout mode's boxes. They need
no editing.

The counter's words come from the **HUD** mod (in the release zip, under
`mods\HUD`) in every language the game has; without it they are in English. If
you use LiD DB Editor's own **Enemies Left** mod, switch it off: the plugin
draws the same counter.

## Why the DLL is built by GitHub

A DLL can do anything, so you should not have to take one on trust. Every
release on this page is built by GitHub Actions from the source in this
repository, on GitHub's own machines, and the tests run before anything is
published. You can check the DLL in two ways:

1. **Signed build record.** Each release DLL has an attestation: a signed
   record, kept by GitHub, of the exact workflow run and commit that built it.
   With the [GitHub CLI](https://cli.github.com/):

   ```
   gh attestation verify lid-hud.dll --repo NicoMullin/LiD-HUD
   ```

2. **Rebuild it yourself.** The build is reproducible: the same source and the
   same compiler give a byte-identical file. See
   [Building it yourself](#building-it-yourself) and compare the SHA-256 with
   the one on the release page.

## Installing

1. Install the [LiD Plugin Loader](https://github.com/NicoMullin/LiD-Plugin-Loader)
   once, if you have not already.
2. Download `lid-hud.dll` from the
   [latest release](https://github.com/NicoMullin/LiD-HUD/releases/latest) and put
   it in the game's `Binaries\Win64\lid-plugins`.

## Building it yourself

You need Windows and Python 3. The compiler is Zig, installed with pip; no
Visual Studio needed.

```
py -3 -m pip install ziglang==0.16.0
py -3 build.py --test
```

The DLL lands in `out\lid-hud.dll` and the SHA-256 is printed at the end. Use
Zig 0.16.0 exactly: another version builds a working DLL, but not a
byte-identical one.

`--test` checks the layout mode's keys and that what it saves reads back the
same, and loads the plugin into a stand-in for the game exe that has none of the
game's code, as after a game update, and checks it changes nothing and says so
in its log. `--exe PATH\BrgGame-Steam.exe` also runs `tests/realexe.c`: the
plugin's own search on the real game file (mapped, not run), which must find
every piece. The game is not in this repository, so GitHub does not run it.

## Making a release

1. Set the new version in [dll/version.h](dll/version.h).
2. Push a tag with the same version, for example `v0.1.0`.

GitHub builds, tests and publishes the release. The build stops if the tag and
`version.h` don't match.

## Credit

How to reach the HUD's gauges and the engine's text drawing was first worked
out by the author of an earlier HUD plugin, who gave us permission to build on
it and asked to stay unnamed.

## License

MIT - see [LICENSE](LICENSE). Made by KSFA.
