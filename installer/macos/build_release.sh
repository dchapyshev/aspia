#!/bin/bash
#
# Builds the macOS artifacts for distribution out of already-built bundles:
#
#   aspia-host-<version>-universal.pkg
#   aspia-client-<version>-universal.dmg
#
# The executable of each bundle is merged from the per-architecture builds with lipo, so a single
# artifact runs on both Apple Silicon and Intel. The rest of a bundle (Info.plist, icon, localized
# strings) does not depend on the architecture and is taken from the arm64 build.
#
# Usage:
#   installer/macos/build_release.sh --arm64 <release-dir> [--x64 <release-dir>] [--output <dir>]
#
# A release dir holds the built bundles, e.g. builds/local-mac-arm64/Release. Without --x64 the
# artifacts carry the Apple Silicon slice only.
#
# Signing comes from the environment. An artifact that was not notarized is rejected by Gatekeeper
# on every Mac except the one that signed it, so a release needs all three:
#
#   APP_SIGN_IDENTITY          "Developer ID Application: ..."
#   INSTALLER_SIGN_IDENTITY    "Developer ID Installer: ..."
#   NOTARIZE_KEYCHAIN_PROFILE  notarytool keychain profile (xcrun notarytool store-credentials)
#
# codesign reaches the private key without asking only while the keychain that holds it is unlocked.
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

HOST_APP_NAME="aspia_host.app"
CLIENT_APP_NAME="Aspia Client.app"

#--------------------------------------------------------------------------------------------------
die() { echo "error: $*" >&2; exit 1; }

#--------------------------------------------------------------------------------------------------
# Arguments and environment.
#--------------------------------------------------------------------------------------------------
ARM64_DIR=""
X64_DIR=""
OUTPUT_DIR=""

while [ $# -gt 0 ]; do
    case "$1" in
        --arm64) ARM64_DIR="$2"; shift 2 ;;
        --x64) X64_DIR="$2"; shift 2 ;;
        --output) OUTPUT_DIR="$2"; shift 2 ;;
        *) die "unknown option: $1" ;;
    esac
done

[ -n "$ARM64_DIR" ] || \
    die "usage: $(basename "$0") --arm64 <release-dir> [--x64 <release-dir>] [--output <dir>]"
[ -d "$ARM64_DIR" ] || die "directory not found: $ARM64_DIR"
[ -z "$X64_DIR" ] || [ -d "$X64_DIR" ] || die "directory not found: $X64_DIR"

[ -n "${APP_SIGN_IDENTITY:-}" ] || die "APP_SIGN_IDENTITY is not set"
[ -n "${INSTALLER_SIGN_IDENTITY:-}" ] || die "INSTALLER_SIGN_IDENTITY is not set"
export APP_SIGN_IDENTITY INSTALLER_SIGN_IDENTITY
[ -z "${NOTARIZE_KEYCHAIN_PROFILE:-}" ] || export NOTARIZE_KEYCHAIN_PROFILE

[ -n "$OUTPUT_DIR" ] || OUTPUT_DIR="$(pwd)"
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"

if [ -n "$X64_DIR" ]; then
    ARCH_TAG="universal"
else
    ARCH_TAG="arm64"
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

#--------------------------------------------------------------------------------------------------
# Copies the arm64 bundle and replaces its executable with the merged one. Any other Mach-O file in
# the bundle aborts the build: it would silently ship as Apple Silicon only.
#--------------------------------------------------------------------------------------------------
universal_bundle()
{
    local name="$1"
    local staged="$WORK_DIR/$name"

    [ -d "$ARM64_DIR/$name" ] || die "bundle not found: $ARM64_DIR/$name"
    ditto "$ARM64_DIR/$name" "$staged"

    local executable
    executable="$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" \
        "$staged/Contents/Info.plist")"

    local extra
    extra="$(find "$staged" -type f -exec file {} + | grep 'Mach-O' | \
        grep -v "/Contents/MacOS/$executable:" || true)"
    [ -z "$extra" ] || die "unexpected Mach-O files in $name: $extra"

    if [ -n "$X64_DIR" ]; then
        [ -d "$X64_DIR/$name" ] || die "bundle not found: $X64_DIR/$name"
        lipo -create "$ARM64_DIR/$name/Contents/MacOS/$executable" \
                     "$X64_DIR/$name/Contents/MacOS/$executable" \
             -output "$staged/Contents/MacOS/$executable"
    fi

    echo "  $name: $(lipo -archs "$staged/Contents/MacOS/$executable")"
}

echo "Staging bundles"
universal_bundle "$HOST_APP_NAME"
universal_bundle "$CLIENT_APP_NAME"

VERSION="$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" \
    "$WORK_DIR/$HOST_APP_NAME/Contents/Info.plist")"

HOST_PKG="$OUTPUT_DIR/aspia-host-${VERSION}-${ARCH_TAG}.pkg"
CLIENT_DMG="$OUTPUT_DIR/aspia-client-${VERSION}-${ARCH_TAG}.dmg"

#--------------------------------------------------------------------------------------------------
# Package, sign and notarize.
#--------------------------------------------------------------------------------------------------
echo
"$SCRIPT_DIR/build_host_pkg.sh" "$WORK_DIR/$HOST_APP_NAME" --output "$HOST_PKG"
echo
"$SCRIPT_DIR/build_client_dmg.sh" "$WORK_DIR/$CLIENT_APP_NAME" --output "$CLIENT_DMG"

#--------------------------------------------------------------------------------------------------
# Check the artifacts the way Gatekeeper does on the machine that receives them.
#--------------------------------------------------------------------------------------------------
echo
if [ -n "${NOTARIZE_KEYCHAIN_PROFILE:-}" ]; then
    xcrun stapler validate "$HOST_PKG"
    xcrun stapler validate "$CLIENT_DMG"
    spctl --assess --type install --verbose=2 "$HOST_PKG"
    spctl --assess --type open --context context:primary-signature --verbose=2 "$CLIENT_DMG"
else
    echo "WARNING: NOTARIZE_KEYCHAIN_PROFILE was not set, the artifacts are not notarized and"
    echo "         Gatekeeper rejects them on every Mac except this one."
fi

echo
echo "Done:"
echo "  $HOST_PKG"
echo "  $CLIENT_DMG"
