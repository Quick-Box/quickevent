#!/bin/bash
# Fix app bundle linking after macdeployqt:
# - Repoints Qt framework paths in all bundle binaries
# - Copies Homebrew libraries into Frameworks/ and fixes their paths (recursively)
# Usage: ./tools/macos-bundle-fix.sh [bundle-path]

set -euo pipefail

BUNDLE="${1:-install/quickevent.app}"

if [[ ! -d "$BUNDLE" ]]; then
    echo "ERROR: bundle not found: $BUNDLE"
    exit 1
fi

FRAMEWORKS="$BUNDLE/Contents/Frameworks"
mkdir -p "$FRAMEWORKS"

fix_binary() {
    local binary="$1"
    local binname; binname=$(basename "$binary")

    # otool -L is the macOS equivalent of ldd; tail -n +2 skips the header line.
    otool -L "$binary" 2>/dev/null | tail -n +2 | awk '{print $1}' | sort -u | while read -r dep; do

        # Unlike ldd, otool -L lists the library's own install name (SONAME
        # equivalent) as the first entry — skip it to avoid self-bundling.
        [[ "$(basename "$dep")" == "$binname" ]] && continue

        case "$dep" in
            @*|/usr/lib/*|/System/*)
                # @executable_path/… etc. are bundle-relative dyld tokens — already correct.
                # /usr/lib and /System are macOS system libs present on every machine.
                ;;
            */Qt*.framework/*/Qt*)
                # Qt uses macOS framework bundles: Name.framework/Versions/A/Name.
                # Rewrite leftover absolute Qt install paths to bundle-relative ones.
                local fw; fw=$(basename "$dep")
                install_name_tool -change "$dep" \
                    "@executable_path/../Frameworks/$fw.framework/Versions/A/$fw" \
                    "$binary"
                ;;
            /*)
                # Third-party absolute path (Homebrew, Postgres.app, …) —
                # macdeployqt ignores these, so we copy them into the bundle.
                local lib; lib=$(basename "$dep")
                local src="$dep"

                # Build-time path may not exist here (e.g. Postgres.app on CI);
                # fall back to searching Homebrew by filename.
                if [[ ! -f "$src" ]]; then
                    src=$(find /opt/homebrew /usr/local \( -type f -o -type l \) -name "$lib" 2>/dev/null | head -1)
                    if [[ -z "$src" ]]; then
                        echo "  WARNING: $lib not found, skipping"
                        continue
                    fi
                fi

                if [[ ! -f "$FRAMEWORKS/$lib" ]]; then
                    echo "  Bundling $lib"
                    cp "$src" "$FRAMEWORKS/$lib"
                    chmod 755 "$FRAMEWORKS/$lib"
                    install_name_tool -id "@executable_path/../Frameworks/$lib" \
                        "$FRAMEWORKS/$lib"       # -id sets own install name (SONAME)
                    fix_binary "$FRAMEWORKS/$lib" # recurse for transitive deps
                fi

                install_name_tool -change "$dep" \
                    "@executable_path/../Frameworks/$lib" "$binary"
                ;;
        esac
    done
}

echo "Fixing bundle: $BUNDLE"
# macdeployqt only fixes the main executable; process all binaries in MacOS/.
find "$BUNDLE/Contents/MacOS" -maxdepth 1 -type f | while read -r f; do fix_binary "$f"; done
# Process all dylibs: plugins, frameworks, QML modules.
find "$BUNDLE/Contents" -name "*.dylib" | while read -r f; do fix_binary "$f"; done
echo "Done"
