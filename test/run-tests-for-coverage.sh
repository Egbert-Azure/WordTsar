#!/bin/bash
# Self-contained coverage script - run from test/build/
# Builds, runs tests, and generates HTML coverage report in one command.
# Note: not using set -e because lcov returns non-zero on harmless warnings

BUILDDIR="/home/gbr/src/wordtsar/test"
BASEDIR="/home/gbr/src/wordtsar/test"

echo "=== Cleaning old gcda files ==="
find "$BUILDDIR" -name '*.gcda' -delete

echo "=== Building for coverage ==="
cd "$BUILDDIR" && cmake "$BASEDIR" -DCMAKE_BUILD_TYPE=Debug && make -j
if [ $? -ne 0 ]; then echo "Build failed"; exit 1; fi

echo "=== Cleaning previous coverage data ==="
lcov --gcov-tool gcov -directory . -b "$BASEDIR" --zerocounters

echo "=== Creating baseline ==="
lcov --gcov-tool gcov -c -i -d . -b "$BASEDIR" -o coverage.base

echo "=== Running tests ==="
QT_TESTING=1 ./WSTest -aa=1000 || true

echo "=== Capturing coverage data ==="
lcov --gcov-tool gcov --directory . -b "$BASEDIR" --capture --output-file coverage.capture

echo "=== Merging baseline and capture ==="
lcov --gcov-tool gcov -a coverage.base -a coverage.capture --output-file coverage.total

echo "=== Filtering excludes ==="
lcov --gcov-tool gcov --remove coverage.total \
    '/usr/include/*' \
    '/home/gbr/src/wordtsar/test/../src/third-party/*' \
    '/home/gbr/src/wordtsar/test/../src/test/*' \
    '/home/gbr/src/wordtsar/test/../src/core/document/math.cpp' \
    '/home/gbr/src/wordtsar/test/.qt/*' \
    '/home/gbr/Qt/*' \
    '/home/gbr/src/wordtsar/test/../test/*' \
    --output-file coverage.info

echo "=== Generating HTML report ==="
genhtml --demangle-cpp -o coverage coverage.info

echo "=== Done ==="
echo "Open ./coverage/index.html in your browser to view the coverage report."
