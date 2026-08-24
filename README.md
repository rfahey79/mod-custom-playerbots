# mod-custom-playerbots

Persistent, named Playerbot Altbots for the Playerbot-compatible AzerothCore branch.  This module uses Playerbots' normal `AddPlayerBot(guid, 0)` login path but maintains its own roster in `characters.custom_playerbots`; it never writes to Rndbot account tables or assigns the random-account prefix.

## Install

1. Use the Playerbot-compatible AzerothCore core and install `mod-playerbots` first.
2. Copy this folder to `<azerothcore>/modules/mod-custom-playerbots`.
3. Import `data/sql/db-characters/base/custom_playerbots.sql` into the characters database.
4. Copy `conf/mod_custom_playerbots.conf.dist` to the worldserver config directory, removing `.dist`.
5. Re-run CMake, build `worldserver`, then restart it.

## Admin commands

```text
.custombot create Name Race Gender Class Level Account AutoLogin [skin=N face=N hairstyle=N haircolor=N facialhair=N]
.custombot list
.custombot login Name
.custombot logout Name
.custombot autologin Name on|off
.custombot unregister Name
```

Race, gender, and class accept their normal names (for example `human female mage`) or WotLK IDs. `Account` is the existing dedicated account name; it is never created or marked as a Rndbot account. `AutoLogin` accepts `on/off`, `true/false`, `yes/no`, or `1/0`.

Example:

```text
.custombot create Rydawg human male warrior 80 RydawgBots01 on
.custombot create Kira nightelf female druid 60 RydawgBots01 on skin=4 face=2 hairstyle=5 haircolor=3 facialhair=0
```

When any appearance field is absent, the module selects a valid matching value from `CharSections.dbc`. A supplied face/skin or hairstyle/haircolor combination is rejected unless that exact combination is valid for the selected race and gender. `facialhair` is likewise validated against the client catalog.

The module queues autologin characters after `CustomPlayerbots.AutoLoginDelayMs` and starts them in small batches. It deliberately does not invoke randomization, so names, race, gender, appearance, and level remain persistent.

`unregister` logs a custom bot out and removes only its roster entry; it never deletes the character.
