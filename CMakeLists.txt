cmake_minimum_required(VERSION 3.20)

project(CatBrowser
    VERSION 1.4.0
    DESCRIPTION "Ultra-light native URL launcher for browser games"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CATBROWSER_WINDOWS_ONLY
    "Windows first: keep macOS/Linux sources in the tree but build no target for them" ON)
option(CATBROWSER_NATIVE_ARCH "Tune codegen for the build machine CPU (non-Apple only)" ON)
option(CATBROWSER_STATIC_RUNTIME "Statically link the C/C++ runtime where supported" ON)

# ---------------------------------------------------------------------------
# Maximum speed, minimal binary size. Debug configs deliberately keep asserts.
# ---------------------------------------------------------------------------
if(WIN32)
    add_compile_definitions(UNICODE _UNICODE NOMINMAX)
endif()

if(MSVC)
    add_compile_options(
        /O2 /Ob2 /Oi /Ot /Gy /GF /GL
        /EHsc /W4 /permissive- /utf-8)
    if(CATBROWSER_STATIC_RUNTIME)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    add_link_options(/LTCG /OPT:REF /OPT:ICF)
else()
    add_compile_options(
        -O3 -flto -ffunction-sections -fdata-sections -fno-plt)
    add_link_options(-flto)
    if(APPLE)
        add_link_options(-Wl,-dead_strip)
    else()
        add_link_options(-Wl,--gc-sections)
        if(CATBROWSER_NATIVE_ARCH)
            add_compile_options(-march=native -mtune=native)
        endif()
        if(CATBROWSER_STATIC_RUNTIME)
            add_link_options(-static-libstdc++ -static-libgcc)
        endif()
    endif()
endif()

set(CATBROWSER_SOURCES
    src/main.cpp
    src/common/Log.cpp
    src/common/UrlUtils.cpp
    src/common/Translation.cpp
    src/common/Search.cpp)

# ---------------------------------------------------------------------------
# Windows: Pure Win32 + WebView2 (static loader, single EXE)
# ---------------------------------------------------------------------------
if(WIN32)
    set(WEBVIEW2_SDK_DIR "" CACHE PATH
        "Root of the WebView2 NuGet package (directory containing build/native)")
    set(CAT_WRL_INCLUDE_DIR "" CACHE PATH
        "Optional Windows SDK 'winrt' include dir, needed by non-MSVC toolchains for wrl.h")

    set(_wv2_roots)
    if(WEBVIEW2_SDK_DIR)
        list(APPEND _wv2_roots "${WEBVIEW2_SDK_DIR}")
    endif()
    if(DEFINED ENV{WEBVIEW2_SDK_DIR} AND NOT "$ENV{WEBVIEW2_SDK_DIR}" STREQUAL "")
        list(APPEND _wv2_roots "$ENV{WEBVIEW2_SDK_DIR}")
    endif()
    list(APPEND _wv2_roots "${CMAKE_CURRENT_SOURCE_DIR}/packages/Microsoft.Web.WebView2")
    file(GLOB _wv2_versioned
        "${CMAKE_CURRENT_SOURCE_DIR}/packages/Microsoft.Web.WebView2.*")
    list(APPEND _wv2_roots ${_wv2_versioned})

    find_path(CAT_WEBVIEW2_INCLUDE_DIR
        NAMES WebView2.h
        PATHS ${_wv2_roots}
        PATH_SUFFIXES build/native/include include
        NO_DEFAULT_PATH)
    if(NOT CAT_WEBVIEW2_INCLUDE_DIR)
        find_path(CAT_WEBVIEW2_INCLUDE_DIR NAMES WebView2.h)
    endif()
    if(NOT CAT_WEBVIEW2_INCLUDE_DIR)
        message(FATAL_ERROR
            "WebView2 SDK not found. Fetch it with:\n"
            "  nuget install Microsoft.Web.WebView2 -ExcludeVersion -OutputDirectory packages")
    endif()

    get_filename_component(_wv2_native "${CAT_WEBVIEW2_INCLUDE_DIR}" DIRECTORY)
    set(_wv2_arch x64)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(_wv2_arch x86)
    else()
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _wv2_proc)
        if(_wv2_proc MATCHES "arm64|aarch64")
            set(_wv2_arch arm64)
        endif()
    endif()

    find_library(CAT_WEBVIEW2_LOADER
        NAMES WebView2LoaderStatic
        PATHS "${_wv2_native}/${_wv2_arch}" ${_wv2_roots}
        PATH_SUFFIXES "${_wv2_arch}" "build/native/${_wv2_arch}"
        NO_DEFAULT_PATH)
    if(NOT CAT_WEBVIEW2_LOADER)
        message(FATAL_ERROR
            "WebView2LoaderStatic.lib (${_wv2_arch}) not found in the SDK")
    endif()
    message(STATUS "WebView2 SDK: ${CAT_WEBVIEW2_INCLUDE_DIR}")
    message(STATUS "WebView2 loader: ${CAT_WEBVIEW2_LOADER}")

    list(APPEND CATBROWSER_SOURCES
        src/win32/App_win.cpp
        src/win32/PlatformWindow.cpp
        src/win32/LocalData.cpp
        src/win32/WebEngine.cpp
        src/win32/GpuExports.cpp)

    enable_language(RC)
    set(CATBROWSER_VERSION_RC
        "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
    configure_file(src/win32/Version.rc.in
                   ${CMAKE_CURRENT_BINARY_DIR}/Version.rc @ONLY)
    list(APPEND CATBROWSER_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/Version.rc)

    add_executable(CatBrowser WIN32 ${CATBROWSER_SOURCES})
    target_include_directories(CatBrowser PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CAT_WEBVIEW2_INCLUDE_DIR}")
    if(CAT_WRL_INCLUDE_DIR)
        target_include_directories(CatBrowser PRIVATE "${CAT_WRL_INCLUDE_DIR}")
    endif()
    target_link_libraries(CatBrowser PRIVATE
        "${CAT_WEBVIEW2_LOADER}"
        kernel32 user32 gdi32 shell32 ole32 oleaut32 uuid
        advapi32 version shlwapi)

# ---------------------------------------------------------------------------
# macOS: Cocoa + WKWebView, .app bundle, Objective-C++ with ARC
# ---------------------------------------------------------------------------
elseif(CATBROWSER_WINDOWS_ONLY)
    message(WARNING
        "CATBROWSER_WINDOWS_ONLY=ON and this is not a Windows host: "
        "no build target was created. Pass -DCATBROWSER_WINDOWS_ONLY=OFF "
        "when the macOS/Linux ports are resumed.")

elseif(APPLE)
    set(CATBROWSER_MAC_SOURCES
        src/mac/App_mac.mm
        src/mac/PlatformWindow.mm
        src/mac/WebEngine.mm)
    set_source_files_properties(${CATBROWSER_MAC_SOURCES}
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    list(APPEND CATBROWSER_SOURCES ${CATBROWSER_MAC_SOURCES})

    add_executable(CatBrowser MACOSX_BUNDLE ${CATBROWSER_SOURCES})
    target_include_directories(CatBrowser PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(CatBrowser PRIVATE
        "-framework AppKit"
        "-framework WebKit"
        "-framework Foundation"
        "-framework QuartzCore")

    set_target_properties(CatBrowser PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.catbrowser.launcher"
        MACOSX_BUNDLE_BUNDLE_NAME "CatBrowser"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/mac/Info.plist.in"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")

# ---------------------------------------------------------------------------
# Linux: GTK3 + WebKitGTK 4.1 (falls back to 4.0 on older distributions)
# ---------------------------------------------------------------------------
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WEBKIT2 QUIET webkit2gtk-4.1)
    if(NOT WEBKIT2_FOUND)
        pkg_check_modules(WEBKIT2 REQUIRED webkit2gtk-4.0)
    endif()
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

    list(APPEND CATBROWSER_SOURCES
        src/linux/App_linux.cpp
        src/linux/PlatformWindow.cpp
        src/linux/WebEngine.cpp)

    add_executable(CatBrowser ${CATBROWSER_SOURCES})
    target_include_directories(CatBrowser PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        ${GTK3_INCLUDE_DIRS}
        ${WEBKIT2_INCLUDE_DIRS})
    target_link_directories(CatBrowser PRIVATE
        ${GTK3_LIBRARY_DIRS}
        ${WEBKIT2_LIBRARY_DIRS})
    target_link_libraries(CatBrowser PRIVATE
        ${GTK3_LIBRARIES}
        ${WEBKIT2_LIBRARIES})
    target_compile_options(CatBrowser PRIVATE
        ${GTK3_CFLAGS_OTHER}
        ${WEBKIT2_CFLAGS_OTHER})
endif()

if(TARGET CatBrowser)
    include(GNUInstallDirs)
    if(WIN32)
        install(TARGETS CatBrowser RUNTIME DESTINATION .)
    elseif(APPLE)
        install(TARGETS CatBrowser BUNDLE DESTINATION .)
    else()
        install(TARGETS CatBrowser RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()
