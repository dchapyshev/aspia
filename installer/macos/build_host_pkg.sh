#!/bin/bash
#
# Builds the macOS installer package (.pkg) for the Aspia Host out of an already-built
# aspia_host.app bundle (see the APPLE section of source/host/CMakeLists.txt).
#
# The installer drops the bundle into /Applications/Aspia Host.app and registers the launchd daemon
# from its postinstall script (aspia_host_service --install, reusing ServiceControllerLaunchd).
#
# Usage:
#   installer/macos/build_host_pkg.sh <path-to-aspia_host.app> [--output <file.pkg>]
#
# Code signing and notarization are optional and driven by environment variables; when they are
# unset the script produces a working unsigned package (for local builds and CI without secrets):
#
#   APP_SIGN_IDENTITY        "Developer ID Application: ..."  - signs the bundle (inside-out)
#   INSTALLER_SIGN_IDENTITY  "Developer ID Installer: ..."    - signs the .pkg
#   NOTARIZE_KEYCHAIN_PROFILE  notarytool keychain profile    - notarizes and staples the .pkg
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BUNDLE_ID="org.aspia.host"
APP_NAME="Aspia Host.app"

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

[ -n "$APP_PATH" ] || die "usage: $(basename "$0") <path-to-aspia_host.app> [--output <file.pkg>]"
[ -d "$APP_PATH" ] || die "bundle not found: $APP_PATH"
[ -x "$APP_PATH/Contents/MacOS/aspia_host_service" ] || die "not a host bundle: $APP_PATH"

# Version comes from the bundle so it always matches what was built.
VERSION="$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" \
    "$APP_PATH/Contents/Info.plist" 2>/dev/null || echo "0.0.0")"

[ -n "$OUTPUT" ] || OUTPUT="$(pwd)/aspia-host-${VERSION}.pkg"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

STAGING="$WORK_DIR/root"
COMPONENT_PKG="$WORK_DIR/component.pkg"

echo "Building Aspia Host installer ${VERSION}"
echo "  bundle: $APP_PATH"
echo "  output: $OUTPUT"

#--------------------------------------------------------------------------------------------------
# Stage the payload: /Applications/Aspia Host.app (renamed from the build-tree aspia_host.app; the
# executables inside keep their names, so applicationDirPath() lookups are unaffected).
#--------------------------------------------------------------------------------------------------
mkdir -p "$STAGING/Applications"
ditto "$APP_PATH" "$STAGING/Applications/$APP_NAME"

# Drop extended attributes so the payload is not littered with AppleDouble ._ files. Signing (below)
# re-establishes everything it needs; the ad-hoc/Developer ID signature lives in the Mach-O.
xattr -rc "$STAGING" 2>/dev/null || true
find "$STAGING" -name '._*' -delete 2>/dev/null || true

#--------------------------------------------------------------------------------------------------
# Optional: sign the bundle inside-out (nested code first, the .app last).
#--------------------------------------------------------------------------------------------------
if [ -n "${APP_SIGN_IDENTITY:-}" ]; then
    echo "Signing bundle with: $APP_SIGN_IDENTITY"
    APP="$STAGING/Applications/$APP_NAME"
    CS_OPTS=(--force --options runtime --timestamp --sign "$APP_SIGN_IDENTITY")

    ENTITLEMENTS_ARG=()
    if [ -f "$SCRIPT_DIR/host.entitlements" ]; then
        ENTITLEMENTS_ARG=(--entitlements "$SCRIPT_DIR/host.entitlements")
    fi

    # Shared library first, then every executable, then the bundle itself.
    codesign "${CS_OPTS[@]}" "$APP/Contents/MacOS/libaspia_host_core.dylib"
    for bin in aspia_host_service aspia_desktop_agent aspia_file_agent aspia_terminal_agent aspia_host; do
        codesign "${CS_OPTS[@]}" "${ENTITLEMENTS_ARG[@]}" "$APP/Contents/MacOS/$bin"
    done
    codesign "${CS_OPTS[@]}" "${ENTITLEMENTS_ARG[@]}" "$APP"

    codesign --verify --deep --strict --verbose=2 "$APP"
else
    echo "APP_SIGN_IDENTITY not set - building an unsigned bundle"
fi

#--------------------------------------------------------------------------------------------------
# Build the component package (payload installs at /, postinstall registers the daemon).
#--------------------------------------------------------------------------------------------------
# Disable bundle relocation. By default the installer redirects the payload to any existing bundle
# with the same CFBundleIdentifier found elsewhere on disk (e.g. the build-tree aspia_host.app);
# the payload then lands outside /Applications and the postinstall script - which hardcodes
# /Applications/Aspia Host.app - fails. Pin the bundle to its fixed install location instead.
COMPONENT_PLIST="$WORK_DIR/component.plist"
pkgbuild --analyze --root "$STAGING" "$COMPONENT_PLIST"
plutil -replace 0.BundleIsRelocatable -bool NO "$COMPONENT_PLIST"

pkgbuild \
    --root "$STAGING" \
    --component-plist "$COMPONENT_PLIST" \
    --identifier "$BUNDLE_ID" \
    --version "$VERSION" \
    --scripts "$SCRIPT_DIR/scripts" \
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
