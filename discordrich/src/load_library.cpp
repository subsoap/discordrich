#include "common.h"

#if defined(DISCORD_RPC_SUPPORTED) && !defined(DISCORD_RPC_STATIC)

#ifndef _WIN32
#include <dlfcn.h>
#include <libgen.h>
#else
#include <Shlwapi.h>
#endif

#include <string.h>
#include <stdlib.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef __linux__
#include <unistd.h>
#endif

#ifndef _WIN32
    #include <dlfcn.h>
    #define dlModuleT void *
#else
    #define dlModuleT HMODULE
#endif

#define STRINGIFY(x) #x
#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)

#ifdef _WIN32
#define getSymbol GetProcAddress
#define getSymbolPrintError(fname) dmLogError("GetProcAddress(\"%s\"): %lu", STRINGIFY(fname), GetLastError())
#else
#define getSymbol dlsym
#define getSymbolPrintError(fname) dmLogError("dlsym(\"%s\"): %s", STRINGIFY(fname), dlerror())
#endif

#ifdef __cplusplus
extern "C" {
#endif

void (*sym_Discord_Initialize)(const char*, DiscordEventHandlers*, int, const char*) = NULL;
void (*sym_Discord_Shutdown)(void) = NULL;
void (*sym_Discord_RunCallbacks)(void) = NULL;
void (*sym_Discord_UpdatePresence)(const DiscordRichPresence* presence) = NULL;
void (*sym_Discord_ClearPresence)(void) = NULL;
void (*sym_Discord_Respond)(const char* userid, int reply) = NULL;
void (*sym_Discord_UpdateHandlers)(DiscordEventHandlers* handlers) = NULL;
void (*sym_Discord_Register)(const char* applicationId, const char* command) = NULL;
void (*sym_Discord_RegisterSteamGame)(const char* applicationId, const char* steamId) = NULL;

#ifdef __cplusplus
}
#endif

static dlModuleT DiscordRich_dlHandle = NULL;

#if defined(_WIN32)
    #define SEP "\\"
    #define SEPCH '\\'
#else
    #define SEP "/"
    #define SEPCH '/'
#endif

#ifdef _WIN32
static char * dirname(char * path) {
    size_t i = strlen(path);
    do {
        i -= 1;
        if (path[i] == SEPCH) {
            path[i] = 0;
            break;
        }
    } while (i);
    return path;
}
#endif

static bool endsIn(const char * haystack, const char * needle) {
    size_t needleLen = strlen(needle);
    size_t haystackLen = strlen(haystack);
    return (haystackLen >= needleLen && 0 == strcmp(needle, haystack + haystackLen - needleLen));
}

void DiscordRich_openLibrary(dmConfigFile::HConfig appConfig)
{
    if (DiscordRich_dlHandle) { return; }

    char *exePath = NULL;
    const char *libPath = ".";
    bool mustFreeLibPath = false;

    char* env = NULL;
    #if defined(__linux__)
    env = secure_getenv("DEFOLD_DISCORD_RPC_LIB_PATH");
    #elif !defined(_WIN32)
    env = getenv("DEFOLD_DISCORD_RPC_LIB_PATH");
    #endif

    if (env && env[0]) {
        libPath = env;
    } else {
        #ifdef __APPLE__
        uint32_t bufsize = 0;
        _NSGetExecutablePath(NULL, &bufsize);
        exePath = new char[bufsize];
        _NSGetExecutablePath(exePath, &bufsize);
        libPath = dirname(exePath);
        char* newPath = new char[strlen(libPath) + 1];
        strcpy(newPath, libPath);
        libPath = newPath;
        mustFreeLibPath = true;

        #elif defined(__linux__)
        exePath = new char[PATH_MAX + 2];
        ssize_t ret = readlink("/proc/self/exe", exePath, PATH_MAX + 2);
        if (ret >= 0 && ret <= PATH_MAX + 1) {
          exePath[ret] = 0;
          char* newPath = new char[ret + 1];
          strcpy(newPath, exePath);
          libPath = dirname(newPath); // dirname() clobbers newPath
          char* finalPath = new char[ret + 1];
          strcpy(finalPath, libPath);
          libPath = finalPath;
          mustFreeLibPath = true;
          delete[] newPath;
        } else {
          exePath[0] = 0;
        }

        #elif defined(_WIN32)
        exePath = new char[MAX_PATH];
        size_t ret = GetModuleFileNameA(GetModuleHandle(NULL), exePath, MAX_PATH);
        if (ret > 0 && ret < MAX_PATH) {
            char* newPath = new char[MAX_PATH];
            strcpy(newPath, exePath);
            dirname(newPath);
            libPath = newPath;
            mustFreeLibPath = true;
        } else {
            exePath[0] = 0;
        }
        #endif

        // Detect if the game is running in the editor

        #if defined(__APPLE__)
        #define FMB_PLATFORM "osx"
        #define FMB_PLATFORM_ALT "darwin"
        #define FMB_EXT ""
        #elif defined(__linux__)
        #define FMB_PLATFORM "linux"
        #define FMB_EXT ""
        #elif defined(_WIN32)
        #define FMB_PLATFORM "win32"
        #define FMB_EXT ".exe"
        #endif

        #if defined(__x86_64__) || defined(_M_X64)
        #define FMB_ARCH "x86_64"
        #elif defined(__i386) || defined(_M_IX86)
        #define FMB_ARCH "x86"
        #endif

        #if defined(FMB_PLATFORM) && defined(FMB_ARCH)
        #define FMB_EDITOR_SUFFIX SEP "build" SEP FMB_ARCH "-" FMB_PLATFORM SEP "dmengine" FMB_EXT

        #ifdef FMB_PLATFORM_ALT
        #define FMB_EDITOR_SUFFIX_ALT SEP "build" SEP FMB_ARCH "-" FMB_PLATFORM_ALT SEP "dmengine" FMB_EXT
        if (endsIn(exePath, FMB_EDITOR_SUFFIX) || endsIn(exePath, FMB_EDITOR_SUFFIX_ALT)) {
        #else
        if (endsIn(exePath, FMB_EDITOR_SUFFIX)) {
        #endif
            dmLogInfo("Running in the editor. Will attempt to load libraries from project");

            const char* projPath = dirname(dirname(dirname(exePath)));
            const char* resPath = dmConfigFile::GetString(appConfig, "discordrich.lib_path", "");

            if (!resPath[0]) {
                dmLogWarning("discordrich.lib_path not found in game.project. See README for details");
            }

            #ifdef __APPLE__
            #define FMB_LIB_PATH SEP FMB_ARCH "-" FMB_PLATFORM SEP "Contents" SEP "MacOS"
            #else
            #define FMB_LIB_PATH SEP FMB_ARCH "-" FMB_PLATFORM
            #endif

            size_t projPathLen = strlen(projPath);
            size_t resPathLen = strlen(resPath);
            size_t libPathLen = strlen(FMB_LIB_PATH);
            size_t len = 0;
            char* newPath = new char[projPathLen + 1 + resPathLen + libPathLen + 1];

            strcpy(newPath, projPath);
            len += projPathLen;

            if (resPath[0] != '/') {
                strcat(newPath, SEP);
                len += 1;
            }

            strcat(newPath + len, resPath);
            #ifdef _WIN32
            for (size_t i = len; i < len + resPathLen; i++) {
                if (newPath[i] == '/') { newPath[i] = SEPCH; }
            }
            #endif
            len += resPathLen;

            strcat(newPath + len, FMB_LIB_PATH);

            if (mustFreeLibPath) { delete[] libPath; }
            libPath = newPath;
            mustFreeLibPath = true;
        }

        #endif
    }

    #if defined(__APPLE__)
        #define LIBEXT "dylib"
    #elif defined(_WIN32)
        #define LIBEXT "dll"
    #else
        #define LIBEXT "so"
    #endif

    #ifdef _WIN32
        #define LIBPREFIX ""
        #define libOpen(var, path) \
            var = LoadLibraryA(path); \
            if (!var) { dmLogWarning("LoadLibrary(\"%s\") failed with error code %lu", path, GetLastError()); }
    #else
        #define LIBPREFIX "lib"
        #define libOpen(var, path) \
            var = dlopen(path, RTLD_NOW | RTLD_GLOBAL); \
            if (!var) { dmLogWarning("%s", dlerror()); }
    #endif

    if (exePath) { delete[] exePath; }
    size_t maxPathLen = strlen(libPath) + 20;
    exePath = new char[maxPathLen + 1];

    strcpy(exePath, libPath);
    strncat(exePath, SEP LIBPREFIX "discord-rpc." LIBEXT, maxPathLen);
    libOpen(DiscordRich_dlHandle, exePath);

    if (mustFreeLibPath) { delete[] libPath; }
    delete[] exePath;

#define ensure(fname, retType, ...) \
    CONCAT(sym_, fname) = (retType (*)(__VA_ARGS__))getSymbol(DiscordRich_dlHandle, STRINGIFY(fname)); \
    if (!CONCAT(sym_, fname)) { \
        getSymbolPrintError(fname); \
    }

    if (DiscordRich_dlHandle) {
        ensure(Discord_Initialize, void, const char*, DiscordEventHandlers*, int, const char*);
        ensure(Discord_Shutdown, void, void);
        ensure(Discord_RunCallbacks, void, void);
        ensure(Discord_UpdatePresence, void, const DiscordRichPresence*);
        ensure(Discord_ClearPresence, void, void);
        ensure(Discord_Respond, void, const char*, int);
        ensure(Discord_UpdateHandlers, void, DiscordEventHandlers*);
        ensure(Discord_Register, void, const char*, const char*);
        ensure(Discord_RegisterSteamGame, void, const char*, const char*);
    }
}

void DiscordRich_closeLibrary()
{
    sym_Discord_Initialize = NULL;
    sym_Discord_Shutdown = NULL;
    sym_Discord_RunCallbacks = NULL;
    sym_Discord_UpdatePresence = NULL;
    sym_Discord_ClearPresence = NULL;
    sym_Discord_Respond = NULL;
    sym_Discord_UpdateHandlers = NULL;
    sym_Discord_Register = NULL;
    sym_Discord_RegisterSteamGame = NULL;

    #ifdef _WIN32
    if (DiscordRich_dlHandle) { FreeLibrary(DiscordRich_dlHandle); }
    #else
    if (DiscordRich_dlHandle) { dlclose(DiscordRich_dlHandle); }
    #endif
}

#endif
