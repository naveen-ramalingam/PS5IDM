cmake_minimum_required(VERSION 3.16)
project(ps5-download-manager VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Options
option(PS5_TARGET "Build for PS5 target" OFF)
option(MOCK_PLATFORM "Build with mock/desktop platform backend" ON)
option(ENABLE_TESTS "Enable unit tests" ON)
option(ENABLE_SANITIZERS "Enable address/thread sanitizers (debug)" OFF)

# Compiler warnings
add_compile_options(
    -Wall -Wextra -Wpedantic -Wno-unused-parameter
    -Wconversion -Wsign-conversion
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
else()
    add_compile_options(-O2 -DNDEBUG)
endif()

if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# Large file support
add_compile_definitions(
    _FILE_OFFSET_BITS=64
    _LARGEFILE64_SOURCE
    _LARGEFILE_SOURCE
)

# Platform definitions
if(PS5_TARGET)
    add_compile_definitions(PS5_PLATFORM=1)
    message(STATUS "Building for PS5 target")
else()
    add_compile_definitions(MOCK_PLATFORM=1)
    message(STATUS "Building with mock/desktop platform backend")
endif()

# Find dependencies
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

# Try to find optional dependencies
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBARCHIVE libarchive)
    pkg_check_modules(SQLITE3 sqlite3)
else()
    # Fallback: try find_package or manual detection
    find_package(LibArchive QUIET)
    if(LibArchive_FOUND)
        set(LIBARCHIVE_FOUND TRUE)
        set(LIBARCHIVE_INCLUDE_DIRS ${LibArchive_INCLUDE_DIRS})
        set(LIBARCHIVE_LIBRARIES ${LibArchive_LIBRARIES})
    endif()
    # SQLite3
    find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
    find_library(SQLITE3_LIBRARY sqlite3)
    if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
        set(SQLITE3_FOUND TRUE)
        set(SQLITE3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIR})
        set(SQLITE3_LIBRARIES ${SQLITE3_LIBRARY})
    endif()
endif()

if(NOT LIBARCHIVE_FOUND)
    message(WARNING "libarchive not found - archive features will be limited")
    add_compile_definitions(NO_LIBARCHIVE=1)
endif()

if(NOT SQLITE3_FOUND)
    message(WARNING "SQLite3 not found - using built-in flat-file database fallback")
    add_compile_definitions(NO_SQLITE=1)
endif()

# ─── Source files ────────────────────────────────────────────────────────────

set(CORE_SOURCES
    src/common/types.cpp
    src/core/logger.cpp
    src/core/event_bus.cpp
    src/core/application.cpp
    src/web/mongoose/mongoose.c
    src/web/web_server.cpp
)

set(NETWORK_SOURCES
    src/network/http_headers.cpp
    src/network/http_client.cpp
    src/network/tls.cpp
)

set(DOWNLOADER_SOURCES
    src/downloader/segment.cpp
    src/downloader/retry_manager.cpp
    src/downloader/connection_manager.cpp
    src/downloader/download_worker.cpp
    src/downloader/download_task.cpp
    src/downloader/download_manager.cpp
)

set(DATABASE_SOURCES
    src/database/database.cpp
    src/database/download_repository.cpp
)

set(ARCHIVE_SOURCES
    src/archive/archive_detector.cpp
    src/archive/archive_group.cpp
    src/archive/extractor.cpp
    src/archive/batch_extractor.cpp
    src/archive/archive_manager.cpp
)

set(FILESYSTEM_SOURCES
    src/filesystem/file_operations.cpp
    src/filesystem/directory_scanner.cpp
    src/filesystem/file_manager.cpp
)

set(HASHING_SOURCES
    src/hashing/checksum.cpp
    src/hashing/hash_worker.cpp
)

set(SETTINGS_SOURCES
    src/settings/settings.cpp
)

set(UI_SOURCES
    src/ui/ui_manager.cpp
    src/ui/theme/theme.cpp
    src/ui/components/progress_bar.cpp
    src/ui/components/dialog.cpp
    src/ui/components/list_view.cpp
    src/ui/components/button_bar.cpp
    src/ui/components/text_input.cpp
    src/ui/components/notification.cpp
    src/ui/screens/downloads_screen.cpp
    src/ui/screens/archives_screen.cpp
    src/ui/screens/files_screen.cpp
    src/ui/screens/history_screen.cpp
    src/ui/screens/settings_screen.cpp
    src/ui/screens/download_detail_screen.cpp
    src/ui/screens/add_download_screen.cpp
)

set(PLATFORM_SOURCES
    src/platform/paths.cpp
)

if(PS5_TARGET)
    list(APPEND PLATFORM_SOURCES src/platform/ps5_platform.cpp)
else()
    list(APPEND PLATFORM_SOURCES src/platform/mock_platform.cpp)
endif()

# ─── Main library (all modules except main.cpp) ─────────────────────────────

add_library(ps5dm_lib STATIC
    ${CORE_SOURCES}
    ${NETWORK_SOURCES}
    ${DOWNLOADER_SOURCES}
    ${DATABASE_SOURCES}
    ${ARCHIVE_SOURCES}
    ${FILESYSTEM_SOURCES}
    ${HASHING_SOURCES}
    ${SETTINGS_SOURCES}
    ${UI_SOURCES}
    ${PLATFORM_SOURCES}
)

target_include_directories(ps5dm_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
)

target_link_libraries(ps5dm_lib PUBLIC
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Threads::Threads
)

if(LIBARCHIVE_FOUND)
    target_include_directories(ps5dm_lib PUBLIC ${LIBARCHIVE_INCLUDE_DIRS})
    target_link_libraries(ps5dm_lib PUBLIC ${LIBARCHIVE_LIBRARIES})
endif()

if(SQLITE3_FOUND)
    target_include_directories(ps5dm_lib PUBLIC ${SQLITE3_INCLUDE_DIRS})
    target_link_libraries(ps5dm_lib PUBLIC ${SQLITE3_LIBRARIES})
endif()

# ─── Main executable ────────────────────────────────────────────────────────

add_executable(ps5-download-manager src/main.cpp)
target_link_libraries(ps5-download-manager PRIVATE ps5dm_lib)

if(PS5_TARGET)
    set_target_properties(ps5-download-manager PROPERTIES
        SUFFIX ".elf"
        OUTPUT_NAME "ps5-download-manager"
    )
endif()

# ─── Tests ───────────────────────────────────────────────────────────────────

if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ─── Install ─────────────────────────────────────────────────────────────────

install(TARGETS ps5-download-manager DESTINATION bin)

message(STATUS "")
message(STATUS "╔══════════════════════════════════════════════════╗")
message(STATUS "║     PS5 Download Manager - Build Configuration  ║")
message(STATUS "╠══════════════════════════════════════════════════╣")
message(STATUS "║ Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "║ PS5 Target:     ${PS5_TARGET}")
message(STATUS "║ Mock Platform:  ${MOCK_PLATFORM}")
message(STATUS "║ Tests:          ${ENABLE_TESTS}")
message(STATUS "║ libarchive:     ${LIBARCHIVE_FOUND}")
message(STATUS "║ SQLite3:        ${SQLITE3_FOUND}")
message(STATUS "║ Sanitizers:     ${ENABLE_SANITIZERS}")
message(STATUS "╚══════════════════════════════════════════════════╝")
message(STATUS "")
