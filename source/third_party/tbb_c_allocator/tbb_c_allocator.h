//
// Aspia Project
// Copyright (C) 2021 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef THIRD_PARTY__TBB_C_ALLOCATOR__TBB_C_ALLOCATOR_H
#define THIRD_PARTY__TBB_C_ALLOCATOR__TBB_C_ALLOCATOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void* tbb_c_malloc(size_t size);
void tbb_c_free(void* ptr);
void* tbb_c_realloc(void* ptr, size_t size);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif // THIRD_PARTY__TBB_C_ALLOCATOR__TBB_C_ALLOCATOR_H
