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
./acore.sh compiler build
```

Let worldserver's automatic database updater apply the module SQL at startup
(character-database updates must be enabled). Do not manually import the module
SQL when using automatic updates.

The autonomous migration creates the roster table if missing and adds the
`autonomous` column only if absent. Fresh installations default to `1`; existing
pre-autonomous rosters are migrated with `0`, preserving opt-in behavior. Existing
values and defaults are unchanged when the migration is reapplied.

If startup previously failed with `Table '...custom_playerbots' doesn't exist`
or `Duplicate column name 'autonomous'`, update this module and restart worldserver:

```bash
git -C modules/mod-custom-playerbots pull --ff-only
```

This SQL-only repair needs no C++ rebuild. Do not drop the roster table or edit
the updater's tracking tables. The failed migration will be retried automatically.

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

# Whether newly created and registered bots start autonomous questing.
CustomPlayerbots.AutonomousByDefault = 1

# Allow one online custom bot to greet a real guild member at login.
CustomPlayerbots.GuildGreeting.Enable = 0
CustomPlayerbots.GuildGreeting.Chance = 100
CustomPlayerbots.GuildGreeting.MinDelayMs = 3000
CustomPlayerbots.GuildGreeting.MaxDelayMs = 9000
CustomPlayerbots.GuildGreeting.Messages = "Welcome back, {player}!|Good to see you, {player}!|{player} is back in action!"
CustomPlayerbots.GuildGreeting.OfficerMaxRank = 1
CustomPlayerbots.GuildGreeting.LeaderMessages = "The boss is back: {player}!|Make way, {player} has returned.|The guildmaster has arrived. Try to look busy, {player}!"
CustomPlayerbots.GuildGreeting.OfficerMessages = "Officer {player} is back. The paperwork can wait.|{player} is online—someone hide the guild bank keys.|Welcome back, {player}; leadership looks exhausting."
CustomPlayerbots.GuildGreeting.MemberMessages = "Welcome back, {player}!|Good to see you, {player}!|{player} is back in action!"
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
.custombot reload
.custombot autologin Name on|off
.custombot autonomous Name|all on|off
.custombot unregister Name
```

`Race`, `Gender`, and `Class` accept readable WotLK names or their numeric IDs. `AutoLogin` accepts `on/off`, `true/false`, `yes/no`, or `1/0`.

Use `.custombot reload` after editing the module configuration. It reloads module configuration files and immediately applies settings that Custom Playerbots reads dynamically, such as guild greeting messages, chances, and delays. Pending greetings are cleared so they cannot use stale settings. Server-start login settings apply on the next restart.

Examples:

```text
.custombot create Aldric human male warrior 80 CustomBots01 on
.custombot create Elowen nightelf female druid 60 CustomBots01 on skin=4 face=2 hairstyle=5 haircolor=3 facialhair=0
.custombot register ExistingCharacter on
.custombot autonomous Aldric on
.custombot autonomous all on
```

If appearance fields are omitted, the module selects valid values from the client's `CharSections.dbc`. Supplied skin/face, hairstyle/hair-color, and facial-hair values are checked against the selected race and gender before the character is created.

`register` adds an existing character to the custom roster without changing its identity or progress. It rejects characters on Rndbot accounts. `unregister` logs a bot out and removes only its custom-roster entry; it never deletes the underlying character.

`CustomPlayerbots.AutonomousByDefault` controls whether newly created and registered bots have `autonomous` enabled; it defaults to `1`. Existing bots retain their stored setting. When enabled, the bot receives Playerbots' non-combat `new rpg` and `grind` strategies and has `follow` removed whenever it logs in. It will then independently seek level-appropriate quests and targets while preserving its own character identity and progress. The module reasserts this behavior when an autonomous bot is no longer in a group, preventing Playerbots from restoring its follow behavior. Disable it to restore ordinary companion behavior; a currently logged-in bot changes behavior immediately. Use `autonomous all on|off` to change the entire roster at once.

## Guild greetings

Set `CustomPlayerbots.GuildGreeting.Enable = 1` to let one online custom bot greet a real guild member when they log in. The bot and player must belong to the same guild. Bots do not greet other bots.

Greetings are rank-aware: guild rank `0` uses `LeaderMessages`; ranks `1` through `OfficerMaxRank` use `OfficerMessages`; higher ranks use `MemberMessages`. The defaults are deliberately a little quippy. All message settings are pipe-separated lists supporting the `{player}` placeholder. Set `MemberMessages` blank to continue using the original `Messages` setting for regular members. `Chance` controls how often it happens and the delay settings make the greeting feel natural.

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
