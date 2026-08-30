To make and run tests/code coverage
===================================

LINUX/MACOS:
============
You can use the build script:
  ./build-tests.sh

Or manually:
  mkdir build
  cd build
  cmake ../
  make -j$(nproc)

WINDOWS:
========
You can use the build script:
  build-tests.bat

Or manually:
  mkdir build
  cd build
  cmake ../
  cmake --build . --config Release

RUNNING TESTS:
==============
Run WSTest (or WSTest.exe on Windows) to get test results

CODE COVERAGE (Linux only):
============================
From the build directory:
  make coverage
  open coverage/index.html in web browser
