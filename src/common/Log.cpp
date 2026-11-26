#include "Log.h"

#include <cstdio>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace catbrowser::log {
namespace {

void write(const char* level, const char* message) {
#if defined(_WIN32)
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), "[CatBrowser][%s] %s\n", level, message);
    OutputDebugStringA(buffer);
#else
    std::fprintf(stderr, "[CatBrowser][%s] %s\n", level, message);
#endif
}

} // namespace

void info(const char* message) { write("INFO", message); }
void warn(const char* message) { write("WARN", message); }

} // namespace catbrowser::log
