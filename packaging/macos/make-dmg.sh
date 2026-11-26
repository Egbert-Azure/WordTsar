#!/bin/bash
# Copyright (C) 2026 Egbert H. Schroeer
#
# Makes WordTsar.dmg from a finished release build folder. Builds nothing.
#
#   packaging/macos/make-dmg.sh <build-folder>
#
# The image holds WordTsar.app, ws, templates/ (from the files committed in
# HEAD) and an Applications link. It is written to <build-folder>/WordTsar.dmg.
# An existing WordTsar.dmg there is removed first, and a run that fails after
# the image was made deletes it, so a failed run leaves none.
# All checks run on the staged copies before the image is made.

set -u

fail() {
    echo "make-dmg: $*" >&2
    exit 1
}

[ $# -eq 1 ] || fail "usage: $0 <release build folder>"
[ -d "$1" ] || fail "build folder not found: $1"

BUILD_DIR=$(cd "$1" && pwd -P) || fail "cannot enter $1"
REPO_DIR=$(cd "$(dirname "$0")/../.." && pwd -P) || fail "cannot find the repository"
APP_SRC="$BUILD_DIR/WordTsar.app"
WS_SRC="$BUILD_DIR/ws"
OUT_DMG="$BUILD_DIR/WordTsar.dmg"

WORK=""
MOUNTED=""
DMG_UNFINISHED=""
cleanup() {
    if [ -n "$MOUNTED" ]; then
        hdiutil detach -quiet "$MOUNTED" >/dev/null 2>&1 || hdiutil detach -quiet -force "$MOUNTED" >/dev/null 2>&1
    fi
    if [ -n "$DMG_UNFINISHED" ]; then
        rm -f "$OUT_DMG"
    fi
    if [ -n "$WORK" ] && [ -d "$WORK" ]; then
        rm -rf "$WORK"
    fi
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

rm -f "$OUT_DMG" || fail "cannot remove old $OUT_DMG"

# Returns 0 if version $1 <= version $2, compared numerically per component.
version_le() {
    local a="$1" b="$2" x y
    while [ -n "$a" ] || [ -n "$b" ]; do
        x=${a%%.*}; y=${b%%.*}
        [ -z "$x" ] && x=0
        [ -z "$y" ] && y=0
        if [ "$x" -lt "$y" ]; then return 0; fi
        if [ "$x" -gt "$y" ]; then return 1; fi
        case "$a" in *.*) a=${a#*.} ;; *) a="" ;; esac
        case "$b" in *.*) b=${b#*.} ;; *) b="" ;; esac
    done
    return 0
}

is_version() {
    echo "$1" | grep -Eq '^[0-9]+(\.[0-9]+)*$'
}

# Prints every minos (LC_BUILD_VERSION) and version (LC_VERSION_MIN_MACOSX)
# of a Mach-O file, one per line, for all architectures.
min_versions() {
    otool -l "$1" | awk '
        /cmd LC_BUILD_VERSION$/      { want = "minos" }
        /cmd LC_VERSION_MIN_MACOSX$/ { want = "version" }
        want != "" && $1 == want     { print $2; want = "" }
        /^Load command/              { want = "" }'
}

# Prints every LC_RPATH path of a Mach-O file, one per line.
rpaths() {
    otool -l "$1" | awk '
        /cmd LC_RPATH$/          { want = 1; next }
        /^Load command/          { want = 0 }
        want && $1 == "path"     { sub(/^[ \t]*path /, ""); sub(/ \(offset [0-9]+\)$/, ""); print; want = 0 }'
}

# Prints every library path otool -L lists for a Mach-O file, one per line.
linked_paths() {
    otool -L "$1" | grep "$(printf '\t')" | sed -e 's/^[[:space:]]*//' -e 's/ (compatibility version .*)$//'
}

is_macho() {
    case "$(file -b "$1")" in
        *Mach-O*) return 0 ;;
        *) return 1 ;;
    esac
}

# --- version.txt -----------------------------------------------------------
VERSION_FILE="$REPO_DIR/version.txt"
[ -f "$VERSION_FILE" ] || fail "missing $VERSION_FILE"
MAJOR=$(sed -n 's/^MAJOR=\([0-9][0-9]*\).*/\1/p' "$VERSION_FILE")
MINOR=$(sed -n 's/^MINOR=\([0-9][0-9]*\).*/\1/p' "$VERSION_FILE")
BUILDNO=$(sed -n 's/^BUILD=\([0-9][0-9]*\).*/\1/p' "$VERSION_FILE")
[ -n "$MAJOR" ] && [ -n "$MINOR" ] && [ -n "$BUILDNO" ] || fail "cannot read MAJOR, MINOR and BUILD from $VERSION_FILE"
VERSION="$MAJOR.$MINOR.$BUILDNO"

# --- inputs ----------------------------------------------------------------
[ -d "$APP_SRC" ] || fail "missing $APP_SRC"
[ -f "$WS_SRC" ] || fail "missing $WS_SRC (configure with -DBUILD_TUI=ON and build ws)"
git -C "$REPO_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1 || fail "$REPO_DIR is not a git work tree"
git -C "$REPO_DIR" diff --quiet HEAD -- templates || fail "templates/ has uncommitted changes; commit them first"
for t in templates/BOOK.WS templates/README.md templates/macros; do
    [ -n "$(git -C "$REPO_DIR" ls-tree HEAD "$t")" ] || fail "$t is not committed in HEAD"
done

# --- staging ---------------------------------------------------------------
WORK=$(mktemp -d "${TMPDIR:-/tmp}/make-dmg.XXXXXX") || fail "mktemp failed"
STAGE="$WORK/WordTsar"
mkdir "$STAGE" || fail "cannot create $STAGE"

ditto "$APP_SRC" "$STAGE/WordTsar.app" || fail "cannot copy WordTsar.app"
ditto "$WS_SRC" "$STAGE/ws" || fail "cannot copy ws"
git -C "$REPO_DIR" archive --format=tar HEAD templates | tar -x -C "$STAGE" -f - \
    || fail "cannot export templates from HEAD"
ln -s /Applications "$STAGE/Applications" || fail "cannot create Applications link"

APP="$STAGE/WordTsar.app"
WS="$STAGE/ws"
PLIST="$APP/Contents/Info.plist"

# --- checks ----------------------------------------------------------------
[ -f "$PLIST" ] || fail "missing Info.plist in WordTsar.app"
APP_VERSION=$(plutil -extract CFBundleShortVersionString raw -o - "$PLIST" 2>/dev/null)
[ "$APP_VERSION" = "$VERSION" ] || fail "WordTsar.app CFBundleShortVersionString is '$APP_VERSION', version.txt says $VERSION"

is_macho "$WS" || fail "ws is not a Mach-O file"
[ -x "$WS" ] || fail "ws is not executable"

STRAY=$(find "$STAGE" -name '.DS_Store' -o -name '._*')
[ -z "$STRAY" ] || fail "stray files in the image: $STRAY"

BAD_LINKS=""
BAD_RPATHS=""
MISSING_FW=""
MACHO_COUNT=0
while IFS= read -r -d '' f; do
    is_macho "$f" || continue
    MACHO_COUNT=$((MACHO_COUNT + 1))
    # Dependency lines start with a tab; header lines name the file itself.
    hits=$(otool -L "$f" | grep "$(printf '\t')" | grep -E "/opt/homebrew|$HOME/Qt/")
    if [ -n "$hits" ]; then
        BAD_LINKS="$BAD_LINKS${f#$STAGE/}:
$hits
"
    fi
    while IFS= read -r r; do
        case "$r" in
            ""|@executable_path|@executable_path/*|@loader_path|@loader_path/*) ;;
            *) BAD_RPATHS="$BAD_RPATHS${f#$STAGE/}: $r
" ;;
        esac
    done <<< "$(rpaths "$f")"
    case "$f" in
        "$APP"/*)
            while IFS= read -r dep; do
                case "$dep" in
                    *Qt*.framework/*) ;;
                    *) continue ;;
                esac
                fw=$(echo "$dep" | sed 's#^\(.*/\)\{0,1\}\(Qt[^/]*\.framework\)/.*#\2#')
                case "$fw" in
                    Qt*.framework) ;;
                    *) continue ;;
                esac
                if [ ! -d "$APP/Contents/Frameworks/$fw" ]; then
                    case "$MISSING_FW" in
                        *"$fw
"*) ;;
                        *) MISSING_FW="$MISSING_FW$fw
" ;;
                    esac
                fi
            done <<< "$(linked_paths "$f")"
            ;;
    esac
done < <(find "$APP" "$WS" -type f -print0)
[ "$MACHO_COUNT" -ge 2 ] || fail "no Mach-O files found in WordTsar.app"
[ -z "$BAD_LINKS" ] || fail "links to /opt/homebrew or $HOME/Qt:
$BAD_LINKS"
[ -z "$BAD_RPATHS" ] || fail "LC_RPATH outside @executable_path or @loader_path:
$BAD_RPATHS"
[ -z "$MISSING_FW" ] || fail "Qt frameworks missing from WordTsar.app/Contents/Frameworks:
$MISSING_FW"

codesign --verify --deep --strict "$APP" || fail "codesign --verify failed for WordTsar.app"
codesign --verify --deep --strict "$WS" || fail "codesign --verify failed for ws"

MIN_SYS=$(plutil -extract LSMinimumSystemVersion raw -o - "$PLIST" 2>/dev/null)
[ -n "$MIN_SYS" ] || fail "WordTsar.app has no LSMinimumSystemVersion"
is_version "$MIN_SYS" || fail "LSMinimumSystemVersion '$MIN_SYS' is not a version"
APP_EXE_NAME=$(plutil -extract CFBundleExecutable raw -o - "$PLIST" 2>/dev/null)
[ -n "$APP_EXE_NAME" ] || fail "WordTsar.app has no CFBundleExecutable"
APP_EXE="$APP/Contents/MacOS/$APP_EXE_NAME"
[ -f "$APP_EXE" ] || fail "missing $APP_EXE_NAME in WordTsar.app"
for bin in "$APP_EXE" "$WS"; do
    name=${bin#$STAGE/}
    mins=$(min_versions "$bin")
    [ -n "$mins" ] || fail "$name has no minimum macOS version"
    for m in $mins; do
        is_version "$m" || fail "$name has minimum macOS version '$m'"
        version_le "$m" "$MIN_SYS" || fail "$name needs macOS $m, above LSMinimumSystemVersion $MIN_SYS"
    done
done

# --- image -----------------------------------------------------------------
TMP_DMG="$WORK/WordTsar.dmg"
hdiutil create -quiet -volname "WordTsar" -srcfolder "$STAGE" -format UDZO "$TMP_DMG" \
    || fail "hdiutil create failed"
DMG_UNFINISHED=1
mv "$TMP_DMG" "$OUT_DMG" || fail "cannot move image to $OUT_DMG"

MNT="$WORK/mnt"
mkdir "$MNT" || fail "cannot create $MNT"
hdiutil attach -quiet -readonly -nobrowse -noautoopen -mountpoint "$MNT" "$OUT_DMG" \
    || fail "cannot attach $OUT_DMG to list it"
MOUNTED="$MNT"
echo "$OUT_DMG (WordTsar $VERSION):"
ls -la "$MNT" || fail "cannot list $MNT"
echo "templates:"
(set -o pipefail; cd "$MNT" && find templates | sort) || fail "cannot list templates in $MNT"
hdiutil detach -quiet "$MNT" || fail "cannot detach $MNT"
MOUNTED=""
DMG_UNFINISHED=""
