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

This module should have 1-to-1 bindings to the official C Discord RPC. All
identifiers have been converted to `snake_case`.

Read the [official How To Guide][howto] to understand how to use these APIs.

[howto]: https://discordapp.com/developers/docs/rich-presence/how-to

### `discordrich.initialize(application_id, handlers, auto_register, optional_steam_id)`

Initializes Discord RPC.

* `application_id`: Your client_id/application_id
* `handlers`: *Optional.* A table of callback functions you registered for each Discord event
* `auto_register`: *Optional. Default `true`.* Whether or not to register an application protocol for your game on the player's computer. Necessary to launch games from Discord
* `optional_steam_id`: *Optional.* Your game's Steam application id, if your game is distributed on Steam. Used for launching your game through Steam if `auto_register` is `true`.

### `discordrich.shutdown()`

Shuts down the Discord RPC connection. **There's no need to call this manually.
It will get called automatically, if needed, when the game exits.**

### `discordrich.update_presence(presence)`

Sets your Discord presence. See
[the available fields](https://discordapp.com/developers/docs/rich-presence/how-to#updating-presence-update-presence-payload-fields)
in the official documentation. **The fields have been renamed to conform
to `snake_case`.** (eg. `largeImageText` -> `large_image_text`)

### `discordrich.clear_presence()`

Clears your Discord presence.

### The `handlers` table of callbacks

All of the following callbacks are optional:

#### `handlers.ready(user)`

Called when the game successfully connects to the Discord client. `user` is a
table describing the currently logged in user:

```lua
{
  user_id = "123456789012345678",
  username = "AwesomeGamer",
  avatar = "0123456789abcdef01234567890abcde",
  discriminator = "1234"
}
```

#### `handlers.disconnected(errcode, message)`

Called when the game disconnects from the Discord client. A numeric `errcode`
and a string `message` are provided describing the reason.

#### `handlers.errored(errcode, message)`

Called when the Discord RPC API raises an error. A numeric `errcode`
and a string `message` are provided describing the error.

#### `handlers.join_game(join_secret)`

Called when the game launches as a result of the user clicking "Join" on another
player's invitation. `join_secret` is the string provided to
`discordrich.update_presence()` by the inviting player's game.

#### `handlers.spectate_game(spectate_secret)`

Called when the game launches as a result of the user clicking "Spectate" on another
player's invitation. `spectate_secret` is the string provided to
`discordrich.update_presence()` by the inviting player's game.

#### `handlers.join_request(user)`

Called when another `user` clicks "Ask to Join" on the current user's profile.
This is when your game should ask the user if he wants to accept the request and
then call `discordrich.respond()` with the user's answer.

The `user` table has the same shape as the one provided by `handlers.ready()`.

### `discordrich.respond(user_id, answer)`

Respond to a join request issued by the user identified by `user_id`. `answer`
must be one of:

* `discordrich.REPLY_NO`
* `discordrich.REPLY_YES`
* `discordrich.REPLY_IGNORE`

### `discordrich.update_handlers(handlers)`

Change the `handlers` callbacks with different ones.

### `discordrich.register(application_id, command)`

Register the game's application protocol manually (`auto_register` does this
automatically for you).

### `discordrich.register_steam(application_id, steam_id)`

Register the game's application protocol manually to launch the game through
Steam. (`auto_register` does this automatically for you).
