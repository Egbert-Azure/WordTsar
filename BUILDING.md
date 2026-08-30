# Building WordTsar

This document covers building WordTsar on macOS.

## Prerequisites

- **Qt6** (6.6.0 or later) - Core, Widgets, PrintSupport modules
- **CMake** (3.16 or later)
- **Git** (for dependency fetching)
- **C++20 compatible compiler** — Xcode Command Line Tools (minimum) or full Xcode
- **Qt6** via Homebrew (`brew install qt@6`) or the Qt Online Installer

## Build Targets

WordTsar builds one or both of these from the same source, depending on how you configure it:

- **WordTsar** — the Qt6 GUI app. Built by default (`BUILD_GUI`, default `ON`).
- **ws** — a terminal UI: full-screen writing in a terminal window, no window chrome, uses the wordstartui toolkit instead of Qt. Off by default; add `-DBUILD_TUI=ON` to the `cmake` configure line to build it too. Both targets share the same document engine and file formats, so which one to build (or both) is purely a matter of how you want to write — there's no functional tradeoff in file compatibility between them.

```bash
# GUI only (default)
cmake .. -DCMAKE_BUILD_TYPE=Release

# GUI + terminal UI
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TUI=ON

# Terminal UI only, no Qt required at all
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=OFF -DBUILD_TUI=ON
```

After building with `-DBUILD_TUI=ON`, run the terminal UI directly from a real terminal window (not through `open`, which is for `.app` bundles):

```bash
./build/ws
```

## Command Line

```bash
# Install Qt6 (if using Homebrew)
brew install qt@6

# Clone and prepare
cd wordtsar/
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(sysctl -n hw.ncpu)
```

## Xcode

```bash
# Generate Xcode project
mkdir build && cd build
cmake .. -G Xcode -DCMAKE_BUILD_TYPE=Release

# Open in Xcode
open WordTsar.xcodeproj
```

Then build and run in Xcode normally.

## VS Code with CMake Tools

1. Install **CMake Tools** extension
2. Open WordTsar folder
3. **Cmd+Shift+P** → "CMake: Configure"
4. Select Clang compiler
5. **Cmd+Shift+P** → "CMake: Build"

## Build Options

### CMake Configuration Options

```bash
# Build TUI executable (ws) alongside GUI (WordTsar)
cmake .. -DBUILD_TUI=ON

# Disable dependency update checking (faster rebuilds, and avoids picking up
# an untested newer version of a pinned dependency)
cmake .. -DUPDATE_DEPENDENCIES=OFF

# Specify Qt installation path
cmake .. -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6"

# Build with debug info in release
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Disable auto-increment of build number
cmake .. -DWORDTSAR_AUTO_INCREMENT_BUILD=OFF

# Set version explicitly
cmake .. -DWORDTSAR_VERSION_MAJOR=1 -DWORDTSAR_VERSION_MINOR=0 -DWORDTSAR_VERSION_BUILD=100
```

### Build Configurations

- **Debug**: Full debug symbols, no optimization
- **Release**: Optimized for performance (`-Ofast`, LTO)
- **RelWithDebInfo**: Optimized with debug symbols

**Important**: Always specify `-DCMAKE_BUILD_TYPE`. Without it, CMake produces an unoptimized binary with no `-O` flags.

## Dependency Management

WordTsar automatically downloads dependencies using CMake FetchContent:

- **pugixml**: XML parsing for DOCX
- **kuba--/zip**: ZIP file handling
- **simpleini**: Configuration files
- **exprtk**: Math expression parsing
- **cpp-unicodelib**: Unicode processing
- **FTXUI**: TUI framework (only when `-DBUILD_TUI=ON`)

Dependencies are downloaded to `third-party/` directory on first build.

**Note**: `UPDATE_DEPENDENCIES` defaults to `OFF`, so a normal build always uses the pinned versions above. Turning it `ON` checks each dependency for a newer tag and silently replaces the pinned version on disk if one exists — without updating the version CMake then reports — which is how a `kuba--/zip` update once broke the build on macOS mid-development. If that ever happens again, delete the affected directory under `third-party/` and reconfigure with `-DUPDATE_DEPENDENCIES=OFF` (the default) to re-fetch the pinned version cleanly.

## Code Signing

Every build codesigns `WordTsar.app` automatically (see `CMakeLists.txt`). With no setup, it uses an ad-hoc signature — good enough to run on your own Mac, but it gets a brand new identity every rebuild.

To give it a stable local identity instead (recommended if this is your daily build), create a free self-signed "Code Signing" certificate once:

```bash
cat > /tmp/wordtsar-codesign.cnf <<'EOF'
[req]
distinguished_name = dn
x509_extensions = v3
prompt = no
[dn]
CN = WordTsar Local Dev
[v3]
keyUsage = critical, digitalSignature
extendedKeyUsage = critical, codeSigning
basicConstraints = critical, CA:false
EOF

openssl req -x509 -newkey rsa:2048 -keyout /tmp/wordtsar-codesign.key \
  -out /tmp/wordtsar-codesign.crt -days 3650 -nodes \
  -config /tmp/wordtsar-codesign.cnf -extensions v3

openssl pkcs12 -export -out /tmp/wordtsar-codesign.p12 \
  -inkey /tmp/wordtsar-codesign.key -in /tmp/wordtsar-codesign.crt \
  -passout pass:wordtsar

security import /tmp/wordtsar-codesign.p12 -k ~/Library/Keychains/login.keychain-db \
  -P wordtsar -T /usr/bin/codesign

security add-trusted-cert -r trustAsRoot -p codeSign \
  -k ~/Library/Keychains/login.keychain-db /tmp/wordtsar-codesign.crt

rm /tmp/wordtsar-codesign.key /tmp/wordtsar-codesign.crt /tmp/wordtsar-codesign.p12 /tmp/wordtsar-codesign.cnf
```

macOS will prompt once to confirm the trust-setting change (Touch ID or password) — that's expected, since it's changing your keychain's trust store. After this, every future build (no extra flags needed) is automatically signed as "WordTsar Local Dev" instead of ad-hoc.

This is a local, free, self-signed identity — it does **not** remove the Gatekeeper "unidentified developer" warning for anyone else you share the app with. That requires a paid Apple Developer Program membership and a real Developer ID certificate, which is a separate, deliberate step if you ever need it.

## Testing

```bash
cd test/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
cd ..
QT_TESTING=1 ./build/WSTest.app/Contents/MacOS/WSTest

# Run a specific test file
QT_TESTING=1 ./build/WSTest.app/Contents/MacOS/WSTest --source-file="*test_layoutstructs.cpp"
```

**Important**: Always set `QT_TESTING=1` when running tests. Without it, tests that trigger Qt dialogs (QMessageBox, QFileDialog, etc.) will hang waiting for user input.

## Troubleshooting

### Qt6 Not Found

```bash
# Homebrew
brew install qt@6
export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6"

# Qt Installer
cmake .. -DCMAKE_PREFIX_PATH="~/Qt/6.8.2/macos"
```

### Build Errors

1. **Clean build directory**: `rm -rf build && mkdir build`
2. **Update CMake**: Ensure CMake 3.16+
3. **Check compiler**: Ensure C++20 support
4. **Dependencies**: Let CMake download fresh copies

### Performance Issues

- Use **Release** build for daily use
- **Debug** builds are significantly slower
- Consider **RelWithDebInfo** for debugging release issues

## IDE-Specific Notes

### VS Code
- Install "CMake Tools", "C/C++", and "C++ Intellisense" extensions
- Configure via CMake Tools status bar
- Use integrated terminal for build scripts

### Xcode
- Generated project includes all source files
- Use scheme editor for run configurations
- Archive for distribution builds
