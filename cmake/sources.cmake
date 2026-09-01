# Shared source-file lists for WordTsar (GUI), ws (TUI), WSTuiTest, and WSTest.
#
# This file is not part of the upstream SourceForge Mercurial repository (it
# was excluded there via .hgignore, apparently as a local/generated file that
# never got committed). It was reconstructed from the src/ tree to make the
# CMake build reproducible from a clean checkout.
#
# Expects WS_SRC_DIR to already be set (top-level CMakeLists.txt does this
# before including this file).

# ------------------------------------------------------------------------
# Core engine (document model, layout, editor base, codepages, utils).
# The comm/ subsystem (commserver/commclient) is test-only for now (see
# test/CMakeLists.txt) and deliberately excluded from the main build.
# macspellcheck.mm is Apple-only and is appended separately by the caller
# under if(APPLE) -- file(GLOB ... *.cpp) does not pick up .mm files anyway.
# ------------------------------------------------------------------------
file(GLOB_RECURSE WORDTSAR_CORE_SOURCES
    "${WS_SRC_DIR}/core/*.cpp"
    "${WS_SRC_DIR}/core/*.h"
)
list(FILTER WORDTSAR_CORE_SOURCES EXCLUDE REGEX "/core/comm/")

# ------------------------------------------------------------------------
# Input handling (modern + WordStar key input).
# ------------------------------------------------------------------------
file(GLOB WORDTSAR_INPUT_SOURCES
    "${WS_SRC_DIR}/input/*.cpp"
    "${WS_SRC_DIR}/input/*.h"
)

# ------------------------------------------------------------------------
# File format readers/writers (WordStar, RTF, DOCX, plain text).
# ------------------------------------------------------------------------
file(GLOB_RECURSE WORDTSAR_FILE_SOURCES
    "${WS_SRC_DIR}/files/*.cpp"
    "${WS_SRC_DIR}/files/*.h"
)

# ------------------------------------------------------------------------
# Qt GUI-only sources. wordtsar.cpp/.h (the main window) and WordTsar.rc
# (Windows resource file) are added separately by the caller.
# ------------------------------------------------------------------------
file(GLOB_RECURSE WORDTSAR_GUI_SOURCES
    "${WS_SRC_DIR}/gui/*.cpp"
    "${WS_SRC_DIR}/gui/*.h"
    "${WS_SRC_DIR}/gui/*.ui"
)
list(FILTER WORDTSAR_GUI_SOURCES EXCLUDE REGEX "/gui/wordtsar\\.(cpp|h)$")

# ------------------------------------------------------------------------
# Combined source list for the GUI (WordTsar) target and for WSTest, which
# both add wordtsar.cpp/.h (or a stub) plus a couple of extras themselves.
# Also carries the third-party implementation files (pugixml, zip, chillout
# base) that the `ws`/`WSTuiTest` targets add explicitly themselves but the
# GUI/WSTest targets do not -- these have to come from somewhere.
# ------------------------------------------------------------------------
set(WORDTSAR_ALL_SOURCES
    ${WORDTSAR_CORE_SOURCES}
    ${WORDTSAR_INPUT_SOURCES}
    ${WORDTSAR_FILE_SOURCES}
    ${WORDTSAR_GUI_SOURCES}
    ${WS_THIRDPARTY_DIR}/pugixml/src/pugixml.cpp
    ${WS_THIRDPARTY_DIR}/zip/src/zip.c
    ${WS_SRC_DIR}/third-party/chillout/src/chillout/chillout.cpp
    ${WS_SRC_DIR}/third-party/chillout/src/chillout/common/common.cpp
)

# ------------------------------------------------------------------------
# Backend-neutral TUI sources: fonts, layout, crash/debug reporting. Shared
# between the `ws` executable and the `WSTuiTest` unit tests. Deliberately
# excludes src/tui/wordstartui (the widget toolkit) -- WSTuiTest lists the
# handful of widget-toolkit files it needs explicitly, to avoid duplicating
# them against WORDTSAR_WSTUI_SOURCES below.
# ------------------------------------------------------------------------
file(GLOB WORDTSAR_TUI_NEUTRAL_SOURCES
    "${WS_SRC_DIR}/tui/fonts/*.cpp"
    "${WS_SRC_DIR}/tui/fonts/*.h"
    "${WS_SRC_DIR}/tui/layout/*.cpp"
    "${WS_SRC_DIR}/tui/layout/*.h"
    "${WS_SRC_DIR}/tui/debugreport/*.cpp"
    "${WS_SRC_DIR}/tui/debugreport/*.h"
)

# ------------------------------------------------------------------------
# ws-only TUI sources: entry point, app class, editor glue, dialogs, and the
# full wordstartui widget toolkit -- plus WORDTSAR_TUI_NEUTRAL_SOURCES itself,
# since the `ws` add_executable() call only pulls WSTUI + core/input/file (it
# does not separately reference WORDTSAR_TUI_NEUTRAL_SOURCES). WSTuiTest pulls
# WORDTSAR_TUI_NEUTRAL_SOURCES directly instead, so nesting it here does not
# create duplicates for that target. (src/tui/print/tuiprintout.cpp is added
# separately by the caller, which links it against Quartz/Core Text.)
# ------------------------------------------------------------------------
file(GLOB WORDTSAR_WSTUI_SOURCES
    "${WS_SRC_DIR}/tui/main.cpp"
    "${WS_SRC_DIR}/tui/wordtsar.cpp"
    "${WS_SRC_DIR}/tui/wordtsar.h"
    "${WS_SRC_DIR}/tui/editor/*.cpp"
    "${WS_SRC_DIR}/tui/editor/*.h"
    "${WS_SRC_DIR}/tui/dialogs/*.cpp"
    "${WS_SRC_DIR}/tui/dialogs/*.h"
    "${WS_SRC_DIR}/tui/wordstartui/src/*.cpp"
    "${WS_SRC_DIR}/tui/wordstartui/src/*.h"
)
list(APPEND WORDTSAR_WSTUI_SOURCES ${WORDTSAR_TUI_NEUTRAL_SOURCES})
