# DiscordRich

Rich Presence for Defold games on Discord

https://discordapp.com/developers/docs/rich-presence/how-to

## Getting started

The minimum you should do is initialize the module, then call `update_presence()`
whenever you want to change the user's Defold status.

```lua
discordrich.initialize(your_discord_client_id)
discordrich.update_presence({
  status = "My status",
  details = "Doing something important in-game",
})
```

### Running in the editor

The game will bundle fine, but in order for DiscordRich to be available when running
from the editor, an extra step is required.

Copy the `discordrich/res` directory from this repo to a directory in your project
and add the path to that directory to your `game.project`:

```
[discordrich]
lib_path = path/to/discordrich/res
```

## API Reference

> TODO
