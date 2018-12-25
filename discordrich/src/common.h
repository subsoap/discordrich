#ifndef _COMMON_H_
#define _COMMON_H_

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

#include "discord_rpc.h"
#include "discord_register.h"

#ifdef DISCORD_RPC_STATIC

#define sym_Discord_Initialize Discord_Initialize
#define sym_Discord_Shutdown Discord_Shutdown
#define sym_Discord_RunCallbacks Discord_RunCallbacks
#define sym_Discord_UpdatePresence Discord_UpdatePresence
#define sym_Discord_ClearPresence Discord_ClearPresence
#define sym_Discord_Respond Discord_Respond
#define sym_Discord_UpdateHandlers Discord_UpdateHandlers
#define sym_Discord_Register Discord_Register
#define sym_Discord_RegisterSteamGame Discord_RegisterSteamGame

#define DiscordRich_openLibrary() do {} while (0)
#define DiscordRich_closeLibrary() do {} while (0)

#else

#ifdef __cplusplus
extern "C" {
#endif

extern void (*sym_Discord_Initialize)(
    const char* applicationId,
    DiscordEventHandlers* handlers,
    int autoRegister,
    const char* optionalSteamId
);
extern void (*sym_Discord_Shutdown)(void);
extern void (*sym_Discord_RunCallbacks)(void);
extern void (*sym_Discord_UpdatePresence)(const DiscordRichPresence* presence);
extern void (*sym_Discord_ClearPresence)(void);
extern void (*sym_Discord_Respond)(const char* userid, int reply);
extern void (*sym_Discord_UpdateHandlers)(DiscordEventHandlers* handlers);
extern void (*sym_Discord_Register)(const char* applicationId, const char* command);
extern void (*sym_Discord_RegisterSteamGame)(const char* applicationId, const char* steamId);

#ifdef __cplusplus
}
#endif

void DiscordRich_openLibrary(dmConfigFile::HConfig appConfig);
void DiscordRich_closeLibrary();

#endif

#endif
#endif

