#
# Aspia Project
# Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

include(CheckIPOSupported)
include(GNUInstallDirs)

# Qt configuration.
set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND CMAKE_AUTORCC_OPTIONS -compress 9 -threshold 5)

if(NOT Qt5LinguistTools_FOUND)
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
    add_definitions(-DI18L_DISABLED)
endif()

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

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "AMD64" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86")
    # For RapidJSON.
    add_definitions(-DRAPIDJSON_SSE2)
endif()

# For Qt.
add_definitions(-DQT_NO_CAST_TO_ASCII
                -DQT_NO_CAST_FROM_BYTEARRAY
                -DQT_USE_QSTRINGBUILDER)

set(QT_COMMON_LIBS
    Qt5::Core
    Qt5::CorePrivate
    Qt5::QGenericEnginePlugin
    Qt5::Gui
    Qt5::GuiPrivate
    Qt5::QICOPlugin
    Qt5::Network
    Qt5::NetworkPrivate
    Qt5::PrintSupport
    Qt5::PrintSupportPrivate
    Qt5::Widgets
    Qt5::WidgetsPrivate
    Qt5::Xml
    Qt5::XmlPrivate)

if (WIN32)
    set(QT_PLATFORM_LIBS
        Qt5::WinMain
        Qt5::WinExtras
        Qt5::WinExtrasPrivate
        Qt5::QWindowsIntegrationPlugin
        Qt5::QWindowsVistaStylePlugin
        Qt5::QWindowsPrinterSupportPlugin)
endif()

set(THIRD_PARTY_LIBS
    CURL::libcurl
    fmt::fmt-header-only
    protobuf::libprotobuf-lite
    zstd::libzstd_static
    OpenSSL::Crypto
    OpenSSL::SSL
    Opus::opus
    modp_b64
    libwebm
    unofficial::libvpx::libvpx
    x11region
    yuv)

include_directories(${PROJECT_SOURCE_DIR}/source ${PROJECT_BINARY_DIR}/source ${WTL_INCLUDE_DIRS})

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
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /W3 /MP")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /W3 /MP")

    # C compiller flags.
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /W3 /MP")
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

    # Static runtime library.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if (LINUX)
    # C++ compliller flags.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC")

    # C compiller flags.
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fPIC")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fPIC")
endif()

check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT error)

if (IPO_SUPPORTED)
    if (LINUX)
        message(STATUS "IPO/LTO supported but disabled for Linux")
    else()
        message(STATUS "IPO/LTO supported")

        # Enable link-time code generatation.
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
else()
    message(STATUS "IPO/LTO not supported")
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

add_subdirectory(base)
add_subdirectory(client)
add_subdirectory(common)
add_subdirectory(console)
add_subdirectory(drivers)
add_subdirectory(proto)
add_subdirectory(relay)
add_subdirectory(router)
add_subdirectory(third_party)
add_subdirectory(tools)

if (WIN32 OR LINUX)
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
        aspia_host_core
        aspia_host_service
        aspia_host
        aspia_desktop_agent
        aspia_file_transfer_agent
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

        # Host icons.
        install(FILES host/linux/icons/16x16/aspia_host.png
            DESTINATION share/icons/hicolor/16x16/apps COMPONENT host)
        install(FILES host/linux/icons/24x24/aspia_host.png
            DESTINATION share/icons/hicolor/24x24/apps COMPONENT host)
        install(FILES host/linux/icons/32x32/aspia_host.png
            DESTINATION share/icons/hicolor/32x32/apps COMPONENT host)
        install(FILES host/linux/icons/48x48/aspia_host.png
            DESTINATION share/icons/hicolor/48x48/apps COMPONENT host)
        install(FILES host/linux/icons/64x64/aspia_host.png
            DESTINATION share/icons/hicolor/64x64/apps COMPONENT host)

        # Service file.
        install(FILES host/linux/aspia-host-service.service
            DESTINATION lib/systemd/system COMPONENT host)

        set(CPACK_DEBIAN_HOST_FILE_NAME
            "${CPACK_COMPONENT_HOST_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.deb")
    endif()

    if (APPLE)
        set(CPACK_DMG_HOST_FILE_NAME "${CPACK_COMPONENT_HOST_NAME}-${CPACK_PACKAGE_VERSION}")
    endif()
endif()

#--------------------------------------------------------------------------------------------------
# Console
#--------------------------------------------------------------------------------------------------

install(TARGETS aspia_console
    BUNDLE DESTINATION . COMPONENT console
    RUNTIME DESTINATION bin COMPONENT console)
set(CPACK_COMPONENT_CONSOLE_NAME "aspia-console")
set(CPACK_COMPONENT_CONSOLE_DISPLAY_NAME "Aspia Console")
set(CPACK_COMPONENT_CONSOLE_DESCRIPTION "Console for managing hosts and routers.")

if (LINUX)
    # Desktop file.
    install(FILES console/linux/aspia-console.desktop
        DESTINATION share/applications COMPONENT console)

    # Mime types.
    install(FILES console/linux/aspia-console.xml
        DESTINATION share/mime/packages COMPONENT console)
    install(CODE "execute_process(COMMAND update-mime-database /usr/share/mime)")

    # Console icons.
    install(FILES console/linux/icons/16x16/aspia_console.png
        DESTINATION share/icons/hicolor/16x16/apps COMPONENT console)
    install(FILES console/linux/icons/24x24/aspia_console.png
        DESTINATION share/icons/hicolor/24x24/apps COMPONENT console)
    install(FILES console/linux/icons/32x32/aspia_console.png
        DESTINATION share/icons/hicolor/32x32/apps COMPONENT console)
    install(FILES console/linux/icons/48x48/aspia_console.png
        DESTINATION share/icons/hicolor/48x48/apps COMPONENT console)
    install(FILES console/linux/icons/64x64/aspia_console.png
        DESTINATION share/icons/hicolor/64x64/apps COMPONENT console)
    install(FILES console/linux/icons/128x128/aspia_console.png
        DESTINATION share/icons/hicolor/128x128/apps COMPONENT console)

    # Address book icons.
    install(FILES console/linux/icons/16x16/aspia_address_book.png
        DESTINATION share/icons/hicolor/16x16/mimetypes COMPONENT console)
    install(FILES console/linux/icons/24x24/aspia_address_book.png
        DESTINATION share/icons/hicolor/24x24/mimetypes COMPONENT console)
    install(FILES console/linux/icons/32x32/aspia_address_book.png
        DESTINATION share/icons/hicolor/32x32/mimetypes COMPONENT console)
    install(FILES console/linux/icons/48x48/aspia_address_book.png
        DESTINATION share/icons/hicolor/48x48/mimetypes COMPONENT console)

    set(CPACK_DEBIAN_CONSOLE_FILE_NAME
        "${CPACK_COMPONENT_CONSOLE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.deb")
endif()

if (APPLE)
    set(CPACK_DMG_CONSOLE_FILE_NAME "${CPACK_COMPONENT_CONSOLE_NAME}-${CPACK_PACKAGE_VERSION}")
endif()

#--------------------------------------------------------------------------------------------------
# Client
#--------------------------------------------------------------------------------------------------

install(TARGETS aspia_client
    BUNDLE DESTINATION . COMPONENT client
    RUNTIME DESTINATION bin COMPONENT client)
set(CPACK_COMPONENT_CLIENT_NAME "aspia-client")
set(CPACK_COMPONENT_CLIENT_DISPLAY_NAME "Aspia Client")
set(CPACK_COMPONENT_CLIENT_DESCRIPTION "Client for managing hosts.")

if (LINUX)
    # Desktop file.
    install(FILES client/linux/aspia-client.desktop
        DESTINATION share/applications COMPONENT client)

    # Icons.
    install(FILES client/linux/icons/16x16/aspia_client.png
        DESTINATION share/icons/hicolor/16x16/apps COMPONENT client)
    install(FILES client/linux/icons/24x24/aspia_client.png
        DESTINATION share/icons/hicolor/24x24/apps COMPONENT client)
    install(FILES client/linux/icons/32x32/aspia_client.png
        DESTINATION share/icons/hicolor/32x32/apps COMPONENT client)
    install(FILES client/linux/icons/48x48/aspia_client.png
        DESTINATION share/icons/hicolor/48x48/apps COMPONENT client)
    install(FILES client/linux/icons/64x64/aspia_client.png
        DESTINATION share/icons/hicolor/64x64/apps COMPONENT client)
    install(FILES client/linux/icons/128x128/aspia_client.png
        DESTINATION share/icons/hicolor/128x128/apps COMPONENT client)

    set(CPACK_DEBIAN_CLIENT_FILE_NAME
        "${CPACK_COMPONENT_CLIENT_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.deb")
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

    # Service file.
    install(FILES router/linux/aspia-router.service DESTINATION lib/systemd/system COMPONENT router)

    set(CPACK_DEBIAN_ROUTER_FILE_NAME
        "${CPACK_COMPONENT_ROUTER_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.deb")
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

    # Service file.
    install(FILES relay/linux/aspia-relay.service DESTINATION lib/systemd/system COMPONENT relay)

    set(CPACK_DEBIAN_RELAY_FILE_NAME
        "${CPACK_COMPONENT_RELAY_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.deb")
endif()

if (LINUX)
    set(CPACK_GENERATOR "DEB")

    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

    set(CPACK_COMPONENTS_ALL host console client router relay)
endif()

if (APPLE)
    set(CPACK_GENERATOR "DragNDrop")

    set(CPACK_COMPONENTS_GROUPING "IGNORE")
    set(CPACK_COMPONENTS_ALL console client)
endif()

include(CPack)
