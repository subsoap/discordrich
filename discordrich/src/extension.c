// Extension lib defines
#define EXTENSION_NAME DiscordRich
#define LIB_NAME "DiscordRich"
#define MODULE_NAME "discordrich"
#define DLIB_LOG_DOMAIN "DiscordRich"
#include <dmsdk/dlib/log.h>

// Defold SDK
#define DLIB_LOG_DOMAIN LIB_NAME
#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_WINDOWS)

#include "discord_rpc.h"

void handleDiscordReady(const DiscordUser *u) 
{
    dmLogInfo("Displaying Presence for %s#%s\n", u->username, u->discriminator);
}

// handle discord disconnected event
void handleDiscordDisconnected(int errcode, const char *message) 
{
    dmLogInfo("Discord: disconnected (%d: %s)\n", errcode, message);
}

// handle discord error event
void handleDiscordError(int errcode, const char *message) 
{
    dmLogError("Discord: error (%d: %s)\n", errcode, message);
    Discord_Shutdown();
}


static int update(lua_State *L)
{
    char buffer[256];
    DiscordRichPresence discordPresence;
    memset(&discordPresence, 0, sizeof(discordPresence));
    //discordPresence.state = "In a Group";
    //sprintf(buffer, "Ranked | Mode: Test");
    //discordPresence.details = buffer;
    //discordPresence.largeImageKey = "test2";
    //discordPresence.smallImageKey = "test";
    //discordPresence.startTimestamp = 1531959948 + 5 * 60;
    //discordPresence.instance = 1;

    Discord_UpdatePresence(&discordPresence);

    return 0;        
}
    

static int init(lua_State *L)
{
    const char *client_id = luaL_checkstring(L, 1);
    
    DiscordEventHandlers handlers;
    memset(&handlers, 0, sizeof(handlers));
    handlers.ready = handleDiscordReady;
    handlers.errored = handleDiscordError;
    handlers.disconnected = handleDiscordDisconnected;
    
    // Discord_Initialize(const char* applicationId, DiscordEventHandlers* handlers, int autoRegister, const char* optionalSteamId)
    Discord_Initialize(client_id, &handlers, 1, NULL);

    dmLogInfo("Initialized");
    return 0;
}


static int shutdown(lua_State *L)
{
    Discord_Shutdown();
    return 0;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"update", update},
    {"init", init},
    {"shutdown", shutdown},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, MODULE_NAME, Module_methods);

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
    return dmExtension::RESULT_OK;
}

#else

static dmExtension::Result AppInitializeExtension(dmExtension::AppParams* params)
{
    dmLogWarning("Registered %s (null) Extension\n", MODULE_NAME);
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

#endif

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeExtension, AppFinalizeExtension, InitializeExtension, 0, 0, FinalizeExtension)
