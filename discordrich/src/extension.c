// Extension lib defines
#define EXTENSION_NAME DiscordRich
#define LIB_NAME "DiscordRich"
#define MODULE_NAME "discordrich"
#define DLIB_LOG_DOMAIN LIB_NAME
#include <dmsdk/dlib/log.h>

// Defold SDK
#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_WINDOWS) || defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_LINUX)
#define DISCORD_RPC_SUPPORTED
#endif

#ifdef DISCORD_RPC_SUPPORTED

#include "discord_rpc.h"
#include "discord_register.h"

static bool discordInitialized = false;

void handleDiscordReady(const DiscordUser * user)
{
    dmLogInfo("Discord: ready (%s#%s)\n", user->username, user->discriminator);
}

void handleDiscordDisconnected(int errcode, const char * message)
{
    dmLogInfo("Discord: disconnected (%d: %s)\n", errcode, message);
}

void handleDiscordErrored(int errcode, const char * message)
{
    dmLogError("Discord: errored (%d: %s)\n", errcode, message);
    Discord_Shutdown();
}

void handleDiscordJoinGame(const char * joinSecret)
{
    dmLogInfo("Discord: join game (%s)\n", joinSecret);
}

void handleDiscordSpectateGame(const char* spectateSecret)
{
    dmLogInfo("Discord: spectate game (%s)\n", spectateSecret);
}

void handleDiscordJoinRequest(const DiscordUser* request)
{
    dmLogInfo("Discord: join request (%s#%s)\n", request->username, request->discriminator);
}

static void saveHandlers(lua_State * L, int index, DiscordEventHandlers * handlers)
{
    memset(&handlers, 0, sizeof(handlers));
    handlers->ready = handleDiscordReady;
    handlers->errored = handleDiscordErrored;
    handlers->disconnected = handleDiscordDisconnected;
    handlers->joinGame = handleDiscordJoinGame;
    handlers->spectateGame = handleDiscordSpectateGame;
    handlers->joinRequest = handleDiscordJoinRequest;

    if (lua_isnil(L, index)) { return; }
    luaL_checktype(L, 2, LUA_TTABLE);
    // TODO: register callbacks from Lua
}

static void freeHandlers()
{
    // TODO: unregister callbacks from Lua
}

static int register_(lua_State *L)
{
    const char * applicationId = luaL_checkstring(L, 1);
    const char * command = luaL_checkstring(L, 2);
    Discord_Register(applicationId, command);
    return 0;
}

static int register_steam_game(lua_State *L)
{
    const char * applicationId = luaL_checkstring(L, 1);
    const char * steamId = luaL_checkstring(L, 2);
    Discord_RegisterSteamGame(applicationId, steamId);
    return 0;
}

static int shutdown(lua_State *L)
{
    if (!discordInitialized) { return 0; }
    Discord_Shutdown();
    freeHandlers();
    return 0;
}

static bool check_boolean(lua_State *L, int index) {
    if (lua_isboolean(L, index)) {
        return !!lua_toboolean(L, index);
    }
    if (lua_isnumber(L, index)) {
        return !!lua_tonumber(L, index);
    }
    return luaL_error(L, "expected boolean value");
}

static int initialize(lua_State *L)
{
    if (discordInitialized) { shutdown(L); }

    int argc = lua_gettop(L);

    const char * applicationId = luaL_checkstring(L, 1);

    DiscordEventHandlers handlers;
    saveHandlers(L, 2, &handlers);

    int autoRegister = 1;
    if (argc >= 3 && !lua_isnil(L, 3)) {
        autoRegister = check_boolean(L, 3);
    }

    const char * optionalSteamId = NULL;
    if (argc >= 4 && !lua_isnil(L, 4)) {
        optionalSteamId = luaL_checkstring(L, 4);
    }

    Discord_Initialize(applicationId, &handlers, autoRegister, optionalSteamId);
    return 0;
}

static int update_presence(lua_State *L)
{
    DiscordRichPresence presence;
    memset(&presence, 0, sizeof(presence));

    luaL_checktype(L, 1, LUA_TTABLE);

#define set_string_field(tname, fname) \
    lua_getfield(L, 1, tname); \
    if (!lua_isnil(L, -1)) { \
        presence.fname = luaL_checkstring(L, -1); \
    } \
    lua_pop(L, 1); \

#define set_number_field(tname, fname) \
    lua_getfield(L, 1, tname); \
    if (!lua_isnil(L, -1)) { \
        presence.fname = luaL_checknumber(L, -1); \
    } \
    lua_pop(L, 1); \

    set_string_field("state", state);
    set_string_field("details", details);
    set_number_field("start_timestamp", startTimestamp);
    set_number_field("end_timestamp", endTimestamp);
    set_string_field("large_image_key", largeImageKey);
    set_string_field("large_image_text", largeImageText);
    set_string_field("small_image_key", smallImageKey);
    set_string_field("small_image_text", smallImageText);
    set_string_field("party_id", partyId);
    set_number_field("party_size", partySize);
    set_number_field("party_max", partyMax);
    set_string_field("match_secret", matchSecret);
    set_string_field("join_secret", joinSecret);
    set_string_field("spectate_secret", spectateSecret);
    set_number_field("instance", instance);

    Discord_UpdatePresence(&presence);
    return 0;
}

static int clear_presence(lua_State *L)
{
    Discord_ClearPresence();
    return 0;
}

static int respond(lua_State *L)
{
    const char * userId = luaL_checkstring(L, 1);
    int reply = luaL_checknumber(L, 2);
    Discord_Respond(userId, reply);
    return 0;
}

static int update_handlers(lua_State *L)
{
    if (!discordInitialized) { return 0; }
    freeHandlers();

    DiscordEventHandlers handlers;
    saveHandlers(L, 1, &handlers);

    Discord_UpdateHandlers(&handlers);
    return 0;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"initialize", initialize},
    {"shutdown", shutdown},
    {"register", register_},
    {"register_steam_game", register_steam_game},
    {"update_presence", update_presence},
    {"clear_presence", clear_presence},
    {"respond", respond},
    {"update_handlers", update_handlers},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_pushnumber(L, DISCORD_REPLY_NO);
    lua_setfield(L, -2, "REPLY_NO");
    lua_pushnumber(L, DISCORD_REPLY_YES);
    lua_setfield(L, -2, "REPLY_YES");
    lua_pushnumber(L, DISCORD_REPLY_IGNORE);
    lua_setfield(L, -2, "REPLY_IGNORE");

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result AppInitializeExtension(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeExtension(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    printf("Registered %s Extension\n", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeExtension(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeExtension(dmExtension::Params* params)
{
    shutdown(params->m_L);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result UpdateExtension(dmExtension::Params* params)
{
    Discord_RunCallbacks();
    return dmExtension::RESULT_OK;
}

#else

static dmExtension::Result AppInitializeExtension(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeExtension(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeExtension(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeExtension(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

#define UpdateExtension 0

#endif

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeExtension, AppFinalizeExtension, InitializeExtension, 0, 0, FinalizeExtension)
