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

list(APPEND SOURCE_ENET
    callbacks.c
    compress.c
    host.c
    list.c
    packet.c
    peer.c
    protocol.c
    unix.c
    win32.c
    include/enet/callbacks.h
    include/enet/enet.h
    include/enet/list.h
    include/enet/protocol.h
    include/enet/time.h
    include/enet/types.h
    include/enet/unix.h
    include/enet/utility.h
    include/enet/win32.h)

source_group("" FILES ${SOURCE_ENET})

add_library(enet STATIC ${SOURCE_ENET})
target_include_directories(enet PUBLIC include)
set_target_properties(enet PROPERTIES LANGUAGE C LINKER_LANGUAGE C C_STANDARD 99)
set_target_properties(enet PROPERTIES FOLDER third_party)

# Vendored third-party code - silence its compiler warnings.
if(MSVC)
    target_compile_options(enet PRIVATE /wd4996 /wd5287)
else()
    target_compile_options(enet PRIVATE -w)
endif()

# The same checks as the CMakeLists.txt of ENet does. Only unix.c uses them.
if (NOT WIN32)
    include(CheckFunctionExists)
    include(CheckStructHasMember)
    include(CheckTypeSize)

    check_function_exists("fcntl" HAS_FCNTL)
    check_function_exists("poll" HAS_POLL)
    check_function_exists("getaddrinfo" HAS_GETADDRINFO)
    check_function_exists("getnameinfo" HAS_GETNAMEINFO)
    check_function_exists("gethostbyname_r" HAS_GETHOSTBYNAME_R)
    check_function_exists("gethostbyaddr_r" HAS_GETHOSTBYADDR_R)
    check_function_exists("inet_pton" HAS_INET_PTON)
    check_function_exists("inet_ntop" HAS_INET_NTOP)
    check_struct_has_member("struct msghdr" "msg_flags" "sys/types.h;sys/socket.h" HAS_MSGHDR_FLAGS)
    set(CMAKE_EXTRA_INCLUDE_FILES "sys/types.h" "sys/socket.h")
    check_type_size("socklen_t" HAS_SOCKLEN_T BUILTIN_TYPES_ONLY)
    unset(CMAKE_EXTRA_INCLUDE_FILES)

    foreach(ENET_CHECK HAS_FCNTL HAS_POLL HAS_GETADDRINFO HAS_GETNAMEINFO HAS_GETHOSTBYNAME_R
                       HAS_GETHOSTBYADDR_R HAS_INET_PTON HAS_INET_NTOP HAS_MSGHDR_FLAGS HAS_SOCKLEN_T)
        if (${ENET_CHECK})
            target_compile_definitions(enet PRIVATE ${ENET_CHECK}=1)
        endif()
    endforeach()
endif()
