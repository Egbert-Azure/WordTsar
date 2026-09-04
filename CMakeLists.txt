cmake_minimum_required(VERSION 3.16)

project(WordTsar)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;Debug" CACHE STRING "" FORCE)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

#======================= OSX stuff =============================
   set(COMPANY_NAME, "WordTsar")

#======================= INCLUSION OF Qt =======================#
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS src/gui/dialogs)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# The Qt GUI (WordTsar) is optional, so the TUI backend (ws) can be
# built without Qt (e.g. on cross-platform CI runners that only verify the TUI).
option(BUILD_GUI "Build the Qt GUI (WordTsar)" ON)

# Try to find Qt6 automatically first, fall back to a Homebrew/Qt-installer path if needed
if(BUILD_GUI)
find_package(Qt6 QUIET COMPONENTS Core)
if(NOT Qt6_FOUND)
    set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.8.2/macos" "$ENV{HOME}/Qt/6.7.0/macos" "$ENV{HOME}/Qt/6.6.0/macos" "/opt/homebrew/opt/qt@6" "/usr/local/opt/qt@6")
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets PrintSupport)
endif()

# TUI Build Options (off by default; the terminal UI is a separate, opt-in target)
option(BUILD_TUI "Build the terminal UI (ws) using the wordstartui toolkit" OFF)


#======================= THIRD-PARTY DEPENDENCIES =======================#
#
# Third-party libraries live in the third-party/ directory.
# By default, the build uses whatever is already downloaded (no network).
#
# To skip update checks (default -- fastest configure, and doesn't silently
# replace a pinned dependency with an untested newer one):
#   cmake .. -DUPDATE_DEPENDENCIES=OFF
#
# To check for updates and download newer versions:
#   cmake .. -DUPDATE_DEPENDENCIES=ON
#

option(UPDATE_DEPENDENCIES "Check for and download newer third-party dependencies (requires network)" OFF)

# Third-party source directories (already downloaded)
set(WS_THIRDPARTY_DIR "${CMAKE_SOURCE_DIR}/third-party")
set(WS_PUGIXML_DIR "${WS_THIRDPARTY_DIR}/pugixml")
set(WS_UNICODELIB_DIR "${WS_THIRDPARTY_DIR}/cpp-unicodelib")
set(WS_SIMPLEINI_DIR "${WS_THIRDPARTY_DIR}/simpleini")
set(WS_PICOMATH_DIR "${WS_THIRDPARTY_DIR}/picomath")
set(WS_ZIP_DIR "${WS_THIRDPARTY_DIR}/zip")

# Dependency versions - single source of truth
set(PUGIXML_VERSION "1.15")
set(SIMPLEINI_VERSION "4.22")
set(PICOMATH_VERSION "1.1.0")
set(KUBAZIP_VERSION "0.3.5")

# Always ensure all dependencies are present (downloads missing ones using pinned versions)
include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${WS_THIRDPARTY_DIR}")
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

if(NOT EXISTS "${WS_PUGIXML_DIR}")
    message(STATUS "  pugixml: not found, downloading v${PUGIXML_VERSION}...")
    FetchContent_Declare(pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG v${PUGIXML_VERSION}
        SOURCE_DIR "${WS_PUGIXML_DIR}")
    FetchContent_Populate(pugixml)
endif()

if(NOT EXISTS "${WS_SIMPLEINI_DIR}")
    message(STATUS "  simpleini: not found, downloading v${SIMPLEINI_VERSION}...")
    FetchContent_Declare(simpleini
        GIT_REPOSITORY https://github.com/brofield/simpleini.git
        GIT_TAG v${SIMPLEINI_VERSION}
        SOURCE_DIR "${WS_SIMPLEINI_DIR}")
    FetchContent_Populate(simpleini)
endif()

if(NOT EXISTS "${WS_PICOMATH_DIR}/include/picomath.hpp")
    message(STATUS "  picomath: not found, downloading v${PICOMATH_VERSION}...")
    FetchContent_Declare(picomath
        GIT_REPOSITORY https://github.com/Nitrillo/picomath.git
        GIT_TAG v${PICOMATH_VERSION}
        SOURCE_DIR "${WS_PICOMATH_DIR}")
    FetchContent_Populate(picomath)
endif()

if(NOT EXISTS "${WS_ZIP_DIR}")
    message(STATUS "  kubazip: not found, downloading v${KUBAZIP_VERSION}...")
    FetchContent_Declare(kubazip
        GIT_REPOSITORY https://github.com/kuba--/zip.git
        GIT_TAG v${KUBAZIP_VERSION}
        SOURCE_DIR "${WS_ZIP_DIR}")
    FetchContent_Populate(kubazip)
endif()

if(NOT EXISTS "${WS_UNICODELIB_DIR}/unicodelib.h")
    message(STATUS "  cpp-unicodelib: not found, downloading...")
    FetchContent_Declare(cpp_unicodelib
        GIT_REPOSITORY https://github.com/yhirose/cpp-unicodelib.git
        GIT_TAG master
        SOURCE_DIR "${WS_UNICODELIB_DIR}")
    FetchContent_Populate(cpp_unicodelib)
endif()

# Optionally check for newer versions of dependencies (requires network)
if(UPDATE_DEPENDENCIES)
    message(STATUS "")
    message(STATUS "=== Checking for dependency updates (requires network) ===")
    message(STATUS "    To skip: cmake .. -DUPDATE_DEPENDENCIES=OFF")
    message(STATUS "")

    # Function to check if a newer version exists and fetch it
    function(check_and_update_dependency NAME REPO_URL CURRENT_VERSION TAG_PREFIX SOURCE_DIR)
        message(STATUS "  Checking ${NAME}...")
        execute_process(
            COMMAND git ls-remote --tags --refs ${REPO_URL}
            OUTPUT_VARIABLE GIT_TAGS
            ERROR_QUIET
            TIMEOUT 5
        )

        set(NEEDS_UPDATE FALSE)
        set(LATEST_VERSION "${CURRENT_VERSION}")

        if(GIT_TAGS)
            # Extract version tags
            # NB: CMake regex has no \s; a trailing [^\s]* greedily swallows the
            # commit hash and tab from ls-remote output, producing a corrupt ref
            # (e.g. "v0.3.8\n<hash>\tref"). Match an exact X.Y.Z version instead.
            string(REGEX MATCHALL "refs/tags/v?[0-9]+\\.[0-9]+\\.[0-9]+" VERSION_MATCHES "${GIT_TAGS}")
            foreach(VERSION_MATCH ${VERSION_MATCHES})
                string(REGEX REPLACE "refs/tags/v?" "" VERSION_NUM "${VERSION_MATCH}")
                string(REGEX REPLACE "refs/tags/" "" VERSION_NUM "${VERSION_NUM}")
                if(VERSION_NUM VERSION_GREATER CURRENT_VERSION)
                    set(LATEST_VERSION "${VERSION_NUM}")
                    set(NEEDS_UPDATE TRUE)
                endif()
            endforeach()
        endif()

        if(NEEDS_UPDATE)
            string(ASCII 27 ESC)
            message("${ESC}[1;93m  UPDATE: ${NAME} v${CURRENT_VERSION} -> v${LATEST_VERSION} available${ESC}[0m")
            message(STATUS "  Downloading ${NAME} v${LATEST_VERSION}...")

            FetchContent_Declare(${NAME}
                GIT_REPOSITORY ${REPO_URL}
                GIT_TAG ${TAG_PREFIX}${LATEST_VERSION}
                SOURCE_DIR "${SOURCE_DIR}"
            )
            FetchContent_Populate(${NAME})
            message(STATUS "  ${NAME} updated to v${LATEST_VERSION}")
        else()
            message(STATUS "  ${NAME} v${CURRENT_VERSION} is up to date")
        endif()
    endfunction()

    # Check each dependency for newer versions
    check_and_update_dependency("pugixml"
        "https://github.com/zeux/pugixml.git"
        "${PUGIXML_VERSION}" "v" "${WS_PUGIXML_DIR}")

    check_and_update_dependency("simpleini"
        "https://github.com/brofield/simpleini.git"
        "${SIMPLEINI_VERSION}" "v" "${WS_SIMPLEINI_DIR}")

    message(STATUS "  picomath v${PICOMATH_VERSION}: up to date")

    check_and_update_dependency("kubazip"
        "https://github.com/kuba--/zip.git"
        "${KUBAZIP_VERSION}" "v" "${WS_ZIP_DIR}")

    message(STATUS "  cpp-unicodelib: master branch (manual update only)")

    message(STATUS "")
    message(STATUS "=== Dependency check complete ===")
    message(STATUS "")
else()
    message(STATUS "Third-party dependencies (update checking disabled):")
    message(STATUS "  pugixml v${PUGIXML_VERSION}")
    message(STATUS "  cpp-unicodelib (master)")
    message(STATUS "  simpleini v${SIMPLEINI_VERSION}")
    message(STATUS "  kuba--/zip v${KUBAZIP_VERSION}")
    message(STATUS "  chillout (local)")
endif()


# The terminal UI (ws) has no TUI-only third-party dependencies left: PDF
# print generation and font discovery/classification all use system
# frameworks (Quartz, Core Text) -- see the CoreGraphics/CoreText framework
# linking below.


#======================= SOURCE FILES =======================#
if(BUILD_GUI)
    qt_standard_project_setup()
endif()

# Set paths for shared source list
set(WS_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
include(${CMAKE_SOURCE_DIR}/cmake/sources.cmake)

# Main executable sources
set(SOURCES
    ${WORDTSAR_ALL_SOURCES}
    src/gui/wordtsar.cpp src/gui/wordtsar.h
    src/main.cpp
    src/third-party/chillout/src/chillout/posix/posixcrashhandler.cpp
    src/core/spellcheck/macspellcheck.mm
)

if(BUILD_GUI)
qt_add_executable(WordTsar MACOSX_BUNDLE ${SOURCES})
endif()



#======================= WORDSTARTUI TUI (ws) =======================#
#
# The terminal UI (ws). Renders on the self-contained wordstartui toolkit,
# reusing the shared engine (core + input + file) and neutral TUI sources
# (fonts, layout, crash reporting) + Quartz/Core Text for PDF print.
#
if(BUILD_TUI)
    add_executable(ws
        ${WORDTSAR_WSTUI_SOURCES}
        ${WORDTSAR_CORE_SOURCES}
        ${WORDTSAR_INPUT_SOURCES}
        ${WORDTSAR_FILE_SOURCES}
        ${WS_THIRDPARTY_DIR}/pugixml/src/pugixml.cpp
        ${WS_THIRDPARTY_DIR}/zip/src/zip.c
        src/third-party/chillout/src/chillout/chillout.cpp
        src/third-party/chillout/src/chillout/common/common.cpp
        src/third-party/chillout/src/chillout/posix/posixcrashhandler.cpp
        src/core/spellcheck/macspellcheck.mm
    )

    # Print preview (PDF via Quartz/Core Text) -- the printout is
    # backend-neutral (cEditorBase + system frameworks, no vendored PDF lib).
    target_sources(ws PRIVATE ${WS_SRC_DIR}/tui/print/tuiprintout.cpp)

    target_include_directories(ws PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/tui/fonts
        ${CMAKE_SOURCE_DIR}/src/third-party/chillout/src
        ${WS_PUGIXML_DIR}/src
        ${WS_UNICODELIB_DIR}
        ${WS_SIMPLEINI_DIR}
        ${WS_PICOMATH_DIR}/include
        ${WS_ZIP_DIR}/src
    )

    target_compile_definitions(ws PRIVATE
        WORDTSAR_TUI_BUILD
        WORDTSAR_NO_QT
        WORDTSAR_USE_SIMPLE_FONTS
        MINIZ_EXPORT=
    )

    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(CORETEXT_LIBRARY CoreText)
    find_library(COREGRAPHICS_LIBRARY CoreGraphics)
    target_link_libraries(ws PRIVATE
        "-framework Foundation" "-framework AppKit"
        ${COREFOUNDATION_LIBRARY} ${CORETEXT_LIBRARY} ${COREGRAPHICS_LIBRARY})

    target_compile_options(ws PRIVATE
        -Wall -Werror=return-type -Wextra
        -Wno-deprecated-declarations
        -Wunused-parameter -Wunused-variable
    )
endif()


#======================= NEUTRAL TUI TESTS (WSTuiTest) =======================#
# Exercises the backend-neutral TUI font/layout code (fonts, layout). Kept in a
# separate executable from WSTest because the GUI and TUI both define cLayout.
if(BUILD_TUI)
    add_executable(WSTuiTest
        src/test/test-tuimain.cpp
        src/test/test-tuifonts.cpp
        src/test/test-tuitextmeasure.cpp
        src/test/test-tuilayout.cpp
        src/test/test-tuicolorutils.cpp
        src/test/test-tuicolorpicker.cpp
        src/test/test-tuimenuwalk.cpp
        src/tui/print/tuiprintout.cpp
        ${WORDTSAR_WSTUI_SOURCES_NO_MAIN}
        ${WORDTSAR_CORE_SOURCES}
        ${WORDTSAR_INPUT_SOURCES}
        ${WORDTSAR_FILE_SOURCES}
        ${WS_THIRDPARTY_DIR}/pugixml/src/pugixml.cpp
        ${WS_THIRDPARTY_DIR}/zip/src/zip.c
        src/third-party/chillout/src/chillout/chillout.cpp
        src/third-party/chillout/src/chillout/common/common.cpp
        src/third-party/chillout/src/chillout/posix/posixcrashhandler.cpp
        src/core/spellcheck/macspellcheck.mm
    )

    target_include_directories(WSTuiTest PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/tui/fonts
        ${CMAKE_SOURCE_DIR}/src/third-party/chillout/src
        ${WS_PUGIXML_DIR}/src
        ${WS_UNICODELIB_DIR}
        ${WS_SIMPLEINI_DIR}
        ${WS_PICOMATH_DIR}/include
        ${WS_ZIP_DIR}/src
    )

    target_compile_definitions(WSTuiTest PRIVATE
        WORDTSAR_TUI_BUILD
        WORDTSAR_NO_QT
        WORDTSAR_USE_SIMPLE_FONTS
        WORDTSAR_TUI_TEST_HOOKS
        MINIZ_EXPORT=
    )

    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(CORETEXT_LIBRARY CoreText)
    find_library(COREGRAPHICS_LIBRARY CoreGraphics)
    target_link_libraries(WSTuiTest PRIVATE
        "-framework Foundation" "-framework AppKit"
        ${COREFOUNDATION_LIBRARY} ${CORETEXT_LIBRARY} ${COREGRAPHICS_LIBRARY})

    set_target_properties(WSTuiTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/test"
    )
endif()


if(BUILD_GUI)
target_include_directories(WordTsar PRIVATE
    src/third-party/chillout/src
    ${WS_PUGIXML_DIR}/src
    ${WS_UNICODELIB_DIR}
    ${WS_SIMPLEINI_DIR}
    ${WS_PICOMATH_DIR}/include
    ${WS_ZIP_DIR}/src
)

target_link_libraries(WordTsar PUBLIC
    Qt::Core
    Qt::Gui
    Qt::PrintSupport
    Qt::Widgets
    "-framework Foundation"
    "-framework AppKit"
)

# Compile options for better compatibility
target_compile_options(WordTsar PRIVATE
    -Wall
    -Werror=return-type
    -Wextra
    -Wno-deprecated-declarations
    -Wunused-parameter
    -Wunused-variable
)
# Define MINIZ_EXPORT for all compilers to avoid missing export file
target_compile_definitions(WordTsar PRIVATE
    MINIZ_EXPORT=
)


# Resources:
set(resources_resource_files
    "src/gui/images/busy.gif"
    "src/gui/images/icon64x64.png"
    "src/gui/images/splash.png"
)

qt_add_resources(WordTsar "resources"
    PREFIX
        "/"
    BASE
        "src"
    FILES
        ${resources_resource_files}
)

# macOS app bundle settings
set_target_properties(WordTsar PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_EXECUTABLE_NAME "WordTsar"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.wordtsar.WordTsar"
    MACOSX_BUNDLE_BUNDLE_NAME "WordTsar"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${WORDTSAR_VERSION_MAJOR}.${WORDTSAR_VERSION_MINOR}.${WORDTSAR_VERSION_BUILD}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${WORDTSAR_VERSION_MAJOR}.${WORDTSAR_VERSION_MINOR}.${WORDTSAR_VERSION_BUILD}"
)

if(CMAKE_BUILD_TYPE STREQUAL Release)
    target_compile_definitions(WordTsar PRIVATE
        NDEBUG
    )
elseif(CMAKE_BUILD_TYPE STREQUAL Debug)
    target_compile_definitions(WordTsar PRIVATE
        DEBUG
    )
endif()


# Code signing ---------------------------------------------------------
# macOS requires every executable to carry some signature to run at all;
# left alone, the linker applies an anonymous ad-hoc signature that gets a
# brand new identity every rebuild. Signing explicitly with a stable local
# identity avoids that churn. Uses a self-signed "WordTsar Local Dev"
# certificate from the login keychain if one has been set up (one-time,
# see BUILDING.md); otherwise falls back to the default ad-hoc signature,
# so the build still works out of the box with zero setup.
execute_process(
    COMMAND security find-identity -v -p codesigning
    OUTPUT_VARIABLE WORDTSAR_CODESIGN_IDENTITIES
    ERROR_QUIET
)
if(WORDTSAR_CODESIGN_IDENTITIES MATCHES "WordTsar Local Dev")
    set(WORDTSAR_CODESIGN_IDENTITY "WordTsar Local Dev")
else()
    set(WORDTSAR_CODESIGN_IDENTITY "-")
endif()
message(STATUS "Code signing identity: ${WORDTSAR_CODESIGN_IDENTITY}")

add_custom_command(TARGET WordTsar POST_BUILD
    COMMAND codesign --force --deep --sign "${WORDTSAR_CODESIGN_IDENTITY}"
            --identifier com.wordtsar.WordTsar
            "$<TARGET_BUNDLE_DIR:WordTsar>"
    COMMENT "Code signing WordTsar.app"
    VERBATIM
)


# Version configuration
set(WORDTSAR_AUTO_INCREMENT_BUILD OFF CACHE BOOL "Auto-increment build number on Release builds")

# Read version from persistent version.txt file
set(VERSION_FILE "${CMAKE_SOURCE_DIR}/version.txt")
if(EXISTS "${VERSION_FILE}")
    file(STRINGS "${VERSION_FILE}" VERSION_LINES)
    foreach(LINE ${VERSION_LINES})
        if(LINE MATCHES "^MAJOR=([0-9]+)")
            set(FILE_MAJOR ${CMAKE_MATCH_1})
        elseif(LINE MATCHES "^MINOR=([0-9]+)")
            set(FILE_MINOR ${CMAKE_MATCH_1})
        elseif(LINE MATCHES "^BUILD=([0-9]+)")
            set(FILE_BUILD ${CMAKE_MATCH_1})
        endif()
    endforeach()
else()
    # Fallback defaults if version.txt doesn't exist
    set(FILE_MAJOR 0)
    set(FILE_MINOR 4)
    set(FILE_BUILD 1)
endif()

# Always use file values as the source of truth, override any cache
set(WORDTSAR_VERSION_MAJOR ${FILE_MAJOR})
set(WORDTSAR_VERSION_MINOR ${FILE_MINOR})
set(WORDTSAR_VERSION_BUILD ${FILE_BUILD})

# Auto-increment build number for Release builds if enabled
if(WORDTSAR_AUTO_INCREMENT_BUILD AND CMAKE_BUILD_TYPE STREQUAL "Release")
    math(EXPR NEW_BUILD_NUM "${WORDTSAR_VERSION_BUILD} + 1")
    set(WORDTSAR_VERSION_BUILD ${NEW_BUILD_NUM})

    # Update version.txt with new build number
    file(WRITE "${VERSION_FILE}" "MAJOR=${WORDTSAR_VERSION_MAJOR}\nMINOR=${WORDTSAR_VERSION_MINOR}\nBUILD=${NEW_BUILD_NUM}")

    message(STATUS "Build number incremented from ${FILE_BUILD} to ${NEW_BUILD_NUM}")
    message(STATUS "Updated ${VERSION_FILE}")
endif()

message(STATUS "WordTsar Version: ${WORDTSAR_VERSION_MAJOR}.${WORDTSAR_VERSION_MINOR}.${WORDTSAR_VERSION_BUILD}")

# Configure version header
configure_file(
    "${CMAKE_SOURCE_DIR}/version.h.in"
    "${CMAKE_SOURCE_DIR}/src/core/include/version.h"
    @ONLY
)


# Installation and deployment
install(TARGETS WordTsar
    BUNDLE DESTINATION .
    COMPONENT Applications
)

# Generate deployment script for distributing the application
qt_generate_deploy_app_script(
    TARGET WordTsar
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
endif() # BUILD_GUI
