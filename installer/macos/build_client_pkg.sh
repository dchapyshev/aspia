#!/bin/bash
#
# Builds the macOS installer package (.pkg) for the Aspia Client out of an already-built
# aspia_client.app bundle (see the APPLE section of source/client/CMakeLists.txt).
#
# The installer drops the bundle into /Applications/Aspia Client.app. The name matters: the system
# shows the localized name of an application only while the bundle on disk is still called what the
# bundle says it is called, and a bundle named after the build target is shown as aspia_client.
# Unlike the host package there is nothing to register afterwards: the client is a plain
# application. The one script it carries ends the client of the version being replaced, before its
# files are overwritten.
#
# Usage:
#   installer/macos/build_client_pkg.sh <path-to-aspia_client.app> [--output <file.pkg>]
#
# Code signing and notarization are optional and driven by environment variables; when they are
# unset the script produces a working unsigned package (for local builds and CI without secrets):
#
#   APP_SIGN_IDENTITY        "Developer ID Application: ..."  - signs the bundle
#   INSTALLER_SIGN_IDENTITY  "Developer ID Installer: ..."    - signs the .pkg
#   NOTARIZE_KEYCHAIN_PROFILE  notarytool keychain profile    - notarizes and staples the .pkg
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BUNDLE_ID="org.aspia.client"
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

[ -n "$APP_PATH" ] || die "usage: $(basename "$0") <path-to-aspia_client.app> [--output <file.pkg>]"
[ -d "$APP_PATH" ] || die "bundle not found: $APP_PATH"

EXECUTABLE="$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" \
    "$APP_PATH/Contents/Info.plist" 2>/dev/null || true)"
[ -n "$EXECUTABLE" ] && [ -x "$APP_PATH/Contents/MacOS/$EXECUTABLE" ] \
    || die "not a client bundle: $APP_PATH"

# Version comes from the bundle so it always matches what was built.
VERSION="$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" \
    "$APP_PATH/Contents/Info.plist" 2>/dev/null || echo "0.0.0")"

[ -n "$OUTPUT" ] || OUTPUT="$(pwd)/aspia-client-${VERSION}.pkg"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

STAGING="$WORK_DIR/root"
COMPONENT_PKG="$WORK_DIR/component.pkg"

echo "Building Aspia Client installer ${VERSION}"
echo "  bundle: $APP_PATH"
echo "  output: $OUTPUT"

#--------------------------------------------------------------------------------------------------
# Stage the payload: /Applications/Aspia Client.app.
#--------------------------------------------------------------------------------------------------
mkdir -p "$STAGING/Applications"
ditto "$APP_PATH" "$STAGING/Applications/$APP_NAME"

# Drop extended attributes so the payload is not littered with AppleDouble ._ files. Signing (below)
# re-establishes everything it needs; the Developer ID signature lives in the Mach-O.
xattr -rc "$STAGING" 2>/dev/null || true
find "$STAGING" -name '._*' -delete 2>/dev/null || true

#--------------------------------------------------------------------------------------------------
# Optional: sign the bundle.
#--------------------------------------------------------------------------------------------------
if [ -n "${APP_SIGN_IDENTITY:-}" ]; then
    echo "Signing bundle with: $APP_SIGN_IDENTITY"

    # The bundle holds a single self-contained executable (aspia_client_core and Qt are linked in
    # statically), so signing the bundle signs that executable and seals the resources. The hardened
    # runtime is a notarization requirement.
    codesign --force --options runtime --timestamp --sign "$APP_SIGN_IDENTITY" \
        "$STAGING/Applications/$APP_NAME"
    codesign --verify --deep --strict --verbose=2 "$STAGING/Applications/$APP_NAME"
else
    echo "APP_SIGN_IDENTITY not set - building an unsigned bundle"
fi

#--------------------------------------------------------------------------------------------------
# Build the component package (payload installs at /).
#--------------------------------------------------------------------------------------------------
# Disable bundle relocation. By default the installer redirects the payload to any existing bundle
# with the same CFBundleIdentifier found elsewhere on disk (e.g. the build-tree bundle), and the
# update then lands outside /Applications. Pin the bundle to its fixed install location instead.
COMPONENT_PLIST="$WORK_DIR/component.plist"
pkgbuild --analyze --root "$STAGING" "$COMPONENT_PLIST"
plutil -replace 0.BundleIsRelocatable -bool NO "$COMPONENT_PLIST"

pkgbuild \
    --root "$STAGING" \
    --component-plist "$COMPONENT_PLIST" \
    --identifier "$BUNDLE_ID" \
    --version "$VERSION" \
    --scripts "$SCRIPT_DIR/client_scripts" \
    --install-location / \
    "$COMPONENT_PKG"

#--------------------------------------------------------------------------------------------------
# Wrap into the distribution product archive (the artifact shipped to users).
#--------------------------------------------------------------------------------------------------
PRODUCTBUILD_ARGS=(--package "$COMPONENT_PKG")
if [ -n "${INSTALLER_SIGN_IDENTITY:-}" ]; then
    echo "Signing installer with: $INSTALLER_SIGN_IDENTITY"
    PRODUCTBUILD_ARGS+=(--sign "$INSTALLER_SIGN_IDENTITY" --timestamp)
else
    echo "INSTALLER_SIGN_IDENTITY not set - building an unsigned installer"
fi
productbuild "${PRODUCTBUILD_ARGS[@]}" "$OUTPUT"

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
