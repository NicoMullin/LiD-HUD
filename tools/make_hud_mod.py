"""Write mods/HUD/mod.json: the HUD manager mod, which writes the Enemies Left
counter's words to master_text in every language the game has.

    py -3 tools/make_hud_mod.py
"""
import json
from pathlib import Path

WORDS = [
    ("int", "Enemies left: #0 / #1", "Enemies left: #0"),
    ("esn", "Enemigos restantes: #0 / #1", "Enemigos restantes: #0"),
    ("deu", "Verbleibende Gegner: #0 / #1", "Verbleibende Gegner: #0"),
    ("fra", "Ennemis restants : #0 / #1", "Ennemis restants : #0"),
    ("ita", "Nemici rimasti: #0 / #1", "Nemici rimasti: #0"),
    ("ptb", "Inimigos restantes: #0 / #1", "Inimigos restantes: #0"),
    ("jpn", "残りの敵：#0 / #1", "残りの敵：#0"),
    ("kor", "남은 적: #0 / #1", "남은 적: #0"),
    ("chn", "剩餘敵人：#0 / #1", "剩餘敵人：#0"),
    ("kan", "剩余敌人：#0 / #1", "剩余敌人：#0"),
]

rows = []
for lang, total, alive in WORDS:
    rows.append(f"  ('TXT_LID_HUD_ENEMIES', '{lang}', '{total}')")
    rows.append(f"  ('TXT_LID_HUD_ENEMIES_ALIVE', '{lang}', '{alive}')")
sql = ("INSERT OR REPLACE INTO master_text (sct, id, snd, lang, txt, type) "
       "SELECT 'TUB', column1, '', column2, column3, 4 FROM (VALUES\n" + ",\n".join(rows) + ");")

mod = {
    "id": "HUD",
    "name": "HUD",
    "description": "The words of LiD HUD's Enemies Left counter (\"Enemies left: 12 / 20\") in every "
                   "language the game has. The counter itself, and the durability numbers on the weapon "
                   "bars, are drawn by lid-hud.dll (LiD HUD) - see the readme. Without this mod the "
                   "counter is in English.",
    "version": "0.2.0",
    "author": "KSFA",
    "category": "Quality of life",
    "tags": ["hud", "enemies", "durability"],
    "game_version": [
        "5.0.4.1.0 - 1.89",
        "5.0.4.1.0 - 1.89 (Steam 5.0.4.2.0)",
        "5.0.4.4.0 - 1.91",
    ],
    "requires": [],
    "conflicts_with": ["Enemies Left"],
    "patches": [
        {
            "type": "raw_sql",
            "id": "words",
            "description": "The Enemies Left counter's words, in every language",
            "sql": sql,
        }
    ],
}
out = Path(__file__).resolve().parent.parent / "mods" / "HUD"
out.mkdir(parents=True, exist_ok=True)
(out / "mod.json").write_text(json.dumps(mod, indent=2, ensure_ascii=False) + "\n", encoding="utf-8",
                              newline="\n")
print("wrote", out / "mod.json")
