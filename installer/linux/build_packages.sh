#!/bin/bash
#
# Builds the Debian (.deb) and RPM (.rpm) packages of the Aspia host, client, router and relay out
# of already-built binaries.
#
# Usage:
#   installer/linux/build_packages.sh --build-dir <dir> [--output-dir <dir>]
#                                     [--component host|client|router|relay]...
#
#                                     [--format deb|rpm]...
#
# --build-dir holds the built binaries (builds/local-linux-x64/Release). The packages are written to
# the current directory unless --output-dir says otherwise, and every component in both formats is
# built unless --component and --format name what is wanted.
#
# The machine needs dpkg-deb for a Debian package and rpmbuild for an RPM. Both are built for the
# architecture of the machine, which is the architecture the binaries were built for.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

VENDOR="Dmitry Chapyshev"
MAINTAINER="Dmitry Chapyshev <dmitry@aspia.ru>"
HOMEPAGE="https://aspia.org/"
LICENSE="GPLv3"

#--------------------------------------------------------------------------------------------------
die() { echo "error: $*" >&2; exit 1; }

#--------------------------------------------------------------------------------------------------
# Arguments.
#--------------------------------------------------------------------------------------------------
BUILD_DIR=""
OUTPUT_DIR="$(pwd)"
COMPONENTS=()
FORMATS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --component) COMPONENTS+=("$2"); shift 2 ;;
        --format) FORMATS+=("$2"); shift 2 ;;
        *) die "unknown option: $1" ;;
    esac
done

[ -n "$BUILD_DIR" ] || die "usage: $(basename "$0") --build-dir <dir> [--output-dir <dir>]"
[ -d "$BUILD_DIR" ] || die "build directory not found: $BUILD_DIR"
[ ${#COMPONENTS[@]} -gt 0 ] || COMPONENTS=(host client router relay)
[ ${#FORMATS[@]} -gt 0 ] || FORMATS=(deb rpm)

BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"

for format in "${FORMATS[@]}"; do
    case "$format" in
        deb)
            command -v dpkg-deb >/dev/null || die "dpkg-deb not found"

            # The dependencies are read out of the binary, and a package without them is built
            # without a word of complaint.
            command -v readelf >/dev/null || die "readelf not found"
            command -v objdump >/dev/null || die "objdump not found"
            ;;
        rpm) command -v rpmbuild >/dev/null || die "rpmbuild not found" ;;
        *) die "unknown format: $format" ;;
    esac
done

#--------------------------------------------------------------------------------------------------
# Version and architecture.
#--------------------------------------------------------------------------------------------------
version_part() { sed -n "s/^set(ASPIA_VERSION_$1 \([0-9][0-9]*\))/\1/p" "$SOURCE_ROOT/CMakeLists.txt"; }

VERSION="$(version_part MAJOR).$(version_part MINOR).$(version_part PATCH)"
[ "$VERSION" != ".." ] || die "no version in $SOURCE_ROOT/CMakeLists.txt"

MACHINE="$(uname -m)"
case "$MACHINE" in
    x86_64)  DEB_ARCH="amd64" ;;
    aarch64) DEB_ARCH="arm64" ;;
    armv7l)  DEB_ARCH="armhf" ;;
    *) die "unknown architecture: $MACHINE" ;;
esac

#--------------------------------------------------------------------------------------------------
# Description of a component.
#--------------------------------------------------------------------------------------------------
summary_of()
{
    case "$1" in
        host)   echo "Remote desktop software." ;;
        client) echo "Client for managing hosts." ;;
        router) echo "Provides a connection routing service." ;;
        relay)  echo "Provides a service for transferring data between peers." ;;
    esac
}

#--------------------------------------------------------------------------------------------------
# Directories the package owns. Everything else it installs into belongs to systemd, polkit, pam or
# the icon theme, and two packages owning one directory is a conflict on install.
#--------------------------------------------------------------------------------------------------
owned_dirs_of()
{
    case "$1" in
        host) echo "/usr/share/aspia" ;;
        *)    echo "" ;;
    esac
}

#--------------------------------------------------------------------------------------------------
# Contents of a package.
#--------------------------------------------------------------------------------------------------
stage_payload()
{
    local component="$1" root="$2"

    install -D -m 755 "$BUILD_DIR/aspia_$component" "$root/usr/bin/aspia_$component"

    case "$component" in
        host)
            install -D -m 644 "$SCRIPT_DIR/host/aspia-host.desktop" \
                "$root/usr/share/applications/aspia-host.desktop"

            # KWin authorizes org.kde.KWin.ScreenShot2 by the executable path of the caller, which it
            # matches to a .desktop declaring X-KDE-DBUS-Restricted-Interfaces.
            install -D -m 644 "$SCRIPT_DIR/host/aspia-desktop-agent.desktop" \
                "$root/usr/share/applications/aspia-desktop-agent.desktop"

            install -D -m 644 "$SOURCE_ROOT/source/common/resources/aspia.svg" \
                "$root/usr/share/icons/hicolor/scalable/apps/aspia-host.svg"
            install -D -m 644 "$SCRIPT_DIR/host/aspia-host-service.service" \
                "$root/usr/lib/systemd/system/aspia-host-service.service"
            install -D -m 644 "$SCRIPT_DIR/host/org.aspia.host.policy" \
                "$root/usr/share/polkit-1/actions/org.aspia.host.policy"

            # The aggregate PAM stack is named differently across families, so both variants are
            # shipped and the maintainer script installs the one its distribution reads. This picks
            # the policy per package instead of by the distribution of the build machine.
            install -D -m 644 "$SCRIPT_DIR/host/aspia-terminal.pam" \
                "$root/usr/share/aspia/aspia-terminal.pam"
            install -D -m 644 "$SCRIPT_DIR/host/aspia-terminal-rhel.pam" \
                "$root/usr/share/aspia/aspia-terminal-rhel.pam"
            ;;
        client)
            install -D -m 644 "$SCRIPT_DIR/client/aspia-client.desktop" \
                "$root/usr/share/applications/aspia-client.desktop"
            install -D -m 644 "$SOURCE_ROOT/source/common/resources/aspia.svg" \
                "$root/usr/share/icons/hicolor/scalable/apps/aspia-client.svg"
            install -D -m 644 "$SCRIPT_DIR/client/org.aspia.client.policy" \
                "$root/usr/share/polkit-1/actions/org.aspia.client.policy"
            ;;
    esac
}

#--------------------------------------------------------------------------------------------------
# Dependencies of a Debian package, named after the SONAMEs the binary asks for. What ships inside
# libc6 is left out, that package is named once. An unknown SONAME stops the build, so a new
# dependency cannot quietly stay out of the control file.
#--------------------------------------------------------------------------------------------------
deb_depends()
{
    local binary="$1" glibc depends="" soname package

    # The oldest glibc that still has every versioned symbol the binary refers to.
    glibc="$(objdump -T "$binary" | sed -n 's/.*GLIBC_\([0-9][0-9.]*\).*/\1/p' | sort -V -u | tail -1)"
    [ -n "$glibc" ] || die "no versioned glibc symbol in $binary"

    for soname in $(readelf -d "$binary" | sed -n 's/.*NEEDED.*\[\(.*\)\].*/\1/p'); do
        case "$soname" in
            libc.so.6)      package="libc6 (>= $glibc)" ;;
            libstdc++.so.6) package="libstdc++6" ;;
            libgcc_s.so.1)  package="libgcc-s1 | libgcc1" ;;
            libdbus-1.so.3) package="libdbus-1-3" ;;
            libpam.so.0)    package="libpam0g" ;;
            libm.so.6|librt.so.1|libpthread.so.0|libdl.so.2|libutil.so.1|ld-linux-*) continue ;;
            *) die "no Debian package known for $soname, add it to deb_depends" ;;
        esac

        case ", $depends," in *", $package,"*) continue ;; esac
        depends="${depends:+$depends, }$package"
    done

    [ -n "$depends" ] || die "no dependencies read out of $binary"

    echo "$depends"
}

#--------------------------------------------------------------------------------------------------
build_deb()
{
    local component="$1"
    local name="aspia-$component"
    local root="$WORK_DIR/deb/$component"
    local output="$OUTPUT_DIR/$name-$VERSION-$MACHINE.deb"
    local script

    stage_payload "$component" "$root"
    mkdir -p "$root/DEBIAN"

    for script in preinst postinst prerm postrm; do
        [ -f "$SCRIPT_DIR/$component/$script" ] || continue
        install -m 755 "$SCRIPT_DIR/$component/$script" "$root/DEBIAN/$script"
    done

    # dpkg checks the installed files against this list and tells whose package replaced a file.
    (cd "$root" && find usr -type f -exec md5sum {} + | sort -k 2 > DEBIAN/md5sums)

    cat > "$root/DEBIAN/control" <<CONTROL
Package: $name
Version: $VERSION
Architecture: $DEB_ARCH
Maintainer: $MAINTAINER
Homepage: $HOMEPAGE
Section: net
Priority: optional
Installed-Size: $(du -k -s "$root/usr" | cut -f1)
Depends: $(deb_depends "$root/usr/bin/aspia_$component")
Description: $(summary_of "$component")
CONTROL

    dpkg-deb --build --root-owner-group "$root" "$output" > /dev/null
    echo "  $output"
}

#--------------------------------------------------------------------------------------------------
# A scriptlet of an RPM, taken from the file the component keeps it in.
#--------------------------------------------------------------------------------------------------
append_scriptlet()
{
    local component="$1" file="$2" section="$3" path="$SCRIPT_DIR/$1/$2"

    [ -f "$path" ] || return 0

    # The scriptlets are plain shell, so a percent sign in one is a percent sign and not the macro
    # rpm would otherwise read it as.
    echo "$section"
    sed "s/%/%%/g" "$path"
    echo
}

#--------------------------------------------------------------------------------------------------
build_rpm()
{
    local component="$1"
    local name="aspia-$component"
    local root="$WORK_DIR/rpm/$component"
    local spec="$WORK_DIR/$name.spec"
    local top="$WORK_DIR/rpmbuild"
    local log="$WORK_DIR/$name-rpmbuild.log"
    local output="$OUTPUT_DIR/$name-$VERSION-$MACHINE.rpm"
    local dir

    stage_payload "$component" "$root"

    {
        echo "Name:      $name"
        echo "Version:   $VERSION"
        echo "Release:   1"
        echo "Summary:   $(summary_of "$component")"
        echo "License:   $LICENSE"
        echo "URL:       $HOMEPAGE"
        echo "Vendor:    $VENDOR"
        echo "Packager:  $MAINTAINER"
        echo "BuildArch: $MACHINE"
        echo

        # Hand over the binary as it was built. The install step of rpm otherwise strips it and adds
        # build-id links of its own.
        echo "%define __os_install_post %{nil}"
        echo "%define _build_id_links none"
        echo

        echo "%description"
        summary_of "$component"
        echo

        echo "%install"
        echo "mkdir -p %{buildroot}"
        echo "cp -a \"$root/.\" %{buildroot}/"
        echo

        echo "%files"
        for dir in $(owned_dirs_of "$component"); do
            echo "%dir $dir"
        done
        (cd "$root" && find . -type f | sed "s/^\.//" | sort)
        echo

        append_scriptlet "$component" rpm_pre "%pre"
        append_scriptlet "$component" rpm_post "%post"
        append_scriptlet "$component" rpm_preun "%preun"
        append_scriptlet "$component" rpm_postun "%postun"
    } > "$spec"

    rpmbuild -bb --define "_topdir $top" --target "$MACHINE" "$spec" > "$log" 2>&1 ||
        { cat "$log"; die "rpmbuild failed for $name"; }

    mv "$top/RPMS/$MACHINE/$name-$VERSION-1.$MACHINE.rpm" "$output"
    echo "  $output"
}

#--------------------------------------------------------------------------------------------------
# Build.
#--------------------------------------------------------------------------------------------------
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

echo "Building Aspia packages $VERSION for $MACHINE"
echo "  binaries: $BUILD_DIR"

for component in "${COMPONENTS[@]}"; do
    case "$component" in
        host|client|router|relay) ;;
        *) die "unknown component: $component" ;;
    esac

    binary="$BUILD_DIR/aspia_$component"
    [ -f "$binary" ] || die "binary not found: $binary"

    # The version of the binary is what the package claims to hold, so a build left over from an
    # earlier version is not packaged under the current one.
    built="$(grep -ao "$VERSION\.[0-9][0-9]*" "$binary" | head -1 || true)"
    [ -n "$built" ] || die "$binary does not carry version $VERSION"

    echo "$component $built"

    for format in "${FORMATS[@]}"; do
        case "$format" in
            deb) build_deb "$component" ;;
            rpm) build_rpm "$component" ;;
        esac
    done
done

echo "Done"
