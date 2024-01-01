//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef BASE_POSIX_EINTR_WRAPPER_H
#define BASE_POSIX_EINTR_WRAPPER_H

#include "build/build_config.h"

#if defined(OS_POSIX)

#include <errno.h>

#if defined(NDEBUG)

#define HANDLE_EINTR(x) ({ \
    decltype(x) eintr_wrapper_result; \
    do \
    { \
        eintr_wrapper_result = (x); \
    } while (eintr_wrapper_result == -1 && errno == EINTR); \
    eintr_wrapper_result; \
})

#else

#define HANDLE_EINTR(x) ({ \
    int eintr_wrapper_counter = 0; \
    decltype(x) eintr_wrapper_result; \
    do \
    { \
        eintr_wrapper_result = (x); \
    } while (eintr_wrapper_result == -1 && errno == EINTR &&  eintr_wrapper_counter++ < 100); \
    eintr_wrapper_result; \
})

#endif // NDEBUG

#define IGNORE_EINTR(x) ({ \
    decltype(x) eintr_wrapper_result; \
    do \
    { \
        eintr_wrapper_result = (x); \
        if (eintr_wrapper_result == -1 && errno == EINTR) \
        { \
            eintr_wrapper_result = 0; \
        } \
    } while (0); \
    eintr_wrapper_result; \
})

#else // !OS_POSIX

#define HANDLE_EINTR(x) (x)
#define IGNORE_EINTR(x) (x)

#endif // !OS_POSIX

#endif // BASE_POSIX_EINTR_WRAPPER_H

