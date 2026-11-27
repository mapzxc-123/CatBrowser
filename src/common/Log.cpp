#include "Log.h"

#include <cstdio>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace astb3ast::log {
namespace {

void write(const char* level, const char* message) {
#if defined(_WIN32)
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), "[AstB3ast][%s] %s\n", level, message);
    OutputDebugStringA(buffer);
#else
    std::fprintf(stderr, "[AstB3ast][%s] %s\n", level, message);
#endif
}

} // namespace

void info(const char* message) { write("INFO", message); }
void warn(const char* message) { write("WARN", message); }

} // namespace astb3ast::log
