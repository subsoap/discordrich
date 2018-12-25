#include "common.h"

#ifdef DISCORD_RPC_SUPPORTED

static bool discordInitialized = false;

static int shutdown(lua_State *L);

struct LuaCallbackInfo {
    LuaCallbackInfo() : m_L(0), m_Callback(LUA_NOREF), m_Self(LUA_NOREF) {}
    lua_State *m_L;
    int m_Callback;
    int m_Self;
};

static struct {
    LuaCallbackInfo ready;
    LuaCallbackInfo disconnected;
    LuaCallbackInfo errored;
    LuaCallbackInfo joinGame;
    LuaCallbackInfo spectateGame;
    LuaCallbackInfo joinRequest;
} callbacks;

static void clearCallback(LuaCallbackInfo * cbk)
{
    if (cbk->m_Callback != LUA_NOREF) {
        dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Callback);
        dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Self);
        cbk->m_Callback = LUA_NOREF;
    }
}

static void setCallback(lua_State * L, int index, LuaCallbackInfo * cbk)
{
    clearCallback(cbk);

    if (lua_isnil(L, index)) {
        cbk->m_Callback = LUA_NOREF;
    } else {
        luaL_checktype(L, index, LUA_TFUNCTION);
        lua_pushvalue(L, index);
        int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);

        cbk->m_L = dmScript::GetMainThread(L);
        cbk->m_Callback = cb;

        dmScript::GetInstance(L);

        cbk->m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
    }
}

static void callCallback(LuaCallbackInfo * cbk, int nargs)
{
    if (cbk->m_Callback == LUA_NOREF) { return; }

    lua_State * L = cbk->m_L;
    int top = lua_gettop(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, cbk->m_Callback);

    // Setup self (the script instance)
    lua_rawgeti(L, LUA_REGISTRYINDEX, cbk->m_Self);
    dmScript::SetInstance(L);

    for (int i = 0; i < nargs; i++) {
        lua_pushvalue(L, -nargs - 1);
    }

    int ret = lua_pcall(L, nargs, 0, 0);
    if (ret != 0) {
        dmLogError("Error running event handler: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    assert(top == lua_gettop(L));

    lua_pop(L, nargs);
}

static void pushDiscordUser(lua_State * L, const DiscordUser * user)
{
    lua_newtable(L);
    if (user->userId) {
        lua_pushstring(L, user->userId);
        lua_setfield(L, -2, "user_id");
    }
    if (user->username) {
        lua_pushstring(L, user->username);
        lua_setfield(L, -2, "username");
    }
    if (user->discriminator) {
        lua_pushstring(L, user->discriminator);
        lua_setfield(L, -2, "discriminator");
    }
    if (user->avatar) {
        lua_pushstring(L, user->avatar);
        lua_setfield(L, -2, "avatar");
    }
}

static void handleDiscordReady(const DiscordUser * user)
{
    LuaCallbackInfo * cbk = &callbacks.ready;
    if (cbk->m_Callback == LUA_NOREF) { return; }
    lua_State * L = cbk->m_L;

    pushDiscordUser(L, user);
    callCallback(cbk, 1);
}

static void handleDiscordDisconnected(int errcode, const char * message)
{
    LuaCallbackInfo * cbk = &callbacks.disconnected;
    if (cbk->m_Callback == LUA_NOREF) { return; }
    lua_State * L = cbk->m_L;

    lua_pushnumber(L, errcode);
    lua_pushstring(L, message);
    callCallback(cbk, 2);
}

static void handleDiscordErrored(int errcode, const char * message)
{
    LuaCallbackInfo * cbk = &callbacks.errored;
    if (cbk->m_Callback == LUA_NOREF) { return; }
    lua_State * L = cbk->m_L;

    lua_pushnumber(L, errcode);
    lua_pushstring(L, message);
    callCallback(cbk, 2);
}

static void handleDiscordJoinGame(const char * joinSecret)
{
    LuaCallbackInfo * cbk = &callbacks.joinGame;
    if (cbk->m_Callback == LUA_NOREF) { return; }
    lua_State * L = cbk->m_L;

    lua_pushstring(L, joinSecret);
    callCallback(cbk, 1);
}

static void handleDiscordSpectateGame(const char* spectateSecret)
{
    LuaCallbackInfo * cbk = &callbacks.spectateGame;
    if (cbk->m_Callback == LUA_NOREF) { return; }
    lua_State * L = cbk->m_L;

    lua_pushstring(L, spectateSecret);
    callCallback(cbk, 1);
}

static void handleDiscordJoinRequest(const DiscordUser* request)
{
    LuaCallbackInfo * cbk = &callbacks.ready;
    if (cbk->m_Callback == LUA_NOREF) { return; }
    lua_State * L = cbk->m_L;

    pushDiscordUser(L, request);
    callCallback(cbk, 1);
}

static void saveHandlers(lua_State * L, int index, DiscordEventHandlers * handlers)
{
    handlers->ready = handleDiscordReady;
    handlers->errored = handleDiscordErrored;
    handlers->disconnected = handleDiscordDisconnected;
    handlers->joinGame = handleDiscordJoinGame;
    handlers->spectateGame = handleDiscordSpectateGame;
    handlers->joinRequest = handleDiscordJoinRequest;

    if (lua_gettop(L) < index) { return; }
    if (lua_isnil(L, index)) { return; }
    luaL_checktype(L, index, LUA_TTABLE);

#define saveHandler(tname, fname) \
    lua_getfield(L, index, tname); \
    setCallback(L, -1, &callbacks.fname); \
    lua_pop(L, 1)

    saveHandler("ready", ready);
    saveHandler("errored", errored);
    saveHandler("disconnected", disconnected);
    saveHandler("join_game", joinGame);
    saveHandler("spectate_game", spectateGame);
    saveHandler("join_request", joinRequest);
}

static void freeHandlers()
{
    clearCallback(&callbacks.ready);
    clearCallback(&callbacks.disconnected);
    clearCallback(&callbacks.errored);
    clearCallback(&callbacks.joinGame);
    clearCallback(&callbacks.spectateGame);
    clearCallback(&callbacks.joinRequest);
}

static int register_(lua_State *L)
{
    if (!sym_Discord_Register) { return 0; }
    const char * applicationId = luaL_checkstring(L, 1);
    const char * command = luaL_checkstring(L, 2);
    sym_Discord_Register(applicationId, command);
    return 0;
}

static int register_steam_game(lua_State *L)
{
    if (!sym_Discord_RegisterSteamGame) { return 0; }
    const char * applicationId = luaL_checkstring(L, 1);
    const char * steamId = luaL_checkstring(L, 2);
    sym_Discord_RegisterSteamGame(applicationId, steamId);
    return 0;
}

static int shutdown(lua_State *L)
{
    if (!discordInitialized) { return 0; }
    if (!sym_Discord_Shutdown) { return 0; }
    sym_Discord_Shutdown();
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
    if (!sym_Discord_Initialize) { return 0; }

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

    sym_Discord_Initialize(applicationId, &handlers, autoRegister, optionalSteamId);
    return 0;
}

static int update_presence(lua_State *L)
{
    if (!sym_Discord_UpdatePresence) { return 0; }

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

    sym_Discord_UpdatePresence(&presence);
    return 0;
}

static int clear_presence(lua_State *L)
{
    if (!sym_Discord_ClearPresence) { return 0; }
    sym_Discord_ClearPresence();
    return 0;
}

static int respond(lua_State *L)
{
    if (!sym_Discord_Respond) { return 0; }
    const char * userId = luaL_checkstring(L, 1);
    int reply = luaL_checknumber(L, 2);
    sym_Discord_Respond(userId, reply);
    return 0;
}

static int update_handlers(lua_State *L)
{
    if (!discordInitialized) { return 0; }
    if (!sym_Discord_UpdateHandlers) { return 0; }
    freeHandlers();

    DiscordEventHandlers handlers;
    saveHandlers(L, 1, &handlers);

    sym_Discord_UpdateHandlers(&handlers);
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
    DiscordRich_openLibrary(params->m_ConfigFile);
    LuaInit(params->m_L);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeExtension(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeExtension(dmExtension::Params* params)
{
    shutdown(params->m_L);
    DiscordRich_closeLibrary();
    return dmExtension::RESULT_OK;
}

static dmExtension::Result UpdateExtension(dmExtension::Params* params)
{
    if (sym_Discord_RunCallbacks) {
        sym_Discord_RunCallbacks();
    }
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

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeExtension, AppFinalizeExtension, InitializeExtension, UpdateExtension, 0, FinalizeExtension)
