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

if (CMAKE_CROSSCOMPILING)
    # |qtprotoc| is a protoc plugin and runs on the build host, so for cross builds it is compiled
    # as a separate host project against the host protobuf installed by vcpkg.
    include(ExternalProject)

    if (CMAKE_HOST_WIN32)
        set(QTPROTOC_HOST_SUFFIX ".exe")
    else()
        set(QTPROTOC_HOST_SUFFIX "")
    endif()

    set(QTPROTOC_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/qtprotoc_host)
    set(QTPROTOC_EXECUTABLE ${QTPROTOC_BINARY_DIR}/qtprotoc${QTPROTOC_HOST_SUFFIX}
        CACHE INTERNAL "Path to the host qtprotoc executable")

    set(QTPROTOC_CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_CONFIGURATION_TYPES=Release
        -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${QTPROTOC_BINARY_DIR}
        -DCMAKE_PREFIX_PATH=${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET})

    ExternalProject_Add(qtprotoc_host
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/qtprotoc
        BINARY_DIR ${QTPROTOC_BINARY_DIR}
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR> ${QTPROTOC_CMAKE_ARGS}
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS ${QTPROTOC_EXECUTABLE})
else()
    add_subdirectory(qtprotoc)
endif()

if (WIN32)
    add_subdirectory(sha256)
endif()
