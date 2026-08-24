# mod-custom-playerbots

`mod-custom-playerbots` adds a small, persistent roster layer to the Playerbot-compatible AzerothCore server. Create named bots with a chosen race, class, level, and optional appearance; keep them on ordinary dedicated accounts; and have selected bots automatically log in whenever the worldserver starts.

Custom bots are **not Rndbots**. The module never adds their accounts to the random-bot account pool and never asks Playerbots to randomize their identity. Their name, race, gender, appearance, level, inventory, and normal character progress remain persistent.

When I first started tinkering with Playerbots, I had an idea: what if I could resurrect old friends and guildmates who had long since moved on from the game? That idea became the foundation for Custom Playerbots—persistent, familiar characters that can once again help bring Azeroth to life and relive some of the good old days.

## Requirements

- A Playerbot-compatible AzerothCore core with `mod-playerbots` working.
- MySQL or MariaDB access to the realm's **characters** database.
- A GM account for the `.custombot` commands.

## Installation

From the root of the Playerbot-compatible AzerothCore checkout:

```bash
git clone https://github.com/rfahey79/mod-custom-playerbots.git modules/mod-custom-playerbots
mysql <characters_database> < modules/mod-custom-playerbots/data/sql/db-characters/base/custom_playerbots.sql
./acore.sh compiler build
```

When updating an existing installation, apply `data/sql/db-characters/updates/2026_08_24_00_custom_playerbots_autonomous.sql` to the same characters database once before starting the updated server.

Copy `modules/mod-custom-playerbots/conf/mod_custom_playerbots.conf.dist` into the worldserver configuration directory and remove `.dist` from its name. The exact directory depends on the installation, but is commonly `env/dist/etc/`.

Ensure Playerbots is enabled in `playerbots.conf`:

```ini
AiPlayerbot.Enabled = 1
```

Restart `worldserver` after compiling and configuring. No change to `AiPlayerbot.RandomBotAccountPrefix` is required.

## Configuration

```ini
# Enable this module.
CustomPlayerbots.Enable = 1

# Wait for the server to settle before requesting custom-bot logins.
CustomPlayerbots.AutoLoginDelayMs = 15000

# Number of login requests issued per world tick after the delay.
CustomPlayerbots.AutoLoginBatchSize = 5
```

## Create a dedicated account

Use a normal account for custom bots. Do not use an account whose name matches the Rndbot account prefix, and do not add it to Playerbots' random-account settings. Create it from the worldserver console or as a GM:

```text
account create CustomBots01 YourStrongPassword
```

## Admin commands

```text
.custombot create Name Race Gender Class Level Account AutoLogin [skin=N face=N hairstyle=N haircolor=N facialhair=N]
.custombot register Name AutoLogin
.custombot list
.custombot login Name
.custombot logout Name
.custombot autologin Name on|off
.custombot autonomous Name on|off
.custombot unregister Name
```

`Race`, `Gender`, and `Class` accept readable WotLK names or their numeric IDs. `AutoLogin` accepts `on/off`, `true/false`, `yes/no`, or `1/0`.

Examples:

```text
.custombot create Aldric human male warrior 80 CustomBots01 on
.custombot create Elowen nightelf female druid 60 CustomBots01 on skin=4 face=2 hairstyle=5 haircolor=3 facialhair=0
.custombot register ExistingCharacter on
.custombot autonomous Aldric on
```

If appearance fields are omitted, the module selects valid values from the client's `CharSections.dbc`. Supplied skin/face, hairstyle/hair-color, and facial-hair values are checked against the selected race and gender before the character is created.

`register` adds an existing character to the custom roster without changing its identity or progress. It rejects characters on Rndbot accounts. `unregister` logs a bot out and removes only its custom-roster entry; it never deletes the underlying character.

`autonomous` is off by default. When enabled, the bot receives Playerbots' non-combat `new rpg` and `grind` strategies and has `follow` removed whenever it logs in. It will then independently seek level-appropriate quests and targets while preserving its own character identity and progress. Disable it to restore ordinary companion behavior; a currently logged-in bot changes behavior immediately.

## Startup and shutdown

At startup, the worldserver reports a compact roster section:

```text
---------------------------------------
 Initializing mod-custom-playerbots
 Loading persistent custom playerbot roster...
---------------------------------------
>> 1 persistent custom playerbots queued for autologin
1/1 custom bot Aldric logged in.
```

At shutdown, the module logs out and saves its roster bots before the server closes:

```text
Logging out all custom bots...
>> 1 custom bots logged out
```
