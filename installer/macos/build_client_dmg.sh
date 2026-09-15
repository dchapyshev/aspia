#!/bin/bash
#
# Builds the macOS disk image (.dmg) for the Aspia Client out of an already-built
# "Aspia Client.app" bundle (see the APPLE section of source/client/CMakeLists.txt).
#
# The image holds the bundle next to a shortcut to /Applications, so it is installed by dragging the
# app over.
#
# Usage:
#   installer/macos/build_client_dmg.sh <path-to-Aspia Client.app> [--output <file.dmg>]
#
# Code signing and notarization are optional and driven by environment variables; when they are
# unset the script produces a working unsigned image (for local builds and CI without secrets):
#
#   APP_SIGN_IDENTITY          "Developer ID Application: ..."  - signs the bundle and the image
#   NOTARIZE_KEYCHAIN_PROFILE  notarytool keychain profile      - notarizes and staples the image
#

set -euo pipefail

VOLUME_NAME="Aspia Client"
APP_NAME="Aspia Client.app"

#--------------------------------------------------------------------------------------------------
die() { echo "error: $*" >&2; exit 1; }

#--------------------------------------------------------------------------------------------------
# Arguments.
#--------------------------------------------------------------------------------------------------
APP_PATH=""
OUTPUT=""

while [ $# -gt 0 ]; do
    case "$1" in
        --output) OUTPUT="$2"; shift 2 ;;
        -*) die "unknown option: $1" ;;
        *) APP_PATH="$1"; shift ;;
    esac
done

[ -n "$APP_PATH" ] || die "usage: $(basename "$0") <path-to-Aspia Client.app> [--output <file.dmg>]"
[ -d "$APP_PATH" ] || die "bundle not found: $APP_PATH"

EXECUTABLE="$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" \
    "$APP_PATH/Contents/Info.plist" 2>/dev/null || true)"
[ -n "$EXECUTABLE" ] && [ -x "$APP_PATH/Contents/MacOS/$EXECUTABLE" ] \
    || die "not a client bundle: $APP_PATH"

# Version comes from the bundle so it always matches what was built.
VERSION="$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" \
    "$APP_PATH/Contents/Info.plist" 2>/dev/null || echo "0.0.0")"

[ -n "$OUTPUT" ] || OUTPUT="$(pwd)/aspia-client-${VERSION}.dmg"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

STAGING="$WORK_DIR/image"

echo "Building Aspia Client disk image ${VERSION}"
echo "  bundle: $APP_PATH"
echo "  output: $OUTPUT"

#--------------------------------------------------------------------------------------------------
# Stage the image contents.
#--------------------------------------------------------------------------------------------------
mkdir -p "$STAGING"
ditto "$APP_PATH" "$STAGING/$APP_NAME"
ln -s /Applications "$STAGING/Applications"

# Drop extended attributes so the payload is not littered with AppleDouble ._ files. Signing (below)
# re-establishes everything it needs; the Developer ID signature lives in the Mach-O.
xattr -rc "$STAGING/$APP_NAME" 2>/dev/null || true
find "$STAGING" -name '._*' -delete 2>/dev/null || true

#--------------------------------------------------------------------------------------------------
# Optional: sign the bundle. The image itself is signed after it is created.
#--------------------------------------------------------------------------------------------------
if [ -n "${APP_SIGN_IDENTITY:-}" ]; then
    echo "Signing bundle with: $APP_SIGN_IDENTITY"

    # The bundle holds a single self-contained executable (aspia_client_core and Qt are linked in
    # statically), so signing the bundle signs that executable and seals the resources. The hardened
    # runtime is a notarization requirement.
    codesign --force --options runtime --timestamp --sign "$APP_SIGN_IDENTITY" "$STAGING/$APP_NAME"
    codesign --verify --deep --strict --verbose=2 "$STAGING/$APP_NAME"
else
    echo "APP_SIGN_IDENTITY not set - building an unsigned bundle"
fi

#--------------------------------------------------------------------------------------------------
# Build the image.
#--------------------------------------------------------------------------------------------------
rm -f "$OUTPUT"
hdiutil create -volname "$VOLUME_NAME" -srcfolder "$STAGING" -fs HFS+ -format UDZO "$OUTPUT"

if [ -n "${APP_SIGN_IDENTITY:-}" ]; then
    echo "Signing image with: $APP_SIGN_IDENTITY"
    codesign --force --timestamp --sign "$APP_SIGN_IDENTITY" "$OUTPUT"
    codesign --verify --strict --verbose=2 "$OUTPUT"
fi

#--------------------------------------------------------------------------------------------------
# Optional: notarize and staple.
#--------------------------------------------------------------------------------------------------
if [ -n "${NOTARIZE_KEYCHAIN_PROFILE:-}" ]; then
    echo "Notarizing with profile: $NOTARIZE_KEYCHAIN_PROFILE"
    xcrun notarytool submit "$OUTPUT" --keychain-profile "$NOTARIZE_KEYCHAIN_PROFILE" --wait
    xcrun stapler staple "$OUTPUT"
else
    echo "NOTARIZE_KEYCHAIN_PROFILE not set - skipping notarization"
fi

echo "Done: $OUTPUT"
