#
# Aspia Project
# Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

include(GNUInstallDirs)

# Qt configuration.
set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND CMAKE_AUTORCC_OPTIONS -compress 9 -threshold 5)

if (WIN32)
    # Target version.
    add_definitions(-DNTDDI_VERSION=0x06010000
                    -D_WIN32_WINNT=0x0601
                    -D_WIN32_WINDOWS=_WIN32_WINNT
                    -DWINVER=_WIN32_WINNT
                    -D_WIN32_IE=0x0800
                    -DPSAPI_VERSION=2)

    # Other definitions.
    add_definitions(-D_UNICODE
                    -DUNICODE
                    -DWIN32_LEAN_AND_MEAN
                    -DNOMINMAX)
endif()

# For Asio.
add_definitions(-DASIO_STANDALONE
                -DASIO_NO_DEPRECATED
                -D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)

# For Qt.
add_definitions(#-DQT_NO_CAST_FROM_ASCII
                -DQT_NO_CAST_TO_ASCII
                -DQT_NO_CAST_FROM_BYTEARRAY
                -DQT_USE_QSTRINGBUILDER)

set(QT_COMMON_LIBS
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    Qt6::Svg
    Qt6::Xml)

if (NOT ANDROID)
    list(APPEND QT_COMMON_LIBS
        Qt6::DeviceDiscoverySupportPrivate
        Qt6::FbSupportPrivate
        Qt6::QICOPlugin
        Qt6::PrintSupport Qt6::PrintSupport_resources_1 Qt6::PrintSupport_resources_2
        Qt6::Widgets_resources_1 Qt6::Widgets_resources_2)
endif()

if (WIN32)
    set(QT_PLATFORM_LIBS Qt6::QWindowsIntegrationPlugin)
endif()

if (LINUX)
    set(QT_PLATFORM_LIBS
        Qt6::XcbQpaPrivate
        Qt6::DBus)
endif()

if (APPLE)
    set(QT_PLATFORM_LIBS
        Qt6::OpenGL
        Qt6::QMacStylePlugin)
endif()

set(THIRD_PARTY_LIBS
    CURL::libcurl
    protobuf::libprotobuf-lite
    zstd::libzstd_static
    OpenSSL::Crypto
    OpenSSL::SSL
    Opus::opus
    ${OPENH264_LIBRARY}
    unofficial::libwebm::libwebm
    unofficial::libvpx::libvpx
    unofficial::enet::enet
    miniupnpc::miniupnpc
    unofficial::nayuki-qr-code-generator::nayuki-qr-code-generator
    unofficial::sqlite3::sqlite3
    vterm
    yuv)

if (LINUX OR APPLE)
    list(APPEND THIRD_PARTY_LIBS ICU::uc)
endif()

if (LINUX)
    list(APPEND THIRD_PARTY_LIBS freetype)
endif()

include_directories(${PROJECT_SOURCE_DIR}/source ${PROJECT_BINARY_DIR}/source)

# C++ compliller flags.
set(CMAKE_CXX_STANDARD 20)

message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_RUNTIME_OUTPUT_DIRECTORY: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "CMAKE_LIBRARY_OUTPUT_DIRECTORY: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message(STATUS "CMAKE_COMMAND: ${CMAKE_COMMAND}")
message(STATUS "CMAKE_CPACK_COMMAND: ${CMAKE_CPACK_COMMAND}")
message(STATUS "CMAKE_MAKE_PROGRAM: ${CMAKE_MAKE_PROGRAM}")
message(STATUS "CMAKE_GENERATOR: ${CMAKE_GENERATOR}")

if (CMAKE_BUILD_TYPE EQUAL "DEBUG")
    message(STATUS "DEBUG build detected")
else()
    message(STATUS "RELEASE build detected")
endif()

if (MSVC)
    # C++ compliller flags.
    # /Zi enables PDB with source and line info so the crash handler can symbolize frames.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /W3 /MP /Zi")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /W3 /MP")

    # C compiller flags.
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /W3 /MP /Zi")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /W3 /MP")

    if (${CMAKE_CL_64} EQUAL 0)
        message(STATUS "x86 compiller detected")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /arch:SSE2")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /arch:SSE2")
    else()
        message(STATUS "x64 compiller detected")
    endif()

    # Linker flags.
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /IGNORE:4099")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /IGNORE:4099")
endif()

if (LINUX)
    # C++ compliller flags.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC")

    # C compiller flags.
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fPIC")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fPIC")
endif()

# Compiller flags.
message(STATUS "CXX compiller version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CXX flags (release): ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "CXX flags (debug): ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "C compiller version: ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "C flags (release): ${CMAKE_C_FLAGS_RELEASE}")
message(STATUS "C flags (debug): ${CMAKE_C_FLAGS_DEBUG}")

# Linker flags.
message(STATUS "Linker flags (shared release): ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
message(STATUS "Linker flags (shared debug): ${CMAKE_SHARED_LINKER_FLAGS_DEBUG}")
message(STATUS "Linker flags (static release): ${CMAKE_STATIC_LINKER_FLAGS_RELEASE}")
message(STATUS "Linker flags (static debug): ${CMAKE_STATIC_LINKER_FLAGS_DEBUG}")
message(STATUS "Linker flags (exe release): ${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
message(STATUS "Linker flags (exe debug): ${CMAKE_EXE_LINKER_FLAGS_DEBUG}")

# Get git branch info.
execute_process(COMMAND git rev-parse --abbrev-ref HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_CURRENT_BRANCH
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get git commit hash.
execute_process(COMMAND git rev-parse HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_COMMIT_HASH
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get git commit count.
execute_process(COMMAND git rev-list --count HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_COMMIT_COUNT
                OUTPUT_STRIP_TRAILING_WHITESPACE)

if (NOT GIT_COMMIT_COUNT GREATER 0)
    message(STATUS "Failed to determine the number of commits")
    set(GIT_COMMIT_COUNT 0)
endif()

# Add git branch and commit hash to definitions.
add_definitions(-DGIT_CURRENT_BRANCH=\"${GIT_CURRENT_BRANCH}\"
                -DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"
                -DGIT_COMMIT_COUNT=${GIT_COMMIT_COUNT})

message(STATUS "Git branch: ${GIT_CURRENT_BRANCH}")
message(STATUS "Git commit: ${GIT_COMMIT_HASH}")
message(STATUS "Git commit count: ${GIT_COMMIT_COUNT}")

add_subdirectory(tools)
add_subdirectory(base)
add_subdirectory(client)
add_subdirectory(common)
add_subdirectory(proto)
add_subdirectory(third_party)

if (NOT ANDROID)
    add_subdirectory(drivers)
    add_subdirectory(relay)
    add_subdirectory(router)
endif()

if (WIN32 OR LINUX OR APPLE OR ANDROID)
    add_subdirectory(host)
endif()

###################################################################################################
# Installers
###################################################################################################

set(CPACK_PACKAGE_NAME "aspia")
set(CPACK_PACKAGE_VENDOR "Dmitry Chapyshev")
set(CPACK_PACKAGE_CONTACT "Dmitry Chapyshev <dmitry@aspia.ru>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://aspia.org/")
set(CPACK_PACKAGE_VERSION "${ASPIA_VERSION_MAJOR}.${ASPIA_VERSION_MINOR}.${ASPIA_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION_MAJOR "${ASPIA_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${ASPIA_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${ASPIA_VERSION_PATCH}")

#--------------------------------------------------------------------------------------------------
# Host
#--------------------------------------------------------------------------------------------------

if (LINUX)
    install(TARGETS
        aspia_host
            BUNDLE DESTINATION . COMPONENT host
            RUNTIME DESTINATION bin COMPONENT host
            LIBRARY DESTINATION lib COMPONENT host)
    set(CPACK_COMPONENT_HOST_NAME "aspia-host")
    set(CPACK_COMPONENT_HOST_DISPLAY_NAME "Aspia Host")
    set(CPACK_COMPONENT_HOST_DESCRIPTION "Remote desktop software.")

    if (LINUX)
        # Desktop file.
        install(FILES host/linux/aspia-host.desktop
            DESTINATION share/applications COMPONENT host)

        # Desktop file for the desktop agent. KWin authorizes org.kde.KWin.ScreenShot2 by matching the
        # caller's executable path to a .desktop that declares X-KDE-DBUS-Restricted-Interfaces.
        install(FILES host/linux/aspia-desktop-agent.desktop
            DESTINATION share/applications COMPONENT host)

        # Application icon.
        install(FILES common/resources/aspia.svg
            DESTINATION share/icons/hicolor/scalable/apps RENAME aspia-host.svg COMPONENT host)

        # Service file.
        install(FILES host/linux/aspia-host-service.service
            DESTINATION lib/systemd/system COMPONENT host)

        # PolicyKit action for the elevated settings/security-log dialogs.
        install(FILES host/linux/org.aspia.host.policy
            DESTINATION share/polkit-1/actions COMPONENT host)

        # PAM policy used to validate operating system credentials for terminal sessions. The policy
        # pulls in the distribution's aggregate auth/account stack, which is named differently across
        # families: Debian/Ubuntu use common-auth/common-account, while RHEL/Fedora and SUSE use
        # system-auth. Ship both variants; each package's maintainer script (postinst/rpm_post) installs
        # the matching one to /etc/pam.d/aspia-terminal. This picks the policy per package rather than by
        # the build host's distribution, so both packages can be built on a single machine.
        install(FILES host/linux/aspia-terminal.pam host/linux/aspia-terminal-rhel.pam
            DESTINATION share/aspia COMPONENT host)

        # The host runs without prior configuration, so the maintainer scripts enable and start the
        # systemd service directly (and restart it on upgrade) rather than via --install.
        set(CPACK_DEBIAN_HOST_PACKAGE_CONTROL_EXTRA
            "${CMAKE_CURRENT_SOURCE_DIR}/host/linux/postinst;${CMAKE_CURRENT_SOURCE_DIR}/host/linux/prerm")

        set(CPACK_DEBIAN_HOST_FILE_NAME
            "${CPACK_COMPONENT_HOST_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.deb")

        # RPM equivalents of the maintainer scripts (RPM scriptlet argument semantics differ from
        # Debian: %post runs on install and upgrade, %preun receives 0 only on final removal).
        set(CPACK_RPM_HOST_POST_INSTALL_SCRIPT_FILE
            "${CMAKE_CURRENT_SOURCE_DIR}/host/linux/rpm_post")
        set(CPACK_RPM_HOST_PRE_UNINSTALL_SCRIPT_FILE
            "${CMAKE_CURRENT_SOURCE_DIR}/host/linux/rpm_preun")
        set(CPACK_RPM_HOST_PACKAGE_NAME "${CPACK_COMPONENT_HOST_NAME}")
        set(CPACK_RPM_HOST_FILE_NAME
            "${CPACK_COMPONENT_HOST_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.rpm")
    endif()

    if (APPLE)
        set(CPACK_DMG_HOST_FILE_NAME "${CPACK_COMPONENT_HOST_NAME}-${CPACK_PACKAGE_VERSION}")
    endif()
endif()

#--------------------------------------------------------------------------------------------------
# Client
#--------------------------------------------------------------------------------------------------

if (NOT ANDROID)
    install(TARGETS aspia_client
        BUNDLE DESTINATION . COMPONENT client
        RUNTIME DESTINATION bin COMPONENT client)
endif()
set(CPACK_COMPONENT_CLIENT_NAME "aspia-client")
set(CPACK_COMPONENT_CLIENT_DISPLAY_NAME "Aspia Client")
set(CPACK_COMPONENT_CLIENT_DESCRIPTION "Client for managing hosts.")

if (LINUX)
    # Desktop file.
    install(FILES client/linux/aspia-client.desktop
        DESTINATION share/applications COMPONENT client)

    # Application icon.
    install(FILES common/resources/aspia.svg
        DESTINATION share/icons/hicolor/scalable/apps RENAME aspia-client.svg COMPONENT client)

    set(CPACK_DEBIAN_CLIENT_FILE_NAME
        "${CPACK_COMPONENT_CLIENT_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.deb")

    set(CPACK_RPM_CLIENT_PACKAGE_NAME "${CPACK_COMPONENT_CLIENT_NAME}")
    set(CPACK_RPM_CLIENT_FILE_NAME
        "${CPACK_COMPONENT_CLIENT_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.rpm")
endif()

if (APPLE)
    set(CPACK_DMG_CLIENT_FILE_NAME "${CPACK_COMPONENT_CLIENT_NAME}-${CPACK_PACKAGE_VERSION}")
endif()

#--------------------------------------------------------------------------------------------------
# Router
#--------------------------------------------------------------------------------------------------

if (LINUX)
    install(TARGETS aspia_router
        BUNDLE DESTINATION . COMPONENT router
        RUNTIME DESTINATION bin COMPONENT router)
    set(CPACK_COMPONENT_ROUTER_NAME "aspia-router")
    set(CPACK_COMPONENT_ROUTER_DISPLAY_NAME "Aspia Router")
    set(CPACK_COMPONENT_ROUTER_DESCRIPTION "Provides a connection routing service.")

    # The service is registered by the application itself (aspia_router --install/--remove), invoked
    # from the package maintainer scripts, so the account and sandboxing are configured in one place.
    set(CPACK_DEBIAN_ROUTER_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_SOURCE_DIR}/router/linux/postinst;${CMAKE_CURRENT_SOURCE_DIR}/router/linux/prerm")

    set(CPACK_DEBIAN_ROUTER_FILE_NAME
        "${CPACK_COMPONENT_ROUTER_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.deb")

    set(CPACK_RPM_ROUTER_POST_INSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/router/linux/rpm_post")
    set(CPACK_RPM_ROUTER_PRE_UNINSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/router/linux/rpm_preun")
    set(CPACK_RPM_ROUTER_PACKAGE_NAME "${CPACK_COMPONENT_ROUTER_NAME}")
    set(CPACK_RPM_ROUTER_FILE_NAME
        "${CPACK_COMPONENT_ROUTER_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.rpm")
endif()

#--------------------------------------------------------------------------------------------------
# Relay
#--------------------------------------------------------------------------------------------------

if (LINUX)
    install(TARGETS aspia_relay
        BUNDLE DESTINATION . COMPONENT relay
        RUNTIME DESTINATION bin COMPONENT relay)
    set(CPACK_COMPONENT_RELAY_NAME "aspia-relay")
    set(CPACK_COMPONENT_RELAY_DISPLAY_NAME "Aspia Relay")
    set(CPACK_COMPONENT_RELAY_DESCRIPTION "Provides a service for transferring data between peers.")

    # The service is registered by the application itself (aspia_relay --install/--remove), invoked
    # from the package maintainer scripts, so the account and sandboxing are configured in one place.
    set(CPACK_DEBIAN_RELAY_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_SOURCE_DIR}/relay/linux/postinst;${CMAKE_CURRENT_SOURCE_DIR}/relay/linux/prerm")

    set(CPACK_DEBIAN_RELAY_FILE_NAME
        "${CPACK_COMPONENT_RELAY_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.deb")

    set(CPACK_RPM_RELAY_POST_INSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/relay/linux/rpm_post")
    set(CPACK_RPM_RELAY_PRE_UNINSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/relay/linux/rpm_preun")
    set(CPACK_RPM_RELAY_PACKAGE_NAME "${CPACK_COMPONENT_RELAY_NAME}")
    set(CPACK_RPM_RELAY_FILE_NAME
        "${CPACK_COMPONENT_RELAY_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.rpm")
endif()

if (LINUX)
    # Build both Debian (.deb) and RPM (.rpm) packages in one cpack run. RPM requires rpmbuild on the
    # build host. Auto-dependency generation (find-requires, on by default for both) resolves to the
    # SONAMEs of linked libraries, which are identical across distributions, so the RPMs built here are
    # installable on RHEL-family systems (e.g. AlmaLinux).
    set(CPACK_GENERATOR "DEB;RPM")

    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

    set(CPACK_RPM_COMPONENT_INSTALL ON)
    set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")

    # Do not let the RPM claim ownership of system directories that other packages (systemd, pam, polkit,
    # hicolor-icon-theme, ...) already own - otherwise rpm reports file conflicts on install. Only the
    # files placed inside them are packaged.
    set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        "/etc/pam.d"
        "/usr/lib/systemd" "/usr/lib/systemd/system"
        "/usr/share/applications"
        "/usr/share/polkit-1" "/usr/share/polkit-1/actions"
        "/usr/share/icons" "/usr/share/icons/hicolor"
        "/usr/share/icons/hicolor/16x16" "/usr/share/icons/hicolor/16x16/apps"
        "/usr/share/icons/hicolor/24x24" "/usr/share/icons/hicolor/24x24/apps"
        "/usr/share/icons/hicolor/32x32" "/usr/share/icons/hicolor/32x32/apps"
        "/usr/share/icons/hicolor/48x48" "/usr/share/icons/hicolor/48x48/apps"
        "/usr/share/icons/hicolor/64x64" "/usr/share/icons/hicolor/64x64/apps"
        "/usr/share/icons/hicolor/128x128" "/usr/share/icons/hicolor/128x128/apps")

    set(CPACK_COMPONENTS_ALL host client router relay)
endif()

if (APPLE)
    set(CPACK_GENERATOR "DragNDrop")

    set(CPACK_COMPONENTS_GROUPING "IGNORE")
    set(CPACK_COMPONENTS_ALL client)
endif()

include(CPack)
