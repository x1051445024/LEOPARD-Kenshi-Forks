# Character Highlight (LEOPARD fork)

While the highlight key is held, characters are tinted by how they stand to
you: your squad, enemies, allies, neutrals (red to yellow to green by their
faction's relation).

Original: **BFrizzleFoShizzle**, [KenshiLib_Examples](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples)
(`CharacterHighlight`), GPLv3. This folder keeps the original commits.

## Changes in this fork

- Settings in Mod Config Menu (Options window, MCM tab): on/off, the show key
  (ALT by default, any key or always on), see through walls, distance
  limit, colours of the squad, enemies, allies and neutrals (by relation or
  one colour). Without MCM - `CharacterHighlight.ini` (made from
  `CharacterHighlight.defaults.ini` on first start, new keys added on update).
- Text in English, Russian and Chinese (`locale/*/LC_MESSAGES/character_highlight.po`).
- Much less work per frame. The original, every frame, walked every
  character of the update list (hundreds in a town), asked isEnemy /
  areYouGonnaGetMe / isAlly against every player character, wrote two shader
  constants on each of them - and with the key not held wrote the "off"
  constants on everyone too. Now:
  - the relation colour is cached per character and re-evaluated once a
    second, at most 40 characters a frame;
  - the constants are written when the colour changes and every 0.5 s (in
    case something wrote over them);
  - with the key not held, the tinted characters are reset once and then
    nothing is done.
- A character with no faction no longer crashes the relation check.
- Each kind (squad, enemies, allies, neutrals) can be turned off on its own.
- Diplomats: faction diplomats get a colour of their own (purple by default),
  before any other kind - the game's `Character::isDiplomaticStatus`, or a
  squad from a "... diplomat" template.
- The portrait and dead-body fixes of the original are kept.

Vital Sense (our fork) tints after the main loop and re-applies its tint
every frame, so its downed and bounty colours stay on top of these.

## Build

VC10 + Windows SDK 7.1, KenshiLib 0.2.1 headers, the shared headers of
[LEOPARD-Kenshi-Plugins](https://github.com/xxxLEOPARDxxx/LEOPARD-Kenshi-Plugins) (`shared/`):

    .\build.ps1 -SrcDir forks\LEOPARD-Kenshi-Forks\Character-Highlight -Name CharacterHighlight
