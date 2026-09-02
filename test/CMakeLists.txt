cmake_minimum_required(VERSION 3.16)

if (WIN32)
    project(WSTest LANGUAGES C CXX)
elseif(UNIX)
    project(WSTest)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Linux and MacOS compile options
if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(CMAKE_CXX_FLAGS "-Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -fprofile-update=atomic")
    set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -fprofile-update=atomic")

    set(CMAKE_C_FLAGS "-Wall -Wextra")
    set(CMAKE_C_FLAGS_DEBUG "-g -fprofile-update=atomic")
    set(CMAKE_C_FLAGS_RELEASE "-Ofast -fprofile-update=atomic")


    if(CMAKE_BUILD_TYPE STREQUAL Release)
        message(STATUS "C Compile flags: ${CMAKE_C_FLAGS_RELEASE} ${CMAKE_C_FLAGS}")
        message(STATUS "C++ Compile flags: ${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS}")
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        message(STATUS "C Compile flags: ${CMAKE_C_FLAGS_DEBUG} ${CMAKE_C_FLAGS}")
        message(STATUS "C++ Compile flags: ${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_FLAGS}")
    endif()

    set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -no-pie" )

endif()


set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;Debug" CACHE STRING "" FORCE)


#======================= INCLUSION OF Qt =======================#
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ../src/gui/dialogs)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Try to find Qt6 automatically first, fall back to hardcoded paths if needed
find_package(Qt6 QUIET COMPONENTS Core)
if(NOT Qt6_FOUND)
    if(WIN32)
        set(CMAKE_PREFIX_PATH "C:/Qt/6.8.2/msvc2022_64" "C:/Qt/6.7.0/msvc2022_64" "C:/Qt/6.6.0/msvc2022_64")
    elseif(APPLE)
        set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.8.2/macos" "$ENV{HOME}/Qt/6.7.0/macos" "$ENV{HOME}/Qt/6.6.0/macos")
    else()
        # Linux - try common installation paths
        set(CMAKE_PREFIX_PATH
            "$ENV{HOME}/Qt/6.8.2/gcc_64"
            "$ENV{HOME}/Qt/6.7.0/gcc_64"
            "$ENV{HOME}/Qt/6.6.0/gcc_64"
            "/opt/Qt/6.8.2/gcc_64"
            "/opt/Qt/6.7.0/gcc_64"
            "/opt/Qt/6.6.0/gcc_64"
            "/usr/lib/x86_64-linux-gnu/qt6"
            "/usr/lib/qt6"
        )
    endif()
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets PrintSupport)

qt_standard_project_setup()

#======================= THIRD-PARTY PATHS =======================#
# Third-party source directories (shared with main build)
set(WS_THIRDPARTY_DIR "${CMAKE_SOURCE_DIR}/../third-party")

#======================= SOURCE FILES =======================#
# Set paths for shared source list
set(WS_SRC_DIR "${CMAKE_SOURCE_DIR}/../src")
include(${CMAKE_SOURCE_DIR}/../cmake/sources.cmake)

# Test executable sources
qt_add_executable(WSTest WIN32 MACOSX_BUNDLE
    # Test files
    ../src/test/test-document.cpp
    ../src/test/test-codepage.cpp
    ../src/test/test-utils.cpp
    ../src/test/test-commserver.cpp
    ../src/test/test_layoutstructs.cpp
    ../src/test/test-baselayout.cpp
    ../src/test/test-guilayout.cpp
    ../src/test/test-guieditor.cpp
    ../src/test/test-baseeditor.cpp
    ../src/test/test-wsfontclassifier.cpp
    ../src/test/test-guiprinter.cpp
    ../src/test/test-spellcheck.cpp
    ../src/test/test-rtf.cpp
    ../src/test/test-docxwriter.cpp
    ../src/test/test-docxreader.cpp
    ../src/test/test-tocindexgenerator.cpp
    ../src/test/test-tuifontutils.cpp
    ../src/test/test-config.cpp
    ../src/test/test-wordstarinput.cpp

    # Shared sources (from cmake/sources.cmake)
    ${WORDTSAR_ALL_SOURCES}

    # TUI layout utils (pure C++ — no font system deps, no name collisions)
    ${WS_SRC_DIR}/tui/layout/tuifontutils.cpp ${WS_SRC_DIR}/tui/layout/tuifontutils.h

    # Test-only stub (replaces gui/wordtsar.cpp for test builds)
    ../src/test/wordtsar-stub.cpp

    # Header-only third-party (needed for include resolution)
    ../third-party/cpp-unicodelib/unicodelib.h
    ../third-party/cpp-unicodelib/unicodelib_encodings.h
    ../third-party/pugixml/src/pugiconfig.hpp
    ../third-party/pugixml/src/pugixml.hpp
    ../third-party/zip/src/miniz.h
    ../third-party/zip/src/zip.h

    # Additional headers for chillout
    ../src/third-party/chillout/src/chillout/chillout.h
    ../src/third-party/chillout/src/chillout/defines.h
    ../src/third-party/chillout/src/chillout/common/common.h

    # Comm subsystem (test-only for now, not in main build)
    ../src/core/comm/commdefs.h
    ../src/core/comm/comm_utils.cpp ../src/core/comm/comm_utils.h
    ../src/core/comm/commserver.cpp ../src/core/comm/commserver.h
    ../src/core/comm/commclient.cpp ../src/core/comm/commclient.h
)

if (WIN32)
  target_sources(WSTest PRIVATE
     ../src/third-party/chillout/src/chillout/windows/StackWalker.h ../src/third-party/chillout/src/chillout/windows/StackWalker.cpp
     ../src/third-party/chillout/src/chillout/windows/windowscrashhandler.h ../src/third-party/chillout/src/chillout/windows/windowscrashhandler.cpp
  )
else()
  target_sources(WSTest PRIVATE
     ../src/third-party/chillout/src/chillout/posix/posixcrashhandler.h ../src/third-party/chillout/src/chillout/posix/posixcrashhandler.cpp
  )
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_include_directories(WSTest PRIVATE /usr/include/freetype2)
  endif()
endif()

if (APPLE)
  target_sources(WSTest PRIVATE
    ../src/core/spellcheck/macspellcheck.mm
  )
endif()

# Code coverage support
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules)
if (CMAKE_COMPILER_IS_GNUCXX)
    include(CodeCoverage)
    append_coverage_compiler_flags()

    # we need to turn off optimization for non-skewed coverage reports
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -O0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")

    # importantly, set the path to the gcovr executable that you downladed
    set(GCOVR_PATH "/usr/bin/gcovr")

    # Create a self-contained coverage script that runs tests and generates HTML report
    file(WRITE ${CMAKE_BINARY_DIR}/run-tests-for-coverage.sh
        "#!/bin/bash\n"
        "# Self-contained coverage script - run from test/build/\n"
        "# Builds, runs tests, and generates HTML coverage report in one command.\n"
        "# Note: not using set -e because lcov returns non-zero on harmless warnings\n"
        "\n"
        "BUILDDIR=\"${CMAKE_BINARY_DIR}\"\n"
        "BASEDIR=\"${PROJECT_SOURCE_DIR}\"\n"
        "\n"
        "echo \"=== Cleaning old gcda files ===\"\n"
        "find \"$BUILDDIR\" -name '*.gcda' -delete\n"
        "\n"
        "echo \"=== Building for coverage ===\"\n"
        "cd \"$BUILDDIR\" && cmake \"$BASEDIR\" -DCMAKE_BUILD_TYPE=Debug && make -j\n"
        "if [ $? -ne 0 ]; then echo \"Build failed\"; exit 1; fi\n"
        "\n"
        "echo \"=== Cleaning previous coverage data ===\"\n"
        "lcov --gcov-tool gcov -directory . -b \"$BASEDIR\" --zerocounters\n"
        "\n"
        "echo \"=== Creating baseline ===\"\n"
        "lcov --gcov-tool gcov -c -i -d . -b \"$BASEDIR\" -o coverage.base\n"
        "\n"
        "echo \"=== Running tests ===\"\n"
        "QT_TESTING=1 ./WSTest -aa=1000 || true\n"
        "\n"
        "echo \"=== Capturing coverage data ===\"\n"
        "lcov --gcov-tool gcov --directory . -b \"$BASEDIR\" --capture --output-file coverage.capture\n"
        "\n"
        "echo \"=== Merging baseline and capture ===\"\n"
        "lcov --gcov-tool gcov -a coverage.base -a coverage.capture --output-file coverage.total\n"
        "\n"
        "echo \"=== Filtering excludes ===\"\n"
        "lcov --gcov-tool gcov --remove coverage.total \\\n"
        "    '/usr/include/*' \\\n"
        "    '${PROJECT_SOURCE_DIR}/../src/third-party/*' \\\n"
        "    '${PROJECT_SOURCE_DIR}/../src/test/*' \\\n"
        "    '${PROJECT_SOURCE_DIR}/../src/core/document/math.cpp' \\\n"
        "    '${CMAKE_BINARY_DIR}/.qt/*' \\\n"
        "    '$ENV{HOME}/Qt/*' \\\n"
        "    '${PROJECT_SOURCE_DIR}/../test/*' \\\n"
        "    --output-file coverage.info\n"
        "\n"
        "echo \"=== Generating HTML report ===\"\n"
        "genhtml --demangle-cpp -o coverage coverage.info\n"
        "\n"
        "echo \"=== Done ===\"\n"
        "echo \"Open ./coverage/index.html in your browser to view the coverage report.\"\n"
    )
    execute_process(COMMAND chmod +x ${CMAKE_BINARY_DIR}/run-tests-for-coverage.sh)

    SETUP_TARGET_FOR_COVERAGE_LCOV(
        NAME coverage
        EXECUTABLE ${CMAKE_BINARY_DIR}/run-tests-for-coverage.sh
        DEPENDENCIES WSTest
        EXCLUDE "/usr/include" "../src/third-party" "../src/test" "../src/core/document/math.cpp" "build/.qt" "$ENV{HOME}/Qt" "../test"
    )
endif ()



target_include_directories(WSTest PRIVATE
    ../src/third-party/chillout/src
    ${WS_THIRDPARTY_DIR}/pugixml/src
    ${WS_THIRDPARTY_DIR}/cpp-unicodelib
    ${WS_THIRDPARTY_DIR}/simpleini
    ${WS_THIRDPARTY_DIR}/picomath/include
    ${WS_THIRDPARTY_DIR}/zip/src
)

target_include_directories(WSTest PRIVATE
    ../
)

set(LIBS
    Qt::Core
    Qt::Gui
    Qt::PrintSupport
    Qt::Widgets
)

if (WIN32)
   list(APPEND LIBS
      ws2_32
      Dbghelp
   )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
   list(APPEND LIBS
      pthread
      hunspell
   )
elseif(APPLE)
   # macOS specific libraries
   list(APPEND LIBS
      "-framework Foundation"
      "-framework AppKit"
   )
endif()

target_link_libraries(WSTest PRIVATE ${LIBS})

# Platform-specific compile options for better compatibility
if(MSVC)
    # Visual Studio specific options
    target_compile_options(WSTest PRIVATE
        /W4  # High warning level
        /utf-8  # Use UTF-8 for source and execution character sets
        /permissive-  # Disable non-conforming code
        /bigobj  # Allow large object files (fixes "number of sections exceeded" error)
        /MP  # Enable multi-processor compilation (parallel builds)
    )
    # Disable specific warnings that are common in Qt projects
    target_compile_definitions(WSTest PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # GCC/Clang specific options
    target_compile_options(WSTest PRIVATE
        -Wall
        -Werror=return-type
        -Wextra
        -Wno-deprecated-declarations
        -Wunused-parameter
        -Wunused-variable
    )
endif()


# Resources:
set(resources_resource_files
    "../src/gui/images/busy.gif"
    "../src/gui/images/icon64x64.png"
    "../src/gui/images/splash.png"
)

qt_add_resources(WSTest "resources"
    PREFIX
        "/"
    BASE
        "../src"
    FILES
        ${resources_resource_files}
)

# Platform-specific build settings
if(WIN32)
    # Windows-specific settings
    set_target_properties(WSTest PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(WSTest PRIVATE /O2)
    endif()
elseif(APPLE)
    # macOS-specific settings
    set_target_properties(WSTest PROPERTIES
        MACOSX_BUNDLE TRUE
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL Release)
    target_compile_definitions(WSTest PRIVATE
        NDEBUG
    )
endif()


install(TARGETS WSTest
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET WSTest
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
